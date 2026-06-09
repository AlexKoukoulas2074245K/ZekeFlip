///------------------------------------------------------------------------------------------------
///  Events.h                                                                                          
///  ZekeFlipClient                                                                                            
///                                                                                                
///  Created by Alex Koukoulas on 02/11/2023                                                       
///------------------------------------------------------------------------------------------------

#ifndef Events_h
#define Events_h

///------------------------------------------------------------------------------------------------

#include <engine/utils/StringUtils.h>
#include <game/BoardState.h>

///------------------------------------------------------------------------------------------------

namespace events
{

///------------------------------------------------------------------------------------------------

class DummyEvent
{
    
};

///------------------------------------------------------------------------------------------------

class CardStateChangeEvent
{
public:
    CardStateChangeEvent(const int row, const int col, const CardState newCardState)
    : mRow(row)
    , mCol(col)
    , mNewCardState(newCardState)
    {
    }

public:
    const int mRow;
    const int mCol;
    const CardState mNewCardState;
};

///------------------------------------------------------------------------------------------------

class CardTypeChangeEvent
{
public:
    CardTypeChangeEvent(const int row, const int col, const CardType newCardType)
    : mRow(row)
    , mCol(col)
    , mNewCardType(newCardType)
    {
    }

public:
    const int mRow;
    const int mCol;
    const CardType mNewCardType;
};

///------------------------------------------------------------------------------------------------

class GameEndedEvent
{
public:
    GameEndedEvent(const GameEndReason reason)
    : mReason(reason)
    {
    }

public:
    const GameEndReason mReason;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* Events_h */
