#pragma once

#include "Application.h"
#include "RefCntAutoPtr.hpp"

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "core/Math.h"

namespace bt {

using namespace Diligent;

class TestCube {
  public:

    TestCube();

    void Update(double CurrTime, double ElapsedTime);
    void DrawCube(const Matrix &ProjView);

    void SetLocation(const Vector& NewLoc) {
        mLocation = NewLoc;
    }

    void SetRotation(double Rot) {
        Rotation = Rot;
    }

  private:

    void CreateVertexBuffer();

    void CreateIndexBuffer();

  private:

    double Rotation = 0.f;

    Vector mLocation;
    Matrix mCubeModelTransform;

    // Cube
    RefCntAutoPtr<IPipelineState> m_pPSOCube;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_VSConstants;
};

}

