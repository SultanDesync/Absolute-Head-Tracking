set_project("AbsoluteHeadTracking")
set_version("0.1.0")
set_languages("c++23")
set_warnings("all")

add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "WINVER=0x0A00", "_WIN32_WINNT=0x0A00")
add_rules("mode.debug", "mode.releasedbg")

-- Keep the same known-good ImGui backend version as AbsoluteHOTAS. The 1.92+
-- DX12 backend uses a different descriptor-allocation contract.
add_requires("imgui v1.91.0", {configs = {glfw = false, opengl = false, win32 = true, dx12 = true}})
add_requires("minhook")
add_requires("simpleini")

option("deploydir")
    set_default("")
    set_showmenu(true)
    set_description("SFSE/Plugins folder used for live-test deployment")
option_end()

target("AbsoluteHeadTracking")
    set_kind("shared")
    add_packages("imgui", "minhook", "simpleini")
    add_includedirs("include")
    add_files("src/**.cpp")
    set_pcxxheader("include/PCH.h")
    add_defines(
        "PLUGIN_VERSION_MAJOR=0",
        "PLUGIN_VERSION_MINOR=1",
        "PLUGIN_VERSION_PATCH=0",
        "PLUGIN_VERSION_PRERELEASE=alpha"
    )
    add_syslinks("d3d12", "dxgi", "version")

    after_build(function (target)
        import("core.project.config")
        local layout = (config.get("mode") == "debug") and "PluginDebug" or "PluginRelease"
        local stage = path.join(os.projectdir(), "contrib", layout, "Data", "SFSE", "Plugins")
        local dllsrc = target:targetfile()
        local inisrc = path.join(os.projectdir(), "config", "AbsoluteHeadTracking.ini")
        os.mkdir(stage)
        os.cp(dllsrc, path.join(stage, "AbsoluteHeadTracking.dll"))
        os.cp(inisrc, path.join(stage, "AbsoluteHeadTracking.ini"))
        print("Staged AbsoluteHeadTracking.dll + .ini -> " .. stage)

        local destination = config.get("deploydir")
        if not destination or destination == "" then
            destination = os.getenv("ABSOLUTE_HEAD_TRACKING_DEPLOY_DIR")
        end
        if destination and destination ~= "" then
            os.mkdir(destination)
            os.cp(dllsrc, path.join(destination, "AbsoluteHeadTracking.dll"))
            os.cp(inisrc, path.join(destination, "AbsoluteHeadTracking.ini"))
            print("Deployed AbsoluteHeadTracking.dll + .ini -> " .. destination)
        end
    end)

target("head_math_test")
    set_kind("binary")
    set_default(false)
    add_tests("head_math")
    add_includedirs("include")
    add_files("tests/head_math_test.cpp")

target("config_policy_test")
    set_kind("binary")
    set_default(false)
    add_tests("config_policy")
    add_includedirs("include")
    add_files("tests/config_policy_test.cpp", "src/ConfigPolicy.cpp")

target("camera_policy_test")
    set_kind("binary")
    set_default(false)
    add_tests("cockpit_freshness")
    add_includedirs("include")
    add_files("tests/camera_policy_test.cpp")
