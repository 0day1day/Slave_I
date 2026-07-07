#include "esp32_ble_scanner.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <host/ble_gap.h>
#include <esp_random.h>
#include "domain/radio/ble_vendor.hpp"
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_adv.h>
#include <host/ble_store.h>
#include <host/util/util.h>
#include <nimble/nimble_npl.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>

#include "core/diagnostics/log.hpp"
#include "services/radio/radio_coordinator.hpp"

namespace spectra5::platform {

using namespace spectra5::domain;

namespace {

constexpr const char* kTag        = "ble-scan";
constexpr std::size_t kHistoryMax = 48;
constexpr EventBits_t kRadioIdleBit = BIT0;

Esp32BleScanner* g_instance = nullptr;

struct ble_npl_event g_host_work_ev{};
bool g_host_work_inited = false;

void host_work_fn(struct ble_npl_event*)
{
    if (g_instance != nullptr) {
        g_instance->apply_host_state();
    }
}

int gap_event_trampoline(struct ble_gap_event* event, void* arg)
{
    auto* self = static_cast<Esp32BleScanner*>(arg);
    if (self == nullptr || event == nullptr) {
        return 0;
    }
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            if (!self->wants_scan()) {
                self->apply_host_state();
                return 0;
            }
            self->on_disc(&event->disc);
            return 0;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            self->on_disc_complete();
            return 0;
        default:
            return 0;
    }
}

BleAddressType map_addr_type(uint8_t type)
{
    switch (type) {
        case BLE_ADDR_PUBLIC:
            return BleAddressType::Public;
        case BLE_ADDR_RANDOM:
            return BleAddressType::Random;
        default:
            return BleAddressType::Unknown;
    }
}

std::string format_addr(const uint8_t addr[6])
{
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3],
                  addr[4], addr[5]);
    return buf;
}

std::string parse_name(const struct ble_hs_adv_fields& fields)
{
    if (fields.name != nullptr && fields.name_len > 0) {
        return std::string(reinterpret_cast<const char*>(fields.name),
                           static_cast<std::size_t>(fields.name_len));
    }
    return {};
}

}  // namespace

Esp32BleScanner::Esp32BleScanner(IClock& clock) : clock_(clock)
{
    g_instance = this;
    cmd_queue_ = xQueueCreate(8, sizeof(Cmd));
    events_    = xEventGroupCreate();
    if (events_ != nullptr) {
        xEventGroupSetBits(events_, kRadioIdleBit);
    }
}

Esp32BleScanner::~Esp32BleScanner()
{
    release_radio();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    if (cmd_queue_ != nullptr) {
        const Cmd wake = Cmd::Wake;
        xQueueSend(cmd_queue_, &wake, 0);
    }
    for (int i = 0; i < 80 && ctrl_task_ != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (events_ != nullptr) {
        vEventGroupDelete(events_);
        events_ = nullptr;
    }
    if (cmd_queue_ != nullptr) {
        vQueueDelete(cmd_queue_);
        cmd_queue_ = nullptr;
    }
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

bool Esp32BleScanner::wants_scan() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return scanning_ && !spam_;  // advertising and scanning are mutually exclusive
}

bool Esp32BleScanner::is_scanning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return scanning_;
}

bool Esp32BleScanner::is_radio_idle() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    // The NimBLE host stays initialised for the firmware lifetime; "idle" means
    // no active discovery on the C6 radio.
    return !disc_active_ && !scanning_ && !release_pending_;
}

bool Esp32BleScanner::ensure_ctrl_task()
{
    if (cmd_queue_ == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (ctrl_task_ != nullptr) {
        return true;
    }
    if (xTaskCreate(&Esp32BleScanner::ctrl_task_trampoline, "ble_ctrl", 12288, this, 4, &ctrl_task_) !=
        pdPASS) {
        spectra5::log::tagError(kTag, "failed to create ble_ctrl task");
        return false;
    }
    return true;
}

void Esp32BleScanner::shutdown_radio_stack()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = false;
        spam_     = false;  // stop advertising spam too, else apply_host_state restarts it
    }

    if (host_running_) {
        stop_adv();  // cancel any spam advertising before freeing the C6 radio
        // Cancel discovery but keep the NimBLE host alive. Tearing the host
        // down (nimble_port_stop -> host task self-delete) makes ESP-IDF flush
        // that task's newlib stdio buffer (vfprintf) from the IDLE task, which
        // overflows IDLE's small stack and panics. Cancelling the scan stops
        // BLE activity on the C6 radio, which is all the Wi-Fi/BLE hand-off
        // needs, and lets us resume scanning instantly later.
        schedule_host_work();
        for (int i = 0; i < 120; ++i) {
            if (!disc_active_) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        spectra5::log::tagInfo(kTag, "BLE discovery cancelled; C6 radio free");
    }

    release_pending_ = false;
    if (events_ != nullptr) {
        xEventGroupSetBits(events_, kRadioIdleBit);
    }
}

bool Esp32BleScanner::init_nimble_stack()
{
    if (host_running_) {
        if (events_ != nullptr) {
            xEventGroupClearBits(events_, kRadioIdleBit);
        }
        return true;
    }

    // nimble_port_init() initialises the host package exactly once. We never
    // call nimble_port_deinit() (its ble_transport_ll_deinit is unavailable on
    // this esp_hosted build), so the package stays initialised for the lifetime
    // of the firmware. Re-enabling BLE after a stop only recreates the host
    // task and re-schedules the host start.
    const bool first_init = !nimble_inited_;
    if (first_init) {
        const esp_err_t ret = nimble_port_init();
        if (ret != ESP_OK) {
            spectra5::log::tagError(kTag, "nimble_port_init failed: {}", esp_err_to_name(ret));
            return false;
        }
        nimble_inited_ = true;
    }

    ble_hs_cfg.reset_cb        = &Esp32BleScanner::on_reset;
    ble_hs_cfg.sync_cb         = &Esp32BleScanner::on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    host_synced_ = false;
    nimble_port_freertos_init(&Esp32BleScanner::host_task);

    // On the first init, ble_hs_init() already queued the start (BLE_HS_AUTO_START).
    // Queuing it again here would make ble_hs_start() return BLE_HS_EALREADY and
    // trip the assert in ble_hs_event_start_stage2. After a stop we must queue it
    // ourselves because the host state was reset to OFF.
    if (!first_init) {
        ble_hs_sched_start();
    }

    host_running_ = true;
    if (events_ != nullptr) {
        xEventGroupClearBits(events_, kRadioIdleBit);
    }
    spectra5::log::tagInfo(kTag, "NimBLE host running ({} init)", first_init ? "first" : "restart");
    return true;
}

void Esp32BleScanner::schedule_host_work()
{
    if (!host_running_) {
        return;
    }
    if (!g_host_work_inited) {
        ble_npl_event_init(&g_host_work_ev, host_work_fn, nullptr);
        g_host_work_inited = true;
    }
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &g_host_work_ev);
}

void Esp32BleScanner::start()
{
    if (auto* coordinator = services::radio_coordinator()) {
        if (!coordinator->acquire_for_ble()) {
            spectra5::log::tagError(kTag, "could not acquire C6 radio for BLE");
            return;
        }
    }
    if (!ensure_ctrl_task()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = true;
    }
    const Cmd wake = Cmd::Wake;
    xQueueSend(cmd_queue_, &wake, 0);
}

void Esp32BleScanner::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = false;
    }
    if (cmd_queue_ != nullptr) {
        const Cmd wake = Cmd::Wake;
        xQueueSend(cmd_queue_, &wake, 0);
    }
}

void Esp32BleScanner::release_radio()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!scanning_ && !disc_active_ && !release_pending_) {
            return;
        }
        scanning_        = false;
        release_pending_ = true;
    }
    if (!ensure_ctrl_task()) {
        shutdown_radio_stack();
        return;
    }
    const Cmd cmd = Cmd::ReleaseRadio;
    xQueueSend(cmd_queue_, &cmd, 0);
    if (events_ != nullptr) {
        xEventGroupWaitBits(events_, kRadioIdleBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    }
}

void Esp32BleScanner::on_reset(int reason)
{
    spectra5::log::tagError(kTag, "NimBLE reset, reason={}", reason);
    if (g_instance != nullptr) {
        std::lock_guard<std::mutex> lock(g_instance->mutex_);
        g_instance->host_synced_ = false;
        g_instance->disc_active_ = false;
    }
}

void Esp32BleScanner::on_sync()
{
    if (g_instance != nullptr) {
        g_instance->on_host_sync();
    }
}

void Esp32BleScanner::on_host_sync()
{
    const int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        spectra5::log::tagError(kTag, "ensure_addr failed: {}", rc);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_synced_ = true;
    }
    spectra5::log::tagInfo(kTag, "NimBLE host synced (C6 VHCI)");
    apply_host_state();
}

void Esp32BleScanner::apply_host_state()
{
    if (!host_synced_) {
        return;
    }
    if (spam_) {
        if (disc_active_) {
            cancel_scan();
        }
        if (!adv_active_) {
            begin_spam();
        }
        return;
    }
    if (adv_active_) {
        stop_adv();
    }
    if (wants_scan() && !disc_active_) {
        begin_scan();
    } else if (!wants_scan() && disc_active_) {
        cancel_scan();
    }
}

void Esp32BleScanner::begin_scan()
{
    if (!host_synced_ || disc_active_) {
        return;
    }

    uint8_t own_addr_type = 0;
    const int rc_id       = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc_id != 0) {
        spectra5::log::tagError(kTag, "infer addr type failed: {}", rc_id);
        return;
    }

    ble_gap_disc_params params{};
    params.filter_duplicates = 0;
    params.passive           = 1;

    const int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, gap_event_trampoline, this);
    if (rc != 0) {
        spectra5::log::tagError(kTag, "ble_gap_disc failed: {}", rc);
        disc_active_ = false;
        return;
    }
    disc_active_ = true;
}

void Esp32BleScanner::cancel_scan()
{
    if (!disc_active_) {
        return;
    }
    const int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        spectra5::log::tagError(kTag, "ble_gap_disc_cancel failed: {}", rc);
    }
    disc_active_ = false;
}

namespace {
// Apple "proximity pairing" device models -> each shows a different popup on
// nearby iPhones/iPads (AirPods, AirPods Pro/Max, Apple TV setup, etc.).
const uint8_t kAppleModels[][2] = {
    {0x0e, 0x20}, {0x0a, 0x20}, {0x0b, 0x20}, {0x0c, 0x20}, {0x11, 0x20},
    {0x02, 0x20}, {0x0f, 0x20}, {0x13, 0x20}, {0x14, 0x20}, {0x03, 0x20},
};

// --- Per-vendor BLE advertisement builders (write into adv[31], return length) ---

int build_apple(uint8_t* adv)
{
    const auto& model = kAppleModels[esp_random() % (sizeof(kAppleModels) / 2)];
    adv[0] = 0x1e;
    adv[1] = 0xff;
    adv[2] = 0x4c;
    adv[3] = 0x00;  // Apple 0x004C
    adv[4] = 0x07;
    adv[5] = 0x19;
    adv[6] = 0x07;
    adv[7] = model[0];
    adv[8] = model[1];
    adv[9] = 0x55;
    esp_fill_random(&adv[10], 21);
    return 31;
}

// Microsoft Swift Pair: Windows "Add a device?" toast. Company 0x0006 + name.
int build_microsoft(uint8_t* adv)
{
    static const char* const kNames[] = {"Spectra5 Mouse", "Wireless KB", "BT Speaker",
                                          "Surface Pen"};
    const char* name = kNames[esp_random() % 4];
    int nlen         = static_cast<int>(strlen(name));
    if (nlen > 18) {
        nlen = 18;
    }
    int i      = 0;
    adv[i++]   = static_cast<uint8_t>(6 + nlen);  // AD length
    adv[i++]   = 0xff;                            // manufacturer specific
    adv[i++]   = 0x06;                            // Microsoft 0x0006
    adv[i++]   = 0x00;
    adv[i++]   = 0x03;  // Swift Pair beacon
    adv[i++]   = 0x00;
    adv[i++]   = 0x80;  // flags
    memcpy(&adv[i], name, nlen);
    i += nlen;
    return i;
}

// Google Fast Pair: Android "device nearby" popup. Service data UUID 0xFE2C + model.
int build_google(uint8_t* adv)
{
    static const uint8_t kModels[][3] = {
        {0xcd, 0x82, 0x56}, {0x00, 0x00, 0x07}, {0xf5, 0x24, 0x94},
        {0x14, 0x00, 0x09}, {0x0e, 0x39, 0x9e}, {0x9a, 0xda, 0xf2},
    };
    const auto& m = kModels[esp_random() % (sizeof(kModels) / 3)];
    int i         = 0;
    adv[i++]      = 0x03;  // complete list of 16-bit service UUIDs
    adv[i++]      = 0x03;
    adv[i++]      = 0x2c;
    adv[i++]      = 0xfe;  // 0xFE2C
    adv[i++]      = 0x06;  // service data
    adv[i++]      = 0x16;
    adv[i++]      = 0x2c;
    adv[i++]      = 0xfe;  // 0xFE2C
    adv[i++]      = m[0];
    adv[i++]      = m[1];
    adv[i++]      = m[2];
    return i;
}

// Samsung Galaxy Buds pairing popup. Company 0x0075 + buds payload (best-effort).
int build_samsung(uint8_t* adv)
{
    static const uint8_t kBuds[][2] = {
        {0x01, 0x01}, {0x9d, 0x17}, {0x39, 0x07}, {0xa6, 0x01}, {0xa4, 0x01},
    };
    const auto& b = kBuds[esp_random() % (sizeof(kBuds) / 2)];
    const uint8_t body[] = {0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21, 0x01, 0x09,
                            b[0], b[1], 0x06, 0x3c, 0x94, 0x8e, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x00};
    int i    = 0;
    adv[i++] = static_cast<uint8_t>(3 + sizeof(body));  // AD length (type + company + body)
    adv[i++] = 0xff;
    adv[i++] = 0x75;
    adv[i++] = 0x00;  // Samsung 0x0075
    memcpy(&adv[i], body, sizeof(body));
    i += sizeof(body);
    return i;
}
}  // namespace

void Esp32BleScanner::begin_spam()
{
    if (!host_synced_ || adv_active_) {
        return;
    }
    uint8_t own_addr_type = 0;
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) {
        return;
    }
    uint8_t adv[31];
    int len  = 0;
    int mode = spam_mode_;
    if (mode <= 0 || mode > 4) {
        mode = 1 + static_cast<int>(esp_random() % 4);  // 0/All -> random vendor per burst
    }
    switch (mode) {
        case 2: len = build_samsung(adv); break;
        case 3: len = build_google(adv); break;
        case 4: len = build_microsoft(adv); break;
        default: len = build_apple(adv); break;
    }
    ble_gap_adv_set_data(adv, len);

    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // 100 ms bursts; the ADV_COMPLETE event rotates to the next payload.
    if (ble_gap_adv_start(own_addr_type, nullptr, 100, &params, &Esp32BleScanner::spam_gap_event,
                          this) == 0) {
        adv_active_ = true;
    }
}

void Esp32BleScanner::stop_adv()
{
    if (!adv_active_) {
        return;
    }
    ble_gap_adv_stop();
    adv_active_ = false;
}

int Esp32BleScanner::spam_gap_event(struct ble_gap_event* event, void* arg)
{
    auto* self = static_cast<Esp32BleScanner*>(arg);
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
        self->adv_active_ = false;
        if (self->spam_) {
            self->begin_spam();  // rotate to a fresh device popup
        }
    }
    return 0;
}

void Esp32BleScanner::start_spam()
{
    if (auto* coordinator = services::radio_coordinator()) {
        if (!coordinator->acquire_for_ble()) {
            spectra5::log::tagError(kTag, "could not acquire C6 radio for BLE spam");
            return;
        }
    }
    if (!ensure_ctrl_task()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = false;
        spam_     = true;
    }
    const Cmd wake = Cmd::Wake;
    xQueueSend(cmd_queue_, &wake, 0);
    spectra5::log::tagInfo(kTag, "BLE spam started");
}

void Esp32BleScanner::stop_spam()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        spam_ = false;
    }
    if (cmd_queue_ != nullptr) {
        const Cmd wake = Cmd::Wake;
        xQueueSend(cmd_queue_, &wake, 0);
    }
}

bool Esp32BleScanner::is_spamming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return spam_;
}

void Esp32BleScanner::set_spam_mode(int mode)
{
    std::lock_guard<std::mutex> lock(mutex_);
    spam_mode_ = mode;
}

void Esp32BleScanner::on_disc(const void* disc_desc)
{
    const auto* disc = static_cast<const ble_gap_disc_desc*>(disc_desc);
    if (disc == nullptr) {
        return;
    }

    ble_hs_adv_fields fields{};
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) != 0) {
        return;
    }

    const std::string address = format_addr(disc->addr.val);
    const Timestamp now       = clock_.now_ms();

    // Vendor from the manufacturer-specific data's company ID (first 2 bytes, LE).
    const char* vendor = "";
    if (fields.mfg_data != nullptr && fields.mfg_data_len >= 2) {
        const uint16_t cid = static_cast<uint16_t>(fields.mfg_data[0]) |
                             (static_cast<uint16_t>(fields.mfg_data[1]) << 8);
        vendor = domain::ble_company_vendor(cid);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& dev : devices_) {
        if (dev.address != address) {
            continue;
        }
        dev.rssi      = disc->rssi;
        dev.last_seen = now;
        dev.rssi_history.push_back(disc->rssi);
        if (dev.rssi_history.size() > kHistoryMax) {
            dev.rssi_history.erase(dev.rssi_history.begin());
        }
        if (dev.name.empty()) {
            dev.name = parse_name(fields);
        }
        if (dev.vendor.empty() && vendor[0] != '\0') {
            dev.vendor = vendor;
        }
        return;
    }

    BleAdvertisement ad;
    ad.address      = address;
    ad.address_type = map_addr_type(disc->addr.type);
    ad.name         = parse_name(fields);
    ad.vendor       = vendor;
    ad.rssi         = disc->rssi;
    ad.first_seen   = now;
    ad.last_seen    = now;
    ad.rssi_history.push_back(disc->rssi);
    devices_.push_back(std::move(ad));
}

void Esp32BleScanner::on_disc_complete()
{
    disc_active_ = false;
    std::size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        count = devices_.size();
    }
    spectra5::log::tagInfo(kTag, "discovery cycle complete, {} devices", static_cast<int>(count));
    apply_host_state();
}

std::vector<BleAdvertisement> Esp32BleScanner::snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

void Esp32BleScanner::host_task(void*)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void Esp32BleScanner::ctrl_task_trampoline(void* arg)
{
    static_cast<Esp32BleScanner*>(arg)->ctrl_loop();
}

void Esp32BleScanner::ctrl_loop()
{
    while (true) {
        bool exit            = false;
        bool release_pending = false;
        bool should_scan     = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            exit            = shutdown_;
            release_pending = release_pending_;
            should_scan     = scanning_ || spam_;  // either keeps the host running
        }
        if (exit) {
            shutdown_radio_stack();
            break;
        }

        if (release_pending) {
            shutdown_radio_stack();
            Cmd cmd{};
            xQueueReceive(cmd_queue_, &cmd, pdMS_TO_TICKS(250));
            continue;
        }

        if (should_scan && !host_running_) {
            if (!init_nimble_stack()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        Cmd cmd{};
        if (xQueueReceive(cmd_queue_, &cmd, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (cmd == Cmd::ReleaseRadio) {
                shutdown_radio_stack();
                continue;
            }
            schedule_host_work();
        } else if (should_scan && host_running_) {
            schedule_host_work();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ctrl_task_ = nullptr;
    }
    vTaskDelete(nullptr);
}

}  // namespace spectra5::platform
