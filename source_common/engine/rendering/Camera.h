///------------------------------------------------------------------------------------------------
///  Camera.h                                                                                          
///  ZekeFlipClient
///                                                                                                
///  Created by Alex Koukoulas on 20/09/2023
///------------------------------------------------------------------------------------------------

#ifndef Camera_h
#define Camera_h

///------------------------------------------------------------------------------------------------

#include <engine/utils/MathUtils.h>
#include <functional>

///------------------------------------------------------------------------------------------------

namespace rendering
{

///------------------------------------------------------------------------------------------------

class Camera final
{
public:
    static const float DEFAULT_SHAKE_STRENGTH_RADIUS;
    
    enum class CameraType
    {
        ORTHO, PERSPECTIVE
    };

public:
    Camera();
    
    void RecalculateMatrices();
    
    const float& GetFOV() const;
    const glm::vec3& GetPosition() const;
    const glm::vec3& GetFront() const;
    const glm::mat4& GetViewMatrix() const;
    const glm::mat4& GetProjMatrix() const;
    math::Frustum CalculateFrustum() const;
    
    ///-----------------------------------------------------------------------------------------------
    /// Performs a camera shake.
    /// @param[in] targetDurationSecs the target (to be exceeded if shakeInterTremmorDelaySecs is set to a value > 0) seconds the shake will run for
    /// @param[in] shakeStrengthRadius (optional) sets the starting radius offset for the camera shake. defaults to 0.05f
    /// @param[in] onCameraShakeEndCallback (optional) callback that will be invoked when the camera shake ends
    /// @param[in] shakeInterTremmorDelaySecs (optional) specifies the delay in between shake tremmors. Will also naturally lengthen the shake duration
    /// specified by durationSecs. Sane values are around 0.01f - 0.1f
    void Shake(const float targetDurationSecs, const float shakeStrengthRadius = DEFAULT_SHAKE_STRENGTH_RADIUS, std::function<void()> onCameraShakeEndCallback = nullptr, const float shakeInterTremmorDelaySecs = 0.0f);
    bool IsShaking() const;
    void StopShake();
    
    void Update(const float dtMillis);
    void SetFOV(const float fovDegrees);
    void SetPosition(const glm::vec3& position);
    void SetFront(const glm::vec3& front);
    void SetCameraType(const CameraType cameraType);

private:
    struct ShakeData
    {
        glm::vec3 mPreShakePosition;
        float mShakeCurrentRadius = 0.0f;
        float mShakeStrengthRadius = 0.0f;
        float mShakeRandomAngle = 0.0f;
        float mShakeTargetDurationMillis = 0.0f;
        float mShakeTimeAccumulatorMillis = 0.0f;
        float mShakeInterTremmorDelayMillis = 0.0f;
        float mShakeInterTremmorAccumMillis = 0.0f;
    };
    
    CameraType mCameraType;
    ShakeData mShakeData;
    glm::vec3 mPosition;
    glm::vec3 mFront;
    glm::mat4 mView;
    glm::mat4 mProj;
    float mFieldOfViewDegrees;
    std::function<void()> mCameraShakeEndCallback;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* Camera_h */
