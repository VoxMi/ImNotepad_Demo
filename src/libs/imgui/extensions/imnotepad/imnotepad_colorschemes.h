#pragma once

#ifndef IMGUI_DISABLE
#include <imgui/imgui.h>

#include <stdint.h>

#define SYNTAX_COLORMAP_SIZE_MAX  90

struct ImNotepadStyleColors
{
    ImU32 BackgroundColor;
    ImU32 GutterLineNumber;
    ImU32 GutterLineNumberCurrent;
};

struct ImNotepadSyntaxColorMap
{
    uint32_t    NameHash;
    ImU32       Value;
};

struct ImNotepadStyleColorScheme
{
    const char*                 Name;
    ImNotepadStyleColors*       StyleColors;
    ImNotepadSyntaxColorMap*    SyntaxColorMap;
};

ImU32 ImNotepadGetSyntaxHighlightColor(ImNotepadSyntaxColorMap* color_map, const char* name, size_t name_len);
#endif // IMGUI_DISABLE
