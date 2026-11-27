#include "imnotepad/imnotepad.h"

#ifndef IMGUI_DISABLE
#if !defined(IMGUI_DISABLE_DEMO_WINDOWS)
#include "imfiletree/imfiletree.h"
#include "imfiletree/imfiletree_internal.h"
#include "imnotepad/imnotepad_internal.h"
#include "imnotepad/imnotepad_colorschemes.h"

#include "misc/path_utils.h"

static ImNotepadTextFile* GLastActiveFile = nullptr;

static void ImNotepadNewFile(const char* file_name = nullptr)
{
    if(file_name == nullptr)
    {
        static int untitled_count = 1;
        char buf[32];
        sprintf_s(buf, "Untitled-%d", untitled_count++);
        file_name = buf;
    }

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();
    gn.FileSystem.NewFile(file_name);
}

static void ImNotepadFileOpen(const char* file_name)
{
    if(file_name == nullptr)
        return;

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

    bool file_is_present = false;

    for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end(); ++it)
    {
        ImNotepadTextFile* file = *it;
        if(file == nullptr)
            continue;

        if(file->FullPathName == file_name)
        {
            file_is_present = true;
            break;
        }
    }

    if(!file_is_present)
        gn.FileSystem.TextFileRead(file_name);
}

static void ImNotepadFileClose(const char* file_name)
{
    if(file_name == nullptr)
        return;

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

    for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end();)
    {
        ImNotepadTextFile* file = *it;
        if(file == nullptr)
            continue;

        if(file->FullPathName == file_name)
        {
            IM_DELETE(file);
            it = gn.FileSystem.TextFiles.erase(it);
        }
        else
            ++it;
    }
}

static void ImNotepadFileRename(const char* old_file_name, const char* new_file_name)
{
    if(old_file_name == nullptr || new_file_name == nullptr)
        return;

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

    for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end(); ++it)
    {
        ImNotepadTextFile* file = *it;
        if(file == nullptr)
            continue;

        if(file->FullPathName == old_file_name)
        {
            file->Name = path_get_file_name(new_file_name);
            file->FullPathName = new_file_name;
        }
    }
}

static void ImNotepadFileSave(ImNotepadTextFile* file_ptr = nullptr)
{
    if(file_ptr == nullptr)
        return;

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

    for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end();)
    {
        ImNotepadTextFile* file = *it;
        if(file == nullptr)
            continue;

        if(file == file_ptr)
        {
            if(gn.FileSystem.TextFileWrite(file))
                file->UpdateSavedVersionId();
            break;
        }
        else
            ++it;
    }
}

static bool IsMouseHoveringDockTabForWindow(ImGuiWindow* root_window, ImGuiHoveredFlags flags = 0)
{
    if(root_window == nullptr)
        return false;

    ImGuiContext& g = *GImGui;

    float delay = 0.0f;
    if(flags & ImGuiHoveredFlags_DelayNormal)
        delay = g.Style.HoverDelayNormal;
    if(flags & ImGuiHoveredFlags_DelayShort)
        delay = g.Style.HoverDelayShort;

    if(ImGui::IsAnyMouseDown())
        return false;

    ImGuiDockNode* node = root_window->DockNode;
    if(node == nullptr || !node->TabBar)
        return false;

    ImGuiTabBar* tab_bar = node->TabBar;
    ImGuiTabItem* tab = ImGui::TabBarFindTabByID(tab_bar, root_window->TabId);
    if(tab == nullptr)
        return false;

    if(tab->Width <= 0.0f)
        return false;

    // The tab rectangle is calculated from BarRect + Offset + Width.
    // Offset is the offset of the tab from the left edge of the TabBar (taking into account scrolling).
    ImRect tab_rect;
    tab_rect.Min.x = tab_bar->BarRect.Min.x + tab->Offset;
    tab_rect.Min.y = tab_bar->BarRect.Min.y;
    tab_rect.Max.x = tab_bar->BarRect.Min.x + tab->Offset + tab->Width;
    tab_rect.Max.y = tab_bar->BarRect.Max.y;

    bool is_hovering = ImGui::IsMouseHoveringRect(tab_rect.Min, tab_rect.Max, false);

    if(delay > 0.0f || (flags & ImGuiHoveredFlags_Stationary))
    {
        if(!is_hovering)
            return false;

        ImGuiID hover_delay_id = tab->ID;
        if((flags & ImGuiHoveredFlags_NoSharedDelay) && (g.HoverItemDelayIdPreviousFrame != hover_delay_id))
            g.HoverItemDelayTimer = 0.0f;

        g.HoverItemDelayId = hover_delay_id;

        if((flags & ImGuiHoveredFlags_Stationary) != 0 && g.HoverItemUnlockedStationaryId != hover_delay_id)
            return false;

        if(g.HoverItemDelayTimer < delay)
            return false;
    }

    return is_hovering;
}

namespace ImGui
{
    // Soft Cherry style by Patitotective from ImThemes
    void StyleColorsSoftCherry()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha                     = 1.0f;
        style.DisabledAlpha             = 0.4f;
        style.WindowPadding             = ImVec2(10.0f, 10.0f);
        style.WindowRounding            = 4.0f;
        style.WindowBorderSize          = 0.0f;
        style.WindowMinSize             = ImVec2(50.0f, 50.0f);
        style.WindowTitleAlign          = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition  = ImGuiDir_Left;
        style.ChildRounding             = 0.0f;
        style.ChildBorderSize           = 1.0f;
        style.PopupRounding             = 1.0f;
        style.PopupBorderSize           = 1.0f;
        style.FramePadding              = ImVec2(5.0f, 3.0f);
        style.FrameRounding             = 3.0f;
        style.FrameBorderSize           = 0.0f;
        style.ItemSpacing               = ImVec2(6.0f, 6.0f);
        style.ItemInnerSpacing          = ImVec2(3.0f, 2.0f);
        style.CellPadding               = ImVec2(3.0f, 3.0f);
        style.IndentSpacing             = 6.0f;
        style.ColumnsMinSpacing         = 6.0f;
        style.ScrollbarSize             = 13.0f;
        style.ScrollbarRounding         = 16.0f;
        style.GrabMinSize               = 20.0f;
        style.GrabRounding              = 4.0f;
        style.TabRounding               = 4.0f;
        style.TabBorderSize             = 1.0f;
        style.ColorButtonPosition       = ImGuiDir_Right;
        style.ButtonTextAlign           = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign       = ImVec2(0.0f, 0.0f);

        style.Colors[ImGuiCol_Text]                     = ImVec4(0.85882354f, 0.92941177f, 0.8862745f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled]             = ImVec4(0.52156866f, 0.54901963f, 0.53333336f, 1.0f);
        style.Colors[ImGuiCol_WindowBg]                 = ImVec4(0.12941177f, 0.13725491f, 0.16862746f, 1.0f);
        style.Colors[ImGuiCol_ChildBg]                  = ImVec4(0.14901961f, 0.15686275f, 0.1882353f, 1.0f);
        style.Colors[ImGuiCol_PopupBg]                  = ImVec4(0.2f, 0.21960784f, 0.26666668f, 1.0f);
        style.Colors[ImGuiCol_Border]                   = ImVec4(0.13725491f, 0.11372549f, 0.13333334f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow]             = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]                  = ImVec4(0.16862746f, 0.18431373f, 0.23137255f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered]           = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive]            = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_TitleBg]                  = ImVec4(0.23137255f, 0.2f, 0.27058825f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive]            = ImVec4(0.5019608f, 0.07450981f, 0.25490198f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed]         = ImVec4(0.2f, 0.21960784f, 0.26666668f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg]                = ImVec4(0.2f, 0.21960784f, 0.26666668f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg]              = ImVec4(0.23921569f, 0.23921569f, 0.21960784f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab]            = ImVec4(0.3882353f, 0.3882353f, 0.37254903f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]     = ImVec4(0.69411767f, 0.69411767f, 0.6862745f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]      = ImVec4(0.69411767f, 0.69411767f, 0.6862745f, 1.0f);
        style.Colors[ImGuiCol_CheckMark]                = ImVec4(0.65882355f, 0.13725491f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab]               = ImVec4(0.6509804f, 0.14901961f, 0.34509805f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive]         = ImVec4(0.70980394f, 0.21960784f, 0.26666668f, 1.0f);
        style.Colors[ImGuiCol_Button]                   = ImVec4(0.6509804f, 0.14901961f, 0.34509805f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered]            = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive]             = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_Header]                   = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered]            = ImVec4(0.6509804f, 0.14901961f, 0.34509805f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]             = ImVec4(0.5019608f, 0.07450981f, 0.25490198f, 1.0f);
        style.Colors[ImGuiCol_Separator]                = ImVec4(0.42745098f, 0.42745098f, 0.49803922f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered]         = ImVec4(0.09803922f, 0.4f, 0.7490196f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive]          = ImVec4(0.09803922f, 0.4f, 0.7490196f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip]               = ImVec4(0.6509804f, 0.14901961f, 0.34509805f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered]        = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive]         = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_Tab]                      = ImVec4(0.1764706f, 0.34901962f, 0.5764706f, 1.0f);
        style.Colors[ImGuiCol_TabHovered]               = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 1.0f);
        style.Colors[ImGuiCol_TabActive]                = ImVec4(0.19607843f, 0.40784314f, 0.6784314f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused]             = ImVec4(0.06666667f, 0.101960786f, 0.14509805f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive]       = ImVec4(0.13333334f, 0.25882354f, 0.42352942f, 1.0f);
        style.Colors[ImGuiCol_PlotLines]                = ImVec4(0.85882354f, 0.92941177f, 0.8862745f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered]         = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram]            = ImVec4(0.30980393f, 0.7764706f, 0.19607843f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered]     = ImVec4(0.45490196f, 0.19607843f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_TableHeaderBg]            = ImVec4(0.1882353f, 0.1882353f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong]        = ImVec4(0.30980393f, 0.30980393f, 0.34901962f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight]         = ImVec4(0.22745098f, 0.22745098f, 0.24705882f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg]               = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TableRowBgAlt]            = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg]           = ImVec4(0.38431373f, 0.627451f, 0.91764706f, 1.0f);
        style.Colors[ImGuiCol_DragDropTarget]           = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight]             = ImVec4(0.25882354f, 0.5882353f, 0.9764706f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingHighlight]    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingDimBg]        = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_ModalWindowDimBg]         = ImVec4(0.8f, 0.8f, 0.8f, 0.3f);
    }

    void StyleColorsOneDarkPro()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha                     = 1.0f;
        style.DisabledAlpha             = 0.6f;
        style.WindowPadding             = ImVec2(10.0f, 10.0f);
        style.WindowRounding            = 4.0f;
        style.WindowBorderSize          = 0.0f;
        style.WindowMinSize             = ImVec2(50.0f, 50.0f);
        style.WindowTitleAlign          = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition  = ImGuiDir_Left;
        style.ChildRounding             = 0.0f;
        style.ChildBorderSize           = 1.0f;
        style.PopupRounding             = 1.0f;
        style.PopupBorderSize           = 1.0f;
        style.FramePadding              = ImVec2(5.0f, 3.0f);
        style.FrameRounding             = 3.0f;
        style.FrameBorderSize           = 0.0f;
        style.ItemSpacing               = ImVec2(6.0f, 6.0f);
        style.ItemInnerSpacing          = ImVec2(3.0f, 2.0f);
        style.CellPadding               = ImVec2(3.0f, 3.0f);
        style.IndentSpacing             = 6.0f;
        style.ColumnsMinSpacing         = 6.0f;
        style.ScrollbarSize             = 13.0f;
        style.ScrollbarRounding         = 16.0f;
        style.GrabMinSize               = 20.0f;
        style.GrabRounding              = 4.0f;
        style.TabRounding               = 4.0f;
        style.TabBorderSize             = 1.0f;
        style.ColorButtonPosition       = ImGuiDir_Right;
        style.ButtonTextAlign           = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign       = ImVec2(0.0f, 0.0f);

        style.Colors[ImGuiCol_Text]                     = ImVec4(0.6706f, 0.6980f, 0.7490f, 1.0000f); // #abb2bf
        style.Colors[ImGuiCol_TextDisabled]             = ImVec4(0.3608f, 0.3882f, 0.4392f, 1.0000f); // #5c6370
        style.Colors[ImGuiCol_WindowBg]                 = ImVec4(0.1569f, 0.1725f, 0.2039f, 1.0000f); // #282c34
        style.Colors[ImGuiCol_ChildBg]                  = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_PopupBg]                  = ImVec4(0.1725f, 0.1922f, 0.2353f, 0.9500f); // #2c313c + alpha
        style.Colors[ImGuiCol_Border]                   = ImVec4(0.2431f, 0.2667f, 0.3216f, 1.0000f); // #3e4452
        style.Colors[ImGuiCol_BorderShadow]             = ImVec4(0.0941f, 0.1020f, 0.1216f, 0.0000f); // #181a1f
        style.Colors[ImGuiCol_FrameBg]                  = ImVec4(0.1137f, 0.1216f, 0.1373f, 1.0000f); // #1d1f23
        style.Colors[ImGuiCol_FrameBgHovered]           = ImVec4(0.1961f, 0.2196f, 0.2588f, 1.0000f); // #323842
        style.Colors[ImGuiCol_FrameBgActive]            = ImVec4(0.2510f, 0.2784f, 0.3294f, 1.0000f); // #404754
        style.Colors[ImGuiCol_TitleBg]                  = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_TitleBgActive]            = ImVec4(0.2510f, 0.2784f, 0.3294f, 1.0000f); // #404754
        style.Colors[ImGuiCol_TitleBgCollapsed]         = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_MenuBarBg]                = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_ScrollbarBg]              = ImVec4(0.1569f, 0.1725f, 0.2039f, 1.0000f); // #282c34
        style.Colors[ImGuiCol_ScrollbarGrab]            = ImVec4(0.3059f, 0.3373f, 0.4000f, 1.0000f); // #4e5666
        style.Colors[ImGuiCol_ScrollbarGrabHovered]     = ImVec4(0.3529f, 0.3882f, 0.4588f, 1.0000f); // #5a6375
        style.Colors[ImGuiCol_ScrollbarGrabActive]      = ImVec4(0.4549f, 0.4902f, 0.5686f, 1.0000f); // #747d91
        style.Colors[ImGuiCol_CheckMark]                = ImVec4(0.5961f, 0.7647f, 0.4745f, 1.0000f); // #98c379 (Green)
        style.Colors[ImGuiCol_SliderGrab]               = ImVec4(0.3373f, 0.7137f, 0.7608f, 1.0000f); // #56b6c2 (Cyan)
        style.Colors[ImGuiCol_SliderGrabActive]         = ImVec4(0.3804f, 0.6863f, 0.9373f, 1.0000f); // #61afef (Blue)
        style.Colors[ImGuiCol_Button]                   = ImVec4(0.2510f, 0.2784f, 0.3294f, 1.0000f); // #404754
        style.Colors[ImGuiCol_ButtonHovered]            = ImVec4(0.2941f, 0.3255f, 0.3843f, 1.0000f); // #4b5362
        style.Colors[ImGuiCol_ButtonActive]             = ImVec4(0.2431f, 0.2667f, 0.3216f, 1.0000f); // #3e4452
        style.Colors[ImGuiCol_Header]                   = ImVec4(0.1961f, 0.2196f, 0.2588f, 1.0000f); // #323842
        style.Colors[ImGuiCol_HeaderHovered]            = ImVec4(0.2431f, 0.2667f, 0.3216f, 1.0000f); // #3e4452
        style.Colors[ImGuiCol_HeaderActive]             = ImVec4(0.2510f, 0.2784f, 0.3294f, 1.0000f); // #404754
        style.Colors[ImGuiCol_Separator]                = ImVec4(0.2431f, 0.2667f, 0.3216f, 1.0000f); // #3e4452
        style.Colors[ImGuiCol_SeparatorHovered]         = ImVec4(0.3373f, 0.7137f, 0.7608f, 1.0000f); // #56b6c2
        style.Colors[ImGuiCol_SeparatorActive]          = ImVec4(0.3804f, 0.6863f, 0.9373f, 1.0000f); // #61afef
        style.Colors[ImGuiCol_ResizeGrip]               = ImVec4(0.3059f, 0.3373f, 0.4000f, 1.0000f); // #4e5666
        style.Colors[ImGuiCol_ResizeGripHovered]        = ImVec4(0.3529f, 0.3882f, 0.4588f, 1.0000f); // #5a6375
        style.Colors[ImGuiCol_ResizeGripActive]         = ImVec4(0.4549f, 0.4902f, 0.5686f, 1.0000f); // #747d91
        style.Colors[ImGuiCol_Tab]                      = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_TabHovered]               = ImVec4(0.1961f, 0.2196f, 0.2588f, 1.0000f); // #323842
        style.Colors[ImGuiCol_TabActive]                = ImVec4(0.1569f, 0.1725f, 0.2039f, 1.0000f); // #282c34
        style.Colors[ImGuiCol_TabUnfocused]             = ImVec4(0.0941f, 0.1020f, 0.1216f, 1.0000f); // #181a1f
        style.Colors[ImGuiCol_TabUnfocusedActive]       = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_PlotLines]                = ImVec4(0.6706f, 0.6980f, 0.7490f, 1.0000f); // #abb2bf
        style.Colors[ImGuiCol_PlotLinesHovered]         = ImVec4(0.3804f, 0.6863f, 0.9373f, 1.0000f); // #61afef
        style.Colors[ImGuiCol_PlotHistogram]            = ImVec4(0.5961f, 0.7647f, 0.4745f, 1.0000f); // #98c379
        style.Colors[ImGuiCol_PlotHistogramHovered]     = ImVec4(0.3804f, 0.6863f, 0.9373f, 1.0000f); // #61afef
        style.Colors[ImGuiCol_TableHeaderBg]            = ImVec4(0.1294f, 0.1451f, 0.1686f, 1.0000f); // #21252b
        style.Colors[ImGuiCol_TableBorderStrong]        = ImVec4(0.2431f, 0.2667f, 0.3216f, 1.0000f); // #3e4452
        style.Colors[ImGuiCol_TableBorderLight]         = ImVec4(0.3608f, 0.3882f, 0.4392f, 0.5000f); // #5c6370 + alpha
        style.Colors[ImGuiCol_TableRowBg]               = ImVec4(0.1569f, 0.1725f, 0.2039f, 1.0000f); // #282c34
        style.Colors[ImGuiCol_TableRowBgAlt]            = ImVec4(0.1725f, 0.1922f, 0.2353f, 1.0000f); // #2c313c
        style.Colors[ImGuiCol_TextSelectedBg]           = ImVec4(0.4039f, 0.4627f, 0.5882f, 0.3765f); // #67769660
        style.Colors[ImGuiCol_DragDropTarget]           = ImVec4(0.3804f, 0.6863f, 0.9373f, 1.0000f); // #61afef
        style.Colors[ImGuiCol_NavHighlight]             = ImVec4(0.3020f, 0.4706f, 0.8000f, 1.0000f); // #4d78cc
        style.Colors[ImGuiCol_NavWindowingHighlight]    = ImVec4(0.8431f, 0.8549f, 0.8784f, 1.0000f); // #d7dae0
        style.Colors[ImGuiCol_NavWindowingDimBg]        = ImVec4(0.1569f, 0.1725f, 0.2039f, 0.7500f); // #282c34 + alpha
        style.Colors[ImGuiCol_ModalWindowDimBg]         = ImVec4(0.1569f, 0.1725f, 0.2039f, 0.7500f); // #282c34 + alpha
    }
}

static void ImNotepadRenderMenuBar()
{
    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("New"))
                ImNotepadNewFile();
            ImGui::MenuItem("Open"); // TODO: Add File Browser/Dialog like as ImFileDialog or ImGuiFileDialog
            ImGui::Separator();
            if(ImGui::MenuItem("Save"))
                ImNotepadFileSave(GLastActiveFile);
            ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Settings"))
        {
            ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

            ImGui::Checkbox("Render Cursor Line",           &gn.Settings.RenderCursorLine);
            ImGui::Checkbox("Render Whitespaces",           &gn.Settings.RenderWhitespaces);
            ImGui::Checkbox("Render Trailing Whitespaces",  &gn.Settings.RenderTrailingWhitespaces);
            ImGui::Checkbox("Token/Scopes Inspector",       &gn.Settings.ShowTokenAndScopesInspector);

            ImGui::Separator();

            static int ui_theme_idx = 0;
            const char* ui_theme_names[] =
            {
                "One Dark Pro",
                "Soft Cherry",
                "Dark",
                "Light",
                "Classic"
            };

            if(ImGui::BeginCombo("Application UI Theme", ui_theme_names[ui_theme_idx]))
            {
                for(int idx = 0; idx < IM_COUNTOF(ui_theme_names); ++idx)
                {
                    if(ImGui::Selectable(ui_theme_names[idx], ui_theme_idx == idx, ImGuiSelectableFlags_SelectOnNav))
                    {
                        ui_theme_idx = idx;
                        switch(ui_theme_idx)
                        {
                            case 0: ImGui::StyleColorsOneDarkPro(); break;
                            case 1: ImGui::StyleColorsSoftCherry(); break;
                            case 2: ImGui::StyleColorsDark(); break;
                            case 3: ImGui::StyleColorsLight(); break;
                            case 4: ImGui::StyleColorsClassic(); break;
                        }
                    }
                    else if(ui_theme_idx == idx)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            static int editor_color_sheme_idx = 0;
            const char* editor_color_sheme_names[] =
            {
                "Catppuccin Frappe (Neovim)",
                "One Dark Pro | Onedark (Neovim)",
                "One Dark Pro | Onelight (Neovim)",
                "One Dark Pro | Dark (Neovim)",
                "Vim Code Dark (Neovim)",
                "One Dark Pro (Visual Studio)"
            };

            if(ImGui::BeginCombo("Editor Color Scheme", editor_color_sheme_names[editor_color_sheme_idx]))
            {
                for(int idx = 0; idx < IM_COUNTOF(editor_color_sheme_names); ++idx)
                {
                    if(ImGui::Selectable(editor_color_sheme_names[idx], editor_color_sheme_idx == idx, ImGuiSelectableFlags_SelectOnNav))
                    {
                        editor_color_sheme_idx = idx;

                        editor_color_sheme_idx = idx;
                        switch(editor_color_sheme_idx)
                        {
                            case 0: ImNotepad::StyleColorsCatppuccinFrappe(); break;
                            case 1: ImNotepad::StyleColorsOnedarkProOnedark(); break;
                            case 2: ImNotepad::StyleColorsOnedarkProOnelight(); break;
                            case 3: ImNotepad::StyleColorsOnedarkProDark(); break;
                            case 4: ImNotepad::StyleColorsVimCodeDark(); break;
                            case 5: ImNotepad::StyleColorsOnedarkProVisualStudio(); break;
                        }

                        for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end(); ++it)
                        {
                            ImNotepadTextFile* file = *it;
                            if(file == nullptr || !file->IsOpened)
                                continue;

                            ImNotepadTreeSitter& ts = file->TreeSitter;
                            ts.Highlights.clear();
                            ts.ScratchIsValid = false;
                        }
                    }
                    else if(editor_color_sheme_idx == idx)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

static void ImNotepadRenderStatusBar()
{
    ImGuiContext& g = *GImGui;
    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();

    // Notify of viewport change so GetFrameHeight() can be accurate in case of DPI change
    ImGui::SetCurrentViewport(NULL, viewport);

    g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    if(ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight(), window_flags))
    {
        g.NextWindowData.MenuBarOffsetMinVal = ImVec2(0.0f, 0.0f);

        if(ImGui::BeginMenuBar())
        {
            ImGuiIO& io = ImGui::GetIO();

            char text_buf[128];
            memset(text_buf, 0, sizeof(text_buf));

            ImFormatString(text_buf, IM_COUNTOF(text_buf), "%.1f ms/frame (%.1f fps)", 1000.0f / io.Framerate, io.Framerate);
            ImVec2 text_size = ImGui::CalcTextSize(text_buf);

            float indent_w = ImGui::GetContentRegionMax().x - text_size.x - ImGui::GetStyle().FramePadding.x * 2.0f;

            ImGui::Indent(indent_w);
            ImGui::Text(text_buf);
            ImGui::Unindent(indent_w);

            ImGui::EndMenuBar();
        }

        ImGuiContext& g = *GImGui;
        if(g.CurrentWindow == g.NavWindow && g.NavLayer == ImGuiNavLayer_Main && !g.NavAnyRequest)
            ImGui::FocusTopMostWindowUnderOne(g.NavWindow, NULL, NULL, ImGuiFocusRequestFlags_RestoreFocusedChild);

        ImGui::End();
    }
}

static void ImNotepadDockNodeWindowMenuHandler(ImGuiContext* ctx, ImGuiDockNode* node, ImGuiTabBar* tab_bar)
{
    IM_UNUSED(ctx);

    for(int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
    {
        ImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
        if(tab->Flags & ImGuiTabItemFlags_Button)
            continue;
        if(ImGui::Selectable(ImGui::TabBarGetTabName(tab_bar, tab), tab->ID == tab_bar->SelectedTabId))
            ImGui::TabBarQueueFocus(tab_bar, tab);
        ImGui::SameLine();
        ImGui::Text("   ");
    }
}

void ImNotepad::ShowDemoWindow()
{
    static bool stylecolors_apply_default = false;
    if(!stylecolors_apply_default)
    {
        ImGui::StyleColorsOneDarkPro();
        stylecolors_apply_default = true;
    }

    ImGuiContext& g = *ImGui::GetCurrentContext();
    g.DockNodeWindowMenuHandler = &ImNotepadDockNodeWindowMenuHandler;

    ImGuiIO& io = ImGui::GetIO();
    bool config_windows_move_from_title_bar_only_old = io.ConfigWindowsMoveFromTitleBarOnly;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImNotepadRenderMenuBar();
    ImNotepadRenderStatusBar();

    ImFileTreeContext& gft = *ImFileTree::GetCurrentContext();
    gft.FileOpenCallback = ImNotepadFileOpen;
    gft.FileCloseCallback = ImNotepadFileClose;
    gft.FileRenameCallback = ImNotepadFileRename;

    ImGuiID dockspace_id = ImGui::GetID("##ImNotepadDockSpace");
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    if(ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_id_left = 0;
        ImGuiID dock_id_main = dockspace_id;
        ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, &dock_id_left, &dock_id_main);

        ImGui::DockBuilderDockWindow("Workspace", dock_id_left);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    ImNotepadContext& gn = *ImNotepad::GetCurrentContext();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    bool last_active_document_is_valid = false;

    // Render each file as a separate window
    for(ImNotepadTextFile** it = gn.FileSystem.TextFiles.begin(); it != gn.FileSystem.TextFiles.end();)
    {
        ImNotepadTextFile* file = *it;
        if(file == nullptr)
            continue;

        if(file->IsOpened)
        {
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

            ImNotepad::RenderFileContent(file);
            if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                GLastActiveFile = file;

            if(IsMouseHoveringDockTabForWindow(gn.RootWindow, ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
                ImGui::SetTooltip("%s", !file->FullPathName.empty() ? file->FullPathName.c_str() : file->Name.c_str());
                ImGui::PopStyleVar();
            }

            ++it;
        }
        else
        {
            IM_DELETE(file);
            it = gn.FileSystem.TextFiles.erase(it);
        }

        if(GLastActiveFile == file)
            last_active_document_is_valid = true;
    }

    if(!last_active_document_is_valid)
        GLastActiveFile = nullptr;

    ImGui::PopStyleVar(2);

    static bool show_workspace_panel = true;
    if(show_workspace_panel)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));

        if(ImGui::Begin("Workspace", &show_workspace_panel, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
            ImFileTree::Render(1.0f);
        ImGui::End();

        ImGui::PopStyleVar();
    }

    io.ConfigWindowsMoveFromTitleBarOnly = config_windows_move_from_title_bar_only_old;
}
#else
void ImNotepad::ShowDemoWindow() {}
#endif
#endif // #ifndef IMGUI_DISABLE
