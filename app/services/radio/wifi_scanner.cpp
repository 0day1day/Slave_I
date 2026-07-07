#include "services/radio/wifi_scanner.hpp"

namespace spectra5::services {

namespace {
IWifiScanner* g_scanner = nullptr;
}

IWifiScanner* wifi_scanner()
{
    return g_scanner;
}

void inject_wifi_scanner(IWifiScanner* scanner)
{
    g_scanner = scanner;
}

bool has_wifi_scanner()
{
    return g_scanner != nullptr;
}

}  // namespace spectra5::services
