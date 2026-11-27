#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if _WIN32
#include <Windows.h>
#endif

#include <locale>
#include <stdio.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnotepad/imnotepad.h"
#include "imfiletree/imfiletree.h"

void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char* argv[])
{
#if _WIN32
    setlocale(LC_ALL, ".UTF-8"); // Only for Windows 10 1903 and later
#else
    setlocale(LC_ALL, "");
#endif
    setlocale(LC_NUMERIC, "C");

    const char* window_title    = "ImNotepad Demo";
    int         window_width    = 1600;
    int         window_height   = 900;
    bool        vsync_enable    = true;

    GLFWwindow* window = nullptr;

    // Renderer initialization
    {
        glfwSetErrorCallback(glfw_error_callback);
        if(!glfwInit())
            return EXIT_FAILURE;

        // GL 3.0 + GLSL 130
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_SAMPLES, 4);

        // Create window with graphics context
        window = glfwCreateWindow(window_width, window_height, window_title, NULL, NULL);
        if(window == nullptr)
            return EXIT_FAILURE;

        glfwMakeContextCurrent(window);

        // Set window position
        const GLFWvidmode* video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowPos(window, (video_mode->width - window_width) / 2, (video_mode->height - window_height) / 2);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(vsync_enable);

        if(glewInit() != GLEW_OK)
            return EXIT_FAILURE;

        glEnable(GL_MULTISAMPLE);
        glEnable(GL_LINE_SMOOTH);

        // Global deth setting
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    // ImGui initialization
    {
        ImGui::CreateContext();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking

        // Load a custom font for the main interface. The font must contain icon set baked as a Nerd fonts: https://www.nerdfonts.com/font-downloads
        io.Fonts->AddFontFromFileTTF(IM_NOTEPAD_FONTS_DIR "DejaVuSansMNerdFont-Regular.ttf", 16.0f);

        ImFileTree::CreateContext();
        ImFileTree::AddRootPath("../../src/libs/imgui", false); // Set the workspace root directory

        ImNotepad::CreateContext();
        // Load a custom font for text rendering. The font must be strictly MONOSPACED!
        ImFont* font = io.Fonts->AddFontFromFileTTF(IM_NOTEPAD_FONTS_DIR "SourceCodePro-Medium.ttf", 18.0f);
        ImNotepad::GetStyle().SetFontText(font);

        ImNotepad::TextFileRead("../../src/libs/imgui/LICENSE.txt");

        // Open file and jump to specific line
        ImNotepadTextFileConfig file_cfg;
        file_cfg.CursorLineIdx = 431;
        ImNotepad::TextFileRead("../../src/libs/imgui/imgui_draw.cpp", &file_cfg);
    }

    while(!glfwWindowShouldClose(window))
    {
        glfwMakeContextCurrent(window);
        glfwPollEvents();

        float clear_color[] = { 0.45f, 0.55f, 0.60f, 1.00f };
        glViewport(0, 0, window_width, window_height);
        glClearColor(clear_color[0] * clear_color[3], clear_color[1] * clear_color[3], clear_color[2] * clear_color[3], clear_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImNotepad::ShowDemoWindow();
        ImGui::Render();
        ImGui::EndFrame();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwMakeContextCurrent(NULL);
    }

    // Cleanup the Dear ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    // ImGui shutdown
    ImNotepad::DestroyContext();
    ImFileTree::DestroyContext();
    ImGui::DestroyContext();

    // Renderer shutdown
    glfwMakeContextCurrent(NULL);
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
