#include "display_power.h"

#include <X11/extensions/dpms.h>

namespace tiny {

DisplayPower::DisplayPower(::Display* display_arg): display(display_arg)
{
    int event = 0;
    int error = 0;
    usable = DPMSQueryExtension(display, &event, &error) != 0 &&
             DPMSCapable(display) != 0;
}

bool DisplayPower::enabled() const
{
    if (!usable) {
        return false;
    }
    CARD16 level = 0;
    BOOL state = False;
    return DPMSInfo(display, &level, &state) != 0 && state != False;
}

std::optional<std::chrono::seconds> DisplayPower::timeout() const
{
    if (!enabled()) {
        return std::nullopt;
    }
    CARD16 standby = 0;
    CARD16 suspend = 0;
    CARD16 off = 0;
    if (DPMSGetTimeouts(display, &standby, &suspend, &off) == 0) {
        return std::nullopt;
    }
    return std::chrono::seconds(off);
}

bool DisplayPower::set_timeout(std::chrono::seconds after)
{
    if (!usable) {
        return false;
    }
    if (after <= std::chrono::seconds::zero()) {
        DPMSDisable(display);
        XFlush(display);
        return true;
    }
    // the protocol takes 16 bits, and a level with 0 is not used
    constexpr long longest = 65535;
    const auto seconds = static_cast<CARD16>(
            after.count() > longest ? longest : after.count());
    if (DPMSSetTimeouts(display, 0, 0, seconds) == 0) {
        return false;
    }
    DPMSEnable(display);
    XFlush(display);
    return true;
}

bool DisplayPower::off()
{
    if (!enabled()) {
        return false;
    }
    DPMSForceLevel(display, DPMSModeOff);
    XFlush(display);
    return true;
}

bool DisplayPower::on()
{
    if (!enabled()) {
        return false;
    }
    DPMSForceLevel(display, DPMSModeOn);
    XFlush(display);
    return true;
}

} // namespace tiny
