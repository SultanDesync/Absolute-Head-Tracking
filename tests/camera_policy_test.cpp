#include "CameraPolicy.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #expression); \
    std::exit(1); } } while (false)

int main() {
    CHECK(!CameraPolicy::CockpitSignalFresh(-1));
    CHECK(CameraPolicy::CockpitSignalFresh(0));
    CHECK(CameraPolicy::CockpitSignalFresh(399));
    CHECK(CameraPolicy::CockpitSignalFresh(400));
    CHECK(!CameraPolicy::CockpitSignalFresh(401));
    CHECK(CameraPolicy::CockpitSignalFresh(0, -1));
    CHECK(!CameraPolicy::CockpitSignalFresh(1, -1));
    return 0;
}
