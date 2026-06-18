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
#include <engine/resloading/ResourceLoadingService.h>
#include <engine/resloading/ImageSurfaceResource.h>
#include <engine/scene/SceneManager.h>
#include <engine/scene/Scene.h>
#include <engine/scene/SceneObject.h>
#include <engine/scene/SceneObjectUtils.h>
#include <engine/sound/SoundManager.h>
#include <engine/utils/Logging.h>
#include <engine/utils/PlatformMacros.h>
#include <game/ui/AnimatedButton.h>
#include <game/Game.h>
#include <float.h>

#if defined(USE_IMGUI)
#include <imgui/imgui.h>
#endif

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

static const strutils::StringId MARK_BUTTON_SO_NAME = strutils::StringId("mark_button");
static const strutils::StringId CARD_MARKED_UNIFORM_NAME = strutils::StringId("marked");

static const std::string CARD_SO_NAME_PREFIX = "card_";
static const std::string CARD_FLIP_ANIMATION_NAME_PREFIX = "flip_animation_";
static const std::string ROW_CLUE_NAME_PREFIX = "row_clue_";
static const std::string COL_CLUE_NAME_PREFIX = "col_clue_";
static const glm::vec3 MARK_BUTTON_INIT_SCALE = glm::vec3(0.05f);

static std::unordered_map<CardType, std::string> CARD_TEXTURES =
{
    { CardType::ONE,   "game/flip_card_one_tex.png" },
    { CardType::TWO,   "game/flip_card_two_tex.png" },
    { CardType::THREE, "game/flip_card_three_tex.png" },
    { CardType::BOMB,  "game/flip_card_bomb_tex.png" },
};

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
    
    CoreSystemsEngine::GetInstance().Start(
        [&](){ Init(); },
        [&](const float dtMillis){ Update(dtMillis); },
        [&](){ ApplicationMovedToBackground(); },
        [&](){ WindowResize(); },
        [&](){ CreateDebugWidgets(); },
        [&](){ OnOneSecondElapsed(); }
    );
}

///------------------------------------------------------------------------------------------------

Game::~Game(){}

///------------------------------------------------------------------------------------------------

void Game::Init()
{
    // Systems Init
    auto& systemsEngine = CoreSystemsEngine::GetInstance();
    systemsEngine.GetFontRepository().LoadFont(game_constants::DEFAULT_FONT_NAME.GetString(), resources::ResourceReloadMode::DONT_RELOAD);
    systemsEngine.GetSoundManager().SetAudioEnabled(true);
    
    // Events
    auto& eventSystem = events::EventSystem::GetInstance();
    mCardStateChangeListener = eventSystem.RegisterForEvent<events::CardStateChangeEvent>([this](const events::CardStateChangeEvent& event)
    {
        OnCardStateChangeEvent(event);
    });
    
    mCardTypeChangeListener = eventSystem.RegisterForEvent<events::CardTypeChangeEvent>([this](const events::CardTypeChangeEvent& event)
    {
        OnCardTypeChangeEvent(event);
    });
    
    mGameEndedListener = eventSystem.RegisterForEvent<events::GameEndedEvent>([this](const events::GameEndedEvent& event)
    {
        auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
        if (event.mReason == GameEndReason::LOSS)
        {
            CoreSystemsEngine::GetInstance().GetSoundManager().Vibrate();
            scene->GetCamera().Shake(0.5f, 0.005f);
        }
        
        auto cards = scene->FindSceneObjectsWhoseNameStartsWith(CARD_SO_NAME_PREFIX);
        
        for (auto cardSceneObject: cards)
        {
            cardSceneObject->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME] = false;

            // Stop existing animation
            auto hoverResetAnimationName = strutils::StringId(CARD_FLIP_ANIMATION_NAME_PREFIX +cardSceneObject->mName.GetString());
            auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
            animationManager.StopAnimation(hoverResetAnimationName);
            
            // And flip it
            animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<float>>(cardSceneObject->mRotation.z, -math::PI, 0.5f, animation_flags::NONE, 0.0f, math::ElasticFunction, math::TweeningMode::EASE_IN), [](){}, hoverResetAnimationName);
        }
        
        mBlockGameInput = true;
        mGameEndReason = event.mReason;
    });
    
    // World Scene Eleements
    auto scene = systemsEngine.GetSceneManager().CreateScene(game_constants::WORLD_SCENE_NAME);
    scene->GetCamera().SetCameraType(rendering::Camera::CameraType::PERSPECTIVE);
    scene->SetLoaded(true);
    
    static const strutils::StringId BOARD_SO_NAME = strutils::StringId("board");
    static const std::string BOARD_MESH = "flip_board.obj";
    static const std::string BOARD_TEXTURE = "game/board_tex.png";
    static const std::string CARD_MESH = "flip_card.obj";
    static const std::string CARD_SHADER = "card.vs";
    static const std::string MARK_BUTTON_TEXTURE = "game/paw_icon.png";
    static const std::string MARKED_CARD_TEXTURE = "game/marked_card.png";
    static const std::string MARK_BUTTON_SHADER = "mark_button.vs";
    
    mMarkingMode = false;
    mBlockGameInput = false;
    mGameEndDelayTimerSecs = 3.0f;
    mDifficulty = 1;
    mBoardState = std::make_unique<BoardState>(5);
    mBoardState->GenerateBoardBasedOnDifficulty(mDifficulty);

    auto board = scene->CreateSceneObject(BOARD_SO_NAME);
    board->mMeshResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_MESHES_ROOT + BOARD_MESH);
    board->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + BOARD_TEXTURE);
    board->mRotation.x = 0.0f;
    board->mScale = glm::vec3(0.5f);
    
    CreateClueSceneObjects(mBoardState->GetRowClues(), true);
    CreateClueSceneObjects(mBoardState->GetColClues(), false);
    
    for (int row = 0; row < mBoardState->GetBoardSize(); ++row)
    {
        for (int col = 0; col < mBoardState->GetBoardSize(); ++col)
        {
            auto card = scene->CreateSceneObject(strutils::StringId(CARD_SO_NAME_PREFIX + std::to_string(row) + "," + std::to_string(col)));
            card->mMeshResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_MESHES_ROOT + CARD_MESH);
            card->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + CARD_TEXTURES.at(mBoardState->GetCardTypeAt(row, col)));
            card->mEffectTextureResourceIds[0] = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + MARKED_CARD_TEXTURE);
            card->mShaderResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_SHADERS_ROOT + CARD_SHADER);
            card->mShaderFloatUniformValues[CUSTOM_ALPHA_UNIFORM_NAME] = 1.0f;
            card->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME] = false;
            card->mPosition.x = -0.185f + col * 0.09f;
            card->mPosition.z = -0.185f + row * 0.09f;
            card->mPosition.y = 0.121f;
            
            card->mScale = glm::vec3(0.03f);
        }
    }
    
    {
        scene::TextSceneObjectData textData;
        textData.mFontName = game_constants::DEFAULT_FONT_NAME;
        textData.mText = "Level: 1";
        
        auto difficultySceneObject = scene->CreateSceneObject(strutils::StringId("level_text"));
        difficultySceneObject->mSceneObjectTypeData = std::move(textData);
        difficultySceneObject->mScale = glm::vec3(0.0004f);
        difficultySceneObject->mPosition = glm::vec3(-0.129f, 0.238f, -0.243f);
        difficultySceneObject->mRotation.x = -math::PI/2.0f;
        difficultySceneObject->mShaderResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_SHADERS_ROOT + game_constants::DEFAULT_FONT_SHADER_NAME);
    }
    
    auto markSceneObject = scene->CreateSceneObject(MARK_BUTTON_SO_NAME);
    
    markSceneObject->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + MARK_BUTTON_TEXTURE);
    markSceneObject->mShaderResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_SHADERS_ROOT + MARK_BUTTON_SHADER);
    markSceneObject->mRotation.x = -math::PI/2.0f;
    markSceneObject->mPosition = glm::vec3(0.275f, 0.139f, 0.258f);
    markSceneObject->mScale = MARK_BUTTON_INIT_SCALE;
    
    // GUI elements
    scene = systemsEngine.GetSceneManager().CreateScene(game_constants::GUI_SCENE_NAME);
    scene->GetCamera().SetCameraType(rendering::Camera::CameraType::ORTHO);
    scene->SetLoaded(true);
    
    // Camera Reposition
    ResetCameraPosition();
}

///------------------------------------------------------------------------------------------------

void Game::Update(const float dtMillis)
{
    if (mTestButton)
    {
        mTestButton->Update(dtMillis);
    }
    
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    
    if (mGameEndReason.has_value())
    {
        mGameEndDelayTimerSecs -= dtMillis/1000.0f;
        if (mGameEndDelayTimerSecs < 0.0f)
        {
            OnGameEnded();
            
            mGameEndDelayTimerSecs = 1.0f;
            mGameEndReason.reset();
        }
    }
    
    if (mBlockGameInput)
    {
        return;
    }

    // Desktop only, card hovering animation
#if defined(DESKTOP_FLOW)
    if (!mMarkingMode)
    {
        CardHoveringAnimation();
    }
#endif
    
    if (scene)
    {
        auto cardPickResult = PickPointedCard();
        if (cardPickResult.selectedCard)
        {
            // Left-Click/Touch
            if (CoreSystemsEngine::GetInstance().GetInputStateManager().VButtonTapped(input::Button::MAIN_BUTTON))
            {
                if (mMarkingMode)
                {
                    cardPickResult.selectedCard->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME] = !cardPickResult.selectedCard->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME];
                }
                else
                {
                    auto coords = GetCardSceneObjectBoardCoords(cardPickResult.selectedCard);
                    mBoardState->SetCardStateAt(coords.mRow, coords.mCol, CardState::FLIPPED);
                }
            }
            // Right-Click
            else if (CoreSystemsEngine::GetInstance().GetInputStateManager().VButtonTapped(input::Button::SECONDARY_BUTTON))
            {
                cardPickResult.selectedCard->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME] = !cardPickResult.selectedCard->mShaderBoolUniformValues[CARD_MARKED_UNIFORM_NAME];
            }            
        }
        else
        {
            UpdateMarkButton(dtMillis);
        }
        
        auto levelText = scene->FindSceneObject(strutils::StringId("level_text"));
        std::get<scene::TextSceneObjectData>(levelText->mSceneObjectTypeData).mText = "Level: " + std::to_string(mDifficulty);
    }
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
    ResetCameraPosition();
}

///------------------------------------------------------------------------------------------------

std::shared_ptr<scene::SceneObject> Game::GetCardSceneObjectFromCoords(const CardCoords& coords) const
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        return scene->FindSceneObject(strutils::StringId(CARD_SO_NAME_PREFIX + std::to_string(coords.mRow) + "," + std::to_string(coords.mCol)));
    }
    return nullptr;
}

///------------------------------------------------------------------------------------------------

Game::CardCoords Game::GetCardSceneObjectBoardCoords(std::shared_ptr<scene::SceneObject> card) const
{
    auto coords = strutils::StringSplit(strutils::StringSplit(card->mName.GetString(), '_')[1], ',');
    return CardCoords{std::stoi(coords[0]), std::stoi(coords[1])};
}

///------------------------------------------------------------------------------------------------

void Game::OnCardStateChangeEvent(const events::CardStateChangeEvent& event)
{
    // Find Card
    auto cardSceneObject = GetCardSceneObjectFromCoords(CardCoords{event.mRow, event.mCol});
    if (cardSceneObject)
    {
        // Stop existing animation
        auto hoverResetAnimationName = strutils::StringId(CARD_FLIP_ANIMATION_NAME_PREFIX + cardSceneObject->mName.GetString());
        auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
        animationManager.StopAnimation(hoverResetAnimationName);
        
        // And flip it
        const auto targetRotation = event.mNewCardState == CardState::FLIPPED ? -math::PI : 0.0f;
        animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<float>>(cardSceneObject->mRotation.z, targetRotation, 0.5f, animation_flags::NONE, 0.0f, math::ElasticFunction, math::TweeningMode::EASE_IN), [](){}, hoverResetAnimationName);
    }
}


///------------------------------------------------------------------------------------------------

void Game::OnCardTypeChangeEvent(const events::CardTypeChangeEvent& event)
{
    // Find Card
    auto cardSceneObject = GetCardSceneObjectFromCoords(CardCoords{event.mRow, event.mCol});
    if (cardSceneObject)
    {
        // Change Card Texture
        cardSceneObject->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + CARD_TEXTURES.at(mBoardState->GetCardTypeAt(event.mRow, event.mCol)));
    }
}

///------------------------------------------------------------------------------------------------

void Game::OnGameEnded()
{
    if (mGameEndReason == GameEndReason::LOSS)
    {
        
        mDifficulty = 1;
    }
    else if (mGameEndReason == GameEndReason::WIN)
    {
        mDifficulty = math::Min(mDifficulty + 1, 10);
    }
    
    mBoardState->GenerateBoardBasedOnDifficulty(mDifficulty);
    
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        scene->RemoveAllSceneObjectsWithNameStartingWith(ROW_CLUE_NAME_PREFIX);
        scene->RemoveAllSceneObjectsWithNameStartingWith(COL_CLUE_NAME_PREFIX);
        
        CreateClueSceneObjects(mBoardState->GetRowClues(), true);
        CreateClueSceneObjects(mBoardState->GetColClues(), false);
    }
    
    auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
    auto levelText = scene->FindSceneObject(strutils::StringId("level_text"));
    std::get<scene::TextSceneObjectData>(levelText->mSceneObjectTypeData).mText = "Level: " + std::to_string(mDifficulty);
    animationManager.StartAnimation(std::make_unique<rendering::PulseAnimation>(levelText, 0.7f, 0.5f), [](){});
    
    auto cards = scene->FindSceneObjectsWhoseNameStartsWith(CARD_SO_NAME_PREFIX);
    auto cardCount = cards.size();
    for (int i = 0; i < cardCount; ++i)
    {
        auto cardSceneObject = cards[i];
    
        // Stop existing animation
        auto hoverResetAnimationName = strutils::StringId(CARD_FLIP_ANIMATION_NAME_PREFIX + cardSceneObject->mName.GetString());
        animationManager.StopAnimation(hoverResetAnimationName);
        
        // And flip it
        animationManager.StartAnimation(std::make_unique<rendering::TweenValueToTargetAnimation<float>>(cardSceneObject->mRotation.z, 0.0f, 0.5f, animation_flags::NONE, i * 0.05f, math::ElasticFunction, math::TweeningMode::EASE_IN), [i, cards, this]()
        {
            if (i == cards.size() - 1)
            {
                for (auto card: cards)
                {
                    auto coords = GetCardSceneObjectBoardCoords(card);
                    card->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + CARD_TEXTURES.at(mBoardState->GetCardTypeAt(coords.mRow, coords.mCol)));
                }
                mBlockGameInput = false;
            }
        }, hoverResetAnimationName);
    }
}


///------------------------------------------------------------------------------------------------

void Game::UpdateMarkButton(const float dtMillis)
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        auto markButton = scene->FindSceneObject(MARK_BUTTON_SO_NAME);
        
        static float time = 0.0f;
        time += dtMillis/1000.0f;
    
        markButton->mShaderFloatUniformValues[TIME_UNIFORM_NAME] = time;
        
        const auto& camera = scene->GetCamera();
        const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
        
        auto rayOrigin = camera.GetPosition();
        auto rayDirection = math::ComputePointingRayDirection(
            CoreSystemsEngine::GetInstance().GetInputStateManager().VGetPointingPos(),
            camera.GetViewMatrix(),
            camera.GetProjMatrix(),
            static_cast<float>(windowDimensions.x),
            static_cast<float>(windowDimensions.y));
        
        float t;
        auto boundingRect = scene_object_utils::GetSceneObjectBoundingRect(*markButton);
        float sphereRadius = math::Max(math::Abs(boundingRect.bottomLeft.x - boundingRect.topRight.x), math::Abs(boundingRect.bottomLeft.y - boundingRect.topRight.y)) * 0.5f;
        
        markButton->mShaderFloatUniformValues[CUSTOM_ALPHA_UNIFORM_NAME] = 1.0f;
        if (math::RayToSphereIntersection(rayOrigin, rayDirection, markButton->mPosition, sphereRadius, t))
        {
            if (CoreSystemsEngine::GetInstance().GetInputStateManager().VButtonTapped(input::Button::MAIN_BUTTON))
            {
                static const strutils::StringId MARK_BUTTON_PULSING_ANIMATION_NAME = strutils::StringId("mark_button_pulsing");
                static const strutils::StringId MARK_BUTTON_SELECTED_UNIFORM_NAME = strutils::StringId("selected");
                
                markButton->mShaderBoolUniformValues[MARK_BUTTON_SELECTED_UNIFORM_NAME] = !markButton->mShaderBoolUniformValues[MARK_BUTTON_SELECTED_UNIFORM_NAME];
                mMarkingMode = markButton->mShaderBoolUniformValues[MARK_BUTTON_SELECTED_UNIFORM_NAME];

                markButton->mScale = MARK_BUTTON_INIT_SCALE;

                auto& animationManager = CoreSystemsEngine::GetInstance().GetAnimationManager();
                if (animationManager.IsAnimationPlaying(MARK_BUTTON_PULSING_ANIMATION_NAME))
                {
                    
                    animationManager.StopAnimation(MARK_BUTTON_PULSING_ANIMATION_NAME);
                }
                else
                {
                    animationManager.StartAnimation(std::make_unique<rendering::PulseAnimation>(markButton, 1.2f, 0.3f, animation_flags::ANIMATE_CONTINUOUSLY), [](){}, MARK_BUTTON_PULSING_ANIMATION_NAME);
                }
            }
        }
    }
}

///------------------------------------------------------------------------------------------------

void Game::CreateClueSceneObjects(const std::vector<Clue>& clues, const bool isRowClues)
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        static const std::string CLUE_NAME_BOMB_TEXT_POSTFIX = "_bomb_text";
        static const std::string CLUE_NAME_POINTS_TEXT_POSTFIX = "_points_text";
        static const std::string CLUE_NAME_BOMB_ICON_POSTFIX = "_bomb_icon";
        static const std::string CLUE_NAME_POINTS_ICON_POSTFIX = "_points_icon";
        static const std::string BOMB_ICON_TEXTURE = "game/bomb_icon.png";
        static const std::string POINTS_ICON_TEXTURE = "game/points_icon.png";
        
        enum class ClueComponentType
        {
            POINTS_TEXT,
            BOMB_TEXT,
            POINTS_ICON,
            BOMB_ICON
        };
        
        auto createSceneObject = [](
            const ClueComponentType clueComponentType,
            const glm::vec3& baseCluePosition,
            const std::vector<Clue>& clues,
            const int index,
            const bool isRowClue)
        {
            auto namePrefix = (isRowClue ? ROW_CLUE_NAME_PREFIX : COL_CLUE_NAME_PREFIX) + std::to_string(index);
            strutils::StringId sceneObjectName;
            
            switch (clueComponentType)
            {
                case ClueComponentType::POINTS_TEXT: sceneObjectName = strutils::StringId(namePrefix + CLUE_NAME_POINTS_TEXT_POSTFIX); break;
                case ClueComponentType::BOMB_TEXT: sceneObjectName = strutils::StringId(namePrefix + CLUE_NAME_BOMB_TEXT_POSTFIX); break;
                case ClueComponentType::POINTS_ICON: sceneObjectName = strutils::StringId(namePrefix + CLUE_NAME_POINTS_ICON_POSTFIX); break;
                case ClueComponentType::BOMB_ICON: sceneObjectName = strutils::StringId(namePrefix + CLUE_NAME_BOMB_ICON_POSTFIX); break;
            }
            
            auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
            auto sceneObject = scene->CreateSceneObject(sceneObjectName);
            
            sceneObject->mPosition = baseCluePosition;
            sceneObject->mRotation.x = -math::PI/2.0f;
    
            switch (clueComponentType)
            {
                case ClueComponentType::POINTS_TEXT:
                case ClueComponentType::BOMB_TEXT:
                {
                    scene::TextSceneObjectData textData;
                    textData.mFontName = game_constants::DEFAULT_FONT_NAME;
                    textData.mText = std::to_string(clueComponentType == ClueComponentType::POINTS_TEXT ? clues[index].mScoreSum : clues[index].mBombCount);
                    sceneObject->mSceneObjectTypeData = std::move(textData);
                    sceneObject->mShaderResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_SHADERS_ROOT + game_constants::DEFAULT_FONT_SHADER_NAME);
                    
                    sceneObject->mScale = glm::vec3(0.0002f);
                    
                    if (clueComponentType == ClueComponentType::BOMB_TEXT)
                    {
                        sceneObject->mPosition.z += 0.03f;
                    }
                } break;
                    
                case ClueComponentType::POINTS_ICON:
                case ClueComponentType::BOMB_ICON:
                {
                    sceneObject->mTextureResourceId = CoreSystemsEngine::GetInstance().GetResourceLoadingService().LoadResource(resources::ResourceLoadingService::RES_TEXTURES_ROOT + (clueComponentType == ClueComponentType::POINTS_ICON ? POINTS_ICON_TEXTURE : BOMB_ICON_TEXTURE));
                    
                    sceneObject->mScale = glm::vec3(0.028f);
                    sceneObject->mPosition.x += 0.03f;
                    sceneObject->mPosition.y -= 0.05f;
                    sceneObject->mPosition.z += clueComponentType == ClueComponentType::POINTS_ICON ? -0.014f : 0.014f;
                        
                } break;
            }
            
        };
        
        for (auto i = 0; i < clues.size(); ++i)
        {
            const auto basePosition = isRowClues ?
                glm::vec3(0.252f, 0.188f, -0.187f + i * 0.088f) :
                glm::vec3(-0.195f + i * 0.088f, 0.188f, 0.256f);
            
            createSceneObject(ClueComponentType::POINTS_TEXT, basePosition, clues, i, isRowClues);
            createSceneObject(ClueComponentType::BOMB_TEXT, basePosition, clues, i, isRowClues);
            createSceneObject(ClueComponentType::POINTS_ICON, basePosition, clues, i, isRowClues);
            createSceneObject(ClueComponentType::BOMB_ICON, basePosition, clues, i, isRowClues);
        }
    }
}

///------------------------------------------------------------------------------------------------

void Game::ResetCameraPosition()
{
    auto scene = CoreSystemsEngine::GetInstance().GetSceneManager().FindScene(game_constants::WORLD_SCENE_NAME);
    if (scene)
    {
        const auto& windowDimensions = CoreSystemsEngine::GetInstance().GetContextRenderableDimensions();
        const auto& currentAspect = static_cast<float>(windowDimensions.x)/windowDimensions.y;
        
        auto minPosition = glm::vec3(0.03f, 1.232f, 0.026f);
        auto minFront = glm::vec3(0.0f, -30.0f, -1.235f);
        
        auto maxPosition = glm::vec3(0.036f, 0.6f, 0.283f);
        auto maxFront = glm::vec3(0.0f, -2.154f, -1.0f);
        
        auto minAspect = 0.4625f;
        auto maxAspect = 1.0f;
        
        // Normalize aspect to [0, 1]
        float t = (currentAspect - minAspect)/(maxAspect - minAspect);
        t = glm::clamp(t, 0.0f, 1.0f);
        
        auto nextPosition = glm::mix(minPosition, maxPosition, t);
        auto nextFront = glm::mix(minFront, maxFront, t);
        
        scene->GetCamera().SetPosition(nextPosition);
        scene->GetCamera().SetFront(nextFront);
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
        auto coords = GetCardSceneObjectBoardCoords(card);
        if (mBoardState->GetCardStateAt(coords.mRow, coords.mCol) == CardState::FLIPPED)
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
        auto coords = GetCardSceneObjectBoardCoords(card);
        if (mBoardState->GetCardStateAt(coords.mRow, coords.mCol) == CardState::FLIPPED)
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
    
    if (mBoardState)
    {
        mBoardState->CreateDebugWidgets();
    }
}
#else
void Game::CreateDebugWidgets()
{
}
#endif

///------------------------------------------------------------------------------------------------
