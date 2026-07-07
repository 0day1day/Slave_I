#include "services/radio/ble_scanner.hpp"

namespace spectra5::services {

namespace {
IBleScanner* g_scanner = nullptr;
}

IBleScanner* ble_scanner()
{
    return g_scanner;
}

void inject_ble_scanner(IBleScanner* scanner)
{
    g_scanner = scanner;
}

bool has_ble_scanner()
{
    return g_scanner != nullptr;
}

}  // namespace spectra5::services
