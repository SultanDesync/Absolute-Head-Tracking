#pragma once

#include "HeadMath.h"
#include <cstdint>

namespace CameraHook {
struct Status {
    bool cameraHookInstalled = false;
    bool cockpitObserverInstalled = false;
    bool poseActive = false;
    std::int64_t cockpitSignalAgeMilliseconds = -1;
};

bool Install();
void Shutdown();
void Publish(const HeadMath::Quaternion& pose, bool active);
void Clear();
Status GetStatus();
} // namespace CameraHook
