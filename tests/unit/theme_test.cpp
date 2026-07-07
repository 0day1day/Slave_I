#include <cassert>

#include "ui/design_system/theme.hpp"
#include "ui/design_system/tokens.hpp"

using namespace spectra5::ui;
using tokens::SemanticColor;

int main()
{
    Theme dark{ThemeId::Dark};
    Theme hc{ThemeId::HighContrast};

    // Colores conocidos del tema "Field Armor" (Boba Fett).
    assert(dark.color(SemanticColor::Background) == 0x14120D);
    assert(dark.color(SemanticColor::TextPrimary) == 0xE8E2D2);

    // Los temas difieren en el fondo.
    assert(dark.color(SemanticColor::Background) != hc.color(SemanticColor::Background));
    assert(hc.color(SemanticColor::Background) == 0x000000);

    // Todos los colores semánticos resuelven dentro de 24 bits.
    for (int i = 0; i <= static_cast<int>(SemanticColor::RadioRf); ++i) {
        auto c = dark.color(static_cast<SemanticColor>(i));
        assert(c <= 0xFFFFFFu);
    }

    // Tema activo configurable.
    set_active_theme(ThemeId::HighContrast);
    assert(active_theme().id() == ThemeId::HighContrast);
    set_active_theme(ThemeId::Dark);
    assert(active_theme().id() == ThemeId::Dark);

    return 0;
}
