#pragma once

namespace spectra5::services {

// P4-side seam for BLE advertisement spam (Apple/Android/etc. pairing popups).
// Implemented on top of the NimBLE host in the platform layer.
class IBleSpammer {
public:
    virtual ~IBleSpammer() = default;
    virtual void start_spam() = 0;
    virtual void stop_spam()  = 0;
    virtual bool is_spamming() const = 0;
    // Target: 0 = all vendors (random), 1 Apple, 2 Samsung, 3 Google, 4 Microsoft.
    virtual void set_spam_mode(int mode) { (void)mode; }
};

IBleSpammer* ble_spammer();
void inject_ble_spammer(IBleSpammer* spammer);
bool has_ble_spammer();

}  // namespace spectra5::services
