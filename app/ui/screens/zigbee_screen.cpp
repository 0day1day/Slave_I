#include "ui/screens/zigbee_screen.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

#include "domain/radio/offensive_codec.hpp"
#include "domain/radio/oui.hpp"
#include "services/radio/ieee154_store.hpp"
#include "services/radio/radio_engine.hpp"
#include "services/radio/zigbee_store.hpp"
#include "services/stats/activity_stats.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;

namespace {
lv_obj_t* label(lv_obj_t* parent, const char* text, SemanticColor color, const lv_font_t* font)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_color(l, lv_semantic(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}
}  // namespace

ZigbeeScreen::ZigbeeScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    label(root_, LV_SYMBOL_GPS "  802.15.4  /  ZIGBEE", SemanticColor::TextPrimary,
          &ibm_plex_mono_32);
    label(root_,
          "Passive energy scan of the 16 channels (11-26). Higher bars = busier: "
          "Zigbee bulbs/plugs, Thread, Matter, smart-home hubs.",
          SemanticColor::TextSecondary, &ibm_plex_mono_18);

    // One aligned controls row: energy scan + channel selector + device sniffer.
    lv_obj_t* controls = lv_obj_create(root_);
    lv_obj_set_size(controls, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_column(controls, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_row(controls, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn = lv_obj_create(controls);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, tokens::TouchTarget);
    lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(btn, tokens::SpaceLg, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* blbl = lv_label_create(btn);
    lv_label_set_text(blbl, LV_SYMBOL_REFRESH "  Energy scan");
    lv_obj_set_style_text_color(blbl, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_text_font(blbl, &ibm_plex_mono_18, 0);
    lv_obj_center(blbl);
    lv_obj_add_event_cb(btn, &ZigbeeScreen::on_scan, LV_EVENT_CLICKED, this);

    chan_dd_ = lv_dropdown_create(controls);
    std::string opts = "All (hop)";
    for (int ch = 11; ch <= 26; ++ch) {
        opts += "\nch " + std::to_string(ch);
    }
    lv_dropdown_set_options(chan_dd_, opts.c_str());
    lv_dropdown_set_selected(chan_dd_, sniff_channel_ - 11 + 1);  // +1 past "All (hop)"
    lv_obj_set_size(chan_dd_, 150, tokens::TouchTarget);
    brand_input(chan_dd_);
    lv_obj_add_event_cb(chan_dd_, &ZigbeeScreen::on_chan_changed, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* sbtn = lv_obj_create(controls);
    lv_obj_set_size(sbtn, LV_SIZE_CONTENT, tokens::TouchTarget);
    lv_obj_set_style_radius(sbtn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(sbtn, 1, 0);
    lv_obj_set_style_border_color(sbtn, lv_semantic(SemanticColor::Success), 0);
    lv_obj_set_style_bg_color(sbtn, lv_semantic(SemanticColor::SurfaceRaised), 0);
    lv_obj_set_style_bg_opa(sbtn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(sbtn, tokens::SpaceLg, 0);
    lv_obj_clear_flag(sbtn, LV_OBJ_FLAG_SCROLLABLE);
    sniff_lbl_ = lv_label_create(sbtn);
    lv_label_set_text(sniff_lbl_, LV_SYMBOL_EYE_OPEN "  Sniff devices");
    lv_obj_set_style_text_color(sniff_lbl_, lv_semantic(SemanticColor::Success), 0);
    lv_obj_set_style_text_font(sniff_lbl_, &ibm_plex_mono_18, 0);
    lv_obj_center(sniff_lbl_);
    lv_obj_add_event_cb(sbtn, &ZigbeeScreen::on_sniff_toggle, LV_EVENT_CLICKED, this);

    status_ = label(root_, "Idle. Tap to scan.", SemanticColor::Info, &ibm_plex_mono_16);

    results_ = lv_obj_create(root_);
    lv_obj_set_width(results_, lv_pct(100));
    lv_obj_set_flex_grow(results_, 1);
    lv_obj_set_style_bg_color(results_, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(results_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(results_, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(results_, 0, 0);
    lv_obj_set_style_pad_all(results_, tokens::SpaceSm, 0);
    lv_obj_set_style_pad_row(results_, 2, 0);
    lv_obj_set_flex_flow(results_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(results_, LV_DIR_VER);

    if (services::ZigbeeStore::instance().has_result()) {
        rev_ = 0;  // force a redraw of the last result
    }
    timer_ = lv_timer_create(&ZigbeeScreen::on_timer, 400, this);
}

ZigbeeScreen::~ZigbeeScreen()
{
    if (sniffing_) {
        if (auto* eng = services::radio_engine()) {
            eng->send(domain::cmd_154_sniff(0));  // stop the C6 sniffer on exit
        }
    }
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void ZigbeeScreen::on_scan(lv_event_t* event)
{
    auto* self   = static_cast<ZigbeeScreen*>(lv_event_get_user_data(event));
    auto* engine = services::radio_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    engine->send(domain::cmd_zigbee_scan());
    services::ActivityStats::instance().zigbee_scans.fetch_add(1);
    self->scanning_ = true;
    if (self->status_ != nullptr) {
        lv_label_set_text(self->status_, LV_SYMBOL_REFRESH " Scanning channels 11-26... (~1s)");
    }
}

void ZigbeeScreen::on_chan_changed(lv_event_t* event)
{
    auto* self = static_cast<ZigbeeScreen*>(lv_event_get_user_data(event));
    auto* dd   = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || dd == nullptr) {
        return;
    }
    const uint16_t sel   = lv_dropdown_get_selected(dd);   // 0 = All (hop), 1..16 = ch 11..26
    self->sniff_channel_ = (sel == 0) ? 0xFF : static_cast<uint8_t>(11 + (sel - 1));
    if (self->sniffing_) {
        if (auto* eng = services::radio_engine()) {
            eng->send(domain::cmd_154_sniff(self->sniff_channel_));  // retune / switch to hop
        }
    }
}

void ZigbeeScreen::on_sniff_toggle(lv_event_t* event)
{
    auto* self = static_cast<ZigbeeScreen*>(lv_event_get_user_data(event));
    auto* eng  = services::radio_engine();
    if (self == nullptr || eng == nullptr) {
        return;
    }
    if (!self->sniffing_) {
        services::Ieee154Store::instance().clear();
        eng->send(domain::cmd_154_sniff(self->sniff_channel_));
        self->sniffing_  = true;
        self->sniff_rev_ = 0xFFFFFFFFu;  // force first device-table redraw
        if (self->sniff_lbl_) lv_label_set_text(self->sniff_lbl_, LV_SYMBOL_STOP "  Stop sniff");
        if (self->status_) {
            char s[96];
            if (self->sniff_channel_ == 0xFF) {
                std::snprintf(s, sizeof(s), "Sniffing all channels (hop)... frames -> .pcap on SD.");
            } else {
                std::snprintf(s, sizeof(s), "Sniffing ch %u... devices below; frames -> .pcap on SD.",
                              self->sniff_channel_);
            }
            lv_label_set_text(self->status_, s);
        }
    } else {
        eng->send(domain::cmd_154_sniff(0));  // stop
        self->sniffing_ = false;
        if (self->sniff_lbl_) lv_label_set_text(self->sniff_lbl_, LV_SYMBOL_EYE_OPEN "  Sniff devices");
        if (self->status_) lv_label_set_text(self->status_, LV_SYMBOL_OK " Sniff stopped.");
    }
}

void ZigbeeScreen::flush_pcap()
{
    auto frames = services::Ieee154Store::instance().drain_frames();
    if (frames.empty()) {
        return;
    }
    char path[64];
    if (sniff_channel_ == 0xFF) {
        std::snprintf(path, sizeof(path), "/sd/spectra5/zigbee_hop.pcap");
    } else {
        std::snprintf(path, sizeof(path), "/sd/spectra5/zigbee_ch%u.pcap", sniff_channel_);
    }
    std::ifstream probe(path);
    const bool is_new = !probe.good();
    probe.close();
    std::ofstream pf(path, std::ios::binary | std::ios::app);
    if (!pf) {
        return;
    }
    auto put32 = [&](std::uint32_t v) { pf.write(reinterpret_cast<const char*>(&v), 4); };
    auto put16 = [&](std::uint16_t v) { pf.write(reinterpret_cast<const char*>(&v), 2); };
    if (is_new) {
        put32(0xa1b2c3d4);  // pcap magic
        put16(2);
        put16(4);
        put32(0);
        put32(0);
        put32(65535);
        put32(230);  // LINKTYPE_IEEE802_15_4_NOFCS
    }
    const std::uint32_t ts = static_cast<std::uint32_t>(::time(nullptr));
    std::uint32_t usec     = 0;
    for (const auto& fr : frames) {
        put32(ts);
        put32(usec++);
        put32(static_cast<std::uint32_t>(fr.size()));
        put32(static_cast<std::uint32_t>(fr.size()));
        pf.write(reinterpret_cast<const char*>(fr.data()), static_cast<std::streamsize>(fr.size()));
    }
}

void ZigbeeScreen::rebuild_devices()
{
    if (results_ == nullptr) {
        return;
    }
    const auto devs = services::Ieee154Store::instance().devices();
    lv_obj_clean(results_);

    auto add_cell = [](lv_obj_t* row, const char* txt, int w, SemanticColor c) {
        lv_obj_t* l = lv_label_create(row);
        lv_label_set_text(l, txt);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, w);
        lv_obj_set_style_text_color(l, lv_semantic(c), 0);
        lv_obj_set_style_text_font(l, &ibm_plex_mono_14, 0);
        return l;
    };
    auto make_row = [&]() {
        lv_obj_t* row = lv_obj_create(results_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceSm, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        return row;
    };

    lv_obj_t* hdr = make_row();
    add_cell(hdr, "PAN", 70, SemanticColor::TextSecondary);
    add_cell(hdr, "ADDRESS", 180, SemanticColor::TextSecondary);
    add_cell(hdr, "VENDOR", 150, SemanticColor::TextSecondary);
    add_cell(hdr, "TYPE", 80, SemanticColor::TextSecondary);
    add_cell(hdr, "LQI", 50, SemanticColor::TextSecondary);
    add_cell(hdr, "CH", 40, SemanticColor::TextSecondary);

    for (const auto& d : devs) {
        lv_obj_t* row = make_row();
        char pan[12];
        std::snprintf(pan, sizeof(pan), "0x%04X", d.pan);
        add_cell(row, pan, 70, SemanticColor::TextPrimary);

        char addr[24];
        const char* vendor = "-";
        if (d.has_ext) {
            std::snprintf(addr, sizeof(addr), "%02X%02X%02X%02X%02X%02X%02X%02X", d.ext_addr[0],
                          d.ext_addr[1], d.ext_addr[2], d.ext_addr[3], d.ext_addr[4], d.ext_addr[5],
                          d.ext_addr[6], d.ext_addr[7]);
            domain::MacAddr m{d.ext_addr[0], d.ext_addr[1], d.ext_addr[2],
                              d.ext_addr[3], d.ext_addr[4], d.ext_addr[5]};
            vendor = domain::oui_vendor(m);
        } else {
            std::snprintf(addr, sizeof(addr), "0x%04X (short)", d.short_addr);
        }
        add_cell(row, addr, 180, SemanticColor::Accent);
        add_cell(row, vendor, 150, SemanticColor::TextSecondary);

        const char* type = d.frame_type == 0   ? "beacon"
                           : d.frame_type == 1 ? "data"
                           : d.frame_type == 3 ? "cmd"
                                               : "?";
        add_cell(row, type, 80, SemanticColor::Info);
        char lqi[8];
        std::snprintf(lqi, sizeof(lqi), "%u", d.lqi);
        add_cell(row, lqi, 50, SemanticColor::TextPrimary);
        char ch[6];
        std::snprintf(ch, sizeof(ch), "%u", d.channel);
        add_cell(row, ch, 40, SemanticColor::TextSecondary);
    }

    if (status_ != nullptr) {
        char s[96];
        if (sniff_channel_ == 0xFF) {
            std::snprintf(s, sizeof(s), LV_SYMBOL_EYE_OPEN " Sniffing all channels - %d devices",
                          static_cast<int>(devs.size()));
        } else {
            std::snprintf(s, sizeof(s), LV_SYMBOL_EYE_OPEN " Sniffing ch %u - %d devices found",
                          sniff_channel_, static_cast<int>(devs.size()));
        }
        lv_label_set_text(status_, s);
    }
}

void ZigbeeScreen::on_timer(lv_timer_t* timer)
{
    auto* self = static_cast<ZigbeeScreen*>(lv_timer_get_user_data(timer));
    if (self == nullptr || self->results_ == nullptr) {
        return;
    }

    // Sniffer mode: drain captured frames to the .pcap + refresh the device table.
    if (self->sniffing_) {
        self->flush_pcap();
        const uint32_t r = services::Ieee154Store::instance().revision();
        if (r != self->sniff_rev_) {
            self->sniff_rev_ = r;
            self->rebuild_devices();
        }
        return;
    }

    const uint32_t rev = services::ZigbeeStore::instance().revision();
    if (rev == self->rev_) {
        return;
    }
    self->rev_      = rev;
    self->scanning_ = false;
    const auto p    = services::ZigbeeStore::instance().snapshot();

    // Common 802.15.4 channel usage hints (2405-2480 MHz, 5 MHz spacing).
    auto note_for = [](int ch) -> const char* {
        switch (ch) {
            case 11: return "Zigbee/Thread default - overlaps Wi-Fi 1";
            case 15: return "Zigbee (ZLL) - between Wi-Fi 1 & 6";
            case 20: return "Zigbee (ZLL) - between Wi-Fi 6 & 11";
            case 25: return "Zigbee (ZLL) - above Wi-Fi 11";
            case 26: return "Zigbee/Thread - clear of Wi-Fi";
            case 12: case 13: case 14: return "overlaps Wi-Fi 1";
            case 16: case 17: case 18: return "overlaps Wi-Fi 6";
            case 21: case 22: case 23: return "overlaps Wi-Fi 11";
            default: return "-";
        }
    };

    lv_obj_clean(self->results_);

    // Header row.
    auto add_cell = [](lv_obj_t* row, const char* txt, int w, SemanticColor c, const lv_font_t* f) {
        lv_obj_t* l = lv_label_create(row);
        lv_label_set_text(l, txt);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, w);
        lv_obj_set_style_text_color(l, lv_semantic(c), 0);
        lv_obj_set_style_text_font(l, f, 0);
        return l;
    };
    auto make_row = [&]() {
        lv_obj_t* row = lv_obj_create(self->results_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceSm, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        return row;
    };

    lv_obj_t* hdr = make_row();
    add_cell(hdr, "CH", 40, SemanticColor::TextSecondary, &ibm_plex_mono_14);
    add_cell(hdr, "ENERGY", 90, SemanticColor::TextSecondary, &ibm_plex_mono_14);
    add_cell(hdr, "LEVEL", 150, SemanticColor::TextSecondary, &ibm_plex_mono_14);
    add_cell(hdr, "TYPICAL USE", 360, SemanticColor::TextSecondary, &ibm_plex_mono_14);

    int busiest = 11;
    for (int i = 1; i < 16; ++i) {
        if (p[i] > p[busiest - 11]) busiest = 11 + i;
    }

    for (int i = 0; i < 16; ++i) {
        const int ch  = 11 + i;
        const int dbm = static_cast<int>(p[i]);
        // -100..-20 dBm -> 0..16 blocks; colour by how busy.
        int blocks = (dbm + 100) / 5;
        blocks     = blocks < 0 ? 0 : (blocks > 16 ? 16 : blocks);
        const SemanticColor lvl = dbm > -45   ? SemanticColor::Danger
                                  : dbm > -65 ? SemanticColor::Warning
                                              : SemanticColor::Success;

        lv_obj_t* row = make_row();
        if (ch == busiest && dbm > -90) {
            lv_obj_set_style_bg_color(row, lv_semantic(SemanticColor::SurfaceRaised), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row, tokens::RadiusSm, 0);
        }
        char cb[8];
        std::snprintf(cb, sizeof(cb), "%d", ch);
        add_cell(row, cb, 40, SemanticColor::TextPrimary, &ibm_plex_mono_16);
        char eb[12];
        std::snprintf(eb, sizeof(eb), "%d dBm", dbm);
        add_cell(row, eb, 90, lvl, &ibm_plex_mono_16);
        std::string bar(static_cast<size_t>(blocks), '|');
        add_cell(row, bar.c_str(), 150, lvl, &ibm_plex_mono_16);
        add_cell(row, note_for(ch), 360, SemanticColor::TextSecondary, &ibm_plex_mono_14);
    }

    if (self->status_ != nullptr) {
        char s[64];
        std::snprintf(s, sizeof(s), LV_SYMBOL_OK " Scan done. Busiest: channel %d.", busiest);
        lv_label_set_text(self->status_, s);
    }
}

}  // namespace spectra5::ui
