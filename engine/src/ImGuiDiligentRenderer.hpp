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

#pragma once

#include <memory>
#include "../../../DiligentCore/Primitives/interface/BasicTypes.h"
#include "../../../DiligentCore/Common/interface/BasicMath.hpp"
#include "../../../DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "imgui.h"

#include "SwapChain.h"

struct ImDrawData;


namespace Diligent {

struct IRenderDevice;
struct IDeviceContext;
struct IBuffer;
struct IPipelineState;
struct ITextureView;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
enum TEXTURE_FORMAT : Uint16;
enum SURFACE_TRANSFORM : Uint32;

}

namespace bt {

using namespace Diligent;

struct BorschDiligentRenderData {
  bt::ImGuiDiligentRenderer* Renderer;
};

struct BorschDiligentViewportData {
  RefCntAutoPtr<ISwapChain> pSwapChain;
  RefCntAutoPtr<IBuffer> m_pVB;
  RefCntAutoPtr<IBuffer> m_pIB;
  Uint32 m_VertexBufferSize = 0;
  Uint32 m_IndexBufferSize = 0;

  BorschDiligentViewportData() :
      m_VertexBufferSize{1024},
      m_IndexBufferSize{1024} {}

};

class ImGuiDiligentRenderer {
  public:
    ImGuiDiligentRenderer(
        void *hwnd,
        IRenderDevice *pDevice,
        TEXTURE_FORMAT BackBufferFmt,
        TEXTURE_FORMAT DepthBufferFmt);

    ~ImGuiDiligentRenderer();

    void NewFrame(SURFACE_TRANSFORM SurfacePreTransform);

    void Render();

    void EndFrame();

    void RenderDrawData(BorschDiligentViewportData *viewportData,
                        ImDrawData *pDrawData);

    void InvalidateDeviceObjects();

    void CreateDeviceObjects();

    void CreateFontsTexture();

    inline float4 TransformClipRect(const ImVec2 &DisplaySize, const float4 &rect) const;

  private:

    void CreateMainViewport();

  private:

    RefCntAutoPtr<IRenderDevice> m_pDevice;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    IShaderResourceVariable *m_pTextureVar = nullptr;
    bool m_BaseVertexSupported = false;
    RefCntAutoPtr<IBuffer> m_pVertexConstantBuffer;

    RefCntAutoPtr<IPipelineState> m_pPSO;

    BorschDiligentViewportData pMainViewportData;

    RefCntAutoPtr<ITextureView> m_pFontSRV;

    const TEXTURE_FORMAT m_BackBufferFmt;
    const TEXTURE_FORMAT m_DepthBufferFmt;
    SURFACE_TRANSFORM m_SurfacePreTransform = SURFACE_TRANSFORM_IDENTITY;
};
} // namespace Diligent
