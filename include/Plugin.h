#pragma once

#include "SFSEInterface.h"
#include <string_view>

#ifndef PLUGIN_VERSION_MAJOR
#define PLUGIN_VERSION_MAJOR 0
#endif
#ifndef PLUGIN_VERSION_MINOR
#define PLUGIN_VERSION_MINOR 0
#endif
#ifndef PLUGIN_VERSION_PATCH
#define PLUGIN_VERSION_PATCH 0
#endif
#ifndef PLUGIN_VERSION_PRERELEASE
#define PLUGIN_VERSION_PRERELEASE dev
#endif

#define AHT_STRINGIZE_IMPL(value) #value
#define AHT_STRINGIZE(value) AHT_STRINGIZE_IMPL(value)

namespace Plugin {
using namespace std::string_view_literals;
inline constexpr auto Name = "AbsoluteHeadTracking"sv;
inline constexpr auto FriendlyName = "Absolute Head Tracking"sv;
inline constexpr auto Author = "Antigravity"sv;
inline constexpr REL::Version Version{
    PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH, 0
};
inline constexpr std::string_view VersionString{
    AHT_STRINGIZE(PLUGIN_VERSION_MAJOR) "." AHT_STRINGIZE(PLUGIN_VERSION_MINOR) "."
    AHT_STRINGIZE(PLUGIN_VERSION_PATCH) "-" AHT_STRINGIZE(PLUGIN_VERSION_PRERELEASE)
};
} // namespace Plugin
