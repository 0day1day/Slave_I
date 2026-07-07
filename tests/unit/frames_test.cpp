#include <cassert>

#include "domain/radio/frames.hpp"
#include "domain/radio/offensive.hpp"

using namespace spectra5::domain;

int main()
{
    const MacAddr bssid   = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const MacAddr station = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    // --- Deauth frame: exact byte layout (802.11 subtype 0xC0) ---
    const ManagementFrame deauth = build_deauth_frame(bssid, station, 7);
    assert(deauth.size() == kDeauthFrameSize);
    assert(deauth[0] == 0xC0 && deauth[1] == 0x00);          // frame control
    assert(deauth[2] == 0x00 && deauth[3] == 0x00);          // duration
    for (int i = 0; i < 6; ++i) {
        assert(deauth[4 + i] == station[i]);                 // Addr1 (DA)
        assert(deauth[10 + i] == bssid[i]);                  // Addr2 (SA)
        assert(deauth[16 + i] == bssid[i]);                  // Addr3 (BSSID)
    }
    assert(deauth[24] == 0x07 && deauth[25] == 0x00);        // reason 7, LE

    // --- 16-bit reason code is little-endian (improvement over the 8-bit donor) ---
    const ManagementFrame r = build_deauth_frame(bssid, station, 0x1234);
    assert(r[24] == 0x34 && r[25] == 0x12);

    // --- Disassoc differs only in the subtype byte ---
    const ManagementFrame disassoc = build_disassoc_frame(bssid, station, 8);
    assert(disassoc[0] == 0xA0);
    assert(disassoc[24] == 0x08 && disassoc[25] == 0x00);

    // --- DeauthParams with all-zero target => broadcast (whole AP) ---
    DeauthParams broadcast;
    broadcast.bssid  = bssid;
    broadcast.target = {0, 0, 0, 0, 0, 0};
    broadcast.reason = 7;
    const ManagementFrame b = build_deauth_frame(broadcast);
    for (int i = 0; i < 6; ++i) {
        assert(b[4 + i] == 0xFF);  // DA = broadcast
    }

    // --- DeauthParams with a specific target => unicast to that station ---
    DeauthParams unicast;
    unicast.bssid  = bssid;
    unicast.target = station;
    unicast.reason = 7;
    const ManagementFrame u = build_deauth_frame(unicast);
    for (int i = 0; i < 6; ++i) {
        assert(u[4 + i] == station[i]);
    }

    return 0;
}
