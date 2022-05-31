#include <memory>
#include <iostream>

#include "Application.h"

#include "ImGuiImplWin32.hpp"
#include "Timer.hpp"
#include "BasicMath.hpp"
#include "input/InputManager.h"
#include "core/Logging.h"
#include "editor/TestCube.h"
#include "core/Logging.h"
#include "imgui.h"

namespace bt {

    std::unique_ptr<Application> gTheApp;

    Application::Application() {
        m_log = new EditorLog();
        log::GLogger->RegisterDelegate(m_log);
    }

    Application::~Application() {
    }

    bool Application::Init(HWND hWnd) {
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
            }
                break;
#endif

            default:
                std::cerr << "Unknown/unsupported device type";
                return false;
                break;
        }

        CreateSwapChain(m_pSwapChain, hWnd, false);

        // Initialize Dear ImGUI
        const auto &SC = m_pSwapChain->GetDesc();
        m_pImGui = std::make_unique<ImGuiImplWin32>(hWnd, m_pDevice, SC.ColorBufferFormat, SC.DepthBufferFormat);

        mCube = std::make_unique<TestCube>();
        mCube2 = std::make_unique<TestCube>();

        mCube->SetLocation(glm::vec3(1.f, 0.f, 0.f));
        mCube2->SetLocation(glm::vec3(-1.f, 0.f, 0.f));

        mTestRenderTarget = std::make_unique<RenderTarget>(m_pDevice);

        return true;
    }

    bool Application::CreateSwapChain(RefCntAutoPtr<ISwapChain> &result, HWND hWnd, bool isAdditional) {
        SwapChainDesc SCDesc;

        if (isAdditional) {
            SCDesc.IsPrimary = !isAdditional;
        }

        switch (m_DeviceType) {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11: {
                Win32NativeWindow Window{hWnd};
                ((IEngineFactoryD3D11 *) m_pEngineFactory.RawPtr())->CreateSwapChainD3D11(m_pDevice,
                                                                                          m_pImmediateContext,
                                                                                          SCDesc,
                                                                                          FullScreenModeDesc{},
                                                                                          Window,
                                                                                          &result);
            }
                break;
#endif


#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12: {
                Win32NativeWindow Window{hWnd};
                ((IEngineFactoryD3D12 *) m_pEngineFactory.RawPtr())->CreateSwapChainD3D12(m_pDevice,
                                                                                          m_pImmediateContext,
                                                                                          SCDesc,
                                                                                          FullScreenModeDesc{},
                                                                                          Window,
                                                                                          &result);
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
                Win32NativeWindow Window{hWnd};
                ((IEngineFactoryVk *) m_pEngineFactory.RawPtr())->CreateSwapChainVk(m_pDevice,
                                                                                    m_pImmediateContext,
                                                                                    SCDesc,
                                                                                    Window,
                                                                                    &result);
            }
                break;
#endif

            default:
                std::cerr << "Unknown/unsupported device type";
                return false;
                break;
        }

        return m_pSwapChain != nullptr;
    }

    void Application::Shutdown() {
    }

    void Application::Tick(double CurrTime, double ElapsedTime) {
        GInputManager->Update();
        Update(CurrTime, ElapsedTime);
        Render();
    }

    void Application::Render() {

        mTestRenderTarget->Activate(m_pImmediateContext);

        mCube->DrawCube(mCamera.GetProjView());

        PrepareRender();
        mCube2->DrawCube(mCamera.GetProjView());

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

    LRESULT Application::HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

        if (m_pImGui) {
            if (const auto Handled = static_cast<ImGuiImplWin32 *>(m_pImGui.get())->Win32_ProcHandler(hWnd, message,
                                                                                                      wParam,
                                                                                                      lParam))
                return Handled;
        }

        if (message == WM_INPUT) {
            if (GInputManager != nullptr) {
                GInputManager->ParseMessage((void *) lParam);
            }
        }

        return 0l;
    }

    void Application::Update(double CurrTime, double ElapsedTime) {
        mCamera.LookAt(glm::vec3(0.f, 2.0f, -5.0f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.f, 0.f));
        mCube->Update(CurrTime, ElapsedTime);
        mCube2->Update(CurrTime, ElapsedTime);
    }

    void Application::DrawImGui() {
        const auto &SCDesc = m_pSwapChain->GetDesc();
        m_pImGui->NewFrame(SCDesc.PreTransform);

        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        bool showGui = true;
        ImGui::ShowDemoWindow(&showGui);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Test");
        auto Size = ImGui::GetContentRegionAvail();
        mCamera.SetViewPortSize(Size.x, Size.y);
        mTestRenderTarget->SetSize(Size.x, Size.y);
        ImGui::PopStyleVar();

        if (ImGui::IsWindowHovered()) {
            if (ImGui::IsKeyDown('A')) {
                mCube->SetRotation(rand());
            }
        }

        ImGui::Image(gTheApp->mTestRenderTarget->GetTexture(), Size);
        ImGui::End();


        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        bool p_open = true;
        ImGui::Begin("Log", &p_open);
        ImGui::End();

        // Actually call in the regular Log helper (which will Begin() into the same window as we just did)
        m_log->Draw("Log", &p_open);


        m_pImGui->Render(m_pImmediateContext);
    }

    void Application::Present() {
        m_pSwapChain->Present();
    }

    void Application::WindowResize(Uint32 Width, Uint32 Height) {

    }
}

void EditorLog::Append(bt::log::LogLevel level, const string &msg) {
    AddLog("%s\n", msg.c_str());
}
