#include "ui/hud.h"

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "game.h"
#include "input.h"

namespace ui {

void init_imgui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 8.0f;
    style.FrameRounding    = 4.0f;
    style.GrabRounding     = 4.0f;
    style.PopupRounding    = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding      = 4.0f;
    style.WindowPadding    = ImVec2(12, 12);
    style.FramePadding     = ImVec2(8, 5);
    style.ItemSpacing      = ImVec2(10, 7);
    style.WindowBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg]        = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_ChildBg]         = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_DockingEmptyBg]  = ImVec4(0.f, 0.f, 0.f, 0.f);
    style.Colors[ImGuiCol_Text]            = ImVec4(0.88f, 0.95f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_Button]          = ImVec4(0.08f, 0.28f, 0.42f, 0.90f);
    style.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.12f, 0.52f, 0.72f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.05f, 0.70f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]      = ImVec4(0.00f, 0.75f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]= ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]       = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]         = ImVec4(0.06f, 0.10f, 0.18f, 0.90f);
    style.Colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.08f, 0.18f, 0.32f, 0.90f);
    style.Colors[ImGuiCol_Header]          = ImVec4(0.00f, 0.50f, 0.72f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered]   = ImVec4(0.00f, 0.68f, 0.88f, 0.60f);
    style.Colors[ImGuiCol_Separator]       = ImVec4(0.00f, 0.45f, 0.65f, 0.50f);
    style.Colors[ImGuiCol_TitleBg]         = ImVec4(0.04f, 0.04f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.04f, 0.10f, 0.18f, 1.00f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    glfwSetScrollCallback(window, scroll_callback);
}

void hud_overlay(const Game& game, float viewport_x0, float viewport_y0)
{
    ImGui::SetNextWindowPos(ImVec2(viewport_x0 + 14.f, viewport_y0 + 14.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGuiWindowFlags hud_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.85f, 1.0f, 0.90f));
    if (ImGui::Begin("##hud", nullptr, hud_flags))
    {
        ImGui::Text("SCORE  %d", game.score());
        ImGui::Text("LEVEL  %d", game.level());
        ImGui::Text("LINES  %d", game.total_cleared());
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void game_over_overlay(Game& game, int& high_score, bool& auto_play_reset, bool& spin_reset, bool& score_saved)
{
    ImGuiIO& io = ImGui::GetIO();
    int cur = game.score();
    bool is_new = cur > high_score && cur > 0;
    if (is_new)
    {
        high_score = cur;
        score_saved = false;
    }

    ImVec2 center{io.DisplaySize.x * 0.5f * 0.68f, io.DisplaySize.y * 0.5f};
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGuiWindowFlags ov_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##gameover", nullptr, ov_flags))
    {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("GAME OVER").x) * 0.5f);
        ImGui::TextColored(ImVec4(1.f, 0.25f, 0.25f, 1.f), "GAME OVER");
        ImGui::Separator();
        ImGui::Text("Score:  %d", cur);
        if (is_new)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f), "  NEW!");
        }
        ImGui::Text("High score:  %d", high_score);
        ImGui::Text("Level:  %d", game.level());
        ImGui::Text("Lines:  %d", game.total_cleared());
        ImGui::Spacing();
        float btn_w = 120.f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btn_w) * 0.5f);
        if (ImGui::Button("Restart", ImVec2(btn_w, 0)))
        {
            game.restart();
            auto_play_reset = true;
            spin_reset = true;
        }
    }
    ImGui::End();
}

void paused_overlay()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center{io.DisplaySize.x * 0.5f * 0.68f, io.DisplaySize.y * 0.5f};
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(240, 100), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.80f);
    ImGuiWindowFlags ov_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##paused", nullptr, ov_flags))
    {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("PAUSED").x) * 0.5f);
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f), "PAUSED");
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Press P to resume").x) * 0.5f);
        ImGui::TextDisabled("Press P to resume");
    }
    ImGui::End();
}

} // namespace ui
