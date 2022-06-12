#pragma once

#include "core/Math.h"
#include "BasicTypes.h"

using namespace Diligent;

namespace bt {

    class Camera {
        Vector mPosition;
        Vector mDirection;
        Vector mUp;

        float mFov;
        float mAspectRatio;
        float mZNear;
        float mZFar;
        Matrix mView;
        Matrix mProj;
        Matrix mProjView;
    public:
        Camera() : mPosition(Vector(0.f)),
                   mDirection(Vector(0.f, 0.f, 1.f)),
                   mUp(Vector(0.f, 1.f, 0.f)),
                   mFov(70.f),
                   mAspectRatio(1.f),
                   mZNear(0.1f),
                   mZFar(1000.0f),
                   mView(Matrix(1.f)),
                   mProj(Matrix(1.f)),
                   mProjView(Matrix(1.f))
                   {}

        void LookAt(const Vector &Position, const Vector &Target, const Vector &Up) {
            mPosition = Position;
            mDirection = Target - Position;
            mUp = Up;
        }

        [[nodiscard]] const Matrix & GetProjView() {
            Update();
            return mProjView;
        }

        void SetViewPortSize(Uint32 Width, Uint32 Height) {
            mAspectRatio = static_cast<Float32>(Width) / static_cast<Float32>(Height);
        }

    private:
        void Update() {
            mView = glm::lookAt(mPosition, mPosition + mDirection, mUp);
            mProj = glm::perspective(glm::radians(mFov), mAspectRatio, mZNear, mZFar);
            mProjView = mProj * mView;
        }
    };

}
