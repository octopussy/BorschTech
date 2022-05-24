#pragma once

#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include <Windows.h>

#ifndef PLATFORM_WIN32
#    define PLATFORM_WIN32 1
#endif

#ifndef ENGINE_DLL
#    define ENGINE_DLL 1
#endif

#ifndef D3D11_SUPPORTED
#    define D3D11_SUPPORTED 1
#endif

#ifndef D3D12_SUPPORTED
#    define D3D12_SUPPORTED 1
#endif

#ifndef GL_SUPPORTED
#    define GL_SUPPORTED 1
#endif

#ifndef VULKAN_SUPPORTED
#    define VULKAN_SUPPORTED 1
#endif

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "ImGuiImpl.hpp"

#include "Engine.h"
#include "core/Math.h"
#include "Camera.h"
#include "input/InputManager.h"

using namespace Diligent;

namespace bt {

extern std::unique_ptr<Engine> GEngine;
extern std::unique_ptr<class Application> gTheApp;
extern std::unique_ptr<bt::input::InputManager> gInputManager;


class TestCube;

class Application {
  public:
    IEngineFactory* GetEngineFactory() { return m_pEngineFactory.RawPtr(); }
    IRenderDevice* GetRenderDevice() { return m_pDevice.RawPtr(); }
    ISwapChain* GetSwapChain() { return m_pSwapChain.RawPtr(); }
    IDeviceContext* GetImmediateContext() { return m_pImmediateContext.RawPtr(); }

    bool CreateSwapChain(RefCntAutoPtr<ISwapChain>& result, HWND hWnd, bool isAdditional);

    Application();

    virtual ~Application();

    bool Init(HWND hWnd);

    void Shutdown();

    void Tick(double CurrTime, double ElapsedTime);

    void WindowResize(Uint32 Width, Uint32 Height);

    virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  private:
    void Update(double CurrTime, double ElapsedTime);

    void PrepareRender();

    void Present();

    void Render();

    void DrawImGui();

  private:

    RefCntAutoPtr<IEngineFactory>   m_pEngineFactory;
    RefCntAutoPtr<IRenderDevice>    m_pDevice;
    RefCntAutoPtr<IDeviceContext>   m_pImmediateContext;
    //std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeviceContexts;
    RefCntAutoPtr<ISwapChain>       m_pSwapChain;
    RENDER_DEVICE_TYPE m_DeviceType = Diligent::RENDER_DEVICE_TYPE_D3D11;

    std::unique_ptr<ImGuiImpl> m_pImGui;

    Camera mCamera;

    std::unique_ptr<TestCube> mCube;
    std::unique_ptr<TestCube> mCube2;
};

}
