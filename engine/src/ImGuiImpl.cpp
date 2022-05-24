/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <cstddef>
#include <memory>
#include "imgui.h"
#include "ImGuiImpl.hpp"
#include "ImGuiDiligentRenderer.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "BasicMath.hpp"
#include "MapHelper.hpp"


namespace bt
{
    using namespace Diligent;

    ImGuiImpl::ImGuiImpl(
        void* hwnd,
        IRenderDevice* pDevice,
                         TEXTURE_FORMAT BackBufferFmt,
                         TEXTURE_FORMAT DepthBufferFmt,
                         Uint32 InitialVertexBufferSize,
                         Uint32 InitialIndexBufferSize)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        //io.ConfigViewportsNoAutoMerge = true;
        //io.ConfigViewportsNoTaskBarIcon = true;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        m_pRenderer = std::make_unique<ImGuiDiligentRenderer>(hwnd, pDevice, BackBufferFmt, DepthBufferFmt);
    }

    ImGuiImpl::~ImGuiImpl()
    {
        ImGui::DestroyContext();
    }

    void ImGuiImpl::NewFrame(Uint32 RenderSurfaceWidth, Uint32 RenderSurfaceHeight, SURFACE_TRANSFORM SurfacePreTransform)
    {
        m_pRenderer->NewFrame(SurfacePreTransform);
        ImGui::NewFrame();
    }

    void ImGuiImpl::EndFrame()
    {
        ImGui::EndFrame();
    }

    void ImGuiImpl::Render(IDeviceContext* pCtx)
    {
        // No need to call ImGui::EndFrame as ImGui::Render calls it automatically
        ImGui::Render();

        m_pRenderer->Render();
      // Update and Render additional Platform Windows
     // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
      {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
      }
    }

    // Use if you want to reset your rendering device without losing ImGui state.
    void ImGuiImpl::InvalidateDeviceObjects()
    {
        m_pRenderer->InvalidateDeviceObjects();
    }

    void ImGuiImpl::CreateDeviceObjects()
    {
        m_pRenderer->CreateDeviceObjects();
    }

    void ImGuiImpl::UpdateFontsTexture()
    {
        m_pRenderer->CreateFontsTexture();
    }
} // namespace Diligent
