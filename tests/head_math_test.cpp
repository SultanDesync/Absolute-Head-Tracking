#include "HeadMath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #expression); \
    std::exit(1); } } while (false)

static bool Near(float left, float right, float tolerance = 0.0001F) {
    return std::abs(left - right) <= tolerance;
}
int main() {
    CHECK(Near(HeadMath::ShapeAngle(0.5F, 1.0F, 80.0F), 0.0F));
    CHECK(Near(HeadMath::ShapeAngle(11.0F, 1.0F, 80.0F), 10.0F));
    CHECK(Near(HeadMath::ShapeAngle(-100.0F, 0.0F, 85.0F), -85.0F));
    CHECK(Near(HeadMath::Smooth(5.0F, 10.0F, 0.0F, 1.0F / 60.0F), 10.0F));

    const auto identity = HeadMath::FromDegrees(0.0F, 0.0F, 0.0F);
    CHECK(Near(identity.w, 1.0F));
    CHECK(Near(identity.x, 0.0F));
    CHECK(Near(identity.y, 0.0F));
    CHECK(Near(identity.z, 0.0F));

    const auto yaw = HeadMath::FromDegrees(90.0F, 0.0F, 0.0F);
    CHECK(Near(yaw.w, std::sqrt(0.5F)));
    CHECK(Near(yaw.z, std::sqrt(0.5F)));
    return 0;
}
