///------------------------------------------------------------------------------------------------
///  BoardState.h
///  ZekeFlipClient
///                                                                                                
///  Created by Alex Koukoulas on 05/06/2026
///------------------------------------------------------------------------------------------------

#ifndef BoardState_h
#define BoardState_h

///------------------------------------------------------------------------------------------------

#include <assert.h>
#include <vector>

///------------------------------------------------------------------------------------------------

enum class CardType
{
    ONE,
    TWO,
    THREE,
    BOMB
};

enum class CardState
{
    HIDDEN,
    FLIPPED
};

enum class GameEndReason
{
    WIN,
    LOSS
};

struct Clue
{
    int mScoreSum;
    int mBombCount;
};

inline const char* GetCardTypeToString(const CardType cardType)
{
    switch (cardType)
    {
        case CardType::ONE: return "1";
        case CardType::TWO: return "2";
        case CardType::THREE: return "3";
        case CardType::BOMB: return "F";
    }
    assert(false);
    return "";
}

inline const char* GetCardStateToString(const CardState cardState)
{
    switch (cardState)
    {
        case CardState::HIDDEN: return "H";
        case CardState::FLIPPED: return "F";
    }
    assert(false);
    return "";
}

///------------------------------------------------------------------------------------------------

class BoardState final
{
public:
    BoardState(int boardSize);
    
    int GetBoardSize() const;
    
    CardType GetCardTypeAt(const int row, const int col) const;
    void SetCardTypeAt(const int row, const int col, const CardType cardType);
    
    CardState GetCardStateAt(const int row, const int col) const;
    void SetCardStateAt(const int row, const int col, const CardState cardState);
    
    const std::vector<Clue>& GetRowClues() const;
    const std::vector<Clue>& GetColClues() const;
    
    void GenerateBoardBasedOnDifficulty(int difficulty);
    void CheckAndDispatchEndGameEvents();
    
    void CreateDebugWidgets();
private:
    struct CardEntry
    {
        CardType mCardType;
        CardState mCardState;
    };
    
    std::vector<std::vector<CardEntry>> mBoard;
    std::vector<Clue> mRowClues;
    std::vector<Clue> mColClues;
    
    const int mBoardSize;
};

///------------------------------------------------------------------------------------------------

#endif /* BoardState_h */
