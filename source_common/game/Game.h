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
#include <game/BoardState.h>
#include <game/events/EventSystem.h>
#include <optional>
#include <vector>

///------------------------------------------------------------------------------------------------

namespace scene
{
    struct SceneObject;
}

class AnimatedButton;
class BoardState;
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
    struct CardCoords
    {
        int mRow;
        int mCol;
    };
    
    std::shared_ptr<scene::SceneObject> GetCardSceneObjectFromCoords(const CardCoords& coords) const;
    CardCoords GetCardSceneObjectBoardCoords(std::shared_ptr<scene::SceneObject> card) const;
    
    void OnCardStateChangeEvent(const events::CardStateChangeEvent& event);
    void OnCardTypeChangeEvent(const events::CardTypeChangeEvent& event);
    void OnGameEnded();
    
    void UpdateMarkButton(const float dtMillis);
    void CreateClueSceneObjects(const std::vector<Clue>& clues, const bool isRowClues);
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
    std::unique_ptr<BoardState> mBoardState;
    std::unique_ptr<events::IListener> mCardStateChangeListener;
    std::unique_ptr<events::IListener> mCardTypeChangeListener;
    std::unique_ptr<events::IListener> mGameEndedListener;
    std::optional<GameEndReason> mGameEndReason;
    float mGameEndDelayTimerSecs;
    int mDifficulty;
    bool mBlockGameInput;
    bool mMarkingMode;
};

///------------------------------------------------------------------------------------------------

#endif /* Game_h */
