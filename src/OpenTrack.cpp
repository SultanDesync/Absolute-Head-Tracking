#include "PCH.h"

#include "OpenTrack.h"
#include "RuntimePaths.h"

namespace {
struct FreeTrackData {
    std::int32_t dataId;
    std::int32_t cameraWidth;
    std::int32_t cameraHeight;
    float yaw;
    float pitch;
    float roll;
    float x;
    float y;
    float z;
    float rawYaw;
    float rawPitch;
    float rawRoll;
    float rawX;
    float rawY;
    float rawZ;
    float pointX1;
    float pointY1;
    float pointX2;
    float pointY2;
    float pointX3;
    float pointY3;
    float pointX4;
    float pointY4;
};

struct FreeTrackHeap {
    FreeTrackData data;
    std::int32_t gameId;
    std::array<std::uint8_t, 8> table;
    std::int32_t gameId2;
};

static_assert(sizeof(FreeTrackData) == 92);
static_assert(sizeof(FreeTrackHeap) == 108);

HANDLE g_mapping = nullptr;
HANDLE g_mutex = nullptr;
const FreeTrackHeap* g_view = nullptr;
std::int32_t g_lastDataId = std::numeric_limits<std::int32_t>::min();
std::chrono::steady_clock::time_point g_lastFrame{};
std::chrono::steady_clock::time_point g_nextAttempt{};
std::atomic<OpenTrack::ConnectionState> g_state{ OpenTrack::ConnectionState::Waiting };
std::atomic<std::int64_t> g_ageMilliseconds{ -1 };
bool g_loggedWaiting = false;

void CloseTransport() {
    if (g_view) UnmapViewOfFile(g_view);
    if (g_mapping) CloseHandle(g_mapping);
    if (g_mutex) CloseHandle(g_mutex);
    g_view = nullptr;
    g_mapping = nullptr;
    g_mutex = nullptr;
    g_lastDataId = std::numeric_limits<std::int32_t>::min();
    g_lastFrame = {};
    g_ageMilliseconds.store(-1, std::memory_order_release);
    g_state.store(OpenTrack::ConnectionState::Waiting, std::memory_order_release);
}

bool EnsureTransport() {
    if (g_view) return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextAttempt) return false;
    g_nextAttempt = now + std::chrono::seconds(1);
    g_mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, L"FT_SharedMem");
    if (!g_mapping) {
        if (!g_loggedWaiting) {
            RuntimePaths::Log("[OpenTrack]", "Waiting for freetrack 2.0 Enhanced output.");
            g_loggedWaiting = true;
        }
        return false;
    }
    g_view = static_cast<const FreeTrackHeap*>(MapViewOfFile(
        g_mapping, FILE_MAP_READ, 0, 0, sizeof(FreeTrackHeap)));
    if (!g_view) {
        CloseHandle(g_mapping);
        g_mapping = nullptr;
        return false;
    }
    // The protocol's historical mutex name intentionally contains this typo.
    g_mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"FT_Mutext");
    g_loggedWaiting = false;
    RuntimePaths::Log("[OpenTrack]", "Connected to FreeTrack shared memory.");
    return true;
}

bool CopyFrame(FreeTrackData& frame) {
    if (!g_view) return false;
    bool locked = false;
    if (g_mutex) {
        const DWORD wait = WaitForSingleObject(g_mutex, 2);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return false;
        locked = true;
    }
    std::memcpy(&frame, &g_view->data, sizeof(frame));
    if (locked) ReleaseMutex(g_mutex);
    return std::isfinite(frame.yaw) && std::isfinite(frame.pitch) &&
           std::isfinite(frame.roll);
}
} // namespace

namespace OpenTrack {

bool Read(Sample& sample, int staleMilliseconds) {
    if (!EnsureTransport()) {
        g_state.store(ConnectionState::Waiting, std::memory_order_release);
        g_ageMilliseconds.store(-1, std::memory_order_release);
        return false;
    }
    FreeTrackData frame{};
    if (!CopyFrame(frame)) return false;
    const auto now = std::chrono::steady_clock::now();
    if (frame.dataId != g_lastDataId) {
        g_lastDataId = frame.dataId;
        g_lastFrame = now;
    }
    if (g_lastFrame.time_since_epoch().count() == 0) return false;
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_lastFrame).count();
    g_ageMilliseconds.store(age, std::memory_order_release);
    if (age > std::clamp(staleMilliseconds, 50, 5000)) {
        g_state.store(ConnectionState::Stale, std::memory_order_release);
        return false;
    }
    sample.radians = { frame.yaw, frame.pitch, frame.roll };
    sample.dataId = frame.dataId;
    g_state.store(ConnectionState::Connected, std::memory_order_release);
    return true;
}

ConnectionState State() {
    return g_state.load(std::memory_order_acquire);
}

std::int64_t SampleAgeMilliseconds() {
    return g_ageMilliseconds.load(std::memory_order_acquire);
}

void Shutdown() {
    CloseTransport();
}

} // namespace OpenTrack
