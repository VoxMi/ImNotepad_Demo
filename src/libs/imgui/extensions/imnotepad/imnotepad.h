#pragma once
#include "imgui.h"

#ifndef IMGUI_DISABLE

#include "imnotepad_colorschemes.h"

#ifdef _WIN32
#define IM_NOTEPAD_FONTS_DIR        ".\\resources\\fonts\\"
#define IM_NOTEPAD_GRAMMARS_DIR     ".\\resources\\grammars\\"
#define IM_NOTEPAD_QUERIES_DIR      ".\\resources\\queries\\"
#else
#define IM_NOTEPAD_FONTS_DIR        "./resources/fonts/"
#define IM_NOTEPAD_GRAMMARS_DIR     "./resources/grammars/"
#define IM_NOTEPAD_QUERIES_DIR      "./resources/queries/"
#endif

enum ImNotepadCol
{
    ImNotepadCol_WindowBg,
    ImNotepadCol_TextDefaultFg,
    ImNotepadCol_TextSelectedBg,
    ImNotepadCol_LineNumber,            // Line number in the line numbering area (gutter)
    ImNotepadCol_CursorLineNumber,      // Highlighting of the line number corresponding to the current screen-line in the line numbering area (gutter)
    ImNotepadCol_CursorLine,            // Screen-line at the cursor
    ImNotepadCol_Whitespace,
    ImNotepadCol_TrailingWhitespace,
    ImNotepadCol_COUNT,
};

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadStyle
//-----------------------------------------------------------------------------

struct ImNotepadStyle
{
    ImFont*                     FontText        = nullptr;
    float                       FontTextSize;
    float                       Alpha;
    float                       ContentPadding;
    ImVec4                      Colors[ImNotepadCol_COUNT];
    ImNotepadSyntaxColorMap     SyntaxColorMap[SYNTAX_COLORMAP_SIZE_MAX];

    ImNotepadStyle();

    bool ValidateFont(ImFont* font);
    void SetFontText(ImFont* font);
};

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadSettings
//-----------------------------------------------------------------------------

struct ImNotepadSettings
{
    unsigned char   IndentSize                  = 4;
    bool            IndentWithSpaces            = true;
    bool            RenderCursorLine            = true;
    bool            RenderWhitespaces           = true;
    bool            RenderTrailingWhitespaces   = true;
    bool            ShowTokenAndScopesInspector = true;
    bool            ScrollBeyondLastLine        = true;
};

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadTextFileConfig
//-----------------------------------------------------------------------------

struct ImNotepadTextFileConfig
{
    bool    IsReadOnly          = false;
    // Sets the cursor position and scroll when opening a file
    size_t  CursorLineIdx       = 0;
    size_t  CursorColumnIdx     = 0;
    float   ScrollX             = 0.0f;
    float   ScrollY             = 0.0f;
};

struct ImNotepadContext;

//-----------------------------------------------------------------------------
// [SECTION] End-user API Functions
//-----------------------------------------------------------------------------

namespace ImNotepad
{
    ImNotepadContext*       CreateContext();
    void                    DestroyContext(ImNotepadContext* ctx = nullptr);
    ImNotepadContext*       GetCurrentContext();
    void                    SetCurrentContext(ImNotepadContext* ctx);

    bool                    TextFileRead(const char* file_name, ImNotepadTextFileConfig* file_cfg = nullptr);

    ImNotepadStyle&         GetStyle();
    ImNotepadSettings&      GetSettings();

    ImU32                   GetColorU32(ImGuiCol idx, float alpha_mul = 1.0f);

    void                    StyleColorsDefault(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsCatppuccinFrappe(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsOnedarkProOnedark(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsOnedarkProOnelight(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsOnedarkProDark(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsVimCodeDark(ImNotepadStyle* dst = nullptr);
    void                    StyleColorsOnedarkProVisualStudio(ImNotepadStyle* dst = nullptr);

    void                    ShowDemoWindow();
}
#endif // #ifndef IMGUI_DISABLE
