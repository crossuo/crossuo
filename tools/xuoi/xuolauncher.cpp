// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein
#define USE_SOKOL 1

#include <stdio.h>
#include <external/gfx/gfx.h>
#if USE_SOKOL
#include <external/gfx/sokol_gfx.h>
#include <external/gfx/sokol_imgui.h>
#endif
#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_impl_sdl.h>
#if !USE_SOKOL
#include <external/gfx/imgui/imgui_impl_opengl2.h>
#endif

#include "common.h"

// return y increment of 0 so to not modify line height
static ImVec2 CustomRichTextElementDrawCallback(
    ImVec2 pos, ImDrawList *drawlist, const char *tag_begin, const char *tag_end)
{
    const float height =
        19.0f; // We're drawing inside the window title bar, so it would be better get the titlebar height here
    const float width = 29.0f;

    auto size = tag_end - tag_begin;
    if (memcmp("icon.png", tag_begin, size) == 0)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImTextureID my_tex_id = io.Fonts->TexID;
        float my_tex_w = (float)io.Fonts->TexWidth;
        float my_tex_h = (float)io.Fonts->TexHeight;
        ImGui::ImageButton(
            my_tex_id,
            ImVec2(height, height),
            ImVec2(0, 0),
            ImVec2(height / my_tex_w, height / my_tex_h),
            0,
            ImColor(0, 0, 0, 255));
        return ImVec2(height, 0.0f);
    }
    else if (memcmp("healthbar", tag_begin, size) == 0)
    {
        // Create a dummy array of contiguous float values to plot
        // Tip: If your float aren't contiguous but part of a structure, you can pass a pointer to your first float and the sizeof() of your structure in the Stride parameter.
        static float values[20] = { 0 };
        static int values_offset = 0;
        static double refresh_time = 0.0;
        while (refresh_time <
               ImGui::GetTime()) // Create dummy data at fixed 60 hz rate for the demo
        {
            static float phase = 0.0f;
            values[values_offset] = cosf(phase);
            values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
            phase += 0.10f * values_offset;
            refresh_time += 1.0f / 60.0f;
        }
        ImGui::PlotLines(
            "HP", values, IM_ARRAYSIZE(values), values_offset, "", -1.0f, 1.0f, ImVec2(30, height));
        return ImVec2(45.0f, 0.0f); // plot width + "hp" width
    }

    return ImVec2(width, 0.0f); // 0 so to not modify line height
}

// return y increment of 0 so to not modify line height
static ImVec2 CustomRichTextElementCalcSizeCallback(const char *tag_begin, const char *tag_end)
{
    const float height = 26.0f; // should be current window TitleBarHeight

    auto size = tag_end - tag_begin;
    if (memcmp("icon.png", tag_begin, size) == 0)
    {
        return ImVec2(19.0f, 0.0f);
    }
    else if (memcmp("healthbar", tag_begin, size) == 0)
    {
        return ImVec2(45.0f, 0.0f);
    }
    return ImVec2(height, 0.0f); // 0 so to not modify line height
}

int main(int argc, char **argv)
{
    const int W = 1280;
    const int H = 720;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    const auto window_flags = (SDL_WindowFlags)(
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow(
        "X:UO Launcher", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

#if USE_SOKOL
    glewInit();
    sg_desc desc = {};
    sg_setup(&desc);

    const float vertices[] = { // positions            // colors
                               0.0f, 0.5f, 0.5f, 1.0f,  0.0f,  0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f,
                               1.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,  1.0f
    };

    sg_buffer_desc bd = {};
    bd.size = sizeof(vertices);
    bd.content = vertices;
    sg_buffer vbuf = sg_make_buffer(&bd);

    sg_shader_stage_desc vs = {};
    vs.source = "#version 330\n"
                "layout(location=0) in vec4 position;\n"
                "layout(location=1) in vec4 color0;\n"
                "out vec4 color;\n"
                "void main() {\n"
                "  gl_Position = position;\n"
                "  color = color0;\n"
                "}\n";
    sg_shader_stage_desc fs = {};
    fs.source = "#version 330\n"
                "in vec4 color;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = color;\n"
                "}\n";
    sg_shader_desc sd = {};
    sd.vs = vs;
    sd.fs = fs;
    sg_shader shd = sg_make_shader(&sd);

    sg_layout_desc layout = {};
    layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;

    sg_pipeline_desc spd = {};
    spd.shader = shd;
    spd.layout = layout;
    sg_pipeline pip = sg_make_pipeline(&spd);

    sg_bindings binds = {};
    binds.vertex_buffers[0] = vbuf;
    sg_pass_action pass_action = {};

    simgui_desc_t sid = {};
    simgui_setup(&sid);
#else
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
#endif // #if USE_SOKOL

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    io.ConfigFlags |= ImGuiViewportFlags_CanHostOtherWindows;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
#if !USE_SOKOL
    ImGui_ImplOpenGL2_Init();
#else
    ImGui_ImplSokol_Init();
#endif

    ImGui::SetRichTextElementCallbacks(
        CustomRichTextElementCalcSizeCallback, CustomRichTextElementDrawCallback);
    static uint64_t g_Time = SDL_GetPerformanceCounter();
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
        }

#if !USE_SOKOL
        ImGui_ImplOpenGL2_NewFrame();
#else
        ImGui_ImplSokol_NewFrame();
#endif
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        if (ImGui::BeginMenu("Menu"))
        {
            ImGui::MenuItem("Submenu", NULL, false, false);
            ImGui::EndMenu();
        }

        // 0. Bind the Titlebar to the application main window and add custom wigets to it
        // This unfortunatelly currently breaks window parenting (and probably docking)
        // I will try to fix it, but it is taking some time to understand the parenting flow
        // and there is always something wrong in all my tries, pointing out to me that I still
        // don't completely understood this logic
        ImGui::ShowDemoWindow();
        ImGui::EnableRichText();
        static bool is_open = true;
        if (ImGui::BeginTitleMenuBar(
                "{icon.png} App Main Window Custom Title Bar - {healthbar} - ", &is_open))
        {
            ImGui::DisableRichText();
            if (ImGui::BeginMenu("Menu"))
            {
                ImGui::MenuItem("Submenu", NULL, false, false);
                ImGui::EndMenu();
            }
            ImGui::EndTitleMenuBar();
        }

        if (!is_open)
        {
            done = true;
        }

#if USE_SOKOL
        sg_begin_default_pass(&pass_action, W, H);
        sg_apply_pipeline(pip);
        sg_apply_bindings(&binds);
        sg_draw(0, 3, 1);

        //simgui_render();
        ImGui::Render();
        ImGui_ImplSokol_RenderDrawData(ImGui::GetDrawData());
#else
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }
#if USE_SOKOL
        sg_end_pass();
        sg_commit();
#endif
        SDL_GL_SwapWindow(window);
    }

#if USE_SOKOL
    simgui_shutdown();
    ImGui_ImplSokol_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    sg_shutdown();
#else
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
#endif

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}