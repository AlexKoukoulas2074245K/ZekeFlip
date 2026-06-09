///------------------------------------------------------------------------------------------------
///  BoardState.cpp
///  ZekeFlipClient
///
///  Created by Alex Koukoulas on 05/06/2026
///------------------------------------------------------------------------------------------------

#include <engine/utils/MathUtils.h>
#include <game/events/EventSystem.h>
#include <game/BoardState.h>
#include <sstream>

#if defined(USE_IMGUI)
#include <imgui/imgui.h>
#endif

///------------------------------------------------------------------------------------------------

static std::unordered_map<CardType, int> sCardTypeToScore =
{
    { CardType::ONE, 1 }, { CardType::TWO, 2 }, { CardType::THREE, 3 },
};

///------------------------------------------------------------------------------------------------

BoardState::BoardState(const int boardSize)
    : mBoardSize(boardSize)
{
    mBoard.resize(boardSize);
    for (int i = 0; i < boardSize; ++i)
    {
        mBoard[i].resize(boardSize, CardEntry{CardType::ONE, CardState::HIDDEN});
    }
    
    mRowClues.resize(boardSize);
    mColClues.resize(boardSize);
}

///------------------------------------------------------------------------------------------------

int BoardState::GetBoardSize() const
{
    return mBoardSize;
}

///------------------------------------------------------------------------------------------------

CardType BoardState::GetCardTypeAt(const int row, const int col) const
{
    assert(row < mBoard.size() && col < mBoard[row].size());
    return mBoard[row][col].mCardType;
}

///------------------------------------------------------------------------------------------------

void BoardState::SetCardTypeAt(const int row, const int col, const CardType cardType)
{
    assert(row < mBoard.size() && col < mBoard[row].size());
    
    if (cardType != mBoard[row][col].mCardType)
    {
        mBoard[row][col].mCardType = cardType;
        events::EventSystem::GetInstance().DispatchEvent<events::CardTypeChangeEvent>(row, col, cardType);
    }
}

///------------------------------------------------------------------------------------------------

CardState BoardState::GetCardStateAt(const int row, const int col) const
{
    assert(row < mBoard.size() && col < mBoard[row].size());
    return mBoard[row][col].mCardState;
}

///------------------------------------------------------------------------------------------------

void BoardState::SetCardStateAt(const int row, const int col, const CardState cardState)
{
    assert(row < mBoard.size() && col < mBoard[row].size());
    
    if (cardState != mBoard[row][col].mCardState)
    {
        mBoard[row][col].mCardState = cardState;
        events::EventSystem::GetInstance().DispatchEvent<events::CardStateChangeEvent>(row, col, cardState);
        CheckAndDispatchEndGameEvents();
    }
}

///------------------------------------------------------------------------------------------------

const std::vector<Clue>& BoardState::GetRowClues() const
{
    return mRowClues;
}

///------------------------------------------------------------------------------------------------

const std::vector<Clue>& BoardState::GetColClues() const
{
    return mColClues;
}

///------------------------------------------------------------------------------------------------

void BoardState::GenerateBoardBasedOnDifficulty(int difficulty)
{
    assert(difficulty >= 1 && difficulty <= 10);
    
    // Board Reset
    for (int row = 0; row < mBoardSize; ++row)
    {
        for (int col = 0; col < mBoardSize; ++col)
        {
            mBoard[row][col].mCardState = CardState::HIDDEN;
            mBoard[row][col].mCardType = CardType::ONE;
        }
    }

    auto placeRandom = [this](const CardType cardType, int count)
    {
        while (count > 0)
        {
            int randRow = math::RandomInt() % mBoardSize;
            int randCol = math::RandomInt() % mBoardSize;
            
            if (GetCardTypeAt(randRow, randCol) == CardType::ONE)
            {
                mBoard[randRow][randCol].mCardType = cardType;
                count--;
            }
        }
    };
    
    int bombCount   = static_cast<int>(math::Lerp(6.0f,10.0f, difficulty/10.0f));
    int twosCount   = static_cast<int>(math::Lerp(2.0f,7.0f, difficulty/10.0f));
    int threesCount = static_cast<int>(math::Lerp(1.0f,6.0f, difficulty/10.0f));
    
    // Card Placement
    placeRandom(CardType::BOMB, bombCount);
    placeRandom(CardType::TWO, twosCount);
    placeRandom(CardType::THREE, threesCount);
    
    // Row Clue Calculation
    for (int row = 0; row < mBoardSize; row++)
    {
        mRowClues[row] = Clue{};
    
        for (int col = 0; col < mBoardSize; col++)
        {
            if (GetCardTypeAt(row, col) == CardType::BOMB)
            {
                mRowClues[row].mBombCount++;
            }
            else
            {
                mRowClues[row].mScoreSum += sCardTypeToScore.at(GetCardTypeAt(row, col));
            }
        }
    }
    
    // Ver Clue Calculation
    for (int col = 0; col < mBoardSize; col++)
    {
        mColClues[col] = {};
        for (int row = 0; row < mBoardSize; row++)
        {
            if (GetCardTypeAt(row, col) == CardType::BOMB)
            {
                mColClues[col].mBombCount++;
            }
            else
            {
                
                mColClues[col].mScoreSum += sCardTypeToScore.at(GetCardTypeAt(row, col));
            }
        }
    }
}

///------------------------------------------------------------------------------------------------

void BoardState::CheckAndDispatchEndGameEvents()
{
    bool twosThreesStillHidden = false;
    for (int row = 0; row < mBoardSize; ++row)
    {
        for (int col = 0; col < mBoardSize; ++col)
        {
            if (GetCardStateAt(row, col) == CardState::FLIPPED && GetCardTypeAt(row, col) == CardType::BOMB)
            {
                events::EventSystem::GetInstance().DispatchEvent<events::GameEndedEvent>(GameEndReason::LOSS);
                return;
            }
            if ((GetCardTypeAt(row, col) == CardType::TWO || GetCardTypeAt(row, col) == CardType::THREE) && GetCardStateAt(row, col) == CardState::HIDDEN)
            {
                twosThreesStillHidden = true;
            }
        }
    }
    
    if (!twosThreesStillHidden)
    {
        events::EventSystem::GetInstance().DispatchEvent<events::GameEndedEvent>(GameEndReason::WIN);
    }
}

///------------------------------------------------------------------------------------------------

#if defined(USE_IMGUI)
void BoardState::CreateDebugWidgets()
{
    ImGui::ShowDemoWindow();
    ImGui::Begin("Board", nullptr, GLOBAL_IMGUI_WINDOW_FLAGS);
    
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersH | ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("table1", mBoardSize, flags))
    {
        for (int row = 0; row < mBoardSize; row++)
        {
            ImGui::TableNextRow();
            for (int col = 0; col < mBoardSize; col++)
            {
                ImGui::TableSetColumnIndex(col);
                
                const auto cardType = GetCardTypeAt(row, col);
                const auto cardState = GetCardStateAt(row, col);
                
                switch (cardType)
                {
                    case CardType::ONE:
                    {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", GetCardTypeToString(cardType));
                    } break;
                    
                    case CardType::TWO:
                    case CardType::THREE:
                    {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", GetCardTypeToString(cardType));
                    } break;
                    
                    case CardType::BOMB:
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", GetCardTypeToString(cardType));
                    } break;
                }
                
                ImGui::SameLine();
                ImGui::PushID((std::to_string(row) + "," + std::to_string(col) + "_statebutton").c_str());
                if (ImGui::Button(cardState == CardState::HIDDEN ? "Flip" : "Hide"))
                {
                    SetCardStateAt(row, col, cardState == CardState::HIDDEN ? CardState::FLIPPED : CardState::HIDDEN);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    
    ImGui::End();
}
#else
void BoardState::CreateDebugWidgets(){}
#endif
