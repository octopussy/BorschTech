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
#include "ImGuiImpl.hpp"
#include "Core/Math.h"

using namespace Diligent;

namespace bt
{
    class Application
    {
        RefCntAutoPtr<IRenderDevice>            m_pDevice;
        RefCntAutoPtr<IDeviceContext>           m_pImmediateContext;
        //std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeviceContexts;
        RefCntAutoPtr<ISwapChain>               m_pSwapChain;
        RENDER_DEVICE_TYPE                      m_DeviceType = RENDER_DEVICE_TYPE_VULKAN;

        // Triangle
        RefCntAutoPtr<IPipelineState>           m_pPSOTriangle;

        // Cube
        RefCntAutoPtr<IPipelineState>           m_pPSOCube;
        RefCntAutoPtr<IEngineFactory>           m_pEngineFactory;
        RefCntAutoPtr<IShaderResourceBinding>   m_pSRB;
        RefCntAutoPtr<IBuffer>                  m_CubeVertexBuffer;
        RefCntAutoPtr<IBuffer>                  m_CubeIndexBuffer;
        RefCntAutoPtr<IBuffer>                  m_VSConstants;
        glm::mat4                               m_WorldViewProjMatrix;

        std::unique_ptr<ImGuiImpl>              m_pImGui;

    public:
        Application()
        {
        }

        virtual ~Application();

        bool Init(HWND hWnd);
        void Tick(double CurrTime, double ElapsedTime);

        void WindowResize(Uint32 Width, Uint32 Height);

        virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        void Update(double CurrTime, double ElapsedTime);

        void PrepareRender();
        void Present();
        void Render();

        void DrawImGui();
        void DrawTriangle();
        void DrawCube();

        void CreateResources_Triangle();
        void CreateResources_Cube();

        // Cube
        void CreateVertexBuffer();
        void CreateIndexBuffer();

        [[nodiscard]] glm::mat4 GetAdjustedProjectionMatrix(float FOV, float NearPlane, float FarPlane) const;
        [[nodiscard]] glm::mat4 GetSurfacePretransformMatrix(const glm::vec3& f3CameraViewAxis) const;
    };
}
