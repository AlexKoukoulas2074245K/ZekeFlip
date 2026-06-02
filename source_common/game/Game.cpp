///------------------------------------------------------------------------------------------------
///  Game.cpp                                                                                        
///  ZekeFlipClient
///
///  Created by Alex Koukoulas on 19/09/2023
///------------------------------------------------------------------------------------------------

#include <engine/CoreSystemsEngine.h>
#include <engine/input/IInputStateManager.h>
#include <engine/rendering/AnimationManager.h>
#include <engine/rendering/Camera.h>
#include <engine/rendering/CommonUniforms.h>
#include <engine/rendering/Fonts.h>
#include <engine/rendering/ParticleManager.h>
#include <engine/rendering/RenderingUtils.h>
#include <engine/resloading/ResourceLoadingService.h>
#include <engine/resloading/ImageSurfaceResource.h>
#include <engine/scene/SceneManager.h>
#include <engine/scene/Scene.h>
#include <engine/scene/SceneObject.h>
#include <engine/scene/SceneObjectUtils.h>
#include <engine/sound/SoundManager.h>
#include <engine/utils/BaseDataFileDeserializer.h>
#include <engine/utils/Date.h>
#include <engine/utils/Logging.h>
#include <engine/utils/FileUtils.h>
#include <engine/utils/OSMessageBox.h>
#include <engine/utils/PlatformMacros.h>
#include <fstream>
#include <game/ui/AnimatedButton.h>
#include <game/Game.h>
#include <imgui/imgui.h>
#include <mutex>
#include <SDL.h>

//#define ALLOW_OFFLINE_PLAY
#if defined(MOBILE_FLOW)
#include <platform_specific/IOSUtils.h>
#endif

#if defined(MACOS) || defined(MOBILE_FLOW)
#include <platform_utilities/AppleUtils.h>
#elif defined(WINDOWS)
#include <platform_utilities/WindowsUtils.h>
#endif
#include <iostream>

///------------------------------------------------------------------------------------------------

static const std::string CARD_SO_NAME_PREFIX = "card_";
static const std::string CARD_FLIP_ANIMATION_NAME_PREFIX = "flip_animation_";

///------------------------------------------------------------------------------------------------

Game::Game(const int argc, char** argv)
{
    if (argc > 0)
    {
        logging::Log(logging::LogType::INFO, "Initializing from CWD : %s", argv[0]);
    }
    
#if defined(MACOS) || defined(MOBILE_FLOW)
    apple_utils::SetAssetFolder();
#endif
    
    CoreSystemsEngine::GetInstance().Start([&](){ Init(); }, [&](const float dtMillis){ Update(dtMillis); }, [&](){ ApplicationMovedToBackground(); }, [&](){ WindowResize(); }, [&](){ CreateDebugWidgets(); }, [&](){ OnOneSecondElapsed(); });
}

///------------------------------------------------------------------------------------------------

Game::~Game(){}

///------------------------------------------------------------------------------------------------

void Game::Init()
{
    auto& systemsEngine = CoreSystemsEngine::GetInstance();
    systemsEngine.GetFontRepository().LoadFont(game_constants::DEFAULT_FONT_NAME.GetString(), resources::ResourceReloadMode::DONT_RELOAD);
    systemsEngine.GetSoundManager().SetAudioEnabled(false);
    
    auto scene = systemsEngine.GetSceneManager().CreateScene(game_constants::WORLD_SCENE_NAME);
    

    scene->GetCamera().SetCameraType(rendering::Camera::CameraType::PERSPECTIVE);
    
    scene->SetLoaded(true);
    
    auto& eventSystem = events::EventSystem::GetInstance();
    (void)eventSystem;
//    mMapChangeEventListener = eventSystem.RegisterForEvent<events::MapChangeEvent>([this](const events::MapChangeEvent& event)
//    {
//        const auto& mapResources = mMapResourceController->GetMapResources(event.mNewMapName);
//        mCurrentNavmap = mapResources.mNavmap;
//    });
    
    static const strutils::StringId BOARD_SO_NAME = strutils::StringId("board");
    static const std::string BOARD_MESH = "flip_board.obj";
    static const std::string BOARD_TEXTURE = "game/board_tex.png";
    static const std::string CARD_MESH = "flip_card.obj";
    static const std::string CARD_TEXTURE = "game/flip_card_poop_tex.png";
    
    auto board = scene->CreateSceneObject(BOARD_SO_NAME);
    board->mMeshResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_MESHES_ROOT + BOARD_MESH);
    board->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + BOARD_TEXTURE);
    board->mRotation.x = 0.0f;
    board->mScale = glm::vec3(0.5f);
    
    for (int row = 0; row < 5; ++row)
    {
        for (int col = 0; col < 5; ++col)
        {
            auto card = scene->CreateSceneObject(strutils::StringId(CARD_SO_NAME_PREFIX + std::to_string(row) + "," + std::to_string(col)));
            card->mMeshResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_MESHES_ROOT + CARD_MESH);
            card->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + CARD_TEXTURE);
            card->mShaderFloatUniformValues[CUSTOM_ALPHA_UNIFORM_NAME] = 1.0f;
            card->mPosition.x = -0.185f + col * 0.09f;
            card->mPosition.z = -0.185f + row * 0.09f;
            card->mPosition.y = 0.121f;
            
            card->mScale = glm::vec3(0.03f);
        }
    }
    
    scene = systemsEngine.GetSceneManager().CreateScene(game_constants::GUI_SCENE_NAME);
    scene->GetCamera().SetCameraType(rendering::Camera::CameraType::ORTHO);
    scene->SetLoaded(true);
    
    //mTestButton = std::make_unique<AnimatedButton>(glm::vec3(-0.0f, 0.0f, 1.0f), glm::vec3(0.0005f), game_constants::DEFAULT_FONT_NAME, "Test my limits, left and right :)", strutils::StringId("test_button"), [](){}, scene);
}

///------------------------------------------------------------------------------------------------

static glm::vec3 sNextCameraPosition;
static glm::vec3 sNextCameraFront;

void Game::Update(const float dtMillis)
{
    if (mTestButton)
    {
        mTestButton->Update(dtMillis);
    }
    
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    
    // Desktop only, card hovering animation
#if defined(DESKTOP_FLOW)
    CardHoveringAnimation();
#endif
    
    if (scene)
    {
        auto cardPickResult = PickPointedCard();
        if (cardPickResult.selectedCard && CoreSystemsEngine::GetInstance().GetInputStateManager().VButtonTapped(input::Button::MAIN_BUTTON))
        {
            // Stop existing animation
            auto hoverResetAnimationName = strutils::StringId(CARD_FLIP_ANIMATION_NAME_PREFIX + cardPickResult.selectedCard->mName.GetString());
            auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
            animationManager.StopAnimation(hoverResetAnimationName);
            
            // And flip it
            animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<float>>(cardPickResult.selectedCard->mRotation.z, -math::PI, 0.5f, animation_flags::NONE, 0.0f, math::ElasticFunction, math::TweeningMode::EASE_IN), [](){}, hoverResetAnimationName);
            mFlippedCards.push_back(cardPickResult.selectedCard);
        }
    }
    
    if (scene)
    {
        static  bool firstTime = true;
        if (firstTime)
        {
            firstTime = false;
            
            const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
            const auto& currentAspect = static_cast<float>(windowDimensions.x)/windowDimensions.y;
            
            auto minPosition = glm::vec3(0.03f, 1.140f, 0.026f);
            auto minFront = glm::vec3(0.0f, -30.0f, -1.235f);
            
            auto maxPosition = glm::vec3(0.0f, 0.6f, 0.229f);
            auto maxFront = glm::vec3(0.0f, -2.154f, -1.0f);
            
            auto minAspect = 0.4625f;
            auto maxAspect = 1.0f;
            
            // Normalize aspect to [0, 1]
            float t = (currentAspect - minAspect)/(maxAspect - minAspect);
            t = glm::clamp(t, 0.0f, 1.0f);
            
            sNextCameraPosition = glm::mix(minPosition, maxPosition, t);
            sNextCameraFront = glm::mix(minFront, maxFront, t);
        }
    }
    
    scene->GetCamera().SetPosition(sNextCameraPosition);
    scene->GetCamera().SetFront(sNextCameraFront);
}

///------------------------------------------------------------------------------------------------

void Game::ApplicationMovedToBackground()
{
}

///------------------------------------------------------------------------------------------------

void Game::OnOneSecondElapsed()
{
}

///------------------------------------------------------------------------------------------------

void Game::WindowResize()
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
        const auto& currentAspect = static_cast<float>(windowDimensions.x)/windowDimensions.y;
        
        auto minPosition = glm::vec3(0.03f, 1.140f, 0.026f);
        auto minFront = glm::vec3(0.0f, -30.0f, -1.235f);
        
        auto maxPosition = glm::vec3(0.0f, 0.6f, 0.229f);
        auto maxFront = glm::vec3(0.0f, -2.154f, -1.0f);
        
        auto minAspect = 0.4625f;
        auto maxAspect = 1.0f;
        
        // Normalize aspect to [0, 1]
        float t = (currentAspect - minAspect)/(maxAspect - minAspect);
        t = glm::clamp(t, 0.0f, 1.0f);
        
        auto nextPosition = glm::mix(minPosition, maxPosition, t);
        auto nextFront = glm::mix(minFront, maxFront, t);
        
        static const auto CAM_POS_ANIMATION_NAME = strutils::StringId("camera_position_tween");
        static const auto CAM_FRONT_ANIMATION_NAME = strutils::StringId("camera_front_tween");
        
        auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
        
        animationManager.StopAnimation(CAM_POS_ANIMATION_NAME);
        animationManager.StopAnimation(CAM_FRONT_ANIMATION_NAME);
        
        animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<glm::vec3>>(sNextCameraPosition, nextPosition, 1.0f), [](){}, CAM_POS_ANIMATION_NAME);
        animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<glm::vec3>>(sNextCameraFront, nextFront, 1.0f), [](){}, CAM_FRONT_ANIMATION_NAME);
    }
}

///------------------------------------------------------------------------------------------------

void Game::CardHoveringAnimation()
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (!scene)
    {
        return;
    }
    
    auto cardPickResult = PickPointedCard();
    if (cardPickResult.selectedCard)
    {
        // Hover card rotation
        static const float HOVER_MAG_VALUE = 3.0f;
        cardPickResult.selectedCard->mRotation.z = cardPickResult.distanceFromCardCenter * HOVER_MAG_VALUE;
    }
    
    // Reset rotation on all cards
    auto cards = scene->FindSceneObjectsWhoseNameStartsWith(CARD_SO_NAME_PREFIX);
    for (auto card: cards)
    {
        if (std::find(mFlippedCards.cbegin(), mFlippedCards.cend(), card) != mFlippedCards.cend())
        {
            continue;
        }

        auto hoverResetAnimationName = strutils::StringId(CARD_FLIP_ANIMATION_NAME_PREFIX + card->mName.GetString());
        auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
        
        if (card == cardPickResult.selectedCard)
        {
            animationManager.StopAnimation(hoverResetAnimationName);
        }
        else
        {
            if (!animationManager.IsAnimationPlaying(hoverResetAnimationName))
            {
                animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<float>>(card->mRotation.z, 0.0f, 0.5f, animation_flags::NONE, 0.0f, math::ElasticFunction, math::TweeningMode::EASE_IN), [](){}, hoverResetAnimationName);
            }
        }
    }
}

///------------------------------------------------------------------------------------------------

Game::CardPickingResult Game::PickPointedCard()
{
    CardPickingResult pickingResult = {};
    pickingResult.distanceFromCardCenter = FLT_MAX;

    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (!scene)
    {
        return pickingResult;
    }

    auto cards = scene->FindSceneObjectsWhoseNameStartsWith(CARD_SO_NAME_PREFIX);
    const auto& camera = scene->GetCamera();
    const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
    
    auto rayOrigin = camera.GetPosition();
    auto rayDirection = math::ComputePointingRayDirection(
        CoreSystemsEngine::GetInstance().GetInputStateManager().VGetPointingPos(),
        camera.GetViewMatrix(),
        camera.GetProjMatrix(),
        static_cast<float>(windowDimensions.x),
        static_cast<float>(windowDimensions.y));
    
    for (auto card: cards)
    {
        // Ignore flipped ones
        if (std::find(mFlippedCards.cbegin(), mFlippedCards.cend(), card) != mFlippedCards.cend())
        {
            continue;
        }
        
        // Intersection test
        float t;
        auto boundingRect = scene_object_utils::GetSceneObjectBoundingRect(*card);
        float sphereRadius = math::Max(math::Abs(boundingRect.bottomLeft.x - boundingRect.topRight.x), math::Abs(boundingRect.bottomLeft.y - boundingRect.topRight.y)) * 1.5f;

        if (math::RayToSphereIntersection(rayOrigin, rayDirection, card->mPosition, sphereRadius, t))
        {
            if (t < pickingResult.distanceFromCardCenter)
            {
                pickingResult.selectedCard = card;
                pickingResult.distanceFromCardCenter = t;
            }
        }
    }
    
    // Calculate distance from card plane center
    if (pickingResult.selectedCard)
    {
        glm::vec3 planeIntersectionPoint;
        math::RayToPlaneIntersection(rayOrigin, rayDirection, pickingResult.selectedCard->mPosition, glm::vec3(0.0f, 1.0f, 0.0f), planeIntersectionPoint);
        pickingResult.distanceFromCardCenter = (pickingResult.selectedCard->mPosition.x - planeIntersectionPoint.x);
    }
    
    return pickingResult;
}

///------------------------------------------------------------------------------------------------

#if defined(USE_IMGUI)
void Game::CreateDebugWidgets()
{
    ImGui::Begin("Game Debug", nullptr, GLOBAL_IMGUI_WINDOW_FLAGS);
    
    {
        static const strutils::StringId DEBUG_SPHERE_COLLIDER_NAME = strutils::StringId("debug_sphere_collider");
        static bool sShowColliders = false;
        if (ImGui::Checkbox("Show Colliders", &sShowColliders))
        {
            auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
            if (scene)
            {
                scene->RemoveAllSceneObjectsWithName(DEBUG_SPHERE_COLLIDER_NAME);
                
                if (sShowColliders)
                {
                    auto cardSceneObjects = scene->FindSceneObjectsWhoseNameStartsWith(CARD_SO_NAME_PREFIX);
                    for (auto cardSceneObject: cardSceneObjects)
                    {
                        static const std::string DEBUG_SPHERE_MESH = "sphere.obj";
                        
                        auto boundingRect = scene_object_utils::GetSceneObjectBoundingRect(*cardSceneObject);
                        auto debugCollider = scene->CreateSceneObject(DEBUG_SPHERE_COLLIDER_NAME);
                        debugCollider->mPosition = cardSceneObject->mPosition;
                        debugCollider->mMeshResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_MESHES_ROOT + DEBUG_SPHERE_MESH);
                        debugCollider->mScale = glm::vec3(math::Max(math::Abs(boundingRect.bottomLeft.x - boundingRect.topRight.x), math::Abs(boundingRect.bottomLeft.y - boundingRect.topRight.y))) * 1.5f;
                        debugCollider->mShaderFloatUniformValues[CUSTOM_ALPHA_UNIFORM_NAME] = 0.5f;
                    }
                }
            }
        }
    }
    
    ImGui::End();
}
#else
void Game::CreateDebugWidgets()
{
}
#endif

///------------------------------------------------------------------------------------------------
