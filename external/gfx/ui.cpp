// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "ui.h"

#include "gfx.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/IconsForkAwesomeData.h"

#if defined(USE_GL3)
#include "imgui/imgui_impl_opengl3.h"
#elif defined(USE_GL2)
#include "imgui/imgui_impl_opengl2.h"
#elif defined(USE_DX11)
#include "imgui/imgui_impl_dx11.h"
#elif defined(USE_METAL)
#include "imgui/imgui_impl_metal.h"
#else
#error "SOKOL 3D API not defined / unknown"
#endif

ui_context ui_init(win_context &win)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

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

#if defined(USE_GL)
    ImGui_ImplSDL2_InitForOpenGL(win.window, win.context);
#endif

#if defined(USE_GL3)
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
#elif defined(USE_GL2)
    ImGui_ImplOpenGL2_Init();
#elif defined(USE_DX11)
    // create device d3d
	SDL_SysWMinfo info;
	HWND hWnd = 0;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(win.window, &info))
		hWnd = info.info.win.window;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    if (D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext) != S_OK)
    {
        if (g_mainRenderTargetView)
        {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
        if (g_pSwapChain)
        {
            g_pSwapChain->Release();
            g_pSwapChain = nullptr;
        }
        if (g_pd3dDeviceContext)
        {
            g_pd3dDeviceContext->Release();
            g_pd3dDeviceContext = nullptr;
        }
        if (g_pd3dDevice)
        {
            g_pd3dDevice->Release();
            g_pd3dDevice = nullptr;
        }
        return {};
    }

    // create render target
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();

    ImGui_ImplSDL2_InitForD3D(win.window);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
	win.context = g_pd3dDeviceContext;
#elif defined(USE_METAL)
    // ImGui_ImplMetal_Init()
#endif

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    ui_context ctx;
    ctx.win = &win;
    const auto fontSize = 13.0f;
    static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
    ImFontConfig config;
    config.MergeMode = true;
    io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromMemoryCompressedTTF(iconsforkawesome_compressed_data, iconsforkawesome_compressed_size, fontSize, &config, icons_ranges);
    //io.Fonts->AddFontFromFileTTF("forkawesome-webfont.ttf", 18.0f, &config, icons_ranges);
    io.Fonts->Build();
    return ctx;
}

void ui_process_event(ui_context &, const SDL_Event *event)
{
    ImGui_ImplSDL2_ProcessEvent(event);
}

void ui_draw(ui_context &ui)
{
	(void)ui;
    ImGui::Render();
#if defined(USE_GL3)
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#elif defined(USE_GL2)
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#elif defined(USE_DX11)
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float *)&ui.clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#elif defined(USE_METAL)
    //ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), ...);
#endif

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
#if defined(USE_GL)
        SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
#endif
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
#if defined(USE_GL)
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
#endif
    }

#if defined(USE_DX11)
	g_pSwapChain->Present(ui.win->vsync, 0);
#endif
}

void ui_update(ui_context &ctx)
{
#if defined(USE_GL3)
    ImGui_ImplOpenGL3_NewFrame();
#elif defined(USE_GL2)
    ImGui_ImplOpenGL2_NewFrame();
#elif defined(USE_DX11)
    ImGui_ImplDX11_NewFrame();
#elif defined(USE_METAL)
    //ImGui_ImplMetal_NewFrame(...);
#endif

    ImGui_ImplSDL2_NewFrame(ctx.win->window);
    ImGui::NewFrame();

    if (ctx.show_demo_window)
        ImGui::ShowDemoWindow();
        
    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    if (ctx.show_stats_window)
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

        ImGui::Text(
            "This is some useful text."); // Display some text (you can use a format strings too)
        ImGui::Checkbox(
            "Demo Window", &ctx.show_demo_window); // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &ctx.show_another_window);

        ImGui::SliderFloat(
            "float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3(
            "clear color", (float *)&ctx.clear_color); // Edit 3 floats representing a color

        if (ImGui::Button(
                "Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (ctx.show_another_window)
    {
        ImGui::Begin(
            "Another Window",
            &ctx.show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            ctx.show_another_window = false;
        ImGui::End();
    }

    ImGuiIO &io = ImGui::GetIO();
    ctx.win->width = (int)io.DisplaySize.x;
    ctx.win->height = (int)io.DisplaySize.y;
}

void ui_shutdown(ui_context &)
{
#if defined(USE_GL3)
    ImGui_ImplOpenGL3_Shutdown();
#elif defined(USE_GL2)
    ImGui_ImplOpenGL2_Shutdown();
#elif defined(USE_DX11)
    ImGui_ImplDX11_Shutdown();
	if (g_mainRenderTargetView)
	{
		g_mainRenderTargetView->Release();
		g_mainRenderTargetView = nullptr;
	}
	if (g_pSwapChain)
	{
		g_pSwapChain->Release();
		g_pSwapChain = nullptr;
	}
	if (g_pd3dDeviceContext)
	{
		g_pd3dDeviceContext->Release();
		g_pd3dDeviceContext = nullptr;
	}
	if (g_pd3dDevice)
	{
		g_pd3dDevice->Release();
		g_pd3dDevice = nullptr;
	}
#elif defined(USE_METAL)
    //ImGui_ImplMetal_Shutdown(...);
#endif
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}
