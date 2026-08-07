#include "PCH.h"

#include "RuntimePaths.h"

namespace {
std::once_flag g_pathsOnce;
std::filesystem::path g_pluginDirectory;
std::filesystem::path g_iniPath;
std::filesystem::path g_logPath;
std::atomic<bool> g_loggingEnabled{ false };
std::mutex g_logMutex;

void ResolvePaths() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ResolvePaths), &module);
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = module
        ? GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()))
        : 0;
    g_pluginDirectory = length
        ? std::filesystem::path(std::wstring_view(buffer.data(), length)).parent_path()
        : std::filesystem::current_path();
    g_iniPath = g_pluginDirectory / "AbsoluteHeadTracking.ini";
    g_logPath = g_pluginDirectory / "AbsoluteHeadTracking.log";
}
} // namespace

namespace RuntimePaths {

const std::filesystem::path& PluginDirectory() {
    std::call_once(g_pathsOnce, ResolvePaths);
    return g_pluginDirectory;
}

const std::filesystem::path& IniPath() {
    std::call_once(g_pathsOnce, ResolvePaths);
    return g_iniPath;
}

const std::filesystem::path& LogPath() {
    std::call_once(g_pathsOnce, ResolvePaths);
    return g_logPath;
}

void InitializeLogging() {
    std::array<char, 32> value{};
    GetPrivateProfileStringA("General", "bEnableLog", "false", value.data(),
        static_cast<DWORD>(value.size()), IniPath().string().c_str());
    std::string normalized(value.data());
    std::ranges::transform(normalized, normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    g_loggingEnabled.store(normalized == "true" || normalized == "1" ||
        normalized == "yes" || normalized == "on", std::memory_order_release);
}

bool LoggingEnabled() {
    return g_loggingEnabled.load(std::memory_order_acquire);
}

void Log(std::string_view component, std::string_view message, bool always) {
    if (!always && !LoggingEnabled()) return;
    std::lock_guard lock(g_logMutex);
    std::ofstream stream(LogPath(), std::ios::app);
    if (!stream) return;
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << ' '
           << component << ' ' << message << '\n';
}

} // namespace RuntimePaths
