add_rules("mode.debug", "mode.release")

set_languages("c++20")

add_requires("imgui", {configs = {glfw_opengl3 = true}})
add_requires("lua")
add_requires("stb")
add_requires("glew")

set_exceptions("cxx", "objc")

add_defines("UNICODE", "_UNICODE")

if is_plat("windows") then
    add_defines("WIN32", "_WIN32", "_WINDOWS", "NOMINMAX")
    add_cxflags("/utf-8", "/Zc:__cplusplus", "/permissive", "/bigobj", "/Zc:preprocessor", "/Zc:wchar_t",
        "/Zc:forScope", "/MP")
    add_syslinks("ws2_32", "wininet")
elseif is_plat("linux") then
    add_cxflags("-Wtautological-compare")
    add_cxflags("-fno-strict-aliasing", "-fms-extensions", "-finline-functions", "-fPIC")

    add_syslinks("GL")
elseif is_plat("macosx") then
    add_cxflags("-Wtautological-compare")
    add_cxflags("-fno-strict-aliasing", "-fms-extensions", "-finline-functions", "-fPIC")
end

if is_mode("release") then
    set_runtimes("MT")
else
    set_runtimes("MTd")
end

target("example")
    set_kind("binary")
    add_headerfiles("lite.h")
    add_files("lite.cpp", "example/main.cpp")
    add_packages("lua", "imgui", "stb", "glew")

    set_targetdir("./")
    set_rundir("./")
