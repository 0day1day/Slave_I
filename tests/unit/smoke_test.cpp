#include "core/version.hpp"

#include <cassert>
#include <cstring>

int main()
{
    static_assert(spectra5::kDisplayWidth == 1280);
    static_assert(spectra5::kDisplayHeight == 720);
    assert(std::strcmp(spectra5::kProjectName, "Slave I") == 0);
    return 0;
}
