#include "ManualLookPolicy.h"

#include <cassert>
#include <cmath>

namespace {
bool Near(float left, float right, float epsilon = 0.0001F) {
    return std::abs(left - right) <= epsilon;
}
}

int main() {
    using namespace ManualLookPolicy;
    assert(Near(ShapeAnalog(0.05F, 0.1F), 0.0F));
    assert(Near(ShapeAnalog(-0.1F, 0.1F), 0.0F));
    assert(Near(ShapeAnalog(1.0F, 0.1F), 1.0F));
    assert(Near(ShapeAnalog(-1.0F, 0.1F), -1.0F));
    assert(Near(ShapeAnalog(0.55F, 0.1F), 0.5F));

    // A centered proportional control contributes no offset. Its two ends map
    // independently into an asymmetric -20 / +90 degree range.
    assert(Near(AnalogOffset(0.0F, 0.1F, -20.0F, 0.0F, 90.0F), 0.0F));
    assert(Near(AnalogOffset(-1.0F, 0.1F, -20.0F, 0.0F, 90.0F), -20.0F));
    assert(Near(AnalogOffset(1.0F, 0.1F, -20.0F, 0.0F, 90.0F), 90.0F));

    assert(Near(Composite(10.0F, 15.0F, 5.0F, -20.0F, 90.0F), 30.0F));
    assert(Near(Composite(80.0F, 15.0F, 5.0F, -20.0F, 90.0F), 90.0F));
    assert(Near(Composite(-15.0F, -10.0F, -5.0F, -20.0F, 90.0F), -20.0F));
    return 0;
}
