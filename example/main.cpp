// Lite - A lightweight text editor written in Lua
// ImLite - An embeddable Lite for dear imgui (MIT license)
// modified from https://github.com/rxi/lite (MIT license)
//               https://github.com/r-lyeh/FWK (public domain)
// modified by KaoruXun(cstom4994) for NekoEngine

#include <GLFW/glfw3.h>
#include <stdio.h>

#include "../lite.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <iostream>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

GLFWwindow* glfw_window;

static void glfw_error_callback(int error, const char* description) { fprintf(stderr, "Glfw Error %d: %s\n", error, description); }

double window_scale() {
    float xscale = 1, yscale = 1;
//#ifndef NEKO_IS_APPLE  // silicon mac M1 hack
//    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
//    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
//#endif
    return NEKO_MAX(xscale, yscale);
}

ImVec2 get_mouse_in_window() {
    double x, y;
    glfwGetCursorPos(glfw_window, &x, &y);
    return ImVec2(x, y);
}

int main(int, char**) {
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    glfw_window = glfwCreateWindow(1280, 720, "ImLite Example", NULL, NULL);
    if (glfw_window == NULL) return 1;
    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();  // http://www.opengl.org/wiki/OpenGL_Loading_Library

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // lua_register(g_app->lite_L, "__neko_loader", neko::vfs_lua_loader);
    // const_str str = "table.insert(package.searchers, 2, __neko_loader) \n";
    // luaL_dostring(g_app->lite_L, str);

    lt_init(L, glfw_window, "./lite", __argc, __argv, window_scale(), "Windows");

    // Main loop
    while (!glfwWindowShouldClose(glfw_window)) {

        glfwPollEvents();

        lt_tick(L);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Lite")) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImVec2 bounds = ImGui::GetContentRegionAvail();
            ImVec2 mouse_pos = get_mouse_in_window();  // 窗口内鼠标坐标
            assert(window);
            ImVec2 pos = window->Pos;
            ImVec2 size = window->Size;
            lt_mx = mouse_pos.x - pos.x;
            lt_my = mouse_pos.y - pos.y;
            lt_wx = pos.x;
            lt_wy = pos.y;
            lt_ww = size.x;
            lt_wh = size.y;
            if (lt_resizesurface(lt_getsurface(0), lt_ww, lt_wh)) {
                // window_refresh_callback(g_app->game_window);
            }
            ImGui::Image((ImTextureID)lt_getsurface(0)->t.id, bounds);
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(glfw_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(glfw_window);
    }

    // Cleanup
    lt_fini();
    lua_close(L);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window);
    glfwTerminate();

    return 0;
}