#pragma once

#include <filesystem>
#include <string_view>

namespace RuntimePaths {
const std::filesystem::path& PluginDirectory();
const std::filesystem::path& IniPath();
const std::filesystem::path& LogPath();
void InitializeLogging();
bool LoggingEnabled();
void Log(std::string_view component, std::string_view message, bool always = false);
} // namespace RuntimePaths
