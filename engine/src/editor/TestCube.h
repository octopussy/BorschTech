#pragma once

#include "Application.h"
#include "RefCntAutoPtr.hpp"

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "glm/detail/type_mat4x4.hpp"

namespace bt {

using namespace Diligent;

class TestCube {
  public:

    TestCube();

    void Update(double CurrTime, double ElapsedTime);
    void DrawCube(const glm::mat4 &ProjView);

    void SetLocation(const glm::vec3& NewLoc) {
        mLocation = NewLoc;
    }

  private:

    void CreateVertexBuffer();

    void CreateIndexBuffer();

  private:

    glm::vec3 mLocation;
    glm::mat4 mCubeModelTransform;

    // Cube
    RefCntAutoPtr<IPipelineState> m_pPSOCube;
    RefCntAutoPtr<IEngineFactory> m_pEngineFactory;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_VSConstants;
};

}

