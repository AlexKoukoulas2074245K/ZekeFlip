///------------------------------------------------------------------------------------------------
///  Camera.cpp                                                                                        
///  ZekeFlipClient
///                                                                                                
///  Created by Alex Koukoulas on 20/09/2023
///------------------------------------------------------------------------------------------------

#include <engine/CoreSystemsEngine.h>
#include <engine/rendering/Camera.h>
#include <engine/utils/Logging.h>
#include <engine/utils/PlatformMacros.h>

#if defined(MOBILE_FLOW)
#include <platform_specific/IOSUtils.h>
#endif

///------------------------------------------------------------------------------------------------

namespace rendering
{

///------------------------------------------------------------------------------------------------

static const glm::vec3 DEFAULT_PERSP_CAMERA_POSITION = {0.0f, 0.339f, 1.129f};
static const glm::vec3 DEFAULT_ORTHO_CAMERA_POSITION = {0.0f, 0.0f, 50.0f};
static const glm::vec3 DEFAULT_CAMERA_FRONT_VECTOR = {0.0f, 0.0f, -1.0f};
static const glm::vec3 DEFAULT_CAMERA_UP_VECTOR    = {0.0f, 1.0f, 0.0f};

static const float DEFAULT_CAMERA_ZNEAR = 0.1f;
static const float DEFAULT_CAMERA_ZFAR  = 100.f;
static const float SHAKE_MIN_RADIUS     = 0.00001f;
static const float TARGET_FOV           = 60.0f;

const float Camera::DEFAULT_SHAKE_STRENGTH_RADIUS = 0.05f;

///------------------------------------------------------------------------------------------------

Camera::Camera()
    : mPosition(DEFAULT_PERSP_CAMERA_POSITION)
    , mFront(DEFAULT_CAMERA_FRONT_VECTOR)
    , mFieldOfViewDegrees(TARGET_FOV)
{
    mCameraShakeEndCallback = nullptr;
    RecalculateMatrices();
}

///------------------------------------------------------------------------------------------------

void Camera::RecalculateMatrices()
{
    const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
    const auto& currentAspect = static_cast<float>(windowDimensions.x)/windowDimensions.y;

    //logging::LogInfo("Camera::RecalculateMatrices with fov=%.6f, aspect=%.6f (w=%.6f,h=%.6f)", mFieldOfViewDegrees, currentAspect, windowDimensions.x, windowDimensions.y);

    mView = glm::lookAt(mPosition, mPosition + mFront, DEFAULT_CAMERA_UP_VECTOR);
    switch (mCameraType)
    {
        case CameraType::PERSPECTIVE:
        {
            // Calculate vertical FOV needed to achieve the target hor FOV
            mProj = glm::perspective(glm::radians(mFieldOfViewDegrees), currentAspect, DEFAULT_CAMERA_ZNEAR, DEFAULT_CAMERA_ZFAR);
        } break;

        case CameraType::ORTHO:
        {
            float targetWidth = 1.0f;
            float height = targetWidth / currentAspect;
            mProj = glm::ortho(-targetWidth/2.0f, targetWidth/2.0f, -height/2.0f, height/2.0f, DEFAULT_CAMERA_ZNEAR, DEFAULT_CAMERA_ZFAR);
        } break;
    }
}

///------------------------------------------------------------------------------------------------

const float& Camera::GetFOV() const
{
    return mFieldOfViewDegrees;
}

///------------------------------------------------------------------------------------------------

const glm::vec3& Camera::GetPosition() const
{
    return mPosition;
}

///------------------------------------------------------------------------------------------------

const glm::vec3& Camera::GetFront() const
{
    return mFront;
}

///------------------------------------------------------------------------------------------------

const glm::mat4& Camera::GetViewMatrix() const
{
    return mView;
}

///------------------------------------------------------------------------------------------------

const glm::mat4& Camera::GetProjMatrix() const
{
    return mProj;
}

///------------------------------------------------------------------------------------------------

math::Frustum Camera::CalculateFrustum() const
{
    math::Frustum cameraFrustum;
    auto viewProjectionMatrix = mProj * mView;

    // Extract rows from combined view projection matrix
    const auto rowX = glm::row(viewProjectionMatrix, 0);
    const auto rowY = glm::row(viewProjectionMatrix, 1);
    const auto rowZ = glm::row(viewProjectionMatrix, 2);
    const auto rowW = glm::row(viewProjectionMatrix, 3);

    // Calculate planes
    cameraFrustum[0] = glm::normalize(rowW + rowX);
    cameraFrustum[1] = glm::normalize(rowW - rowX);
    cameraFrustum[2] = glm::normalize(rowW + rowY);
    cameraFrustum[3] = glm::normalize(rowW - rowY);
    cameraFrustum[4] = glm::normalize(rowW + rowZ);
    cameraFrustum[5] = glm::normalize(rowW - rowZ);

    // Normalize planes
    for (auto i = 0U; i < math::FRUSTUM_SIDES; ++i)
    {
        glm::vec3 planeNormal(cameraFrustum[i].x, cameraFrustum[i].y, cameraFrustum[i].z);
        const auto length = glm::length(planeNormal);
        cameraFrustum[i] = -cameraFrustum[i] / length;
    }

    return cameraFrustum;
}

///------------------------------------------------------------------------------------------------

void Camera::Shake(const float targetDurationSecs, const float shakeStrengthRadius /* = DEFAULT_SHAKE_STRENGTH_RADIUS */, std::function<void()> onCameraShakeEndCallback /* = nullptr */, const float shakeInterTremmorDelaySecs /* = 0.0f */)
{
    mCameraShakeEndCallback = onCameraShakeEndCallback;
    
    if (mShakeData.mShakeCurrentRadius <= SHAKE_MIN_RADIUS)
    {
        mShakeData.mPreShakePosition = mPosition;
        mShakeData.mShakeTimeAccumulatorMillis = 0.0f;
        mShakeData.mShakeTargetDurationMillis = targetDurationSecs * 1000.0f;
        mShakeData.mShakeStrengthRadius = shakeStrengthRadius;
        mShakeData.mShakeCurrentRadius = shakeStrengthRadius;
        mShakeData.mShakeInterTremmorAccumMillis = 0.0;
        mShakeData.mShakeInterTremmorDelayMillis = shakeInterTremmorDelaySecs * 1000.0f;
        
        mShakeData.mShakeRandomAngle = math::RandomFloat(0.0f, 2.0f * math::PI);
        auto offset = glm::vec2(math::Sinf(mShakeData.mShakeRandomAngle) * mShakeData.mShakeCurrentRadius, math::Cosf(mShakeData.mShakeRandomAngle) * mShakeData.mShakeCurrentRadius);
        
        SetPosition(glm::vec3(mShakeData.mPreShakePosition.x + offset.x, mShakeData.mPreShakePosition.y + offset.y, GetPosition().z));
    }
    else
    {
        if (mCameraShakeEndCallback)
        {
            mCameraShakeEndCallback();
        }
    }
}

///------------------------------------------------------------------------------------------------

void Camera::Update(const float dtMillis)
{
    if (mShakeData.mShakeCurrentRadius > SHAKE_MIN_RADIUS)
    {
        if (mShakeData.mShakeInterTremmorDelayMillis > 0.0f)
        {
            mShakeData.mShakeInterTremmorAccumMillis += dtMillis;
            if (mShakeData.mShakeInterTremmorAccumMillis < mShakeData.mShakeInterTremmorDelayMillis)
            {
                return;
            }
            else
            {
                mShakeData.mShakeInterTremmorAccumMillis -= mShakeData.mShakeInterTremmorDelayMillis;
            }
        }
        
        mShakeData.mShakeCurrentRadius = mShakeData.mShakeStrengthRadius * (1.0f - (mShakeData.mShakeTimeAccumulatorMillis/mShakeData.mShakeTargetDurationMillis));
        mShakeData.mShakeTimeAccumulatorMillis += dtMillis;
        
        if (mShakeData.mShakeCurrentRadius <= SHAKE_MIN_RADIUS)
        {
            mShakeData.mShakeCurrentRadius = SHAKE_MIN_RADIUS;
            SetPosition(mShakeData.mPreShakePosition);
            
            if (mCameraShakeEndCallback)
            {
                mCameraShakeEndCallback();
            }
        }
        else
        {
            mShakeData.mShakeRandomAngle = math::RandomFloat(0.0f, 2.0f * math::PI);
            auto offset = glm::vec2(math::Sinf(mShakeData.mShakeRandomAngle) * mShakeData.mShakeCurrentRadius, math::Cosf(mShakeData.mShakeRandomAngle) * mShakeData.mShakeCurrentRadius);
            
            SetPosition(glm::vec3(mShakeData.mPreShakePosition.x + offset.x, mShakeData.mPreShakePosition.y + offset.y, GetPosition().z));
        }
    }
}

///------------------------------------------------------------------------------------------------

bool Camera::IsShaking() const
{
    return mShakeData.mShakeCurrentRadius > SHAKE_MIN_RADIUS;
}

///------------------------------------------------------------------------------------------------

void Camera::StopShake()
{
    if (mShakeData.mShakeCurrentRadius > SHAKE_MIN_RADIUS)
    {
        SetPosition(mShakeData.mPreShakePosition);
    }
    mShakeData.mShakeCurrentRadius = 0.0f;
}

///------------------------------------------------------------------------------------------------

void Camera::SetFOV(const float fovDegrees)
{
    mFieldOfViewDegrees = fovDegrees;
    RecalculateMatrices();
}

///------------------------------------------------------------------------------------------------

void Camera::SetPosition(const glm::vec3& position)
{
    mPosition = position;
    RecalculateMatrices();
}

///------------------------------------------------------------------------------------------------

void Camera::SetFront(const glm::vec3& front)
{
    mFront = front;
    RecalculateMatrices();
}

///------------------------------------------------------------------------------------------------

void Camera::SetCameraType(const CameraType cameraType)
{
    mCameraType = cameraType;
    switch (mCameraType)
    {
        case CameraType::PERSPECTIVE:
        {
            SetPosition(DEFAULT_PERSP_CAMERA_POSITION);
        } break;
        
        case CameraType::ORTHO:
        {
            SetPosition(DEFAULT_ORTHO_CAMERA_POSITION);
        } break;
    }
}

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------
