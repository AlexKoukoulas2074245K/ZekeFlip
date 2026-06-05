///------------------------------------------------------------------------------------------------
///  BoardState.cpp
///  ZekeFlipClient
///
///  Created by Alex Koukoulas on 05/06/2026
///------------------------------------------------------------------------------------------------

#include <game/events/EventSystem.h>
#include <game/BoardState.h>
#include <sstream>

#if defined(USE_IMGUI)
#include <imgui/imgui.h>
#endif

///------------------------------------------------------------------------------------------------

BoardState::BoardState(const int boardSize)
    : mBoardSize(boardSize)
{
    mBoard.resize(boardSize);
    for (int i = 0; i < boardSize; ++i)
    {
        mBoard[i].resize(boardSize, CardEntry{CardType::ONE, CardState::HIDDEN});
    }
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
    mBoard[row][col].mCardType = cardType;
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
