#pragma once

#include "Core/Math.h"
#include "BasicTypes.h"


using namespace glm;
using namespace Diligent;

namespace bt {

    class Camera {
        vec3 mPosition;
        vec3 mDirection;
        vec3 mUp;

        float mFov;
        float mAspectRatio;
        float mZNear;
        float mZFar;
        mat4 mView;
        mat4 mProj;
        mat4 mProjView;
    public:
        Camera() : mPosition(vec3(0.f)),
                   mDirection(vec3(0.f, 0.f, 1.f)),
                   mUp(vec3(0.f, 1.f, 0.f)),
                   mFov(70.f),
                   mAspectRatio(1.f),
                   mView(mat4(1.f)),
                   mProj(mat4(1.f)),
                   mProjView(mat4(1.f)),
                   mZNear(0.1f),
                   mZFar(1000.0f)
                   {}

        void LookAt(const vec3 &Position, const vec3 &Target, const vec3 &Up) {
            mPosition = Position;
            mDirection = Target - Position;
            mUp = Up;
        }

        [[nodiscard]] const mat4& GetProjView() {
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
