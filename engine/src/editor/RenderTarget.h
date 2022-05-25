#pragma once

#include "EngineFactory.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "BasicMath.hpp"

namespace bt {

using namespace Diligent;

class RenderTarget {
  public:
    explicit RenderTarget(IRenderDevice* m_pDevice) {
        // Create window-size offscreen render target
        TextureDesc RTColorDesc;
        RTColorDesc.Name      = "Offscreen render target";
        RTColorDesc.Type      = RESOURCE_DIM_TEX_2D;
        RTColorDesc.Width     = 256;
        RTColorDesc.Height    = 256;
        RTColorDesc.MipLevels = 1;
        RTColorDesc.Format    = RenderTargetFormat;
        // The render target can be bound as a shader resource and as a render target
        RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        // Define optimal clear value
        RTColorDesc.ClearValue.Format   = RTColorDesc.Format;
        RTColorDesc.ClearValue.Color[0] = 0.350f;
        RTColorDesc.ClearValue.Color[1] = 1.000f;
        RTColorDesc.ClearValue.Color[2] = 0.350f;
        RTColorDesc.ClearValue.Color[3] = 1.f;
        RefCntAutoPtr<ITexture> pRTColor;
        m_pDevice->CreateTexture(RTColorDesc, nullptr, &pRTColor);
        // Store the render target view
        m_pColorRTV = pRTColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        m_pColorSRV = pRTColor->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);


        // Create window-size depth buffer
        TextureDesc RTDepthDesc = RTColorDesc;
        RTDepthDesc.Name        = "Offscreen depth buffer";
        RTDepthDesc.Format      = DepthBufferFormat;
        RTDepthDesc.BindFlags   = BIND_DEPTH_STENCIL;
        // Define optimal clear value
        RTDepthDesc.ClearValue.Format               = RTDepthDesc.Format;
        RTDepthDesc.ClearValue.DepthStencil.Depth   = 1;
        RTDepthDesc.ClearValue.DepthStencil.Stencil = 0;
        RefCntAutoPtr<ITexture> pRTDepth;
        m_pDevice->CreateTexture(RTDepthDesc, nullptr, &pRTDepth);
        // Store the depth-stencil view
        m_pDepthDSV = pRTDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

        // We need to release and create a new SRB that references new off-screen render target SRV
       // m_pRTSRB.Release();
       // m_pRTPSO->CreateShaderResourceBinding(&m_pRTSRB, true);

        // Set render target color texture SRV in the SRB
       // m_pRTSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRTColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }

    void Activate(IDeviceContext* m_pImmediateContext) {
        // Clear the offscreen render target and depth buffer
        const float ClearColor[] = {0.350f, 1.000f, 0.350f, 1.0f};
        m_pImmediateContext->SetRenderTargets(1, &m_pColorRTV, m_pDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(m_pColorRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_pDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    ITextureView* GetTexture() {
        return m_pColorSRV;
    }

  private:

    RefCntAutoPtr<ITextureView> m_pColorRTV;
    RefCntAutoPtr<ITextureView> m_pColorSRV;
    RefCntAutoPtr<ITextureView> m_pDepthDSV;

    static constexpr TEXTURE_FORMAT RenderTargetFormat = TEX_FORMAT_RGBA8_UNORM;
    static constexpr TEXTURE_FORMAT DepthBufferFormat  = TEX_FORMAT_D32_FLOAT;
};

}
