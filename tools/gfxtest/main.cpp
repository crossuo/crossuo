// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein
#include <stdio.h>
#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>
#include <external/gfx/sokol_gfx.h>
#include <external/gfx/imgui/imgui.h>

void XUODefaultStyle()
{
    ImVec4 *colors = ImGui::GetStyle().Colors;
    /* clang-format off */
    colors[ImGuiCol_Text]                   = ImVec4(0.62f, 0.58f, 0.58f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.25f, 0.25f, 0.25f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.43f, 0.25f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.50f, 0.50f, 0.50f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.50f, 0.50f, 0.50f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.32f, 0.31f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.50f, 0.50f, 0.50f, 0.63f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.71f, 0.71f, 0.71f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.33f, 0.33f, 0.33f, 0.63f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.50f, 0.50f, 0.50f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.50f, 0.50f, 0.50f, 0.63f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.47f, 0.47f, 0.47f, 0.39f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.22f, 0.22f, 0.22f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.31f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.49f, 0.49f, 0.49f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.80f, 0.80f, 0.80f, 0.78f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    /* clang-format on */
}

#if !defined(USE_GL2)
static sg_desc sg_default_desc;
static sg_shader_stage_desc sg_default_vs;
static sg_shader_stage_desc sg_default_fs;
static sg_shader_desc sg_default_shader_desc;
#endif // #if !defined(USE_GL2)

void sokol_init()
{
#if !defined(USE_GL2)
    memset(&sg_default_desc, 0, sizeof(sg_default_desc));
    memset(&sg_default_vs, 0, sizeof(sg_default_vs));
    memset(&sg_default_fs, 0, sizeof(sg_default_fs));
    memset(&sg_default_shader_desc, 0, sizeof(sg_default_shader_desc));

    // Old OpenGL is not supported by sokol
    sg_setup(&sg_default_desc);

    // clang-format off
#if defined(USE_GLES2)
    sg_default_shader_desc.attrs[0].name = "position";
    sg_default_shader_desc.attrs[1].name = "color0";
    sg_default_vs.source = "attribute vec4 position;\n"
            "attribute vec4 color0;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_Position = position;\n"
            "  color = color0;\n"
            "}\n";
    sg_default_fs.source = "precision mediump float;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}\n";
#elif defined(USE_GL3)
    sg_default_vs.source = "#version " GL_SHADER_VERSION "\n"
                "layout(location=0) in vec4 position;\n"
                "layout(location=1) in vec4 color0;\n"
                "out vec4 color;\n"
                "void main() {\n"
                "  gl_Position = position;\n"
                "  color = color0;\n"
                "}\n";
    sg_default_fs.source = "#version " GL_SHADER_VERSION "\n"
                "in vec4 color;\n"
                "out vec4 frag_color;\n"
                "void main() {\n"
                "  frag_color = color;\n"
                "}\n";
#elif defined(USE_DX11)
    sg_default_shader_desc.attrs[0].sem_name = "POS";
    sg_default_shader_desc.attrs[1].sem_name = "COLOR";
    sg_default_vs.source = "struct vs_in {\n"
                "  float4 pos: POS;\n"
                "  float4 color: COLOR;\n"
                "};\n"
                "struct vs_out {\n"
                "  float4 color: COLOR0;\n"
                "  float4 pos: SV_Position;\n"
                "};\n"
                "vs_out main(vs_in inp) {\n"
                "  vs_out outp;\n"
                "  outp.pos = inp.pos;\n"
                "  outp.color = inp.color;\n"
                "  return outp;\n"
                "}\n";
    sg_default_fs.source = "float4 main(float4 color: COLOR0): SV_Target0 {\n"
                "  return color;\n"
                "}\n";
#endif
    // clang-format on
    sg_default_shader_desc.vs = sg_default_vs;
    sg_default_shader_desc.fs = sg_default_fs;
#endif // #if !defined(USE_GL2)
}

int main(int argc, char **argv)
{
    win_context win;
    win.title = "Sokol+ImGui";
    win.width = 800;
    win.height = 600;
    win.vsync = 0;
    if (win_init(&win) < 0)
    {
        return -1;
    }

#if !defined(USE_GL2)
    /* clang-format off */
    const float vertices[] = {
        // positions            // colors
         0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
    /* clang-format on */

    sg_buffer_desc bd = {};
    bd.size = sizeof(vertices);
    bd.content = vertices;
    sokol_init();
    sg_buffer vbuf = sg_make_buffer(&bd);
    sg_shader shd = sg_make_shader((const sg_shader_desc *)win.sg_default_shader_desc);

    sg_layout_desc layout = {};
    layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;

    sg_pipeline_desc spd = {};
    spd.shader = shd;
    spd.layout = layout;
    sg_pipeline pip = sg_make_pipeline(&spd);

    sg_bindings binds = {};
    binds.vertex_buffers[0] = vbuf;
#endif // #if !defined(USE_GL2)

    auto ui = ui_init(win);
    ui.show_stats_window = true;
    ui.show_demo_window = true;
    XUODefaultStyle();

    // Main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ui_process_event(ui, &event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(win.window))
                done = true;
        }
        ui_update(ui);

#if !defined(USE_GL2)
        sg_color_attachment_action clear_action;
        clear_action.action = SG_ACTION_CLEAR;
        clear_action.val[0] = ui.clear_color.x;
        clear_action.val[1] = ui.clear_color.y;
        clear_action.val[2] = ui.clear_color.z;
        clear_action.val[3] = ui.clear_color.w;

        sg_pass_action pass_action = {};
        pass_action.colors[0] = clear_action;

        sg_begin_default_pass(&pass_action, win.width, win.height);
        sg_apply_pipeline(pip);
        sg_apply_bindings(&binds);
        sg_draw(0, 3, 1);
        sg_end_pass();
        sg_commit();
#endif // #if !defined(USE_GL2)

        ui_draw(ui);
        win_flip(&win);
    }
    ui_shutdown(ui);
#if !defined(USE_GL2)
    sg_shutdown();
#endif // #if !defined(USE_GL2)
    win_shutdown(&win);
    return 0;
}
