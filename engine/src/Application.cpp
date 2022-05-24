#include <memory>
#include <iostream>

#include "Application.h"

#include "ImGuiImplWin32.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Timer.hpp"
#include "MapHelper.hpp"
#include "BasicMath.hpp"
#include "input/InputManager.h"
#include "core/Logging.h"

#include "editor/TestCube.h"

namespace bt {

std::unique_ptr<Engine> GEngine;
std::unique_ptr<Application> gTheApp;
std::unique_ptr<bt::input::InputManager> gInputManager;

}

using namespace bt;



// For this tutorial, we will use simple vertex shader
// that creates a procedural triangle

// Diligent Engine can use HLSL source on all supported platforms.
// It will convert HLSL to GLSL in OpenGL mode, while Vulkan backend will compile it directly to SPIRV.

static const char *VSSource = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn)
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

// Pixel shader simply outputs interpolated vertex color
static const char *PSSource = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

Application::Application() {}

Application::~Application() {
}

bool Application::Init(HWND hWnd) {

    GEngine = std::make_unique<Engine>();
    GEngine->Init("d:/_borsch_project");

    gInputManager = std::make_unique<input::InputManager>();
    gInputManager->Init();

    bt::log::Debug("===== BorschTech initialized!!! ======");

    SwapChainDesc SCDesc;
    switch (m_DeviceType) {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11: {
            EngineD3D11CreateInfo EngineCI;
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D11() function
            auto *GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#    endif
            auto *pFactoryD3D11 = GetEngineFactoryD3D11();

            m_pEngineFactory = pFactoryD3D11;
            pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pImmediateContext);
            Win32NativeWindow Window{hWnd};
            pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, m_pImmediateContext, SCDesc, FullScreenModeDesc{},
                                                Window,
                                                &m_pSwapChain);
        }
            break;
#endif


#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12: {
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D12() function
            auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#    endif
            EngineD3D12CreateInfo EngineCI;

            auto *pFactoryD3D12 = GetEngineFactoryD3D12();

            m_pEngineFactory = pFactoryD3D12;

            pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
            Win32NativeWindow Window{hWnd};
            pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, m_pImmediateContext, SCDesc, FullScreenModeDesc{},
                                                Window,
                                                &m_pSwapChain);
        }
            break;
#endif


#if GL_SUPPORTED
        case RENDER_DEVICE_TYPE_GL: {
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
            // Load the dll and import GetEngineFactoryOpenGL() function
            auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#    endif
            auto *pFactoryOpenGL = GetEngineFactoryOpenGL();

            m_pEngineFactory = pFactoryOpenGL;

            EngineGLCreateInfo EngineCI;
            EngineCI.Window.hWnd = hWnd;

            pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &m_pDevice, &m_pImmediateContext, SCDesc,
                                                       &m_pSwapChain);
        }
            break;
#endif


#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN: {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
            // Load the dll and import GetEngineFactoryVk() function
            auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#    endif
            EngineVkCreateInfo EngineCI;

            auto *pFactoryVk = GetEngineFactoryVk();
            pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, &m_pImmediateContext);

            m_pEngineFactory = pFactoryVk;

            if (!m_pSwapChain && hWnd != nullptr) {
                Win32NativeWindow Window{hWnd};
                pFactoryVk->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
            }
        }
            break;
#endif

        default:
            std::cerr << "Unknown/unsupported device type";
            return false;
            break;
    }

    // Initialize Dear ImGUI
    const auto &SC = m_pSwapChain->GetDesc();
    m_pImGui = std::make_unique<ImGuiImplWin32>(hWnd, m_pDevice, SC.ColorBufferFormat, SC.DepthBufferFormat);

    mCube = std::make_unique<TestCube>();

    return true;
}

void Application::Shutdown() {
    GEngine->Shutdown();
}

void Application::Tick(double CurrTime, double ElapsedTime) {
    gInputManager->Update();
    Update(CurrTime, ElapsedTime);
    Render();
}

void Application::Render() {
    PrepareRender();

    mCube->DrawCube(mCamera.GetProjView());

    DrawImGui();

    Present();
}

void Application::PrepareRender() {
    // Set render targets before issuing any draw command.
    // Note that Present() unbinds the back buffer if it is set as render target.
    auto *pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto *pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    // Let the engine perform required state transitions
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
                                           RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

LRESULT BORSCH_ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT Application::HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    if (BORSCH_ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    /*if (m_pImGui) {
      if (const auto Handled = static_cast<ImGuiImplWin32 *>(m_pImGui.get())->Win32_ProcHandler(hWnd, message, wParam,
                                                                                                lParam))
        return Handled;
    }*/

    if (message == WM_INPUT) {
        if (gInputManager != nullptr) {
            gInputManager->ParseMessage((void *) lParam);
        }
    }

    return 0l;
}

void Application::Update(double CurrTime, double ElapsedTime) {
    mCamera.LookAt(glm::vec3(0.f, 2.0f, -5.0f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.f, 0.f));
    mCube->Update(CurrTime, ElapsedTime);
}

void Application::DrawImGui() {
    const auto &SCDesc = m_pSwapChain->GetDesc();
    m_pImGui->NewFrame(SCDesc.Width, SCDesc.Height, SCDesc.PreTransform);

    bool showGui = true;
    ImGui::ShowDemoWindow(&showGui);

    if (ImGui::Begin("Test")) {
        //ImGui::GetWindowDrawList()->AddImage();
        ImGui::End();
    }

    m_pImGui->Render(m_pImmediateContext);
}

void Application::Present() {
    m_pSwapChain->Present();
}

void Application::WindowResize(Uint32 Width, Uint32 Height) {
    mCamera.SetViewPortSize(Width, Height);
    if (m_pSwapChain)
        m_pSwapChain->Resize(Width, Height);
}

// Called every time the NativeNativeAppBase receives a message
LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (gTheApp) {
        auto res = gTheApp->HandleWin32Message(wnd, message, wParam, lParam);
        if (res != 0)
            return res;
    }

    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(wnd, &ps);
            EndPaint(wnd, &ps);
            return 0;
        }
        case WM_SIZE: // Window size has been changed
            if (gTheApp) {
                gTheApp->WindowResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_CHAR:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;

            /*    case WM_KEYDOWN:
                    //gInputManager->HandleKeyDown();
                    return 0;*/

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO) lParam;

            lpMMI->ptMinTrackSize.x = 320;
            lpMMI->ptMinTrackSize.y = 240;
            return 0;
        }

        default:
            return DefWindowProc(wnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);

int run_vulkan_imgui_test();

int run_win31_dx12_test();

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow) {
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    //return run_vulkan_imgui_test();
    //return run_win31_dx12_test();

    gTheApp = std::make_unique<Application>();

    /*const auto* cmdLine = GetCommandLineA();
    if (!gTheApp->ProcessCommandLine(cmdLine))
        return -1;

    std::wstring Title(L"Tutorial00: Hello Win32");
    switch (gTheApp->GetDeviceType())
    {
        case RENDER_DEVICE_TYPE_D3D11: Title.append(L" (D3D11)"); break;
        case RENDER_DEVICE_TYPE_D3D12: Title.append(L" (D3D12)"); break;
        case RENDER_DEVICE_TYPE_GL: Title.append(L" (GL)"); break;
        case RENDER_DEVICE_TYPE_VULKAN: Title.append(L" (VK)"); break;
    }*/

    std::wstring WindowClass(L"BorschWindow");
    std::wstring WindowTitle(L"BorschTech");

    // Register our window class
    WNDCLASSEX wcex = {
        sizeof(WNDCLASSEX), CS_CLASSDC, MessageProc,
        0L, 0L, GetModuleHandle(NULL), nullptr, nullptr, nullptr, nullptr, WindowClass.c_str(), nullptr
    };
    RegisterClassEx(&wcex);

    // Create a window
    const LONG WindowWidth = 1280;
    const LONG WindowHeight = 1024;
    RECT Rc = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&Rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(WindowClass.c_str(), WindowTitle.c_str(),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            Rc.right - Rc.left, Rc.bottom - Rc.top, NULL, NULL, wcex.hInstance, NULL);
    if (!wnd) {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, SW_SHOWDEFAULT);
    UpdateWindow(wnd);

    if (!gTheApp->Init(wnd))
        return -1;

    Diligent::Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            auto CurrTime = Timer.GetElapsedTime();
            auto ElapsedTime = CurrTime - PrevTime;
            PrevTime = CurrTime;
            gTheApp->Tick(CurrTime, ElapsedTime);
        }
    }

    gTheApp->Shutdown();

    gTheApp.reset();

    return static_cast<int>(msg.wParam);
}
