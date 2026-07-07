#pragma once

#include <string>

namespace spectra5::services {

// P4-side seam for the Evil Portal (captive-portal credential harvester). The
// concrete implementation lives in the platform layer; the UI drives it through
// this interface so app/ stays platform-agnostic.
class IEvilPortal {
public:
    virtual ~IEvilPortal() = default;
    virtual bool start(const std::string& ssid) = 0;
    virtual void stop()                         = 0;
    virtual bool active() const                 = 0;
    virtual int captured() const                = 0;  // credential submissions

    // Evil Twin password verify: test the last captured password against the real
    // AP (STA connect). verify_state: 0 idle, 1 verifying, 2 valid, 3 invalid.
    virtual void verify_last() {}
    virtual int verify_state() const { return 0; }
    virtual std::string last_password() const { return {}; }

    // Evil Twin: put the clone on the real AP's channel so a parallel deauth coexists.
    virtual void set_ap_channel(uint8_t ch) { (void)ch; }
    // Choose a captive-portal HTML template by file path (empty = built-in page).
    virtual void set_template(const std::string& path) { (void)path; }
    // Use an inline HTML template (embedded built-ins). Empty = clear. {SSID} works.
    virtual void set_template_html(const std::string& html) { (void)html; }
};

IEvilPortal* evil_portal();
void inject_evil_portal(IEvilPortal* portal);
bool has_evil_portal();

}  // namespace spectra5::services
