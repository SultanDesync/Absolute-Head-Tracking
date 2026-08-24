#pragma once

#include "OpenTrack.h"
#include <array>

namespace HeadTracking {
struct LiveState {
    std::array<float, 3> trackerDegrees{};
    std::array<float, 3> outputDegrees{};
    OpenTrack::ConnectionState connection = OpenTrack::ConnectionState::Waiting;
    bool runtimeEnabled = true;
    bool centered = false;
    bool poseAvailable = false;
};

bool Start();
void Stop();
void RequestRecenter();
void ToggleRuntime();
void SetRuntimeEnabled(bool enabled);
bool RuntimeEnabled();
LiveState GetLiveState();
} // namespace HeadTracking
