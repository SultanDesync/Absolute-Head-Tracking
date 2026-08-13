#include "Config.h"
#include "HotkeyPolicy.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #expression); \
    std::exit(1); } } while (false)

int main() {
    CHECK(ConfigPolicy::Defaults().pitch.inverted);
    Config config;
    config.pollRateHz = 1;
    config.staleMilliseconds = 99999;
    config.smoothing = 4.0F;
    config.deadzoneDegrees = -10.0F;
    config.yaw.sensitivity = 0.0F;
    config.pitch.maximumDegrees = 500.0F;
    config.workbench.virtualKey = 999;
    ConfigPolicy::Sanitize(config);
    CHECK(config.pollRateHz == 30);
    CHECK(config.staleMilliseconds == 5000);
    CHECK(config.smoothing == 0.99F);
    CHECK(config.deadzoneDegrees == 0.0F);
    CHECK(config.yaw.sensitivity == 0.05F);
    CHECK(config.pitch.maximumDegrees == 180.0F);
    CHECK(config.workbench.virtualKey == 0xFF);

    CHECK(!ConfigPolicy::ShouldBlockForAbsoluteHOTAS(false, false));
    CHECK(ConfigPolicy::ShouldBlockForAbsoluteHOTAS(true, false));
    CHECK(ConfigPolicy::ShouldBlockForAbsoluteHOTAS(false, true));

    const HotkeySettings ctrlOne{ 0x31, true, false, false };
    const HotkeySettings anotherCtrlOne{ 0x31, true, false, false };
    const HotkeySettings one{ 0x31, false, false, false };
    const HotkeySettings unbound{ 0, false, false, false };
    CHECK(HotkeyPolicy::IsBound(ctrlOne));
    CHECK(!HotkeyPolicy::IsBound(unbound));
    CHECK(HotkeyPolicy::Same(ctrlOne, anotherCtrlOne));
    CHECK(!HotkeyPolicy::Same(ctrlOne, one));
    CHECK(!HotkeyPolicy::Same(unbound, unbound));
    return 0;
}
