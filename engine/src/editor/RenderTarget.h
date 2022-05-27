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
    explicit RenderTarget(IRenderDevice* Device) {
        m_Width = 0;
        m_Height = 0;
        m_pDevice = Device;
        SetSize(256, 256);
    }

    [[nodiscard]] Uint32 GetWidth() const { return m_Width; }
    [[nodiscard]] Uint32 GetHeight() const { return m_Height; }

    void Activate(IDeviceContext* m_pImmediateContext) {
        // Clear the offscreen render target and depth buffer
        m_pImmediateContext->SetRenderTargets(1, &m_pColorRTV, m_pDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(m_pColorRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_pDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void SetSize(Uint32 Width, Uint32 Height) {
        if (m_Width == Width && m_Height == Height) {
            return;
        }

        m_Width = Width;
        m_Height = Height;

// Create window-size offscreen render target
        TextureDesc RTColorDesc;
        RTColorDesc.Name      = "Offscreen render target";
        RTColorDesc.Type      = RESOURCE_DIM_TEX_2D;
        RTColorDesc.Width     = Width;
        RTColorDesc.Height    = Height;
        RTColorDesc.MipLevels = 1;
        RTColorDesc.Format    = RenderTargetFormat;
        // The render target can be bound as a shader resource and as a render target
        RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        // Define optimal clear value
        RTColorDesc.ClearValue.Format   = RTColorDesc.Format;
        RTColorDesc.ClearValue.Color[0] = ClearColor[0];
        RTColorDesc.ClearValue.Color[1] = ClearColor[1];
        RTColorDesc.ClearValue.Color[2] = ClearColor[2];
        RTColorDesc.ClearValue.Color[3] = ClearColor[3];
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

    ITextureView* GetTexture() {
        return m_pColorSRV;
    }

  private:

    Uint32 m_Width;
    Uint32 m_Height;

    RefCntAutoPtr<ITextureView> m_pColorRTV;
    RefCntAutoPtr<ITextureView> m_pColorSRV;
    RefCntAutoPtr<ITextureView> m_pDepthDSV;

    RefCntAutoPtr<IRenderDevice> m_pDevice;

    static constexpr float ClearColor[] = {0.020f, 0.020f, 0.020f, 1.0f};
    static constexpr TEXTURE_FORMAT RenderTargetFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    static constexpr TEXTURE_FORMAT DepthBufferFormat  = TEX_FORMAT_D32_FLOAT;
};

}
