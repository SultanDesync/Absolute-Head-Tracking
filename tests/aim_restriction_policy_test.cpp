#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "AimRestrictionPolicy.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #expression);          \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

namespace {
using namespace AimRestrictionPolicy;

Vector3 AtDegrees(float degrees) {
    const float radians = degrees * kDegreesToRadians;
    return {std::sin(radians), std::cos(radians), 0.0F};
}
} // namespace

int main() {
    RimHistory history;
    auto base = ComputeBaseRay({0, 1, 0}, AtDegrees(20), true, 30, history);
    CHECK(base.valid);
    CHECK(base.mode == Mode::CameraDirected);
    CHECK(std::abs(base.angleDegrees - 20.0F) < 0.01F);

    base = ComputeBaseRay({0, 1, 0}, AtDegrees(80), false, 30, history);
    CHECK(base.valid);
    CHECK(base.mode == Mode::CameraDirected);
    CHECK(Dot(base.ray, AtDegrees(80)) > 0.999F);

    base = ComputeBaseRay({0, 1, 0}, AtDegrees(80), true, 30, history);
    CHECK(base.valid);
    CHECK(base.mode == Mode::ConeLimited);
    CHECK(std::abs(AngleDegrees(Dot(Vector3{0, 1, 0}, base.ray)) - 30.0F) < 0.01F);

    const Vector3 remembered = base.ray;
    base = ComputeBaseRay({0, 1, 0}, {0, -1, 0}, true, 30, history);
    CHECK(base.valid);
    CHECK(Dot(base.ray, remembered) > 0.999F);

    RimHistory cold;
    base = ComputeBaseRay({0, 1, 0}, {0, -1, 0}, true, 30, cold);
    CHECK(base.valid);
    CHECK(Dot(base.ray, Vector3{0, 1, 0}) > 0.999F);

    auto target = EvaluateTarget({0, 1, 0}, AtDegrees(28.9F), 30, false);
    CHECK(target.eligible);
    target = EvaluateTarget({0, 1, 0}, AtDegrees(30.5F), 30, true);
    CHECK(target.eligible);
    target = EvaluateTarget({0, 1, 0}, AtDegrees(31.1F), 30, true);
    CHECK(!target.eligible);

    const auto direction = DirectionFromLaunchAngles(0.0F, 0.5F);
    CHECK(std::abs(direction.x - std::sin(0.5F)) < 0.0001F);
    CHECK(std::abs(direction.y - std::cos(0.5F)) < 0.0001F);
    CHECK(std::abs(direction.z) < 0.0001F);

    const auto transformed = TransformDirection({0, -1, 0}, {1, 0, 0}, {0, 0, 1},
                                                AtDegrees(20.0F));
    CHECK(std::abs(transformed.x - std::cos(20.0F * kDegreesToRadians)) < 0.0001F);
    CHECK(std::abs(transformed.y + std::sin(20.0F * kDegreesToRadians)) < 0.0001F);
    CHECK(std::abs(transformed.z) < 0.0001F);

    const auto recovered = InverseTransformDirection(
        {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, transformed);
    CHECK(Dot(recovered, AtDegrees(20.0F)) > 0.9999F);

    // HUD and ballistics share the ship-local cone-rim ray. Ballistics only
    // changes its basis; it must not make a second clamp or fall back to the
    // transformed ship-forward vector.
    RimHistory sharedHistory;
    const auto localBounded = ComputeBaseRay(
        {0, 1, 0}, AtDegrees(70.0F), true, 25.0F, sharedHistory);
    CHECK(localBounded.valid);
    CHECK(localBounded.mode == Mode::ConeLimited);
    const Vector3 launchRight{0, -1, 0};
    const Vector3 launchForward{1, 0, 0};
    const Vector3 launchUp{0, 0, 1};
    Vector3 worldBounded;
    CHECK(Normalize(TransformDirection(
        launchRight, launchForward, launchUp, localBounded.ray), worldBounded));
    Vector3 worldShipForward;
    CHECK(Normalize(TransformDirection(
        launchRight, launchForward, launchUp, {0, 1, 0}), worldShipForward));
    CHECK(std::abs(AngleDegrees(Dot(worldShipForward, worldBounded)) - 25.0F) < 0.01F);
    const auto localReadBack = InverseTransformDirection(
        launchRight, launchForward, launchUp, worldBounded);
    CHECK(Dot(localReadBack, localBounded.ray) > 0.9999F);
    CHECK(Dot(worldBounded, worldShipForward) < 0.95F);
    return 0;
}
