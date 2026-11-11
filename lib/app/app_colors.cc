#include "app/app_colors.h"

#include "wm/xsettings.h"

namespace tiny::app {

AppColors app_colors(Variant variant)
{
    if (variant == Variant::Dark) {
        return {0x242729, 0x1C1F20, 0x2E3436, 0xEEEEEC, 0x9A9C98,
                0x4A5052, 0x729FCF, 0x3E4446, 0xFF7B63};
    }
    return {0xF8F8F8, 0xEDEDED, 0xFFFFFF, 0x2E3436, 0x6E7476,
            0xC8C8C8, 0x729FCF, 0xDCE8F5, 0xC01C28};
}

Variant variant_in_use(::Display* display, int screen, Variant configured)
{
    return wm::running_variant(display, screen).value_or(configured);
}

} // namespace tiny::app
