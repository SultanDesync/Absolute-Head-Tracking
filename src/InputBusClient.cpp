#include "PCH.h"

#include "InputBusClient.h"

#include "InputBusBinding.h"
#include "InputBusConsumerPolicy.h"
#include "RuntimePaths.h"

namespace {
using BusApi = AbsoluteInputBusApi::ApiV1;
using BusResult = AbsoluteInputBusApi::Result;

constexpr std::string_view kProviderId = "absolute.hotas.input-bus";
constexpr std::string_view kConsumerId = "absolute.head_tracking";

std::atomic<const BusApi*> g_api{};
std::mutex g_connectMutex;
std::mutex g_actionMutex;
std::mutex g_captureMutex;
std::uint64_t g_captureSession{};

struct RuntimeBinding {
    std::string text;
    InputBusBinding::Parsed parsed;
    InputBusBinding::PressCounter presses;
    std::uint32_t deviceIndex{};
    bool valid{};
    bool resolved{};
    std::chrono::steady_clock::time_point nextResolve{};
};

RuntimeBinding g_toggle;
RuntimeBinding g_recenter;

template <std::size_t Size>
bool Terminated(const char (&text)[Size]) noexcept {
    return std::memchr(text, '\0', Size) != nullptr;
}

template <std::size_t Size>
void Copy(char (&target)[Size], std::string_view source) noexcept {
    const auto count = (std::min)(source.size(), Size - 1);
    std::memcpy(target, source.data(), count);
    target[count] = '\0';
}

bool Valid(const BusApi* api) noexcept {
    constexpr auto required = offsetof(BusApi, cancelCapture) +
        sizeof(api->cancelCapture);
    constexpr auto capabilities =
        AbsoluteInputBusApi::kCapabilityDeviceEnumeration |
        AbsoluteInputBusApi::kCapabilitySnapshots |
        AbsoluteInputBusApi::kCapabilityButtonCapture |
        AbsoluteInputBusApi::kCapabilityRuntimeContext;
    return api && api->structSize >= required &&
        api->abiVersion == AbsoluteInputBusApi::kAbiVersion &&
        api->providerId && std::string_view(api->providerId) == kProviderId &&
        (api->capabilities & capabilities) == capabilities &&
        api->getDeviceCount && api->getDevice && api->getSnapshot &&
        api->getRuntimeContext &&
        api->beginCapture && api->pollCapture && api->cancelCapture;
}

bool FindDevice(const BusApi& api, std::string_view persistentId,
    std::uint32_t& deviceIndex, std::string* productName = nullptr) noexcept {
    try {
        const auto count = api.getDeviceCount();
        for (std::uint32_t index = 0; index < count; ++index) {
            AbsoluteInputBusApi::DeviceInfoV1 device;
            if (api.getDevice(index, &device) != BusResult::Ok ||
                !Terminated(device.persistentId) ||
                !Terminated(device.productName)) {
                continue;
            }
            if (std::string_view(device.persistentId) != persistentId) continue;
            deviceIndex = device.deviceIndex;
            if (productName) *productName = device.productName;
            return true;
        }
    } catch (...) {}
    return false;
}

bool Prepare(RuntimeBinding& binding, std::string_view text) noexcept {
    if (binding.text == text) return binding.valid;
    binding = {};
    binding.text.assign(text);
    binding.valid = InputBusBinding::Parse(text, binding.parsed);
    return binding.valid;
}

bool Resolve(RuntimeBinding& binding, const BusApi& api,
    std::chrono::steady_clock::time_point now) noexcept {
    if (binding.resolved) return true;
    if (now < binding.nextResolve) return false;
    binding.nextResolve = now + std::chrono::seconds(1);
    binding.resolved = FindDevice(api, binding.parsed.persistentId,
        binding.deviceIndex);
    if (binding.resolved) binding.presses = {};
    return binding.resolved;
}

std::uint32_t PollOne(RuntimeBinding& binding, std::string_view text,
    const BusApi& api, bool suppressed,
    std::chrono::steady_clock::time_point now) noexcept {
    if (!Prepare(binding, text) || !Resolve(binding, api, now)) return 0;
    AbsoluteInputBusApi::DeviceSnapshotV1 snapshot;
    if (api.getSnapshot(binding.deviceIndex, &snapshot) != BusResult::Ok ||
        snapshot.connected == 0 ||
        binding.parsed.digitalChannel >=
            AbsoluteInputBusApi::kDigitalControlCount) {
        return 0;
    }
    if (binding.presses.initialized &&
        binding.presses.producerGeneration != snapshot.producerGeneration) {
        binding.resolved = false;
        binding.nextResolve = {};
        if (!Resolve(binding, api, now) ||
            api.getSnapshot(binding.deviceIndex, &snapshot) != BusResult::Ok ||
            snapshot.connected == 0) {
            return 0;
        }
    }
    return InputBusBinding::ObservePresses(binding.presses,
        snapshot.producerGeneration,
        snapshot.pressCount[binding.parsed.digitalChannel], suppressed);
}
} // namespace

namespace InputBusClient {

ConnectResult Connect() noexcept {
    if (g_api.load(std::memory_order_acquire)) return ConnectResult::Connected;
    std::lock_guard lock(g_connectMutex);
    if (g_api.load(std::memory_order_relaxed)) return ConnectResult::Connected;
    const auto module = GetModuleHandleW(L"AbsoluteHOTAS.dll");
    if (!module) return ConnectResult::NotInstalled;
    const auto address = GetProcAddress(module, "AbsoluteHOTAS_QueryInputBusApi");
    if (!address) return ConnectResult::Incompatible;
    const auto query = reinterpret_cast<AbsoluteInputBusQueryApi>(address);
    const auto* api = query(AbsoluteInputBusApi::kAbiVersion);
    if (!Valid(api)) return ConnectResult::Incompatible;
    g_api.store(api, std::memory_order_release);

    std::string profile = "unavailable";
    std::uint32_t profileSlot{};
    if (api->getProfileState) {
        AbsoluteInputBusApi::ProfileStateV1 state;
        if (api->getProfileState(&state) == BusResult::Ok &&
            Terminated(state.profileId)) {
            profile = state.profileId;
            profileSlot = state.activeSlot;
        }
    }
    RuntimePaths::Log("[InputBus]", std::format(
        "Connected to Absolute Input Bus ABI {} (HOTAS version {}, devices {}, profile {} / slot {}).",
        AbsoluteInputBusApi::kAbiVersion, api->version ? api->version : "unknown",
        api->getDeviceCount(), profile, profileSlot), true);
    return ConnectResult::Connected;
}

bool Connected() noexcept {
    return g_api.load(std::memory_order_acquire) != nullptr;
}

bool IsDigitalBinding(std::string_view binding) noexcept {
    if (binding.empty()) return true;
    InputBusBinding::Parsed parsed;
    return InputBusBinding::Parse(binding, parsed);
}

std::string DisplayBinding(std::string_view binding) {
    if (binding.empty()) return "Unbound";
    InputBusBinding::Parsed parsed;
    if (!InputBusBinding::Parse(binding, parsed)) return "Invalid controller binding";
    std::string product;
    std::uint32_t index{};
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api || !FindDevice(*api, parsed.persistentId, index, &product)) {
        product = "Unavailable device";
    }
    return std::format("{} / {}", product,
        InputBusBinding::ControlLabel(parsed));
}

ActionEdges PollActions(std::string_view toggleBinding,
    std::string_view recenterBinding, bool suppressed) noexcept {
    ActionEdges actions;
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api) return actions;
    try {
        std::lock_guard lock(g_actionMutex);
        const auto now = std::chrono::steady_clock::now();
        actions.togglePresses = PollOne(
            g_toggle, toggleBinding, *api, suppressed, now);
        actions.recenterPresses = PollOne(
            g_recenter, recenterBinding, *api, suppressed, now);
    } catch (...) {}
    return actions;
}

std::int64_t __cdecl PilotSignalAgeMilliseconds() noexcept {
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api || !api->getRuntimeContext) return -1;
    try {
        AbsoluteInputBusApi::RuntimeContextV1 context;
        if (api->getRuntimeContext(&context) != BusResult::Ok) return -1;
        return InputBusConsumerPolicy::PilotSignalAge(context);
    } catch (...) {
        return -1;
    }
}

BusResult BeginDigitalCapture() noexcept {
    if (!Connected() && Connect() != ConnectResult::Connected)
        return BusResult::NotReady;
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api) return BusResult::NotReady;
    std::lock_guard lock(g_captureMutex);
    if (g_captureSession != 0) return BusResult::Busy;
    AbsoluteInputBusApi::CaptureRequestV1 request;
    request.allowedControls = AbsoluteInputBusApi::kCaptureDigital;
    request.settleMilliseconds = 50;
    request.timeoutMilliseconds = 8000;
    Copy(request.consumerId, kConsumerId);
    std::uint64_t session{};
    const auto result = api->beginCapture(&request, &session);
    if (result == BusResult::Ok) {
        g_captureSession = session;
        RuntimePaths::Log("[InputBus]", std::format(
            "Controller capture {} started for Head Tracking.", session), true);
    }
    return result;
}

BusResult PollCapture(AbsoluteInputBusApi::CaptureResultV1& result) noexcept {
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api) return BusResult::NotReady;
    std::lock_guard lock(g_captureMutex);
    if (g_captureSession == 0) return BusResult::StaleSession;
    const auto status = api->pollCapture(g_captureSession, &result);
    if (status == BusResult::Ok && result.state !=
        AbsoluteInputBusApi::CaptureState::Capturing &&
        result.state != AbsoluteInputBusApi::CaptureState::Idle) {
        const auto completedSession = g_captureSession;
        g_captureSession = 0;
        const std::string_view binding = Terminated(result.binding.bindingText)
            ? std::string_view(result.binding.bindingText) : std::string_view{};
        RuntimePaths::Log("[InputBus]", std::format(
            "Controller capture {} ended in state {}{}{}.", completedSession,
            static_cast<std::uint32_t>(result.state),
            binding.empty() ? "" : " with ", binding), true);
    }
    return status;
}

BusResult CancelCapture() noexcept {
    const auto* api = g_api.load(std::memory_order_acquire);
    if (!api) return BusResult::NotReady;
    std::lock_guard lock(g_captureMutex);
    if (g_captureSession == 0) return BusResult::StaleSession;
    const auto session = g_captureSession;
    const auto result = api->cancelCapture(session);
    if (result == BusResult::Ok || result == BusResult::StaleSession) {
        g_captureSession = 0;
    }
    RuntimePaths::Log("[InputBus]", std::format(
        "Controller capture {} cancellation returned {}.", session,
        static_cast<std::uint32_t>(result)), true);
    return result;
}

} // namespace InputBusClient
