///------------------------------------------------------------------------------------------------
///  Game.h                                                                                          
///  ZekeFlipClient
///                                                                                                
///  Created by Alex Koukoulas on 19/09/2023
///------------------------------------------------------------------------------------------------

#ifndef Game_h
#define Game_h

///------------------------------------------------------------------------------------------------

#include <atomic>
#include <memory>
#include <engine/utils/MathUtils.h>
#include <engine/utils/StringUtils.h>
#include <game/events/EventSystem.h>
#include <vector>

///------------------------------------------------------------------------------------------------

namespace scene
{
    struct SceneObject;
}

class AnimatedButton;
class Game final
{
public:
    Game(const int argc, char** argv);
    ~Game();
    
    void Init();
    void Update(const float dtMillis);
    void ApplicationMovedToBackground();
    void WindowResize();
    void OnOneSecondElapsed();    
    void CreateDebugWidgets();

private:
    void ResetCameraPosition();
    void CardHoveringAnimation();
    
    struct CardPickingResult
    {
        std::shared_ptr<scene::SceneObject> selectedCard;
        float distanceFromCardCenter;
    };
    
    CardPickingResult PickPointedCard();

private:
    std::unique_ptr<AnimatedButton> mTestButton;
    std::vector<std::shared_ptr<scene::SceneObject>> mFlippedCards;
};

///------------------------------------------------------------------------------------------------

#endif /* Game_h */
