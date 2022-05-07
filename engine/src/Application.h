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

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Imgui/interface/ImGuiImplDiligent.hpp"

using namespace Diligent;

namespace bt
{
    class Application
    {
        RefCntAutoPtr<IRenderDevice> m_pDevice;
        RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
        RefCntAutoPtr<ISwapChain> m_pSwapChain;
        RefCntAutoPtr<IPipelineState> m_pPSO;
        RENDER_DEVICE_TYPE m_DeviceType = RENDER_DEVICE_TYPE_D3D11;

        std::unique_ptr<ImGuiImplDiligent> m_pImGui;

    public:
        Application()
        {
        }

        virtual ~Application();

        bool Init(HWND hWnd);
        void CreateResources();
        void Render();
        void Present();
        void WindowResize(Uint32 Width, Uint32 Height);

        virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:

        void DrawGui();
    };
}
