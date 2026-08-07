#pragma once

// Minimal self-contained SFSE load/version ABI. This deliberately keeps the
// plugin independent of CommonLibSF, matching AbsoluteHOTAS's runtime model.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace REL {
class Version {
public:
    using value_type = std::uint16_t;
    constexpr Version() noexcept = default;
    constexpr Version(value_type major, value_type minor = 0,
                      value_type patch = 0, value_type build = 0) noexcept :
        values_{ major, minor, patch, build } {}

    [[nodiscard]] constexpr std::uint32_t pack() const noexcept {
        return static_cast<std::uint32_t>(
            (values_[0] & 0x0FFu) << 24 | (values_[1] & 0x0FFu) << 16 |
            (values_[2] & 0xFFFu) << 4 | (values_[3] & 0x00Fu));
    }

private:
    std::array<value_type, 4> values_{};
};
} // namespace REL

namespace SFSE {
struct LoadInterface;

struct PluginVersionData {
    enum Version : std::uint32_t { kVersion = 1 };

    constexpr void PluginVersion(REL::Version value) noexcept { pluginVersion = value.pack(); }
    constexpr void PluginName(std::string_view value) noexcept { SetBuffer(value, pluginName); }
    constexpr void AuthorName(std::string_view value) noexcept { SetBuffer(value, author); }
    constexpr void UsesSigScanning(bool value) noexcept { SetBit(addressIndependence, 1u << 0, value); }
    constexpr void UsesAddressLibrary(bool value) noexcept { SetBit(addressIndependence, 1u << 2, value); }
    constexpr void HasNoStructUse(bool value) noexcept { SetBit(structureCompatibility, 1u << 0, value); }
    constexpr void IsLayoutDependent(bool value) noexcept { SetBit(structureCompatibility, 1u << 3, value); }
    constexpr void CompatibleVersions(std::initializer_list<REL::Version> versions) noexcept {
        assert(versions.size() < std::size(compatibleVersions));
        std::ranges::transform(versions, std::begin(compatibleVersions),
            [](const REL::Version& version) { return version.pack(); });
    }
    constexpr void MinimumRequiredXSEVersion(REL::Version value) noexcept { xseMinimum = value.pack(); }

    const std::uint32_t dataVersion{ kVersion };
    std::uint32_t pluginVersion = 0;
    char pluginName[256]{};
    char author[256]{};
    std::uint32_t addressIndependence = 0;
    std::uint32_t structureCompatibility = 0;
    std::uint32_t compatibleVersions[16]{};
    std::uint32_t xseMinimum = 0;
    const std::uint32_t reservedNonBreaking = 0;
    const std::uint32_t reservedBreaking = 0;

private:
    template <std::size_t Size>
    static constexpr void SetBuffer(std::string_view source, char (&destination)[Size]) noexcept {
        assert(source.size() < Size);
        std::ranges::fill(destination, '\0');
        std::ranges::copy(source, destination);
    }
    static constexpr void SetBit(std::uint32_t& data, std::uint32_t bit, bool set) noexcept {
        if (set) data |= bit; else data &= ~bit;
    }
};

static_assert(offsetof(PluginVersionData, pluginVersion) == 0x004);
static_assert(offsetof(PluginVersionData, pluginName) == 0x008);
static_assert(offsetof(PluginVersionData, author) == 0x108);
static_assert(offsetof(PluginVersionData, compatibleVersions) == 0x210);
static_assert(sizeof(PluginVersionData) == 0x25C);
} // namespace SFSE

#define SFSE_EXPORT extern "C" [[maybe_unused]] __declspec(dllexport)
#define SFSE_PLUGIN_LOAD(...) SFSE_EXPORT bool SFSEPlugin_Load(__VA_ARGS__)
#define SFSE_PLUGIN_VERSION SFSE_EXPORT constinit SFSE::PluginVersionData SFSEPlugin_Version
