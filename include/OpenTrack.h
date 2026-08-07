#pragma once

#include <array>
#include <cstdint>

namespace OpenTrack {
enum class ConnectionState : std::uint8_t { Waiting, Connected, Stale };

struct Sample {
    std::array<float, 3> radians{};
    std::int32_t dataId = 0;
};

bool Read(Sample& sample, int staleMilliseconds);
ConnectionState State();
std::int64_t SampleAgeMilliseconds();
void Shutdown();
} // namespace OpenTrack
