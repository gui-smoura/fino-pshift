#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "fino/audio_engine.hpp"
#include "fino/config_manager.hpp"
#include "fino/ui.hpp"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char* argv[]) {
#if defined(_WIN32)
    // If user explicitly asks for --console in command line, attach or allocate console
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--console") {
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                AllocConsole();
            }
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            break;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // OpenGL 3.0 Context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(680, 800, "fino-pshift - Real-Time Audio Pitch Shifter", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Pacing handled by high-precision 30 FPS timer

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Initialize ImGui Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Initialize Audio Engine & Configurations
    const std::string config_path = "config.json";
    fino::AppConfig config;
    fino::ConfigManager::load_from_file(config_path, config);

    fino::AudioEngine engine;
    engine.initialize();

    fino::UI ui(engine, config, config_path);
    ui.setup_style();

    // Frame pacing: 30 FPS active (~33.33ms), 10 FPS minimized (100ms)
    constexpr auto TARGET_FRAME_DURATION = std::chrono::microseconds(33333);
    constexpr auto BACKGROUND_FRAME_DURATION = std::chrono::microseconds(100000);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        const auto frame_start = std::chrono::steady_clock::now();

        glfwPollEvents();

        const bool is_iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;

        if (!is_iconified) {
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Render UI panels
            ui.render();

            // Rendering
            ImGui::Render();
            int display_w = 0, display_h = 0;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.09f, 0.11f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        // Pacing sleep to cap CPU/GPU resource usage
        const auto target_duration = is_iconified ? BACKGROUND_FRAME_DURATION : TARGET_FRAME_DURATION;
        const auto elapsed = std::chrono::steady_clock::now() - frame_start;
        if (elapsed < target_duration) {
            std::this_thread::sleep_for(target_duration - elapsed);
        }
    }

    // Save configuration before exiting
    fino::ConfigManager::save_to_file(config_path, config);

    // Stop audio engine safely (audio thread join & uninit)
    engine.stop();

    // Cleanup ImGui & GLFW
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
