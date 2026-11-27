#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imnotepad.h"

#ifndef IMGUI_DISABLE
#include "imnotepad_internal.h"

#include "misc/hash_utils.h"
#include "misc/os_dynlib.h"
#include "misc/os_filesystem.h"
#include "misc/path_utils.h"

#include <sys/stat.h>

#if !defined(IMGUI_USE_WCHAR32)
#error "To use the full set of icons from Nerd Fonts, you need to uncomment the '#define IMGUI_USE_WCHAR32' in the 'imconfig.h' file."
#endif

//-----------------------------------------------------------------------------
// [SECTION] Macros, Defines and etc
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define DIR_SEPARATOR   "\\"
#define DYN_LIB_EXT     ".dll"
#elif defined(__APPLE__)
#define DIR_SEPARATOR   "/"
#define DYN_LIB_EXT     ".dylib"
#else
#define DIR_SEPARATOR   "/"
#define DYN_LIB_EXT     ".so"
#endif

inline int64_t ImAbs(int64_t x) { return x < 0 ? -x : x; }

#ifndef GImNotepad
ImNotepadContext* GImNotepad = nullptr;
#endif

// Defines the mapping of file extensions to loaded languages. Edit manually to customize for your needs.
static const ImNotepadLanguageDescriptor ts_language_registry[] =
{
//   DisplayName       Name (id)         Extensions
    { "Daslang",    "daslang",      "das" },
    { "GLSL",       "glsl",         "glsl" },
    { "C++",        "cpp",          "c cc cpp cxx hpp hxx hh h" }
};

//-----------------------------------------------------------------------------
// [SECTION] Styling and Settings
//-----------------------------------------------------------------------------

ImNotepadStyle::ImNotepadStyle()
{
    FontTextSize    = 18.0f;
    Alpha           = 1.0f;
    ContentPadding  = 5.0f;

    ImGuiContext& g = *GImGui;
    FontText = g.IO.FontDefault;
    if(FontText == nullptr)
        FontText = !g.IO.Fonts->Fonts.empty() ? g.IO.Fonts->Fonts[0] : nullptr;

    if(FontText == nullptr)
        FontText = g.IO.Fonts->AddFontDefaultVector(nullptr);

    ImNotepad::StyleColorsDefault(this);
}

bool ImNotepadStyle::ValidateFont(ImFont* font)
{
    if(font == nullptr)
        return false;

    ImFontBaked* baked = font->GetFontBaked(font->LegacySize);
    if(baked == nullptr)
        return false;

    ImFontGlyph* glyph = baked->FindGlyphNoFallback((ImWchar)' ');
    if(glyph == nullptr)
        return false;

    float advance_x = glyph->AdvanceX;

    for(char c = 'A'; c <= 'z'; ++c)
    {
        glyph = baked->FindGlyphNoFallback((ImWchar)c);
        if(glyph == nullptr)
            continue;

        if(advance_x == glyph->AdvanceX)
        {
            advance_x = glyph->AdvanceX;
            continue;
        }

        return false;
    }

    return true;
}

void ImNotepadStyle::SetFontText(ImFont* font)
{
    if(!ValidateFont(font))
    {
        printf("[ImNotepad] The font '%s' used for text rendering is invalid or is not a MONOSPACED font!\n", font->GetDebugName());
        return;
    }

    FontText = font;
    FontTextSize = font->LegacySize;

    ImNotepadContext& gn = *GImNotepad;
    gn.MonospaceCharWidth = gn.Style.FontText->CalcTextSizeA(gn.Style.FontTextSize, FLT_MAX, 0.0f, " ", " " + 1).x;
}

ImNotepadStyle& ImNotepad::GetStyle()
{
    IM_ASSERT(GImNotepad != nullptr && "No current context. Did you call ImNotepad::CreateContext() and ImNotepad::SetCurrentContext() ?");
    return GImNotepad->Style;
}

void ImNotepad::StyleColorsDefault(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    ImNotepad::StyleColorsOnedarkProVisualStudio(dst);
}

ImU32 ImNotepad::GetColorU32(ImGuiCol idx, float alpha_mul)
{
    ImNotepadStyle& style = GImNotepad->Style;
    ImVec4 c = style.Colors[idx];
    c.w *= style.Alpha * alpha_mul;
    return ImGui::ColorConvertFloat4ToU32(c);
}

ImNotepadSettings& ImNotepad::GetSettings()
{
    return GImNotepad->Settings;
}

//-----------------------------------------------------------------------------
// [SECTION] Misc Helpers/Utilites
//-----------------------------------------------------------------------------

// Returns the number of UTF-8 bytes required to encode a Unicode code point (1-4).
// Note: expects a decoded character value, not a raw UTF-8 byte. Falls back to 3 for invalid inputs.
static size_t ImTextCountUtf8BytesFromChar(char c)
{
    unsigned char uc = (unsigned char)c;
    if(uc < 0x80) return 1;
    if((uc & 0xE0) == 0xC0) return 2;
    if((uc & 0xF0) == 0xE0) return 3;
    if((uc & 0xF8) == 0xF0) return 4;
    return 1;
}

// Return number of UTF-8 code-points, including the tab length (NOT bytes count)
static size_t ImTextCountCharsFromUtf8Ex(const char* in_text, const char* in_text_end)
{
    ImNotepadContext& gn = *GImNotepad;

    size_t char_count = 0;
    while((!in_text_end || in_text < in_text_end) && *in_text)
    {
        unsigned int c;
        in_text += ImTextCharFromUtf8(&c, in_text, in_text_end);
        if(c == '\r' || c == '\n')
            break;
        else if(c == '\t')
            char_count += gn.Settings.IndentSize;
        else
            char_count++;
    }

    return char_count;
}

static size_t ImStringCountCharsFromUtf8(const ImString& string_buf)
{
    return ImTextCountCharsFromUtf8Ex(string_buf.begin(), string_buf.end());
}

static size_t ImTextBufferGetLineColumnCount(ImNotepadTextBuffer* text_buf, size_t line_idx)
{
    ImNotepadContext& gn = *GImNotepad;
    ImString& line_text = gn.TextBuffer;

    text_buf->GetLineContent(&line_text, line_idx);
    if(line_text.empty())
        return 1;

    return ImStringCountCharsFromUtf8(line_text) + 1; // ImNotepad uses 1-based indexing for rows and columns
}

// Returns true if the character is a word boundary (whitespace, punctuation, or common separators)
static bool ImCharIsWordBoundary(char c)
{
    static bool is_init = false;
    static bool boundary_table[256];

    if(!is_init)
    {
        memset(boundary_table, 0, sizeof(boundary_table));
        const char boundaries[] = " .,;%!(){}[]+-=*/:<>\"`|^~?@#'\\\r\n\t";
        for(char b : boundaries)
            boundary_table[b] = true;
        is_init = true;
    }

    return boundary_table[(unsigned char)(c)];
}

// Returns the number of decimal digits in 'n'.
size_t ImCountDigitsFromNum(size_t n)
{
    if(n == 0)
        return 1;

    size_t count = 0;

    while(n > 0)
    {
        n /= 10;
        count++;
    }

    return count;
}

// Adjusts tab glyph advance for correct ImFont::CalcTextSizeA measurements with custom tab size
static void ImFontSetTabGlyphAdvanceX(ImFont* font, int tab_size)
{
    ImFontBaked* baked = font->GetFontBaked(font->LegacySize);
    ImFontGlyph* space_glyph = baked->FindGlyphNoFallback((ImWchar)' ');
    ImFontGlyph* tab_glyph = baked->FindGlyphNoFallback((ImWchar)'\t');

    if(space_glyph != nullptr && tab_glyph != nullptr)
    {
        tab_glyph->AdvanceX = space_glyph->AdvanceX * tab_size;
        if((int)'\t' < baked->IndexAdvanceX.Size)
            baked->IndexAdvanceX.Data['\t'] = tab_glyph->AdvanceX;
    }
}

static inline float ImCalcTextWidth(const char* in_text, const char* in_text_end)
{
    ImNotepadContext& gn = *GImNotepad;
    return ImTextCountCharsFromUtf8Ex(in_text, in_text_end) * gn.MonospaceCharWidth;
}

static inline float ImCalcTextWidth(const char* in_text)
{
    ImNotepadContext& gn = *GImNotepad;
    return ImTextCountCharsFromUtf8Ex(in_text, in_text + strlen(in_text)) * gn.MonospaceCharWidth;
}

static inline float ImCalcTextWidth(const char* in_text, size_t in_text_len)
{
    ImNotepadContext& gn = *GImNotepad;
    return ImTextCountCharsFromUtf8Ex(in_text, in_text + in_text_len) * gn.MonospaceCharWidth;
}

static inline float ImCalcTextWidth(const ImString& in_text)
{
    ImNotepadContext& gn = *GImNotepad;
    return ImTextCountCharsFromUtf8Ex(in_text.begin(), in_text.end()) * gn.MonospaceCharWidth;
}

static inline float ImCalcTextWidth(const ImStringView& in_text)
{
    ImNotepadContext& gn = *GImNotepad;
    return ImTextCountCharsFromUtf8Ex(in_text.begin(), in_text.end()) * gn.MonospaceCharWidth;
}

static inline ImU32 ImColorWithAlpha(ImU32 color, float alpha_mul = 1.0f)
{
    ImU32 old_alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
    ImU32 new_alpha = (ImU32)((float)old_alpha * alpha_mul);
    return (color & ~IM_COL32_A_MASK) | (new_alpha << IM_COL32_A_SHIFT);
}

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadTextBuffer
//-----------------------------------------------------------------------------

// Finds the start and end byte indices of the logical token (word, whitespace block, punctuation sequence, or line break)
FredBuf::Range ImNotepadTextBuffer::GetWordBoundariesByByteIndex(size_t offset)
{
    FredBuf::Range char_range;
    const size_t text_content_len = PieceTree.length();
    if(offset > text_content_len)
        offset = text_content_len;
    if(text_content_len == 0)
        return char_range;

    ImNotepadContext& gn = *GImNotepad;
    ImNotepadLine& line = gn.TextLineBuffer;
    GetLineContent(&line, GetLineIndexAt(offset));

    const size_t line_global_start = line.Range.first;
    size_t local_offset = offset - line_global_start;

    const char* text = line.Text.c_str();
    const size_t text_len = line.Text.size();

    // If the click is outside the string, truncate to the last character
    if(local_offset >= text_len)
    {
        if(text_len == 0)
        {
            char_range.first = offset;
            char_range.last = offset;
            return char_range;
        }

        local_offset = text_len - 1;
        offset = line_global_start + local_offset;
    }

    char c = text[local_offset];

    // If the click is on '\r' or '\n', check if there is content before it on the same line.
    // If there is content, then shift the offset to the last character of the content so that the selection logic is triggered.
    // If there is another line break (empty line) before it, then select only the line break character.
    if(c == '\r' || c == '\n')
    {
        size_t lb_local = local_offset;
        if(c == '\n' && lb_local > 0 && text[lb_local - 1] == '\r')
            lb_local--;
        if(lb_local > 0)
        {
            char prev = text[lb_local - 1];
            if(prev != '\r' && prev != '\n')
            {
                local_offset = lb_local - 1;
                offset = line_global_start + local_offset;
                c = text[local_offset];
            }
            else
            {
                char_range.first = offset;
                char_range.last = offset;
                return char_range;
            }
        }
        else
        {
            char_range.first = offset;
            char_range.last = offset;
            return char_range;
        }
    }

    size_t local_first = local_offset;
    size_t local_last = local_offset;

    // Expand selection to cover the entire contiguous block of whitespace: spaces, tabs, etc.
    if(ImCharIsBlankA(c))
    {
        while(local_first > 0 && ImCharIsBlankA(text[local_first - 1]))
            local_first--;
        local_last = local_offset + 1;
        while(local_last < text_len && ImCharIsBlankA(text[local_last]))
            local_last++;
    }
    // Expand to cover contiguous sequences of boundary characters: Punctuation/Special symbols (e.g., '-', '{', '=', ';', etc.)
    else if(ImCharIsWordBoundary(c))
    {
        // Stops at line breaks, whitespace, or alphanumeric chars to prevent merging separate logical blocks across lines or words (e.g., "{{}}" vs "----\n++++").
        while(local_first > 0)
        {
            char prev = text[local_first - 1];
            if(prev == '\r' || prev == '\n' || ImCharIsBlankA(prev) || !ImCharIsWordBoundary(prev))
                break;
            local_first--;
        }
        local_last = local_offset + 1;
        while(local_last < text_len)
        {
            char next = text[local_last];
            if(next == '\r' || next == '\n' || ImCharIsBlankA(next) || !ImCharIsWordBoundary(next))
                break;
            local_last++;
        }
    }
    // Alphanumeric/Standard word characters
    else
    {
        // Expand until a boundary character, whitespace, or line break is encountered
        while(local_first > 0 && !ImCharIsWordBoundary(text[local_first - 1]))
            local_first--;
        local_last = local_offset + 1;
        while(local_last < text_len && !ImCharIsWordBoundary(text[local_last]))
            local_last++;
    }

    // Converting local indices back to global indices
    char_range.first = line_global_start + local_first;
    char_range.last = line_global_start + local_last;

    // Ensure output indices never exceed the visible text range
    if(char_range.first > text_content_len)
        char_range.first = text_content_len;
    if(char_range.last > text_content_len)
        char_range.last = text_content_len;

    return char_range;
}

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadTextFile
//-----------------------------------------------------------------------------

void ImNotepadTextFile::SetCursorPosition(size_t line_idx, size_t column_idx, ImNotepadCursorAlignmentFlags alignment_flags)
{
    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;

    column_idx = ImMax((size_t)1, column_idx);
    Cursor.LineIdx = ImClamp(line_idx, (size_t)1, Buffer.GetLineCount());

    ImString& line_text = gn.TextBuffer;
    Buffer.GetLineContent(&line_text, Cursor.LineIdx);

    FredBuf::Range line_range = Buffer.GetLineRange(Cursor.LineIdx);

    size_t col_idx = 1;
    size_t char_idx = 0;
    size_t byte_idx = line_range.first;
    float char_width_accum = 0.0f;
    while(char_idx < line_text.size())
    {
        if(char_idx > 0 && line_text[char_idx - 1] == '\t')
            col_idx += (gn.Settings.IndentSize - 1);

        if(line_text[char_idx] == '\0' || col_idx >= column_idx)
            break;

        size_t char_len = ImTextCountUtf8BytesFromChar(line_text[char_idx]);
        float char_width = ImCalcTextWidth(line_text.begin() + char_idx, char_len);
        char_width_accum += char_width;

        byte_idx += char_len;
        char_idx += char_len;
        col_idx += 1;
    }

    Cursor.ByteIdx = byte_idx;
    Cursor.ColumnIdx = col_idx;
    Cursor.BlinkingPos.x = char_width_accum;
    Cursor.BlinkingPos.y = (Cursor.LineIdx - 1) * gn.LineHeight;

    UpdateCursorScrollIntoView(alignment_flags);
}

void ImNotepadTextFile::SetCursorPositionByByteIdx(size_t byte_idx)
{
    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;

    Cursor.ByteIdx = ImClamp(byte_idx, (size_t)0, Buffer.GetSize());
    Cursor.LineIdx = Buffer.GetLineIndexAt(Cursor.ByteIdx);

    ImNotepadLine& line = gn.TextLineBuffer;
    Buffer.GetLineContent(&line, Cursor.LineIdx);

    size_t col_idx = 1;
    size_t char_idx = 0;
    float char_width_accum = 0.0f;
    while(char_idx < line.Text.size())
    {
        if(line.Range.first + char_idx >= Cursor.ByteIdx)
            break;

        size_t char_len = ImTextCountUtf8BytesFromChar(line.Text[char_idx]);
        float char_width = ImCalcTextWidth(line.Text.begin() + char_idx, char_len);
        char_width_accum += char_width;

        if(line.Text[char_idx] == '\t')
            col_idx += gn.Settings.IndentSize;
        else
            col_idx += 1;

        char_idx += char_len;
    }

    Cursor.ColumnIdx = col_idx;
    Cursor.BlinkingPos.x = char_width_accum;
    Cursor.BlinkingPos.y = (Cursor.LineIdx - 1) * gn.LineHeight;
}

void ImNotepadTextFile::UpdateCursorScrollPos()
{
    ImNotepadContext& gn = *GImNotepad;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    canvas_pos.x += gn.Style.ContentPadding;

    ImVec2 cursor_pos(canvas_pos.x + GutterWidth + Cursor.BlinkingPos.x,
                      canvas_pos.y + Cursor.BlinkingPos.y);

    const float scroll_step_x = gn.MonospaceCharWidth;
    const ImS64 scroll_step_y = (ImS64)gn.LineHeight;

    if(cursor_pos.x - scroll_step_x < ContentBB.Min.x)
        ImGui::SetScrollX(ImGui::GetScrollX() - (ContentBB.Min.x - cursor_pos.x) - scroll_step_x);

    if(cursor_pos.x + scroll_step_x >= ContentBB.Max.x - ImGui::GetStyle().ScrollbarSize)
        ImGui::SetScrollX(ImGui::GetScrollX() + (cursor_pos.x - ContentBB.Max.x) + scroll_step_x + ImGui::GetStyle().ScrollbarSize);

    if(cursor_pos.y < ContentBB.Min.y)
    {
        ImS64 scroll_y = (ImS64)(ImGui::GetScrollY() - (ContentBB.Min.y - cursor_pos.y));
        ImGui::SetScrollY((float)((scroll_y / scroll_step_y) * scroll_step_y)); // Normalizing new 'Y' value by line height
    }

    if(cursor_pos.y + gn.LineHeight > ContentBB.Max.y)
    {
        ImS64 scroll_y = (ImS64)(ImGui::GetScrollY() + (cursor_pos.y + gn.LineHeight * 2.0f - ContentBB.Max.y));
        ImGui::SetScrollY((float)((scroll_y / scroll_step_y) * scroll_step_y));
    }
}

void ImNotepadTextFile::UpdateCursorScrollIntoView(ImNotepadCursorAlignmentFlags alignment_flags)
{
    if(alignment_flags == ImNotepadCursorAlignmentFlags_None)
        return;

    ImGuiContext& g = *GImGui;
    ImNotepadContext& gn = *GImNotepad;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    canvas_pos.x += gn.Style.ContentPadding;

    ImVec2 cursor_pos(canvas_pos.x + GutterWidth + Cursor.BlinkingPos.x,
                      canvas_pos.y + Cursor.BlinkingPos.y);

    cursor_pos.x = ImCeil(cursor_pos.x);

    ImVec2 scroll(0.0f, 0.0f);

    const float scroll_step_x = gn.MonospaceCharWidth;
    const ImS64 scroll_step_y = (ImS64)gn.LineHeight;

    if(alignment_flags & ImNotepadCursorAlignmentFlags_AlignLeft)
        scroll.x = gn.ChildWindow->Scroll.x - (ContentBB.Min.x - cursor_pos.x) - scroll_step_x;
    if(alignment_flags & ImNotepadCursorAlignmentFlags_AlignRight)
        scroll.x = gn.ChildWindow->Scroll.x + (cursor_pos.x - ContentBB.Max.x) + scroll_step_x + g.Style.ScrollbarSize;
    if(alignment_flags & ImNotepadCursorAlignmentFlags_AlignTop)
        scroll.y = cursor_pos.y - canvas_pos.y;
    if(alignment_flags & ImNotepadCursorAlignmentFlags_AlignBottom)
        scroll.y = ImMax(gn.ChildWindow->Scroll.y + (cursor_pos.y - ContentBB.Max.y) + gn.LineHeight * 3.0f, 0.0f); // 3.0f defines the number of lines for indentation at the bottom edge
    if(alignment_flags & ImNotepadCursorAlignmentFlags_AlignCenter)
    {
        scroll.x = cursor_pos.x - canvas_pos.x - gn.ChildWindow->Size.x * 0.5f;
        scroll.y = cursor_pos.y - canvas_pos.y - gn.ChildWindow->Size.y * 0.5f;
    }

    ImGui::SetScrollX(gn.ChildWindow, scroll.x);
    ImGui::SetScrollY(gn.ChildWindow, (float)(((ImS64)scroll.y / scroll_step_y) * scroll_step_y)); // Normalizing new 'Y' value by line height
}

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadLanguage
//-----------------------------------------------------------------------------

ImNotepadLanguage::~ImNotepadLanguage()
{
    if(Query != nullptr)
    {
        ts_query_delete(Query);
        Query = nullptr;
    }

    if(DynLibHandle != nullptr)
    {
        dynlib_unload(DynLibHandle);
        DynLibHandle = (void*)0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadTreeSitter
//-----------------------------------------------------------------------------

ImNotepadTreeSitter::~ImNotepadTreeSitter()
{
    if(SyntaxTree)
    {
        ts_tree_delete(SyntaxTree);
        SyntaxTree = nullptr;
    }

    if(Parser)
    {
        ts_parser_delete(Parser);
        Parser = nullptr;
    };
}

// Adapter callback for Tree-sitter to read text chunks from the buffer
const char* ImNotepadTreeSitter::ReadTextChunk(void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read)
{
    TSInputPayload* input_payload = (TSInputPayload*)payload;

    size_t total_len = input_payload->PieceTree->length();
    if(byte_index >= total_len)
    {
        *bytes_read = 0;
        return nullptr;
    }

    size_t max_read = ImMin(total_len - byte_index, input_payload->ReadBufSize);
    size_t idx = 0;

    FredBuf::TreeWalker walker(input_payload->PieceTree, byte_index);
    while(idx < max_read && !walker.exhausted())
        input_payload->ReadBuf[idx++] = walker.next();

    *bytes_read = (uint32_t)idx;
    return input_payload->ReadBuf;
}

// Compares the text of a specific capture against a string literal
bool ImNotepadTreeSitter::CompareCaptureWithLiteral(const TSQueryMatch& match, uint32_t capture_id, FredBuf::Tree* piece_tree, const char* literal, size_t literal_len)
{
    for(uint16_t i = 0; i < match.capture_count; ++i)
    {
        if(match.captures[i].index != capture_id)
            continue;

        const TSNode node = match.captures[i].node;
        uint32_t node_start_byte = ts_node_start_byte(node);
        uint32_t node_end_byte = ts_node_end_byte(node);
        uint32_t node_len = node_end_byte - node_start_byte;

        if(node_len != literal_len)
            return false;

        // Byte-by-byte comparison directly against the PieceTree
        FredBuf::TreeWalker walker(piece_tree, node_start_byte);
        for(size_t j = 0; j < literal_len && !walker.exhausted(); ++j)
        {
            if(walker.next() != literal[j])
                return false;
        }

        return true;
    }

    return false;
}

// Compares two captures byte-for-byte. Unlike the literal variant, we cannot use a fast length-only check against a known string.
bool ImNotepadTreeSitter::CompareCaptures(const TSQueryMatch& match, uint32_t lhs_capture_id, uint32_t rhs_capture_id, FredBuf::Tree* piece_tree)
{
    const TSQueryCapture* lhs = nullptr;
    const TSQueryCapture* rhs = nullptr;

    for(uint16_t i = 0; i < match.capture_count; ++i)
    {
        if(match.captures[i].index == lhs_capture_id)
            lhs = &match.captures[i];
        if(match.captures[i].index == rhs_capture_id)
            rhs = &match.captures[i];
        if(lhs && rhs)
            break;
    }

    if(lhs == nullptr || rhs == nullptr)
        return false;

    const uint32_t lhs_node_start_byte = ts_node_start_byte(lhs->node);
    const uint32_t lhs_node_end_byte = ts_node_end_byte(lhs->node);
    const uint32_t rhs_node_start_byte = ts_node_start_byte(rhs->node);
    const uint32_t rhs_node_end_byte = ts_node_end_byte(rhs->node);

    const size_t lhs_node_len = (size_t)(lhs_node_end_byte - lhs_node_start_byte);
    const size_t rhs_node_len = (size_t)(rhs_node_end_byte - rhs_node_start_byte);

    if(lhs_node_len != rhs_node_len)
        return false;
    if(lhs_node_len == 0)
        return true;

    FredBuf::TreeWalker lhs_walker(piece_tree, lhs_node_start_byte);
    FredBuf::TreeWalker rhs_walker(piece_tree, rhs_node_start_byte);

    for(size_t i = 0; i < lhs_node_len; ++i)
    {
        if(lhs_walker.next() != rhs_walker.next())
            return false;
    }

    return true;
}

// Parses a predicate step into a structured argument (string or capture ID)
TSQueryPredicateArg ImNotepadTreeSitter::QueryPredicateArgFromStep(TSQuery* query, const TSQueryPredicateStep& step)
{
    TSQueryPredicateArg arg;
    arg.Type = step.type;

    if(step.type == TSQueryPredicateStepTypeCapture)
        arg.CaptureID = step.value_id;
    else if(step.type == TSQueryPredicateStepTypeString)
    {
        uint32_t len;
        const char* str = ts_query_string_value_for_id(query, step.value_id, &len);
        arg.StringVal = ImString(str, len);
    }

    return arg;
}

/*
 Universal predicate handler

 Evaluates whether a query match satisfies all predicate conditions ('#eq?', '#any-of?', etc.)

 Note:
        Predicates are not handled directly by the Tree-sitter C library. They are just
        exposed in a structured form so that higher-level code can perform the filtering.
        However, higher-level bindings to Tree-sitter like the Rust Crate or the WebAssembly
        binding do implement a few common predicates like the #eq?, #match?, and #any-of? predicates explained above.
        Source: https://web.archive.org/web/20241212104722/https://tree-sitter.github.io/tree-sitter/using-parsers/#predicates

 TODO: Add support for many other predicates (e.g. '#match?' and '#set!')
 */
bool ImNotepadTreeSitter::QueryEvaluatePredicates(const TSQueryParsedPredicate* predicates, const TSQueryPredicateArg* cached_args, uint32_t predicate_count, const TSQueryMatch& match, FredBuf::Tree* piece_tree)
{
    for(uint32_t i = 0; i < predicate_count; ++i)
    {
        bool result = true;
        const TSQueryParsedPredicate& predicate = predicates[i];
        // Get a pointer to the start of the current predicate's arguments in the flat array
        // TODO: Consider passing indices (StartIndex/Count) instead of raw pointers or add a strict guard to ensure the cache remains immutable during query execution.
        const TSQueryPredicateArg* args = cached_args + predicate.ArgsStartIndex;

        switch(predicate.NameHash)
        {
            case CONSTEXPR_HASH_FNV1A_32("eq?"):
            case CONSTEXPR_HASH_FNV1A_32("not-eq?"):
            {
                if(predicate.ArgsCount < 2 || args[0].Type != TSQueryPredicateStepTypeCapture)
                {
                    result = false;
                    break;
                }

                if(args[1].Type == TSQueryPredicateStepTypeString)
                    result = CompareCaptureWithLiteral(match, args[0].CaptureID, piece_tree, args[1].StringVal.c_str(), args[1].StringVal.size());
                else if(args[1].Type == TSQueryPredicateStepTypeCapture)
                    result = CompareCaptures(match, args[0].CaptureID, args[1].CaptureID, piece_tree);
                else
                    result = false;

                if(predicate.NameHash == CONSTEXPR_HASH_FNV1A_32("not-eq?"))
                    result = !result;
                break;
            }

            case CONSTEXPR_HASH_FNV1A_32("any-of?"):
            {
                if(predicate.ArgsCount < 2 || args[0].Type != TSQueryPredicateStepTypeCapture)
                {
                    result = false;
                    break;
                }

                result = false;

                for(uint32_t arg_idx = 1; arg_idx < predicate.ArgsCount; arg_idx++)
                {
                    if(args[arg_idx].Type != TSQueryPredicateStepTypeString)
                        continue;

                    if(CompareCaptureWithLiteral(match, args[0].CaptureID, piece_tree, args[arg_idx].StringVal.data(), args[arg_idx].StringVal.size()))
                        result = true;
                }

                break;
            }

            // TODO: Add support for regular expressions (mb PCRE2)
            case CONSTEXPR_HASH_FNV1A_32("match?"):
            {
                result = false;
                break;
            }

            // Unknown predicates (e.g., #set!) should not break filtering
            default:
                result = true;
                break;
        }

        if(!result)
            return false;
    }

    return true;
}

// Pre-parses and caches query predicates for efficient match filtering
void ImNotepadTreeSitter::CacheQueryPredicates(ImNotepadTreeSitter& ts, TSQuery* query)
{
    if(ts.QueryCache.Query == query)
        return;

    ts.QueryCache.Query = query;
    ts.QueryCache.Predicates.clear();
    ts.QueryCache.PredicateArgs.clear();
    ts.QueryCache.PatternRanges.clear();

    uint32_t pattern_count = ts_query_pattern_count(query);
    ts.QueryCache.PatternRanges.resize(pattern_count);

    for(uint32_t pattern_idx = 0; pattern_idx < pattern_count; pattern_idx++)
    {
        TSQueryPredicateRange& range = ts.QueryCache.PatternRanges[pattern_idx];
        range.StartIndex = ts.QueryCache.Predicates.size();
        range.PredicatesCount = 0;

        uint32_t step_count = 0;
        const TSQueryPredicateStep* steps = ts_query_predicates_for_pattern(query, pattern_idx, &step_count);
        if(step_count == 0)
            continue;

        TSQueryParsedPredicate current;
        current.ArgsStartIndex = ts.QueryCache.PredicateArgs.size();
        bool parsing = false;

        for(uint32_t step_idx = 0; step_idx < step_count; step_idx++)
        {
            const TSQueryPredicateStep& step = steps[step_idx];

            if(step.type == TSQueryPredicateStepTypeDone)
            {
                if(parsing)
                {
                    ts.QueryCache.Predicates.push_back(current);
                    range.PredicatesCount++;
                    parsing = false;
                }

                current.NameHash = 0;
                current.ArgsCount = 0;
                current.ArgsStartIndex = (uint32_t)ts.QueryCache.PredicateArgs.size();
                continue;
            }

            if(!parsing)
            {
                if(step.type == TSQueryPredicateStepTypeString)
                {
                    uint32_t len;
                    current.Name = ts_query_string_value_for_id(query, step.value_id, &len);
                    current.NameHash = Hash::fnv1a_32(current.Name, len);
                    parsing = true;
                }
            }
            else
            {
                TSQueryPredicateArg arg = ts.QueryPredicateArgFromStep(query, step);
                ts.QueryCache.PredicateArgs.push_back(arg);
                current.ArgsCount++;
            }
        }
    }
}

// Checks if a language supports the given file extension
bool ImNotepadTreeSitter::ResolveLanguageForExtension(const char* language, const char* extension)
{
    if(language == nullptr || extension == nullptr)
        return false;

    ImString extensions;
    for(const ImNotepadLanguageDescriptor& desc : ts_language_registry)
    {
        if(strcmp(desc.Name, language) == 0)
        {
            extensions = desc.Extensions;
            break;
        }
    }

    if(extensions.empty())
        return false;

    size_t ext_len = strlen(extension);
    ImString lower_ext;
    lower_ext.reserve(ext_len + 1);
    for(size_t i = 0; i <= ext_len; ++i)
        lower_ext.push_back((char)tolower((unsigned char)extension[i]));

    char* token = strtok(extensions.data(), ", ");
    while(token != nullptr)
    {
        if(strcmp(token, lower_ext.c_str()) == 0)
            return true;
        token = strtok(nullptr, ", ");
    }

    return false;
}

void ImNotepadTreeSitter::TreeEdit(size_t start_byte, size_t old_len, size_t new_len)
{
    if(SyntaxTree == nullptr || TextBuffer == nullptr)
        return;

    // Converts byte offset to Tree-sitter point (row, column)
    auto ts_offset_to_point = [](ImNotepadTextBuffer* text_buffer, size_t byte_offset) -> TSPoint
    {
        FredBuf::Position pos = text_buffer->GetPositionAt(byte_offset);
        return { (uint32_t)(pos.line - 1), (uint32_t)(pos.column - 1) }; // Tree-sitter uses 0-based indexing for rows and columns
    };

    TSInputEdit edit;
    edit.start_byte = (uint32_t)start_byte;
    edit.old_end_byte = (uint32_t)(start_byte + old_len);
    edit.new_end_byte = (uint32_t)(start_byte + new_len);
    edit.start_point = ts_offset_to_point(TextBuffer, start_byte);
    edit.old_end_point = ts_offset_to_point(TextBuffer, start_byte + old_len);
    edit.new_end_point = ts_offset_to_point(TextBuffer, edit.new_end_byte);
    ts_tree_edit(SyntaxTree, &edit);

    // The tree and cache is outdated
    IsDirty = true;
    ScratchIsValid = false;
}

void ImNotepadTokenScopeInspector::QueryCaptures(ImNotepadTextFile* file)
{
    ImNotepadTreeSitter& ts = file->TreeSitter;

    if(ts.SyntaxTree == nullptr || file->Language == nullptr)
        return;

    if(StartByte == 0 && EndByte == 0)
        return;

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_set_byte_range(cursor, StartByte, EndByte);
    ts_query_cursor_exec(cursor, file->Language->Query, ts_tree_root_node(ts.SyntaxTree));

    TSQueryMatch match;
    uint32_t capture_index = 0;

    while(ts_query_cursor_next_capture(cursor, &match, &capture_index))
    {
        const TSQueryPredicateRange& predicate_range = ts.QueryCache.PatternRanges[match.pattern_index];
        if(predicate_range.PredicatesCount)
        {
            const TSQueryParsedPredicate* pred_ptr = ts.QueryCache.Predicates.Data + predicate_range.StartIndex;
            if(!ts.QueryEvaluatePredicates(pred_ptr, ts.QueryCache.PredicateArgs.Data, predicate_range.PredicatesCount, match, &file->Buffer.PieceTree))
                continue;
        }

        const TSQueryCapture* capture = &match.captures[capture_index];

        uint32_t capture_name_len;
        ImNotepadTokenScopeInspector::CaptureInfo cap;
        cap.Name = ts_query_capture_name_for_id(file->Language->Query, capture->index, &capture_name_len);
        Captures.push_back(cap);
    }

    ts_query_cursor_delete(cursor);
}

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadFileSystem
//-----------------------------------------------------------------------------

static ImString GetFileFullPath(const char* file_name)
{
    char* full_path_name = nullptr;
    char full_path_name_buf[PATH_MAX];
#ifdef _WIN32
    full_path_name = _fullpath(full_path_name_buf, file_name, PATH_MAX);
#else
    full_path_name = realpath(file_name, full_path_name_buf);
#endif
    if(full_path_name != nullptr)
        return full_path_name;
    else
        return "";
}

ImNotepadLanguage* ImNotepadFileSystem::TextFileGetLanguage(ImNotepadTextFile* file)
{
    ImNotepadLanguage* result = nullptr;

    const char* file_ext = path_get_file_name_ext(file->Name.c_str(), true);
    if(file_ext != nullptr)
    {
        for(ImNotepadLanguage* language : Languages)
        {
            const char* lang = language->Name.c_str(); // the 'ts_language_name()' function is available on ABI version >= 15
            if(lang && file->TreeSitter.ResolveLanguageForExtension(lang, file_ext))
            {
                result = language;
                break;
            }
        }
    }

    // Plain text
    if(result == nullptr)
    {
        for(ImNotepadLanguage* language : Languages)
        {
            if(language->Grammar == nullptr && language->Query == nullptr)
                result = language;
        }
    }

    return result;
}

ImNotepadTextFile* ImNotepadFileSystem::NewFile(const char* file_name)
{
    if(file_name == nullptr)
        return nullptr;

    ImNotepadTextFile* file = IM_NEW(ImNotepadTextFile);
    file->Name = path_get_file_name(file_name);
    file->IsOpened = true;
    file->Language = TextFileGetLanguage(file);
    file->TreeSitter.IsDirty = true;
    file->TreeSitter.ScratchStartByte = false;

    TextFiles.push_back(file);

    return file;
}

ImNotepadTextFile* ImNotepadFileSystem::TextFileRead(const char* file_name, ImNotepadTextFileConfig* file_cfg)
{
    if(file_name == nullptr || !*file_name)
        return nullptr;

    ImString full_path_name = GetFileFullPath(file_name);
    if(full_path_name.empty())
        return nullptr;

    // For mimetype detection we can use libmagic or https://github.com/file/file/blob/master/src/ascmagic.c)
    ImFileHandle file_handle;
    if((file_handle = ImFileOpen(full_path_name.c_str(), "rb")) == nullptr)
        return nullptr;

    struct stat st;
#ifdef _WIN32
    int fd = _fileno(file_handle);
#else
    int fd = fileno(file_handle);
#endif
    if(fstat(fd, &st) != 0 || st.st_size < 0)
    {
        ImFileClose(file_handle);
        return nullptr;
    }

    void* file_data = IM_ALLOC(st.st_size + sizeof(char));
    if(file_data == nullptr)
    {
        ImFileClose(file_handle);
        return nullptr;
    }

    memset(file_data, 0, st.st_size + sizeof(char));

    if(ImFileRead(file_data, 1, st.st_size, file_handle) != st.st_size)
    {
        ImFileClose(file_handle);
        IM_FREE(file_data);
        return nullptr;
    }

    ImFileClose(file_handle);

    ImNotepadTextFile* file = IM_NEW(ImNotepadTextFile);
    file->Name = path_get_file_name(full_path_name.c_str());
    file->FullPathName = full_path_name;
    file->IsOpened = true;
    file->LoadContentFromMemory((char*)file_data, st.st_size);
    IM_FREE(file_data);

    if(file_cfg != nullptr)
        file->Config = *file_cfg;

    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;
    ImString& line_text = gn.TextBuffer;

    // Calculate the maximum content width based on the largest line.
    file->Buffer.GetLineContent(&line_text, file->Buffer.GetWidestLineIndex(gn.Settings.IndentSize));
    file->LineMaxWidth = ImCalcTextWidth(line_text);

    ImFontSetTabGlyphAdvanceX(gn.Style.FontText, gn.Settings.IndentSize);

    file->Language = TextFileGetLanguage(file);
    // Forcing a full parse of the loaded text
    file->TreeSitter.IsDirty = true;
    file->TreeSitter.ScratchIsValid = false;

    TextFiles.push_back(file);

    return file;
}

bool ImNotepadFileSystem::TextFileWrite(ImNotepadTextFile* file)
{
    ImString& full_path_name = file->FullPathName;
    if(full_path_name.empty())
        return false;

    bool eol_is_cr = file->Buffer.GetEndOfLineSequence() == FredBuf::EndOfLineSequence_CR;
    bool eol_is_crlf = file->Buffer.GetEndOfLineSequence() == FredBuf::EndOfLineSequence_CRLF;

    size_t file_data_size = file->Buffer.GetSize() + (eol_is_crlf ? file->Buffer.GetLineCount() - 1 : 0);
    if(file_data_size == 0)
        return false;

    char* file_data = (char*)IM_ALLOC(file_data_size);
    if(file_data == nullptr)
        return false;

    size_t idx = 0;
    FredBuf::TreeWalker walker(&file->Buffer.PieceTree, 0);
    while(idx < file_data_size && !walker.exhausted())
    {
        char c = walker.next();
        if(c == '\n')
        {
            if(eol_is_cr)
                file_data[idx++] = '\r';
            else if(eol_is_crlf)
            {
                file_data[idx++] = '\r';
                file_data[idx++] = '\n';
            }
            else
                file_data[idx++] = '\n';
        }
        else
            file_data[idx++] = c;
    }

    ImFileHandle f = ImFileOpen(full_path_name.c_str(), "wb");
    if(f == nullptr)
    {
        IM_FREE(file_data);
        return false;
    }

    size_t written_size = ImFileWrite(file_data, sizeof(char), file_data_size, f);
    ImFileClose(f);

    IM_FREE(file_data);

    return (written_size == file_data_size);
}

void ImNotepadFileSystem::LoadLanguages()
{
    ImNotepadLanguage* language = IM_NEW(ImNotepadLanguage);
    language->DisplayName = "Plain text";
    Languages.push_back(language);

    ImString grammars_dir_name = IM_NOTEPAD_GRAMMARS_DIR;
    DIR* grammars_dir = fs_opendir(grammars_dir_name.c_str());
    if(grammars_dir == nullptr)
        return;

    ImString queries_dir_name = IM_NOTEPAD_QUERIES_DIR;
    DIR* queries_dir = fs_opendir(queries_dir_name.c_str());
    if(queries_dir == nullptr)
    {
        fs_closedir(grammars_dir);
        return;
    }

    dirent* ent;
    while((ent = fs_readdir(grammars_dir)) != nullptr)
    {
        if(ent->d_type != DT_REG)
            continue;

        const char* dynlib_file_name = ent->d_name;

        const char* file_name_ext = path_get_file_name_ext(dynlib_file_name, false);
        if(file_name_ext == nullptr || strcmp(file_name_ext, DYN_LIB_EXT) != 0)
            continue;

        ImString dynlib_file_path = grammars_dir_name + dynlib_file_name;
        void* dyn_lib_handle = dynlib_load(dynlib_file_path.c_str());
        if(dyn_lib_handle == nullptr)
            continue;

        char* file_name_stem = path_get_file_name_stem(dynlib_file_name);
        if(file_name_stem == nullptr)
        {
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        ImString lang_name(file_name_stem);
        free(file_name_stem);

        ImString exported_func_name = (ImString)"tree_sitter_" + lang_name;

        typedef const TSLanguage* (*TSLanguageFunc)(void);
        TSLanguageFunc exported_func = (TSLanguageFunc)dynlib_get_symbol(dyn_lib_handle, exported_func_name.c_str());
        if(exported_func == nullptr)
        {
            printf("[Tree-sitter] Exported function '%s' not found for language grammar `%s`\n", exported_func_name.c_str(), dynlib_file_name);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        const TSLanguage* lang_grammar = exported_func();
        if(lang_grammar == nullptr)
        {
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        uint32_t abi_version = ts_language_abi_version(lang_grammar);

        if(abi_version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION || abi_version > TREE_SITTER_LANGUAGE_VERSION)
        {
            printf("[Tree-sitter] ABI version mismatch for language grammar '%s'.\n"
                   "              The language grammar was generated with ABI version %u, but the\n"
                   "              installed runtime supports ABI versions >= %u and <= %u.\n"
                   "              Regenerate the language grammar with the required ABI version:\n"
                   "                  tree-sitter generate src/grammar.json --abi=15\n"
                   "              Then rebuild the language grammar and try again.\n\n",
                   dynlib_file_name, abi_version,
                   TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION, TREE_SITTER_LANGUAGE_VERSION);

            dynlib_unload(dyn_lib_handle);
            continue;
        }

        const TSLanguageMetadata* lang_metadata = ts_language_metadata(lang_grammar);
        if(lang_metadata != nullptr)
            printf("[Tree-sitter] Exported language grammar: `%s` version: %u.%u.%u ABI version: %u\n", lang_name.c_str(), lang_metadata->major_version, lang_metadata->minor_version, lang_metadata->patch_version, abi_version);
        else
            printf("[Tree-sitter] Exported language grammar: `%s` ABI version: %u\n", lang_name.c_str(), abi_version);

        ImString query_highlights_file_path = queries_dir_name + lang_name + DIR_SEPARATOR + "highlights.scm";

        ImFileHandle file_handle = ImFileOpen(query_highlights_file_path.c_str(), "rb");
        if(file_handle == nullptr)
        {
            printf("[Tree-sitter] Query highlights file not found for language grammar `%s`\n", dynlib_file_name);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        struct stat st;
#ifdef _WIN32
        int fd = _fileno(file_handle);
#else
        int fd = fileno(file_handle);
#endif
        if(fstat(fd, &st) != 0 || st.st_size < 0)
        {
            ImFileClose(file_handle);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        void* query_highlights_file_data = IM_ALLOC(st.st_size + sizeof(char));
        if(query_highlights_file_data == nullptr)
        {
            ImFileClose(file_handle);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        memset(query_highlights_file_data, 0, st.st_size + sizeof(char));

        auto sz = ImFileRead(query_highlights_file_data, 1, st.st_size, file_handle);
        if(sz != st.st_size)
        {
            ImFileClose(file_handle);
            IM_FREE(query_highlights_file_data);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        ImFileClose(file_handle);

        uint32_t error_offset;
        TSQueryError error_type;
        TSQuery* query = ts_query_new(lang_grammar, (const char*)query_highlights_file_data, (uint32_t)st.st_size, &error_offset, &error_type);
        if(query == nullptr)
        {
            printf("[Tree-sitter] Error at %u with type %u in '%s'\n", error_offset, error_type, query_highlights_file_path.c_str());
            IM_FREE(query_highlights_file_data);
            dynlib_unload(dyn_lib_handle);
            continue;
        }

        IM_FREE(query_highlights_file_data);

        language = IM_NEW(ImNotepadLanguage);
        language->DynLibHandle = dyn_lib_handle;
        language->Grammar = lang_grammar;
        language->Query = query;
        language->Name = lang_name;

        for(const ImNotepadLanguageDescriptor& desc : ts_language_registry)
        {
            if(strcmp(desc.Name, lang_name.c_str()) == 0)
            {
                language->DisplayName = desc.DisplayName;
                break;
            }
        }

        if(language->DisplayName.empty())
            language->DisplayName = lang_name;

        Languages.push_back(language);
    }

    fs_closedir(grammars_dir);
    fs_closedir(queries_dir);

    // Sort alphabetically
    for(int i = 1; i < Languages.Size; ++i)
    {
        ImNotepadLanguage* lang = Languages[i];
        const char* str1 = Languages[i]->DisplayName.c_str();

        int j = i - 1;
        while(j >= 0)
        {
            const char* str2 = Languages[j]->DisplayName.c_str();
            if(str1 && str2 && ImStricmp(str1, str2) > 0)
            {
                Languages[j + 1] = Languages[j];
                j--;
            }
            else
                break;
        }

        Languages[j + 1] = lang;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

ImNotepadContext* ImNotepad::CreateContext()
{
    ImNotepadContext* ctx = IM_NEW(ImNotepadContext)();
    if(GImNotepad == nullptr)
        SetCurrentContext(ctx);
    Initialize(ctx);
    return ctx;
}

void ImNotepad::DestroyContext(ImNotepadContext* ctx)
{
    if(ctx == nullptr)
        ctx = GetCurrentContext();
    if(GImNotepad == ctx)
        SetCurrentContext(nullptr);
    IM_DELETE(ctx);
}

ImNotepadContext* ImNotepad::GetCurrentContext()
{
    return GImNotepad;
}

void ImNotepad::SetCurrentContext(ImNotepadContext* ctx)
{
    GImNotepad = ctx;
}

bool ImNotepad::TextFileRead(const char* file_name, ImNotepadTextFileConfig* file_cfg)
{
    IM_ASSERT(GImNotepad != nullptr && "No current context. Did you call ImNotepad::CreateContext() and ImNotepad::SetCurrentContext() ?");
    return GImNotepad->FileSystem.TextFileRead(file_name, file_cfg) != nullptr;
}

//-----------------------------------------------------------------------------
// [SECTION] Internal API
//-----------------------------------------------------------------------------

void ImNotepad::Initialize(ImNotepadContext* ctx)
{
    ImGuiContext& g = *GImGui;

    if(ctx->Style.FontText != nullptr)
    {
        if(!ctx->Style.ValidateFont(ctx->Style.FontText))
        {
            ImFontConfig font_cfg;
            font_cfg.SizePixels = ctx->Style.FontTextSize;
            ImFont* font_default = g.IO.Fonts->AddFontDefaultVector(&font_cfg);
            printf("[ImNotepad] The font '%s' used for text rendering is invalid or is not a MONOSPACED font! Falling back to the embedded ImGui font '%s'.\n", ctx->Style.FontText->GetDebugName(), font_default->GetDebugName());
            ctx->Style.FontText = font_default;
        }

        ctx->MonospaceCharWidth = ctx->Style.FontText->CalcTextSizeA(ctx->Style.FontTextSize, FLT_MAX, 0.0f, " ", " " + 1).x;
    }
}

/*
 CONTROLS GUIDE
 ==============

 - MOUSE CONTROLS
   - Mouse wheel:                   Scroll vertically.
   - Shift + Mouse wheel:           Scroll horizontally.
   - Ctrl + Mouse wheel:           	Adjusts the zoom level of the text editor (font size).
   - Double click:                  Select single word.
   - Triple click:                  Select the entire line.
   - Multiple clicks:               Select All.

 - KEYBOARD CONTROLS
   - Ctrl + Up/Down:                Scrolling the page line by line.
   - Ctrl + Left/Right:             Word jump.
   - Ctrl+Shift + Left/Right:       Select words.
   - Ctrl+A:                        Select All.
   - Ctrl+C, Ctrl+X, Ctrl+V:        Copy, Cut and Paste text.
   - Ctrl+D:                        Duplicate selected word or entire line.
   - Ctrl+Z:                        Undo.
   - Ctrl+Y or Ctrl+Shift+Z:        Redo.
   - Shift + Home/End:              Select to line start/end.
   - Shift + Page Up/Down:          Select viewport text.
   - Shift + Up/Down arrow keys:    Select line up/down.
   - Page Up, Page Down:            Previous page, next page.
   - Home, End:                     Set cursor to line start/end.
   - ESCAPE:                        Deselect text.
*/

void ImNotepad::UpdateMouseWheel(ImNotepadTextFile* file)
{
    ImGuiIO& io = ImGui::GetIO();
    ImNotepadContext& gn = *GImNotepad;

    if(io.MouseWheel == 0.0f)
        return;

    bool is_ctrl_down = io.KeyCtrl;
    bool is_shift_down = io.KeyShift;

    // Font size zooming
    if(is_ctrl_down && io.MouseWheel != 0.0f)
    {
        float font_size_prev = gn.Style.FontTextSize;

        gn.Style.FontTextSize += io.MouseWheel;
        gn.Style.FontTextSize = ImClamp(gn.Style.FontTextSize, 8.0f, 100.0f);
        gn.LineHeight = gn.Style.FontTextSize;

        if(font_size_prev != gn.Style.FontTextSize)
        {
            gn.MonospaceCharWidth = gn.Style.FontText->CalcTextSizeA(gn.Style.FontTextSize, FLT_MAX, 0.0f, " ", " " + 1).x;

            // Recalculate the maximum content width based on the largest line
            ImString& line_text_buf = gn.TextBuffer;
            file->Buffer.GetLineContent(&line_text_buf, file->Buffer.GetWidestLineIndex(gn.Settings.IndentSize));
            file->LineMaxWidth = ImCalcTextWidth(line_text_buf); // FIXME: LineMaxWidth is recalculated only for the current file. Other open files will retain their previous width.

            float scale = gn.Style.FontTextSize / font_size_prev;

            float scroll_y_new = ImGui::GetScrollY() * scale;
            scroll_y_new = ImMax(0.0f, ImMin(scroll_y_new, ImGui::GetScrollMaxY()));
            ImGui::SetScrollY(scroll_y_new);

            float scroll_x_new = ImGui::GetScrollX() * scale;
            scroll_x_new = ImMax(0.0f, ImMin(scroll_x_new, ImGui::GetScrollMaxX()));
            ImGui::SetScrollX(scroll_x_new);

            file->SetCursorPosition(file->Cursor.LineIdx, file->Cursor.ColumnIdx);
        }

        return;
    }

    // Scrolling
    if(is_shift_down)
    {
        float scroll_step = gn.MonospaceCharWidth * 5.0f;
        ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseWheel * scroll_step);
    }
    else
    {
        float scroll_step = gn.LineHeight * 5.0f;
        ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseWheel * scroll_step);
    }
}

void ImNotepad::UpdateMouseInputs(ImNotepadTextFile* file)
{
    ImGuiIO& io = ImGui::GetIO();
    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;

    ImRect canvas_bb(file->ContentBB);

    if(ImGui::GetScrollMaxX() > 0.0f)
        canvas_bb.Max.y -= ImGui::GetStyle().ScrollbarSize;
    if(ImGui::GetScrollMaxY() > 0.0f)
        canvas_bb.Max.x -= ImGui::GetStyle().ScrollbarSize;

    gn.MouseState.InCanvas = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(canvas_bb.Min, canvas_bb.Max);

    if(gn.MouseState.InCanvas)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        UpdateMouseWheel(file);
    }

    bool is_ctrl_down = io.KeyCtrl;
    bool is_shift_down = io.KeyShift;

    bool is_left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool is_right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool is_any_clicked = is_left_clicked || is_right_clicked;

    bool is_left_down = io.MouseDown[ImGuiMouseButton_Left];
    bool is_right_down = io.MouseDown[ImGuiMouseButton_Right];
    bool is_any_down = is_left_down || is_right_down;

    // Drag = button is pressed, but NOT the first frame of the click
    bool is_left_dragging = is_left_down && !is_left_clicked;

    // Early return: if there are no pressed buttons, exit and reset the flag
    if(!is_any_down)
    {
        gn.MouseState.DragStartedInCanvas = false;
        gn.MouseState.ScrollAccX = 0.0f;
        gn.MouseState.ScrollAccY = 0.0f;
        return;
    }

    // If the cursor is OUTSIDE the canvas: process ONLY if the selection is active (drag continuation).
    // BUT: if this is a new click (not drag) outside the canvas, ignore.
    if(!gn.MouseState.InCanvas)
    {
        if(!file->HasSelection)
        {
            gn.MouseState.DragStartedInCanvas = false;
            return;
        }
        if(is_any_clicked)
        {
            gn.MouseState.DragStartedInCanvas = false;
            return;
        }
    }

    ImVec2 mouse_pos_clamped = io.MousePos;
    mouse_pos_clamped.x = ImClamp(mouse_pos_clamped.x, canvas_bb.Min.x, canvas_bb.Max.x);
    mouse_pos_clamped.y = ImClamp(mouse_pos_clamped.y, canvas_bb.Min.y, canvas_bb.Max.y);

    ImVec2 mouse_pos_relative;
    mouse_pos_relative.x = mouse_pos_clamped.x - canvas_bb.Min.x + ImGui::GetScrollX();
    mouse_pos_relative.y = mouse_pos_clamped.y - canvas_bb.Min.y + ImGui::GetScrollY();

    size_t hover_line = (size_t)(mouse_pos_relative.y / gn.LineHeight) + 1;
    hover_line = ImClamp(hover_line, (size_t)1, file->Buffer.GetLineCount());

    ImNotepadLine& line = gn.TextLineBuffer;
    file->Buffer.GetLineContent(&line, hover_line);

    size_t col_idx = 0;
    size_t char_idx = 0;
    size_t byte_idx = line.Range.first;
    float char_width_accum = 0.0f;

    while(char_idx < line.Text.size())
    {
        size_t char_len = ImTextCountUtf8BytesFromChar(line.Text[char_idx]);
        float char_width = ImCalcTextWidth(line.Text.begin() + char_idx, char_len);

        if(char_width_accum + char_width * 0.50f > mouse_pos_relative.x)
            break;

        if(line.Text[char_idx] == '\t')
            col_idx += gn.Settings.IndentSize;
        else
            col_idx += 1;

        char_width_accum += char_width;
        char_idx += char_len;
        byte_idx += char_len;
    }

    // Autoscroll
    if(!file->HasSelectionWord && is_left_dragging && (gn.MouseState.InCanvas || gn.MouseState.DragStartedInCanvas))
    {
        const float AUTO_SCROLL_ZONE_SLOW       = 30.0f; // Slow scrolling zone from the edge
        const float AUTO_SCROLL_ZONE_MEDIUM     = 50.0f; // Mid-scroll zone
        const float AUTO_SCROLL_RATE_SLOW       = 5.0f;  // in pixels/sec
        const float AUTO_SCROLL_RATE_MEDIUM     = 20.0f;
        const float AUTO_SCROLL_RATE_FAST       = 35.0f;

        const float scroll_step_y = gn.LineHeight;
        float scroll_step_x = gn.MonospaceCharWidth;
        float line_total_width = 0.0f;

        if(hover_line >= 0 && hover_line < file->Buffer.GetLineCount())
        {
            for(size_t ch_idx = 0; ch_idx < line.Text.size();)
            {
                size_t char_len = ImTextCountUtf8BytesFromChar(line.Text[ch_idx]);
                float char_width = ImCalcTextWidth(line.Text.begin() + ch_idx, char_len);

                line_total_width += char_width;
                ch_idx += char_len;
            }

            if(char_idx >= 1 && !line.Text.empty())
            {
                size_t char_len = ImTextCountUtf8BytesFromChar(line.Text[char_idx - 1]);
                float char_width = ImCalcTextWidth(line.Text.begin() + char_idx, char_len);
                scroll_step_x = char_width;
            }
        }

        for(int axis = 0; axis < 2; ++axis)
        {
            const float mouse_pos   = (axis == 0) ? io.MousePos.x : io.MousePos.y;
            const float canvas_min  = (axis == 0) ? canvas_bb.Min.x : canvas_bb.Min.y;
            const float canvas_max  = (axis == 0) ? canvas_bb.Max.x : canvas_bb.Max.y;
            const float scroll_size = (axis == 0) ? scroll_step_x : scroll_step_y;
            float& scroll_accum     = (axis == 0) ? gn.MouseState.ScrollAccX : gn.MouseState.ScrollAccY;

            if(mouse_pos < canvas_min || mouse_pos > canvas_max)
            {
                const float dist = (mouse_pos < canvas_min) ? (canvas_min - mouse_pos) : (mouse_pos - canvas_max);
                const float dir_polarity = (mouse_pos < canvas_min) ? -1.0f : 1.0f;
                const float scroll_rate = (dist <= AUTO_SCROLL_ZONE_SLOW) ? AUTO_SCROLL_RATE_SLOW : (dist <= AUTO_SCROLL_ZONE_MEDIUM) ? AUTO_SCROLL_RATE_MEDIUM : AUTO_SCROLL_RATE_FAST;

                // skip horizontal scroll to the right if at end of line
                if(axis == 0 && dir_polarity > 0.0f && line_total_width > 0.0f)
                {
                    float visible_width = canvas_bb.Max.x - canvas_bb.Min.x;
                    float max_scroll_x = ImMax(0.0f, line_total_width - visible_width);
                    if(ImGui::GetScrollX() >= max_scroll_x)
                        continue;
                }

                scroll_accum += scroll_rate * io.DeltaTime;

                if(scroll_accum >= 1.0f)
                {
                    float new_scroll = 0.0f;
                    const float delta = scroll_size * dir_polarity;

                    if(axis == 0)
                    {
                        new_scroll = ImClamp(ImGui::GetScrollX() + delta, 0.0f, ImGui::GetScrollMaxX());
                        if(new_scroll != ImGui::GetScrollX())
                            ImGui::SetScrollX(new_scroll);
                    }
                    else
                    {
                        new_scroll = ImClamp(ImGui::GetScrollY() + delta, 0.0f, ImGui::GetScrollMaxY());
                        if(new_scroll != ImGui::GetScrollY())
                            ImGui::SetScrollY(new_scroll);
                    }

                    scroll_accum = 0.0f;
                }
            }
        }
    }

    // Double/Triple click
    if(!is_shift_down && io.MouseClickedCount[0] >= 2 && gn.MouseState.InCanvas)
    {
        // Select word
        if(io.MouseClickedCount[0] == 2)
        {
            float line_range_min = canvas_bb.Min.x + ImGui::GetScrollX();
            float line_range_max = line_range_min + char_width_accum;
            if(byte_idx != line.Range.first && io.MousePos.x >= line_range_min && io.MousePos.x <= line_range_max)
                byte_idx = ImMax(byte_idx - 1, line.Range.first);

            FredBuf::Range word_range = file->Buffer.GetWordBoundariesByByteIndex(byte_idx);
            if(word_range.first != word_range.last)
            {
                file->SelectionStart = word_range.first;
                file->SelectionEnd = word_range.last;
                file->Cursor.ByteIdx = word_range.last;
                file->HasSelection = true;
                file->HasSelectionWord = true;
            }

            file->SetCursorPositionByByteIdx(file->Cursor.ByteIdx);
            file->Cursor.GoalColumnIdx = file->Cursor.ColumnIdx;
            file->UpdateCursorScrollPos();
            return;
        }
        // Select line
        else if(io.MouseClickedCount[0] == 3)
        {
            file->SelectionStart = line.Range.first;

            if(file->Cursor.LineIdx != file->Buffer.GetLineCount())
            {
                FredBuf::Range line_next_range = file->Buffer.GetLineRange(hover_line + 1);
                file->SelectionEnd = line_next_range.first;
                file->Cursor.ByteIdx = line_next_range.first;
            }
            else
            {
                file->SelectionEnd = line.Range.last;
                file->Cursor.ByteIdx = line.Range.last;
            }

            file->HasSelection = true;
            file->HasSelectionWord = true;
            file->Cursor.GoalColumnIdx = 1;

            file->SetCursorPositionByByteIdx(file->Cursor.ByteIdx);
            file->UpdateCursorScrollPos();
            return;
        }
        // Select all
        else
        {
            file->SelectionStart = 0;
            file->SelectionEnd = file->Buffer.GetSize();
            file->Cursor.ByteIdx = file->Buffer.GetSize();
            file->Cursor.LineIdx = file->Buffer.GetLineIndexAt(file->Cursor.ByteIdx);
            file->HasSelection = true;

            file->SetCursorPositionByByteIdx(file->Cursor.ByteIdx);
            return;
        }
    }

    // Single click
    if(is_any_clicked && gn.MouseState.InCanvas)
    {
        if(is_left_down)
            gn.MouseState.DragStartedInCanvas = true;

        if(!file->HasSelection)
        {
            if(is_shift_down)
            {
                file->SelectionStart = file->Cursor.ByteIdx;
                file->SelectionEnd = byte_idx;
            }
            else
            {
                file->SelectionStart = byte_idx;
                file->SelectionEnd = byte_idx;
                file->HasSelection = false;
            }
        }
        else
        {
            if(!is_shift_down)
            {
                file->SelectionStart = byte_idx;
                file->SelectionEnd = byte_idx;
                file->HasSelection = false;
            }
            else
                file->SelectionEnd = byte_idx;
        }

        file->HasSelection = (file->SelectionStart != file->SelectionEnd);
        file->Cursor.ByteIdx = byte_idx;
        file->Cursor.LineIdx = hover_line;
        file->Cursor.ColumnIdx = col_idx + 1;
        file->Cursor.GoalColumnIdx = col_idx + 1;
        file->Cursor.BlinkingPos = ImVec2(char_width_accum, (hover_line - 1) * gn.LineHeight);
        file->HasSelectionWord = false;
    }

    // Left-click drag
    if(!file->HasSelectionWord && is_left_dragging)
    {
        if(gn.MouseState.DragStartedInCanvas)
        {
            file->SelectionEnd = byte_idx;
            file->HasSelection = (file->SelectionStart != file->SelectionEnd);

            file->Cursor.ByteIdx = byte_idx;
            file->Cursor.LineIdx = hover_line;
            file->Cursor.ColumnIdx = col_idx + 1;
            if(file->SelectionStart < file->SelectionEnd)
                file->Cursor.GoalColumnIdx = col_idx;

            file->Cursor.BlinkingPos = ImVec2(char_width_accum, (hover_line - 1) * gn.LineHeight);
        }
    }

    if(!is_left_down)
    {
        gn.MouseState.DragStartedInCanvas = false;
        gn.MouseState.ScrollAccX = 0.0f;
        gn.MouseState.ScrollAccY = 0.0f;
    }
}

bool ImNotepad::UpdateKeyboardInputs(ImNotepadTextFile* file)
{
    ImGuiIO& io = ImGui::GetIO();
    ImNotepadContext& gn = *GImNotepad;
    ImNotepadSettings& settings = gn.Settings;

    ImFont* font = gn.Style.FontText;

    ImString& text_buf = gn.TextBuffer;
    size_t column_shift_size = 1;
    bool go_to_prev_line = false;
    bool go_to_next_line = false;

    ImNotepadCursorAlignmentFlags alignment_flags = ImNotepadCursorAlignmentFlags_None;

    size_t sel_start = ImMin(file->SelectionStart, file->SelectionEnd);
    size_t sel_end = ImMax(file->SelectionStart, file->SelectionEnd);
    size_t sel_size = sel_end - sel_start;

    // A lambda function to synchronize the cursor display with its current ByteIdx value
    auto commit_cursor_state = [&]()
    {
        file->Cursor.DisableBlinking = true;
        file->SetCursorPositionByByteIdx(file->Cursor.ByteIdx);
        file->Cursor.GoalColumnIdx = file->Cursor.ColumnIdx;
        file->UpdateCursorScrollPos();
    };

    // Regular text input
    if(io.InputQueueCharacters.Size > 0)
    {
        if(file->Config.IsReadOnly)
            return false;

        ImString buf;
        for(ImWchar c : io.InputQueueCharacters)
        {
            if(c > 0 && c != 127 && c != '\r' && c != '\n')
            {
                char utf8[5];
                int len = ImTextCharToUtf8(utf8, c);
                for(int j = 0; j < len; ++j)
                    buf.push_back(utf8[j]);
            }
        }

        io.InputQueueCharacters.resize(0);

        if(!buf.empty())
        {
            size_t insert_pos = file->Cursor.ByteIdx;
            size_t new_len = 0;

            if(file->HasSelection)
            {
                new_len = file->Buffer.Replace(sel_start, sel_size, buf.c_str(), file->SelectionStart, file->SelectionEnd);
                file->TreeSitter.TreeEdit(sel_start, sel_size, new_len);
                file->Cursor.ByteIdx = sel_start + new_len;
                file->HasSelection = false;
            }
            else
            {
                file->Buffer.Insert(insert_pos, buf.c_str());
                new_len = buf.size();
                file->TreeSitter.TreeEdit(insert_pos, 0, new_len);
                file->Cursor.ByteIdx += new_len;
            }

            commit_cursor_state();
            return true;
        }

        return true;
    }

    bool is_ctrl_down = io.KeyCtrl;
    bool is_shift_down = io.KeyShift;

    // Hotkey: Undo
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_Z))
    {
        FredBuf::UndoRedoResult result = is_shift_down ? file->Redo() : file->Undo();
        if(result.success)
        {
            file->TreeSitter.TreeEdit(result.op_offset, result.old_len, result.new_len);

            if(is_shift_down)
            {
                file->SelectionStart = 0;
                file->SelectionEnd = 0;
                file->HasSelection = false;
                file->Cursor.ByteIdx = result.op_offset + result.new_len;
            }
            else
            {
                size_t sel_start = ImMin(result.sel_start, result.sel_end);
                size_t sel_end = ImMax(result.sel_start, result.sel_end);
                if(sel_start != sel_end)
                {
                    file->SelectionStart = sel_start;
                    file->SelectionEnd = sel_end;
                    file->HasSelection = true;
                    file->Cursor.ByteIdx = sel_end;
                }
                else
                {
                    file->SelectionStart = 0;
                    file->SelectionEnd = 0;
                    file->HasSelection = false;
                    file->Cursor.ByteIdx = result.op_offset + result.new_len;
                }
            }

            commit_cursor_state();
            return true;
        }

        return false;
    }

    // Hotkey: Redo
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_Y))
    {
        FredBuf::UndoRedoResult result = file->Redo();
        if(result.success)
        {
            file->TreeSitter.TreeEdit(result.op_offset, result.old_len, result.new_len);

            size_t sel_start = ImMin(result.sel_start, result.sel_end);
            size_t sel_end = ImMax(result.sel_start, result.sel_end);

            file->SelectionStart = 0;
            file->SelectionEnd = 0;
            file->HasSelection = false;
            file->Cursor.ByteIdx = result.op_offset + result.new_len;

            commit_cursor_state();
            return true;
        }

        return false;
    }

    // Hotkey: Ctrl+A
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_A))
    {
        file->SelectionStart = 0;
        file->SelectionEnd = file->Buffer.GetSize();
        file->Cursor.ByteIdx = file->Buffer.GetSize();
        file->Cursor.LineIdx = file->Buffer.GetLineIndexAt(file->Cursor.ByteIdx);
        file->HasSelection = true;

        commit_cursor_state();
        return false;
    }

    // Key: Escape
    if(ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        file->HasSelection = false;
        return false;
    }

    // Save the previous cursor position for all selection operations
    file->Cursor.ByteIdxPrev = file->Cursor.ByteIdx;

    // Navigation
    if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        if(file->Cursor.ByteIdx == 0)
            return false;

        if(!is_shift_down && file->HasSelection)
        {
            file->Cursor.ByteIdx = sel_start;
            file->HasSelection = false;
            file->HasSelectionWord = false;
            commit_cursor_state();
            return false;
        }

        // The beginning of the line has been reached. Go to the end of the previous line.
        if(file->Cursor.ColumnIdx <= 1)
            go_to_prev_line = true;
        else
        {
            if(is_ctrl_down)
            {
                FredBuf::Range word_range = file->Buffer.GetWordBoundariesByByteIndex(file->Cursor.ByteIdx - 1);

                file->Buffer.GetTextInRange(&text_buf, word_range.first, file->Cursor.ByteIdx);
                if(!text_buf.empty())
                {
                    column_shift_size = ImTextCountCharsFromUtf8Ex(text_buf.begin(), text_buf.end());
                    file->Cursor.ColumnIdx -= column_shift_size;
                    file->Cursor.GoalColumnIdx -= column_shift_size;
                }
            }
            else
            {
                if(file->Buffer.GetCharAt(file->Cursor.ByteIdx - 1) == '\t')
                    column_shift_size = gn.Settings.IndentSize;

                file->Cursor.ColumnIdx -= column_shift_size;
                file->Cursor.GoalColumnIdx -= column_shift_size;
            }
        }

        if(go_to_prev_line)
        {
            size_t prev_line_idx = file->Cursor.LineIdx - 1;

            // Get visible length for preview line
            size_t prev_line_length = ImTextBufferGetLineColumnCount(&file->Buffer, prev_line_idx);

            file->Cursor.LineIdx = prev_line_idx;
            file->Cursor.ColumnIdx = prev_line_length;
            file->Cursor.GoalColumnIdx = prev_line_length;
        }

        file->Cursor.DisableBlinking = true;
        file->SetCursorPosition(file->Cursor.LineIdx, file->Cursor.ColumnIdx);
        file->UpdateCursorScrollPos();

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdx;
                file->HasSelection = true;
            }
            else
                file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else
            file->HasSelection = false;

        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        if(file->Cursor.ByteIdx >= file->Buffer.GetSize())
            return false;

        if(!is_shift_down && file->HasSelection)
        {
            file->Cursor.ByteIdx = sel_end;
            file->HasSelection = false;
            file->HasSelectionWord = false;
            commit_cursor_state();
            return false;
        }

        if(is_ctrl_down)
        {
            char c = file->Buffer.GetCharAt(file->Cursor.ByteIdx);

            // The end of the line has been reached. Go to the beginning of the next line.
            if(c == '\r' || c == '\n')
                go_to_next_line = true;
            else
            {
                FredBuf::Range word_range = file->Buffer.GetWordBoundariesByByteIndex(file->Cursor.ByteIdx);

                file->Buffer.GetTextInRange(&text_buf, file->Cursor.ByteIdx, word_range.last);
                if(!text_buf.empty())
                {
                    column_shift_size = ImTextCountCharsFromUtf8Ex(text_buf.begin(), text_buf.end());
                    file->Cursor.ColumnIdx += column_shift_size;
                    file->Cursor.GoalColumnIdx += column_shift_size;
                }

                file->Cursor.ByteIdx = word_range.last;
            }
        }
        else
        {
            char c = file->Buffer.GetCharAt(file->Cursor.ByteIdx);

            // The end of the line has been reached. Go to the beginning of the next line.
            if(c == '\r' || c == '\n')
                go_to_next_line = true;
            else
            {
                if(c == '\t')
                    column_shift_size = gn.Settings.IndentSize;

                file->Cursor.ColumnIdx += column_shift_size;
                file->Cursor.GoalColumnIdx += column_shift_size;
            }
        }

        if(go_to_next_line)
        {
            file->Cursor.LineIdx++;
            file->Cursor.ColumnIdx = 1;
            file->Cursor.GoalColumnIdx = 1;
        }

        file->Cursor.DisableBlinking = true;
        file->SetCursorPosition(file->Cursor.LineIdx, file->Cursor.ColumnIdx);
        file->UpdateCursorScrollPos();

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdx;
                file->HasSelection = true;
            }
            else
                file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else if(file->HasSelection)
        {
            file->HasSelection = false;
            if(!is_ctrl_down)
                return false;
        }

        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        // Scroll the page up one line
        if(is_ctrl_down)
        {
            const float scroll_y = ImGui::GetScrollY();
            if(scroll_y - gn.LineHeight >= 0.0f)
                ImGui::SetScrollY(scroll_y - gn.LineHeight);
            return false;
        }

        if(file->Cursor.LineIdx > 1)
        {
            file->Cursor.LineIdx -= 1;
            file->Cursor.ColumnIdx = file->Cursor.GoalColumnIdx;
        }
        else
        {
            file->Cursor.LineIdx = 1;
            file->Cursor.ColumnIdx = 1;
        }

        file->Cursor.DisableBlinking = true;
        file->SetCursorPosition(file->Cursor.LineIdx, file->Cursor.ColumnIdx);
        file->UpdateCursorScrollPos();

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdxPrev;
                file->HasSelection = true;
            }
            file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else
            file->HasSelection = false;

        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        // Scroll the page down one line
        if(is_ctrl_down)
        {
            const float scroll_y = ImGui::GetScrollY();
            if(scroll_y + gn.LineHeight <= ImGui::GetScrollMaxY())
                ImGui::SetScrollY(scroll_y + gn.LineHeight);
            return false;
        }

        file->Cursor.DisableBlinking = true;
        file->SetCursorPosition(file->Cursor.LineIdx + 1, file->Cursor.GoalColumnIdx);
        file->UpdateCursorScrollPos();

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdxPrev;
                file->HasSelection = true;
            }
            file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else
            file->HasSelection = false;

        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_Home))
    {
        if(is_ctrl_down && is_shift_down)
        {
            file->Cursor.ByteIdx = 0;

            if(!file->HasSelection)
                file->SelectionStart = file->Cursor.ByteIdxPrev;
            else
                file->SelectionStart = ImMax(file->SelectionStart, file->SelectionEnd);

            file->SelectionEnd = file->Cursor.ByteIdx;
            file->HasSelection = true;

            alignment_flags = ImNotepadCursorAlignmentFlags_AlignLeftTop;
        }
        else if(is_ctrl_down)
        {
            file->Cursor.ByteIdx = 0;
            file->HasSelection = false;
            alignment_flags = ImNotepadCursorAlignmentFlags_AlignLeftTop;
        }
        else
        {
            size_t first_char_idx = file->Cursor.CurrentLine.Range.first;
            while(first_char_idx < file->Cursor.CurrentLine.Range.last && ImCharIsBlankA(file->Buffer.PieceTree.at(first_char_idx)))
                first_char_idx++;

            // If the cursor is already at the first non-space character (and this is not the beginning of the line), go to the beginning of the line. Otherwise, go to the first non-space character.
            if(file->Cursor.ByteIdx == first_char_idx && first_char_idx > file->Cursor.CurrentLine.Range.first)
                file->Cursor.ByteIdx = file->Cursor.CurrentLine.Range.first;
            else
                file->Cursor.ByteIdx = first_char_idx;

            if(is_shift_down)
            {
                if(!file->HasSelection)
                {
                    file->SelectionStart = file->Cursor.ByteIdxPrev;
                    file->SelectionEnd = file->Cursor.ByteIdxPrev;
                    file->HasSelection = true;
                }
                file->SelectionEnd = file->Cursor.ByteIdx;
            }
            else
                file->HasSelection = false;
        }

        commit_cursor_state();
        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_End))
    {
        if(is_ctrl_down && is_shift_down)
        {
            file->Cursor.ByteIdx = file->Buffer.GetSize();

            if(!file->HasSelection)
                file->SelectionStart = file->Cursor.ByteIdxPrev;
            else
                file->SelectionStart = ImMin(file->SelectionStart, file->SelectionEnd);

            file->SelectionEnd = file->Cursor.ByteIdx;
            file->HasSelection = true;

            alignment_flags = ImNotepadCursorAlignmentFlags_AlignRightBottom;
        }
        else if(is_ctrl_down)
        {
            file->Cursor.ByteIdx = file->Buffer.GetSize();
            file->HasSelection = false;
            alignment_flags = ImNotepadCursorAlignmentFlags_AlignRightBottom;
        }
        else
        {
            file->Cursor.ByteIdx = file->Cursor.CurrentLine.Range.last;
            if(is_shift_down)
            {
                if(!file->HasSelection)
                {
                    file->SelectionStart = file->Cursor.ByteIdxPrev;
                    file->SelectionEnd = file->Cursor.ByteIdxPrev;
                    file->HasSelection = true;
                }
                file->SelectionEnd = file->Cursor.ByteIdx;
            }
            else
                file->HasSelection = false;
        }

        commit_cursor_state();
        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        size_t lines_per_page = (size_t)(ImGui::GetWindowHeight() / gn.LineHeight);
        if(lines_per_page)
            lines_per_page -= 2;

        size_t line_idx_new = file->Cursor.LineIdx > lines_per_page ? file->Cursor.LineIdx - lines_per_page : 1;

        FredBuf::Range line_range_cur = file->Buffer.GetLineRange(file->Cursor.LineIdx);
        size_t byte_idx_cur = file->Cursor.ByteIdx - line_range_cur.first;

        FredBuf::Range line_range_new = file->Buffer.GetLineRange(line_idx_new);
        size_t line_len_new = line_range_new.last - line_range_new.first;

        if(line_range_new.first + byte_idx_cur < line_range_new.last)
            file->Cursor.ByteIdx = line_range_new.first + byte_idx_cur;
        else
            file->Cursor.ByteIdx = line_range_new.last;

        size_t column_idx = file->Cursor.GoalColumnIdx;
        if(file->Cursor.LineIdx == line_idx_new || file->Cursor.LineIdx <= lines_per_page)
        {
            column_idx = 1;
            file->Cursor.ByteIdx = 0;
        }

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdxPrev;
                file->HasSelection = true;
            }
            file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else
            file->HasSelection = false;

        file->Cursor.DisableBlinking = true;

        ImNotepadCursorAlignmentFlags flags = ImNotepadCursorAlignmentFlags_None;
        if(line_idx_new <= file->RenderedLines.first)
            flags = ImNotepadCursorAlignmentFlags_AlignTop;

        file->SetCursorPosition(line_idx_new, column_idx, flags);

        return false;
    }
    else if(ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        size_t lines_per_page = (size_t)(ImGui::GetWindowHeight() / gn.LineHeight);
        if(lines_per_page)
            lines_per_page -= 2;

        FredBuf::Range line_range_cur = file->Buffer.GetLineRange(file->Cursor.LineIdx);
        size_t byte_idx_cur = file->Cursor.ByteIdx - line_range_cur.first;

        size_t line_idx_new = file->Cursor.LineIdx + lines_per_page <= file->Buffer.GetLineCount() ? file->Cursor.LineIdx + lines_per_page : file->Buffer.GetLineCount();
        FredBuf::Range line_range_new = file->Buffer.GetLineRange(line_idx_new);

        size_t line_len_new = line_range_new.last - line_range_new.first;
        if(line_range_new.first + byte_idx_cur < line_range_new.last)
            file->Cursor.ByteIdx = line_range_new.first + byte_idx_cur;
        else
            file->Cursor.ByteIdx = line_range_new.last;

        size_t column_idx = file->Cursor.GoalColumnIdx;

        if(line_idx_new == file->Buffer.GetLineCount() && line_idx_new - file->Cursor.LineIdx <= lines_per_page)
        {
            file->Cursor.ByteIdx = file->Buffer.GetSize();
            file->Buffer.GetLineContent(&text_buf, line_idx_new);
            column_idx = ImStringCountCharsFromUtf8(text_buf) + 1;
        }

        if(is_shift_down)
        {
            if(!file->HasSelection)
            {
                file->SelectionStart = file->Cursor.ByteIdxPrev;
                file->SelectionEnd = file->Cursor.ByteIdxPrev;
                file->HasSelection = true;
            }
            file->SelectionEnd = file->Cursor.ByteIdx;
        }
        else
            file->HasSelection = false;

        file->Cursor.DisableBlinking = true;

        ImNotepadCursorAlignmentFlags flags = ImNotepadCursorAlignmentFlags_None;
        if(line_idx_new >= file->RenderedLines.last && file->RenderedLines.last - file->RenderedLines.first >= lines_per_page)
            flags = ImNotepadCursorAlignmentFlags_AlignBottom;

        file->SetCursorPosition(line_idx_new, column_idx, flags);
        return false;
    }

    // Hotkey: Ctrl+C
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_C))
    {
        if(file->HasSelection)
        {
            file->Buffer.GetTextInRange(&text_buf, sel_start, sel_end);
            if(!text_buf.empty())
                ImGui::SetClipboardText(text_buf.c_str());
        }
        return false;
    }

    // Hotkey: Ctrl+X
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_X))
    {
        if(file->Config.IsReadOnly)
            return false;

        if(file->HasSelection)
        {
            file->Buffer.GetTextInRange(&text_buf, sel_start, sel_end);
            if(!text_buf.empty())
            {
                file->Buffer.Remove(sel_start, sel_size, file->SelectionStart, file->SelectionEnd);
                file->TreeSitter.TreeEdit(sel_start, sel_size, 0);
                file->Cursor.ByteIdx = sel_start;
                file->HasSelection = false;

                ImGui::SetClipboardText(text_buf.c_str());

                commit_cursor_state();
                return true;
            }
        }

        return false;
    }

    // Hotkey: Ctrl+V
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_V))
    {
        if(file->Config.IsReadOnly)
            return false;

        const char* clip_buf = ImGui::GetClipboardText();
        if(clip_buf)
        {
            size_t insert_pos = file->Cursor.ByteIdx;
            size_t normalized_len = 0;

            if(file->HasSelection)
            {
                normalized_len = file->Buffer.Replace(sel_start, sel_size, clip_buf, file->SelectionStart, file->SelectionEnd);
                file->TreeSitter.TreeEdit(sel_start, sel_size, normalized_len);
                file->Cursor.ByteIdx = sel_start + normalized_len;
                file->HasSelection = false;
            }
            else
            {
                normalized_len = file->Buffer.Insert(insert_pos, clip_buf);
                file->TreeSitter.TreeEdit(insert_pos, 0, normalized_len);
                file->Cursor.ByteIdx += normalized_len;
            }

            commit_cursor_state();
            return true;
        }

        return false;
    }

    // Hotkey: Ctrl+D
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_D))
    {
        if(file->Config.IsReadOnly)
            return false;

        if(file->HasSelection)
        {
            file->Buffer.GetTextInRange(&text_buf, sel_start, sel_end);
            if(!text_buf.empty())
            {
                file->Buffer.Insert(file->Cursor.ByteIdx, text_buf.c_str());
                file->TreeSitter.TreeEdit(file->Cursor.ByteIdx, 0, text_buf.size());
                file->Cursor.ByteIdx += text_buf.size();
            }
        }
        else
        {
            if(file->Cursor.CurrentLine.Text.size() != 0)
            {
                file->Buffer.Insert(file->Cursor.CurrentLine.Range.last, "\n");
                file->Buffer.Insert(file->Cursor.CurrentLine.Range.last + 1, file->Cursor.CurrentLine.Text.c_str());
                file->TreeSitter.TreeEdit(file->Cursor.CurrentLine.Range.last, 0, file->Cursor.CurrentLine.Text.size() + 1);
                file->Cursor.ByteIdx += file->Cursor.CurrentLine.Text.size() + 1;
            }
            else
            {
                file->Buffer.Insert(file->Cursor.ByteIdx, "\n");
                file->TreeSitter.TreeEdit(file->Cursor.ByteIdx, 0, 1);
                file->Cursor.ByteIdx += 1;
            }
        }

        commit_cursor_state();
        return true;
    }

    // Hotkey: Ctrl+S
    if(is_ctrl_down && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        if(file->Config.IsReadOnly)
            return false;

        if(gn.FileSystem.TextFileWrite(file))
            file->UpdateSavedVersionId();

        return false;
    }

    // Key: Backspace
    if(ImGui::IsKeyPressed(ImGuiKey_Backspace))
    {
        if(file->Config.IsReadOnly)
            return false;

        if(file->HasSelection)
        {
            file->Buffer.Remove(sel_start, sel_size, file->SelectionStart, file->SelectionEnd);
            file->TreeSitter.TreeEdit(sel_start, sel_size, 0);
            file->Cursor.ByteIdx = sel_start;
            file->HasSelection = false;

            commit_cursor_state();
            return true;
        }
        else if(file->Cursor.ByteIdx > 0)
        {
            // The beginning of the line has been reached. Go to the end of the previous line.
            if(file->Cursor.ColumnIdx <= 1)
            {
                size_t prev_line_idx = file->Cursor.LineIdx - 1;
                size_t prev_line_length = ImTextBufferGetLineColumnCount(&file->Buffer, prev_line_idx);

                file->Cursor.LineIdx = prev_line_idx;
                file->Cursor.ColumnIdx = prev_line_length;
            }
            else
            {
                if(file->Buffer.GetCharAt(file->Cursor.ByteIdx - 1) == '\t')
                    file->Cursor.ColumnIdx -= gn.Settings.IndentSize;
                else
                    file->Cursor.ColumnIdx -= 1;
            }

            file->Cursor.DisableBlinking = true;
            file->SetCursorPosition(file->Cursor.LineIdx, file->Cursor.ColumnIdx);
            file->Cursor.GoalColumnIdx = file->Cursor.ColumnIdx;
            file->UpdateCursorScrollPos();

            char c = file->Buffer.GetCharAt(file->Cursor.ByteIdx);
            size_t del_pos = file->Cursor.ByteIdx;
            size_t del_len = ImTextCountUtf8BytesFromChar(c);

            file->Buffer.Remove(del_pos, del_len, del_pos + del_len, del_pos + del_len);
            file->TreeSitter.TreeEdit(del_pos, del_len, 0);
            return true;
        }

        return false;
    }

    // Key: Delete
    if(ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if(file->Config.IsReadOnly)
            return false;

        if(file->HasSelection)
        {
            file->Buffer.Remove(sel_start, sel_size, file->SelectionStart, file->SelectionEnd);
            file->TreeSitter.TreeEdit(sel_start, sel_size, 0);
            file->Cursor.ByteIdx = sel_start;
            file->HasSelection = false;
        }
        else if(file->Cursor.ByteIdx < file->Buffer.GetSize())
        {
            char c = file->Buffer.GetCharAt(file->Cursor.ByteIdx);
            size_t del_pos = file->Cursor.ByteIdx;
            size_t del_len = ImTextCountUtf8BytesFromChar(c);

            file->Buffer.Remove(del_pos, del_len, del_pos, del_pos);
            file->TreeSitter.TreeEdit(file->Cursor.ByteIdx, del_len, 0);
        }

        commit_cursor_state();
        return true;
    }

    // Key: Tab
    if(ImGui::IsKeyPressed(ImGuiKey_Tab))
    {
        if(file->Config.IsReadOnly)
            return false;

        ImString indent_str;
        if(gn.Settings.IndentWithSpaces)
        {
            for(int i = 0; i < gn.Settings.IndentSize; ++i)
                indent_str.push_back(' ');
        }
        else
            indent_str.push_back('\t');

        if(file->HasSelection)
        {
            size_t new_len = file->Buffer.Replace(sel_start, sel_size, indent_str.c_str(), file->SelectionStart, file->SelectionEnd);
            file->TreeSitter.TreeEdit(sel_start, sel_size, new_len);
            file->Cursor.ByteIdx = sel_start + new_len;
            file->HasSelection = false;
        }
        else
        {
            size_t insert_size = (gn.Settings.IndentWithSpaces) ? gn.Settings.IndentSize : 1;

            file->Buffer.Insert(file->Cursor.ByteIdx, indent_str.c_str());
            file->TreeSitter.TreeEdit(file->Cursor.ByteIdx, 0, insert_size);
            file->Cursor.ByteIdx += insert_size;
        }

        commit_cursor_state();
        return true;
    }

    // Key: Enter
    if(ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
    {
        if(file->Config.IsReadOnly)
            return false;

        size_t insert_pos = file->Cursor.ByteIdx;

        if(file->HasSelection)
        {
            size_t new_len = file->Buffer.Replace(sel_start, sel_size, "\n", file->SelectionStart, file->SelectionEnd);
            file->TreeSitter.TreeEdit(sel_start, sel_size, new_len);
            file->Cursor.ByteIdx = sel_start + new_len;
            file->HasSelection = false;
        }
        else
        {
            file->Buffer.Insert(insert_pos, "\n", 0, 0);
            file->TreeSitter.TreeEdit(insert_pos, 0, 1);
            file->Cursor.ByteIdx = file->Cursor.ByteIdx + 1;
        }

        commit_cursor_state();
        return true;
    }

    return false;
}

/*
 TREE-SITTER LIMITATIONS & LARGE FILE HANDLING
 =============================================

 IMPORTANT: Tree-sitter is NOT designed for very large files (e.g., > 2-5 MB).
 Although it is an incremental parser, it strictly requires the FULL buffer
 context to build a correct AST. It CANNOT parse only a visible viewport.
 Synchronous parsing of massive files will inevitably block the UI thread.

 When dealing with large files, we must use one of the following approaches:
    1. Hard Size Limit (Fallback): Disable Tree-sitter entirely for files
       exceeding a safe threshold (e.g., 2 MB). Fall back to simple regex or
       plain text rendering.
    2. Background Thread: Offload `ts_parser_parse` to a worker thread.
       Requires thread-safe state management and cancellation logic.
    3. Chunked File Loading: Load the file into the buffer in chunks rather
       than all at once. Tree-sitter will incrementally update the tree as new
       data is appended, avoiding a single massive parsing spike.
*/

void ImNotepad::UpdateSyntaxHighlighting(ImNotepadTextFile* file)
{
    ImNotepadContext& gn = *GImNotepad;
    ImNotepadStyle& style = GImNotepad->Style;
    ImNotepadTreeSitter& ts = file->TreeSitter;

    if(file == nullptr || file->Language == nullptr || file->Buffer.GetSize() == 0)
        return;

    if(file->RenderedLines.first != file->RenderedLinesPrev.first || file->RenderedLines.last != file->RenderedLinesPrev.last)
    {
        if(file->RenderedLinesPrev.first != 0 && file->RenderedLines.last != 0)
            ts.ScratchIsValid = false;

        file->RenderedLinesPrev.first = file->RenderedLines.first;
        file->RenderedLinesPrev.last = file->RenderedLines.last;
    }

    size_t query_start_byte = 0;
    size_t query_end_byte = file->Buffer.GetSize();
    if(file->RenderedLines.first <= file->RenderedLines.last)
    {
        const int line_reserve = 2;
        if(file->RenderedLines.first > line_reserve)
            query_start_byte = file->Buffer.GetLineRange(file->RenderedLines.first - line_reserve).first;
        if(file->RenderedLines.last + line_reserve < file->Buffer.GetLineCount())
            query_end_byte = file->Buffer.GetLineRange(file->RenderedLines.last + line_reserve).last;
    }

    if(!ts.IsDirty && ts.ScratchIsValid && !(query_start_byte != ts.ScratchStartByte || query_end_byte != ts.ScratchEndByte))
        return;

    if(ts.IsDirty)
    {
        // Initialize the parser
        if(ts.Parser == nullptr)
            ts.Parser = ts_parser_new();
        ts_parser_set_language(ts.Parser, file->Language->Grammar);

        TSInputPayload input_payload;
        input_payload.ReadBuf = ts.ReadBuf;
        input_payload.ReadBufSize = sizeof(ts.ReadBuf);
        input_payload.PieceTree = &file->Buffer.PieceTree;

        // Initialize input adapter (without copying text)
        TSInput input;
        input.payload = &input_payload;
        input.read = ts.ReadTextChunk;
        input.encoding = TSInputEncodingUTF8;
        input.decode = nullptr;

        // Incremental parsing via TSInput
        TSTree* new_tree = ts_parser_parse(ts.Parser, ts.SyntaxTree, input);
        if(new_tree == nullptr)
        {
            ts.IsDirty = true;
            ts.ScratchIsValid = false;
            return;
        }

        if(ts.SyntaxTree)
            ts_tree_delete(ts.SyntaxTree);

        ts.SyntaxTree = new_tree;
        ts.IsDirty = false;
        ts.ScratchIsValid = false;
    }

    const size_t query_len = query_end_byte - query_start_byte;

    if(!(ts.ScratchIsValid && ts.ScratchStartByte <= query_start_byte && ts.ScratchEndByte >= query_end_byte))
    {
        if(query_len == 0)
            return;

        ts.ScratchStartByte = query_start_byte;
        ts.ScratchEndByte = query_end_byte;

        ts.ScratchByteColors.resize((int)query_len);
        ts.ScratchByteNames.resize((int)query_len);
        memset(ts.ScratchByteColors.Data, 0, sizeof(ImU32) * query_len);
        memset(ts.ScratchByteNames.Data, 0, sizeof(char*) * query_len);

        // Pre-parsing of predicates
        ts.CacheQueryPredicates(ts, file->Language->Query);

        TSQueryCursor* cursor = ts_query_cursor_new();
        ts_query_cursor_set_byte_range(cursor, (uint32_t)query_start_byte, (uint32_t)query_end_byte);
        ts_query_cursor_exec(cursor, file->Language->Query, ts_tree_root_node(ts.SyntaxTree));

        TSQueryMatch match;
        uint32_t capture_index;

        while(ts_query_cursor_next_capture(cursor, &match, &capture_index))
        {
            const TSQueryCapture* capture = &match.captures[capture_index];
            const uint16_t query_id = capture->index;
            uint32_t node_start_byte = ts_node_start_byte(capture->node);
            uint32_t node_end_byte = ts_node_end_byte(capture->node);

            // Ignore captures that are completely outside the visible area
            if(node_end_byte <= query_start_byte || node_start_byte >= query_end_byte)
                continue;

            // Crop the capture range to the boundaries of the visible area
            int local_start = (int)((node_start_byte < query_start_byte) ? 0 : (node_start_byte - query_start_byte));
            int local_end = (int)((node_end_byte > query_end_byte) ? query_len : (node_end_byte - query_start_byte));

            // Checking the validity of a pattern using predicates
            const TSQueryPredicateRange& predicate_range = ts.QueryCache.PatternRanges[match.pattern_index];
            if(predicate_range.PredicatesCount)
            {
                // Obtaining a raw pointer to the beginning of the predicate subarray for this pattern
                const TSQueryParsedPredicate* pred_ptr = ts.QueryCache.Predicates.Data + predicate_range.StartIndex;

                // Passing pointers directly. No temporary objects, no compiler errors
                if(!ts.QueryEvaluatePredicates(pred_ptr, ts.QueryCache.PredicateArgs.Data, predicate_range.PredicatesCount, match, &file->Buffer.PieceTree))
                    continue; // The pattern failed validation
            }

            uint32_t capture_name_len;
            const char* capture_name = ts_query_capture_name_for_id(file->Language->Query, query_id, &capture_name_len);
            ImU32 color = ImNotepadGetSyntaxHighlightColor(style.SyntaxColorMap, capture_name, capture_name_len);

            for(int i = local_start; i < local_end; ++i)
            {
                ts.ScratchByteColors[i] = color;
                ts.ScratchByteNames[i] = capture_name;
            }
        }

        ts_query_cursor_delete(cursor);
        ts.ScratchIsValid = true;
    }

    ts.Highlights.reserve((int)query_len);

    const int offset_in_cache = (int)(query_start_byte - ts.ScratchStartByte);
    const ImU32* colors = ts.ScratchByteColors.Data + offset_in_cache;
    const char** names = ts.ScratchByteNames.Data + offset_in_cache;

    // Compressing the byte array back into a list of highlight spans.
    // A linear pass ensures that the resulting array is perfectly sorted by StartByte without using ImQsort.
    int span_count = 0;
    int i = 0;
    while(i < query_len)
    {
        while(i < query_len && colors[i] == 0)
            i++;

        if(i >= query_len)
            break;

        ImU32 color = colors[i];
        const char* name = names[i];
        int start = i;
        i++;

        while(i < query_len && colors[i] == color && names[i] == name)
            i++;

        ts.Highlights.Data[span_count].Name = name;
        ts.Highlights.Data[span_count].StartByte = query_start_byte + start;
        ts.Highlights.Data[span_count].EndByte = query_start_byte + i;
        ts.Highlights.Data[span_count].Color = color;
        span_count++;
    }

    ts.Highlights.Size = span_count;
}

void ImNotepad::RenderLineSegment(ImString* line_text, FredBuf::Range& segment_range, FredBuf::Range& trailing_whitespace_range, float pos_x, float pos_y, ImU32 text_color, size_t line_start_byte, size_t sel_start, size_t sel_end, bool has_selection)
{
    ImNotepadContext& gn = *GImNotepad;
    ImNotepadSettings& settings = gn.Settings;
    ImFont* font = gn.Style.FontText;
    float font_size = gn.Style.FontTextSize;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImStringView segment(line_text->begin() + segment_range.first, segment_range.last - segment_range.first);

    if(has_selection && sel_start < sel_end)
    {
        size_t global_seg_start = line_start_byte + segment_range.first;
        size_t global_seg_end = line_start_byte + segment_range.last;

        size_t sel_overlap_start = ImMax(global_seg_start, sel_start);
        size_t sel_overlap_end = ImMin(global_seg_end, sel_end);

        if(sel_overlap_start < sel_overlap_end)
        {
            size_t local_sel_start = sel_overlap_start - global_seg_start;
            size_t local_sel_end = sel_overlap_end - global_seg_start;

            const ImStringView sel_segment_start(line_text->begin() + segment_range.first, local_sel_start);
            float sel_x = pos_x + ImCalcTextWidth(sel_segment_start);

            float sel_width = 0.0f;
            bool selection_continues = (sel_end > global_seg_end);

            if(selection_continues && local_sel_end >= segment.size())
            {
                // The selection goes beyond the segment: draw to the end of the segment.
                float segment_end_x = pos_x + ImCalcTextWidth(segment);
                sel_width = segment_end_x - sel_x;
            }
            else
            {
                const ImStringView sel_segment_end(line_text->begin() + segment_range.first + local_sel_start, local_sel_end - local_sel_start);
                sel_width = ImCalcTextWidth(sel_segment_end);
            }

            if(sel_width > 0.0f)
                draw_list->AddRectFilled(ImVec2(sel_x, pos_y), ImVec2(sel_x + sel_width, pos_y + gn.LineHeight), GetColorU32(ImNotepadCol_TextSelectedBg));
        }
    }

    if(segment.empty())
        return;

    draw_list->AddText(font, font_size, ImVec2(pos_x, pos_y), text_color, segment.begin(), segment.end());

    if(!settings.RenderWhitespaces && !settings.RenderTrailingWhitespaces)
        return;

    ImU32 dot_color = GetColorU32(ImNotepadCol_Whitespace);
    ImU32 arrow_color = dot_color;

    for(size_t i = 0; i < segment.size(); )
    {
        size_t char_len = ImTextCountUtf8BytesFromChar(segment[i]);

        const ImStringView ch = segment.substr(i, char_len);
        unsigned int cp;
        ImTextCharFromUtf8(&cp, ch.begin(), ch.end());

        size_t global_byte_pos = segment_range.first + i;
        float char_width = ImCalcTextWidth(ch.begin(), ch.begin() + char_len);

        if(cp == ' ' || cp == '\t')
        {
            // Draw rectangles for trailing spaces/tabs
            if(settings.RenderTrailingWhitespaces)
            {
                // Check if this character is in the range of trailing spaces/tabs
                if(global_byte_pos >= trailing_whitespace_range.first && global_byte_pos < trailing_whitespace_range.last)
                {
                    float render_width = char_width;
                    if(cp == '\t')
                        render_width = gn.MonospaceCharWidth * gn.Settings.IndentSize;

                    draw_list->AddRectFilled(ImVec2(pos_x, pos_y), ImVec2(pos_x + render_width, pos_y + gn.LineHeight), GetColorU32(ImNotepadCol_TrailingWhitespace));
                }
            }

            // Draw dots/arrows for all spaces/tabs
            if(settings.RenderWhitespaces)
            {
                if(cp == ' ')
                {
                    const float dot_radius = ImMax(0.50f, font_size * 0.06f);
                    int dot_segments = 4;
                    if(dot_radius > 1.0f)
                        dot_segments = 16;
                    else if(dot_radius >= 4.0f)
                        dot_segments = 32;

                    const float center_x = pos_x + char_width * 0.5f;
                    const float center_y = pos_y + (gn.LineHeight + dot_radius) * 0.5f;
                    draw_list->AddCircleFilled(ImVec2(center_x, center_y), dot_radius, dot_color, dot_segments);
                }
                else if(cp == '\t')
                {
                    float tab_width = gn.MonospaceCharWidth * gn.Settings.IndentSize;
                    char_width = tab_width;

                    if(tab_width <= gn.MonospaceCharWidth)
                        tab_width = gn.MonospaceCharWidth - 2.0f;
                    else
                        tab_width -= 4.0f;

                    const float line_thickness = ImMax(0.50f, font_size * 0.05f);
                    const float center_y = pos_y + (gn.LineHeight + line_thickness) * 0.5f;
                    ImVec2 arrow_size(tab_width, gn.LineHeight * 0.25f);

                    draw_list->AddLine(ImVec2(pos_x, center_y), ImVec2(pos_x + arrow_size.x + line_thickness * 0.25f, center_y), arrow_color, line_thickness);
                    draw_list->AddLine(ImVec2(pos_x + arrow_size.x, center_y), ImVec2(pos_x + arrow_size.x - arrow_size.y * 0.6f, center_y - arrow_size.y * 0.6f), arrow_color, line_thickness);
                    draw_list->AddLine(ImVec2(pos_x + arrow_size.x, center_y), ImVec2(pos_x + arrow_size.x - arrow_size.y * 0.6f, center_y + arrow_size.y * 0.6f), arrow_color, line_thickness);
                }
            }
        }

        pos_x += char_width;
        i += char_len;
    }
}

void ImNotepad::RenderLine(ImNotepadTextFile* file, size_t line_idx, int& highlight_index, float pos_x, float pos_y)
{
    ImNotepadContext& gn = *GImNotepad;
    ImNotepadSettings& settings = gn.Settings;

    ImFont* font = gn.Style.FontText;
    float font_size = gn.Style.FontTextSize;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    file->Buffer.GetLineContent(&gn.TextLineBuffer, line_idx);

    ImString* line_text = &gn.TextLineBuffer.Text;
    size_t line_start_byte = gn.TextLineBuffer.Range.first;
    size_t line_end_byte = gn.TextLineBuffer.Range.last;
    size_t line_len = line_text->size();

    size_t sel_start = file->HasSelection ? ImMin(file->SelectionStart, file->SelectionEnd) : 0;
    size_t sel_end = file->HasSelection ? ImMax(file->SelectionStart, file->SelectionEnd) : 0;

    const ImVector<ImNotepadHighlightSpan>& highlights = file->TreeSitter.Highlights;
    const int highlights_count = highlights.size();

    bool line_is_empty = gn.TextLineBuffer.Text.empty();

    // Detecting trailing spaces/tabs. Scan from the END of the line backwards.
    FredBuf::Range trailing_whitespaces_range;
    if(settings.RenderTrailingWhitespaces)
    {
        for(size_t i = line_text->size(); i > 0;)
        {
            // Find the start of the previous UTF-8 character by skipping continuation bytes (10xxxxxx)
            size_t char_start = i - 1;
            while(char_start > 0 && (line_text->data()[char_start] & 0xC0) == 0x80)
                char_start--;

            size_t char_len = i - char_start;
            const ImStringView ch(line_text->begin() + char_start, char_len);
            unsigned int cp;
            ImTextCharFromUtf8(&cp, ch.begin(), ch.end());

            if(cp != ' ' && cp != '\t' && cp != '\r' && cp != '\n')
            {
                // Found the last significant character - trailing whitespace starts right after it
                trailing_whitespaces_range.first = i;
                break;
            }

            i = char_start;
        }

        trailing_whitespaces_range.last = line_text->size();
    }

    // Draw text and highlight segments
    if(!line_is_empty)
    {
        size_t line_offset = 0;
        float line_seg_width = 0.0f;

        // Skip highlights that end before the current line starts.
        // This advances highlight_index past all spans belonging to previous lines.
        while(highlight_index < highlights_count && highlights[highlight_index].EndByte <= line_start_byte)
            ++highlight_index;

        // Process highlights overlapping with the current line
        int highlight_scan_index = highlight_index;
        while(highlight_scan_index < highlights_count && highlights[highlight_scan_index].StartByte < line_end_byte)
        {
            const ImNotepadHighlightSpan& hl = highlights[highlight_scan_index];

            size_t seg_start = ImMax(hl.StartByte, line_start_byte) - line_start_byte;
            size_t seg_end = ImMin(hl.EndByte, line_end_byte) - line_start_byte;

            if(seg_start > line_offset)
            {
                FredBuf::Range segment_range = { line_offset, seg_start };
                RenderLineSegment(line_text, segment_range, trailing_whitespaces_range, pos_x, pos_y, GetColorU32(ImNotepadCol_TextDefaultFg), line_start_byte, sel_start, sel_end, file->HasSelection);

                const ImStringView gap(line_text->begin() + line_offset, seg_start - line_offset);
                float gap_width = ImCalcTextWidth(gap);

                if(settings.ShowTokenAndScopesInspector)
                {
                    ImRect gap_bb_rect = { pos_x, pos_y, pos_x + gap_width, pos_y + gn.LineHeight };
                    if(ImGui::IsMouseHoveringRect(gap_bb_rect.Min, gap_bb_rect.Max, false))
                    {
                        gn.TokenScopeInspector.Name = nullptr;
                        gn.TokenScopeInspector.HasCapture = true;
                        gn.TokenScopeInspector.SetScopeBounds(gap_bb_rect, (uint32_t)(line_start_byte + line_offset), (uint32_t)(line_start_byte + line_offset + gap.size()));
                    }

                }

                pos_x += gap_width;
                line_offset = seg_start;
            }

            FredBuf::Range segment_range = { seg_start, seg_end };
            RenderLineSegment(line_text, segment_range, trailing_whitespaces_range, pos_x, pos_y, ImColorWithAlpha(hl.Color), line_start_byte, sel_start, sel_end, file->HasSelection);
            const ImStringView hl_seg(line_text->begin() + seg_start, seg_end - seg_start);
            line_seg_width = ImCalcTextWidth(hl_seg);

            if(settings.ShowTokenAndScopesInspector)
            {
                ImRect seg_bb_rect = { pos_x, pos_y, pos_x + line_seg_width, pos_y + gn.LineHeight };
                if(ImGui::IsMouseHoveringRect(seg_bb_rect.Min, seg_bb_rect.Max, false))
                {
                    gn.TokenScopeInspector.Name = hl.Name;
                    gn.TokenScopeInspector.HasCapture = true;
                    gn.TokenScopeInspector.SetScopeBounds(seg_bb_rect, (uint32_t)hl.StartByte, (uint32_t)hl.EndByte);
                    gn.TokenScopeInspector.QueryCaptures(file);
                }
            }

            pos_x += line_seg_width;
            line_offset = seg_end;
            ++highlight_scan_index;
        }

        // The spans do not overlap and are sorted by StartByte/EndByte, so it suffices to stop at the first one that extends beyond the end of the string.
        while(highlight_index < highlights_count && highlights[highlight_index].EndByte <= line_end_byte)
            ++highlight_index;

        // Draw the rest of the line or the entire line if there are no highlights
        if(line_offset < line_len)
        {
            size_t rest_start = line_offset;
            size_t rest_end = line_len;
            FredBuf::Range segment_range = { rest_start, rest_end };
            RenderLineSegment(line_text, segment_range, trailing_whitespaces_range, pos_x, pos_y, GetColorU32(ImNotepadCol_TextDefaultFg), line_start_byte, sel_start, sel_end, file->HasSelection);
            const ImStringView rest(line_text->begin() + rest_start, rest_end - rest_start);
            line_seg_width = ImCalcTextWidth(rest);

            if(settings.ShowTokenAndScopesInspector)
            {
                ImRect rest_bb_rect = { pos_x, pos_y, pos_x + line_seg_width, pos_y + gn.LineHeight };
                if(ImGui::IsMouseHoveringRect(rest_bb_rect.Min, rest_bb_rect.Max, false))
                {
                    gn.TokenScopeInspector.Name = nullptr;
                    gn.TokenScopeInspector.HasCapture = true;
                    gn.TokenScopeInspector.SetScopeBounds(rest_bb_rect, (uint32_t)(line_start_byte + rest_start), (uint32_t)(line_start_byte + rest_end));
                }
            }

            pos_x += line_seg_width;
            line_offset = rest_end;
        }

        // Draw the "tail" of the line only if the selection crosses the line and goes further
        if(file->HasSelection && sel_start <= line_end_byte && sel_end > line_end_byte)
            draw_list->AddRectFilled(ImVec2(pos_x, pos_y), ImVec2(pos_x + gn.MonospaceCharWidth, pos_y + gn.LineHeight), GetColorU32(ImNotepadCol_TextSelectedBg));
    }
    else
    {
        // Even for empty lines, advance highlight_index past any spans ending here
        while(highlight_index < highlights_count && highlights[highlight_index].EndByte <= line_start_byte)
            ++highlight_index;

        // Draw selection for the empty line
        if(file->HasSelection && sel_start <= line_end_byte && sel_end > line_start_byte)
            draw_list->AddRectFilled(ImVec2(pos_x, pos_y), ImVec2(pos_x + gn.MonospaceCharWidth, pos_y + gn.LineHeight), GetColorU32(ImNotepadCol_TextSelectedBg));
    }
}

void ImNotepad::RenderGutter(ImNotepadTextFile* file)
{
    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;
    float font_size = gn.Style.FontTextSize;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushClipRect(file->GutterBB.Min, file->GutterBB.Max, true);

    for(size_t line_idx = file->RenderedLines.first; line_idx <= file->RenderedLines.last; ++line_idx)
    {
        float text_width = ImCountDigitsFromNum(line_idx) * gn.MonospaceCharWidth;
        float pos_x = file->GutterBB.Min.x + file->GutterWidth - text_width;
        pos_x -= gn.MonospaceCharWidth;

        float pos_y = file->GutterBB.Min.y - ImGui::GetScrollY() + (gn.LineHeight * (line_idx - 1));

        ImU32 color = GetColorU32(ImNotepadCol_LineNumber);
        if(gn.Settings.RenderCursorLine && line_idx == file->Cursor.LineIdx)
            color = GetColorU32(ImNotepadCol_CursorLineNumber);

        char line_num_buf[21];
        snprintf(line_num_buf, 21, "%zu", line_idx);
        draw_list->AddText(font, font_size, ImVec2(pos_x, pos_y), color, line_num_buf);
    }

    ImGui::PopClipRect();
}

void ImNotepad::RenderContent(ImNotepadTextFile* file)
{
    ImNotepadContext& gn = *GImNotepad;
    ImNotepadSettings& settings = gn.Settings;

    gn.TokenScopeInspector.HasCapture = false;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushClipRect(file->ContentBB.Min, file->ContentBB.Max, true);

    // Reset highlight scanning index once per frame for all rendered lines
    int highlight_index = 0;

    for(size_t line_idx = file->RenderedLines.first; line_idx <= file->RenderedLines.last; ++line_idx)
    {
        float pos_x = file->ContentBB.Min.x - ImGui::GetScrollX();
        float pos_y = file->ContentBB.Min.y - ImGui::GetScrollY() + (gn.LineHeight * (line_idx - 1));

        if(gn.Settings.RenderCursorLine && !file->HasSelection && line_idx == file->Cursor.LineIdx)
        {
            ImVec2 box_min(file->ContentBB.Min.x, pos_y);
            ImVec2 box_max(file->ContentBB.Max.x, pos_y + gn.LineHeight);
            draw_list->AddRectFilled(box_min, box_max, GetColorU32(ImNotepadCol_CursorLine));
        }

        RenderLine(file, line_idx, highlight_index, pos_x, pos_y);
    }

    ImGui::PopClipRect();
}

void ImNotepad::RenderScrollbar(ImNotepadTextFile* file, ImGuiAxis axis)
{
    ImGuiContext& g = *GImGui;
    ImNotepadContext& gn = *GImNotepad;
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImGuiID id = ImGui::GetWindowScrollbarID(window, axis);

    // Calculate scrollbar bounding box
    const ImRect outer_rect = window->Rect();
    const ImRect inner_rect = window->InnerRect;
    const float scrollbar_size = g.Style.ScrollbarSize;
    const float border_size = IM_ROUND(window->WindowBorderSize * 0.5f);
    const float border_top = (window->Flags & ImGuiWindowFlags_MenuBar) ? IM_ROUND(g.Style.FrameBorderSize * 0.5f) : 0.0f;
    ImRect bb;
    if(axis == ImGuiAxis_X)
    {
        bb = ImRect(inner_rect.Min.x + file->GutterWidth + gn.Style.ContentPadding,
                    ImMax(outer_rect.Min.y + border_size, outer_rect.Max.y - border_size - scrollbar_size),
                    inner_rect.Max.x - border_size,
                    outer_rect.Max.y - border_size);

        if(ImGui::GetScrollMaxY() > 0.0f)
            bb.Max.x -= scrollbar_size;
    }
    else
    {
        bb = ImRect(ImMax(outer_rect.Min.x, outer_rect.Max.x - border_size - scrollbar_size),
                    inner_rect.Min.y + border_top,
                    outer_rect.Max.x - border_size,
                    inner_rect.Max.y - border_size);

        if(ImGui::GetScrollMaxX() > 0.0f)
            bb.Max.y -= scrollbar_size;
    }

    ImRect host_rect = (window->DockIsActive ? window->DockNode->HostWindow : window)->Rect();
    ImDrawFlags rounding_corners = ImGui::CalcRoundingFlagsForRectInRect(bb, host_rect, g.Style.WindowBorderSize);
    float size_visible = window->InnerRect.Max[axis] - window->InnerRect.Min[axis];
    float size_contents = window->ContentSize[axis] + window->WindowPadding[axis] * 2.0f;
    ImS64 scroll = (ImS64)window->Scroll[axis];

    ImVec4* colors = g.Style.Colors;
    ImVec4 col_scrollbar_bg_old             = colors[ImGuiCol_ScrollbarBg];
    ImVec4 col_scrollbar_grab_old           = colors[ImGuiCol_ScrollbarGrab];
    ImVec4 col_scrollbar_brag_hovered_old   = colors[ImGuiCol_ScrollbarGrabHovered];
    ImVec4 col_scrollbar_grab_active_old    = colors[ImGuiCol_ScrollbarGrabActive];

    // Adding a transparency effect to the scrollbar
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab]          = { col_scrollbar_grab_old.x, col_scrollbar_grab_old.y, col_scrollbar_grab_old.z, 0.5f };
    colors[ImGuiCol_ScrollbarGrabHovered]   = { col_scrollbar_brag_hovered_old.x, col_scrollbar_brag_hovered_old.y, col_scrollbar_brag_hovered_old.z, 0.5f };
    colors[ImGuiCol_ScrollbarGrabActive]    = { col_scrollbar_grab_active_old.x, col_scrollbar_grab_active_old.y, col_scrollbar_grab_active_old.z, 0.5f };

    ImGui::ScrollbarEx(bb, id, axis, &scroll, (ImS64)size_visible, (ImS64)size_contents, rounding_corners);

    colors[ImGuiCol_ScrollbarBg]            = col_scrollbar_bg_old;
    colors[ImGuiCol_ScrollbarGrab]          = col_scrollbar_grab_old;
    colors[ImGuiCol_ScrollbarGrabHovered]   = col_scrollbar_brag_hovered_old;
    colors[ImGuiCol_ScrollbarGrabActive]    = col_scrollbar_grab_active_old;

    if(axis == ImGuiAxis_Y)
    {
        // Normalizing new 'Y' value by line height
        const ImS64 scroll_step_y = (ImS64)gn.LineHeight;
        scroll = (scroll / scroll_step_y) * scroll_step_y;
    }

    window->Scroll[axis] = (float)scroll;
}

void ImNotepad::RenderCursor(ImNotepadTextFile* file)
{
    ImNotepadContext& gn = *GImNotepad;
    ImGuiIO& io = ImGui::GetIO();

    if(!ImGui::IsWindowFocused())
        return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_pos(file->ContentBB.Min.x - ImGui::GetScrollX() + file->Cursor.BlinkingPos.x,
                      file->ContentBB.Min.y - ImGui::GetScrollY() + file->Cursor.BlinkingPos.y);

    if(!file->ContentBB.Contains(cursor_pos))
        return;

    // Allow blinking on first and last character
    if(file->Cursor.DisableBlinking)
        file->Cursor.DisableBlinking = (file->Cursor.ByteIdx == 0 || file->Cursor.ByteIdx == file->Buffer.GetSize()) ? false : true;

    if(file->Cursor.DisableBlinking)
    {
        file->Cursor.ShowTime = 0.0f;
        file->Cursor.Show = true;
        file->Cursor.DisableBlinking = false;
    }
    else
    {
        file->Cursor.ShowTime += io.DeltaTime;
        file->Cursor.Show = (file->Cursor.ShowTime <= 0.0f) || ImFmod(file->Cursor.ShowTime, 1.0f) <= 0.50f;
    }

    if(file->Cursor.Show)
        draw_list->AddLine(cursor_pos, ImVec2(cursor_pos.x, cursor_pos.y + gn.LineHeight), IM_COL32(255, 255, 255, 255), 2.0f);
}

void ImNotepad::RenderFileInfo(ImNotepadTextFile* file)
{
    ImGuiContext& g = *GImGui;
    ImNotepadContext& gn = *GImNotepad;

    char line_col_buf[64];
    ImFormatString(line_col_buf, IM_ARRAYSIZE(line_col_buf), "Ln %zu, Col %zu", file->Cursor.LineIdx, file->Cursor.ColumnIdx);

    char pos_buf[32];
    ImFormatString(pos_buf, IM_ARRAYSIZE(pos_buf), "Pos: %zu", file->Cursor.ByteIdx);

    char spaces_buf[16];
    ImFormatString(spaces_buf, IM_ARRAYSIZE(spaces_buf), "%s: %u", gn.Settings.IndentWithSpaces ? "Spaces" : "Tab Size", gn.Settings.IndentSize);

    const char* eol_sequence_names[] = { "LF", "CR", "CRLF" };
    char eol_sequence_buf[8];
    ImFormatString(eol_sequence_buf, IM_ARRAYSIZE(eol_sequence_buf), eol_sequence_names[file->Buffer.GetEndOfLineSequence()]);

    char language_name_buf[32];
    ImFormatString(language_name_buf, IM_ARRAYSIZE(language_name_buf), file->Language ? file->Language->DisplayName.c_str() : "Plain text");

    float line_col_width        = ImGui::CalcTextSize(line_col_buf).x;
    float pos_width             = ImGui::CalcTextSize(pos_buf).x;
    float spaces_width          = ImGui::CalcTextSize(spaces_buf).x;
    float eol_sequence_width    = ImGui::CalcTextSize(eol_sequence_buf).x;
    float language_name_width   = ImGui::CalcTextSize(language_name_buf).x;

    float padding_x = g.Style.FramePadding.x * 2.0f;

    float line_col_button_width         = line_col_width + padding_x;
    float spaces_button_width           = spaces_width + padding_x;
    float eol_sequence_button_width     = eol_sequence_width + padding_x;
    float language_name_button_width    = language_name_width + padding_x;

    float total_w = (line_col_button_width + g.Style.ItemSpacing.x) + (pos_width + g.Style.ItemSpacing.x) + (spaces_button_width + g.Style.ItemSpacing.x) + (eol_sequence_button_width + g.Style.ItemSpacing.x) + language_name_button_width;

    ImGui::SetCursorScreenPos(ImVec2(file->ContentBB.Max.x - total_w - (ImGui::IsWindowDocked() ? 0.0f : 20.0f), file->ContentBB.Max.y + 1.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    // Modal window for switching to a row/column
    if(ImGui::BeginPopupModal("##GoToLinePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        static ImU64 input_line = (ImU64)1;
        static ImU64 input_col = (ImU64)1;

        // We initialize the values ​​only on the first frame of opening, so as not to overwrite the data while the user is typing
        if(ImGui::IsWindowAppearing())
        {
            input_line = file->Cursor.LineIdx;
            input_col = file->Cursor.ColumnIdx;
        }

        ImGui::Text("Go to line");
        ImGui::Separator();
        ImGui::PushItemWidth(100.0f);

        // Autofocus on the first input field when opening
        if(ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere(0);

        ImGui::InputScalar("Line", ImGuiDataType_U64, &input_line, nullptr, nullptr, nullptr, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::InputScalar("Column", ImGuiDataType_U64, &input_col, nullptr, nullptr, nullptr, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::PopItemWidth();

        ImGui::Separator();

        bool is_confirmed = ImGui::Button("Go", ImVec2(100, 0));
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();

        // Confirm with Enter / NumPad Enter
        if(is_confirmed || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            if(input_line == 0)
                input_line = 1;
            if(input_col == 0)
                input_col = 1;

            if(input_line > 0 && input_col > 0)
            {
                file->SetCursorPosition(input_line, input_col, ImNotepadCursorAlignmentFlags_AlignCenter);
                ImGui::CloseCurrentPopup();
            }
        }

        // Closing by pressing Escape
        if(ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if(ImGui::Button(line_col_buf, ImVec2(line_col_button_width, 0.0f)))
        ImGui::OpenPopup("##GoToLinePopup");

    // Widget 2: Text "Pos N"
    ImGui::SameLine();
    ImGui::TextUnformatted(pos_buf);

    // Widget 3: Button "Spaces/Tab Size: N" --> Opens the context menu
    ImGui::SameLine();
    ImGui::Button(spaces_buf, ImVec2(spaces_button_width, 0.0f));
    if(ImGui::BeginPopupContextItem("SpacesContextMenu", ImGuiPopupFlags_MouseButtonLeft))
    {
        ImGui::Text("Indent Mode:");
        ImGui::Separator();

        if(ImGui::MenuItem("Indent Using Spaces", nullptr, gn.Settings.IndentWithSpaces))
            gn.Settings.IndentWithSpaces = true;

        if(ImGui::MenuItem("Indent Using Tabs", nullptr, !gn.Settings.IndentWithSpaces))
            gn.Settings.IndentWithSpaces = false;

        ImGui::Separator();

        if(ImGui::BeginMenu("Indent size"))
        {
            for(int tab_size_sel = 1; tab_size_sel <= 8; ++tab_size_sel)
            {
                char buf[8];
                ImFormatString(buf, sizeof(buf), "%d", tab_size_sel);
                if(ImGui::MenuItem(buf, nullptr, gn.Settings.IndentSize == tab_size_sel, gn.Settings.IndentSize == tab_size_sel ? false : true))
                {
                    if(gn.Settings.IndentSize != tab_size_sel)
                    {
                        gn.Settings.IndentSize = tab_size_sel;
                        file->Buffer.SetTabSize(tab_size_sel);

                        // Calculate the maximum content width based on the largest line
                        ImString& line_text_buf = gn.TextBuffer;
                        file->Buffer.GetLineContent(&line_text_buf, file->Buffer.GetWidestLineIndex(gn.Settings.IndentSize));
                        file->LineMaxWidth = ImCalcTextWidth(line_text_buf); // FIXME: LineMaxWidth is recalculated only for the current file. Other open files will retain their previous width.

                        ImFontSetTabGlyphAdvanceX(gn.Style.FontText, gn.Settings.IndentSize);

                        file->SetCursorPositionByByteIdx(file->Cursor.ByteIdx);
                        file->Cursor.GoalColumnIdx = file->Cursor.ColumnIdx;
                        file->UpdateCursorScrollPos();
                    }
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    // Widget 4: Button "LF/CR/CRLF" --> Opens the context menu
    ImGui::SameLine();
    ImGui::Button(eol_sequence_buf, ImVec2(eol_sequence_button_width, 0.0f));
    if(ImGui::BeginPopupContextItem("EOLSequenceContextMenu", ImGuiPopupFlags_MouseButtonLeft))
    {
        FredBuf::EndOfLineSequence eol_sequence = file->Buffer.GetEndOfLineSequence();

        if(ImGui::MenuItem("Linux (LF)", nullptr, false, eol_sequence == FredBuf::EndOfLineSequence_LF ? false : true))
            file->Buffer.SetEndOfLineSequence(FredBuf::EndOfLineSequence_LF, file->Cursor.ByteIdx, file->SelectionStart, file->SelectionEnd);

        if(ImGui::MenuItem("Macintosh (CR)", nullptr, false, eol_sequence == FredBuf::EndOfLineSequence_CR ? false : true))
            file->Buffer.SetEndOfLineSequence(FredBuf::EndOfLineSequence_CR, file->Cursor.ByteIdx, file->SelectionStart, file->SelectionEnd);

        if(ImGui::MenuItem("Windows (CRLF)", nullptr, false, eol_sequence == FredBuf::EndOfLineSequence_CRLF ? false : true))
            file->Buffer.SetEndOfLineSequence(FredBuf::EndOfLineSequence_CRLF, file->Cursor.ByteIdx, file->SelectionStart, file->SelectionEnd);

        ImGui::EndPopup();
    }

    // Widget 5: Button "<language mode>" --> Opens the context menu
    ImGui::SameLine();
    ImGui::Button(language_name_buf, ImVec2(language_name_button_width, 0.0f));
    if(ImGui::BeginPopupContextItem("LanguageModeContextMenu", ImGuiPopupFlags_MouseButtonLeft))
    {
        for(ImNotepadLanguage* language : gn.FileSystem.Languages)
        {
            if(ImGui::MenuItem(language->DisplayName.c_str(), nullptr, language == file->Language, language == file->Language ? false : true))
            {
                if(file->Language != language)
                {
                    file->Language = language;

                    ImNotepadTreeSitter& ts = file->TreeSitter;
                    if(ts.SyntaxTree)
                    {
                        // Important: after changing the language grammar, we need to delete the current tree so that a new one can be created.
                        ts_tree_delete(ts.SyntaxTree);
                        ts.SyntaxTree = nullptr;
                    }

                    if(file->Language->Grammar == nullptr && !ts.Highlights.empty())
                        ts.Highlights.clear();

                    ts.IsDirty = true;
                    ts.ScratchIsValid = false;
                }
            }
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
}

void ImNotepad::RenderTokenAndScopesInspector(ImNotepadTextFile* file)
{
    ImNotepadContext& gn = *GImNotepad;

    if(!gn.Settings.ShowTokenAndScopesInspector)
        return;

    if(!gn.MouseState.InCanvas || !gn.TokenScopeInspector.HasCapture)
        return;

    if(!ImGui::IsMouseHoveringRect(gn.TokenScopeInspector.RectBB.Min, gn.TokenScopeInspector.RectBB.Max, true))
        return;

    ImGui::GetWindowDrawList()->AddRect(gn.TokenScopeInspector.RectBB.Min, gn.TokenScopeInspector.RectBB.Max, IM_COL32(0, 255, 0, 255));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));

    char tooltip_buf[1024];
    char* buf_p = tooltip_buf;
    char* buf_end = tooltip_buf + sizeof(tooltip_buf);
    buf_p += ImFormatString(buf_p, buf_end - buf_p, "Range: %u..%u\n", gn.TokenScopeInspector.StartByte, gn.TokenScopeInspector.EndByte);
    if(gn.TokenScopeInspector.Name != nullptr)
        buf_p += ImFormatString(buf_p, buf_end - buf_p, "Node:  @%s\n", gn.TokenScopeInspector.Name);

    if(gn.TokenScopeInspector.Captures.size() > 0)
    {
        buf_p += ImFormatString(buf_p, buf_end - buf_p, "Scopes:\n");
        for(const ImNotepadTokenScopeInspector::CaptureInfo& cap : gn.TokenScopeInspector.Captures)
            buf_p += ImFormatString(buf_p, buf_end - buf_p, "  - @%s\n", cap.Name);
    }
    else
        buf_p += ImFormatString(buf_p, buf_end - buf_p, "Plain text\n");

    ImGui::SetTooltip("%s", tooltip_buf);
    ImGui::PopStyleVar();

    gn.TokenScopeInspector.HasCapture = false;
}

void ImNotepad::RenderFileContent(ImNotepadTextFile* file)
{
    ImGuiContext& g = *GImGui;

    ImNotepadContext& gn = *GImNotepad;
    ImFont* font = gn.Style.FontText;
    ImNotepadSettings& settings = gn.Settings;

    // The root window name must be unique otherwise ImGui will lose its dock state.
    // The "###" hack solves the problem of switching focus and moving the docking tab when renaming a document.
    ImString window_label;
    const char* window_label_id = !file->FullPathName.empty() ? file->FullPathName.c_str() : file->Name.c_str();
    window_label.appendf(file->IsDirty() ? "%s*###%s" : "%s###%s", file->Name.c_str(), window_label_id);
    if(ImGui::Begin(window_label.c_str(), &file->IsOpened, ImGuiWindowFlags_NoCollapse))
    {
        gn.LineHeight = gn.Style.FontTextSize;

        size_t total_lines = file->Buffer.GetLineCount();
        float content_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight();

        ImVec2 dummy_bb(file->LineMaxWidth + file->GutterWidth + gn.MonospaceCharWidth + g.Style.ScrollbarSize + 30.0f,
                        gn.LineHeight * total_lines + (settings.ScrollBeyondLastLine ? gn.LineHeight * (content_height / gn.LineHeight) - gn.LineHeight : 0.0f));

        ImGui::SetNextWindowContentSize(dummy_bb);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, gn.Style.Colors[ImNotepadCol_WindowBg]);
        float padding = g.Style.FramePadding.y <= 2.0f ? (3.0f - g.Style.FramePadding.y) : 0.0f;
        ImGui::BeginChild(window_label_id, ImVec2(0.0f, -(ImGui::GetFrameHeight() + padding)), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PopStyleColor();

        if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow))
            ImGui::SetKeyboardFocusHere();

        // Calculating the visible range of lines
        file->RenderedLines.first = (size_t)(ImGui::GetScrollY() / gn.LineHeight) + 1; // ImNotepad uses 1-based indexing for rows and columns
        file->RenderedLines.last = file->RenderedLines.first + (size_t)(ImGui::GetWindowSize().y / gn.LineHeight);
        if(file->RenderedLines.last > total_lines)
            file->RenderedLines.last = total_lines;

        UpdateSyntaxHighlighting(file);

        // Calculating the width of the gutter
        size_t count_digits = ImCountDigitsFromNum(total_lines);
        file->GutterWidth = gn.MonospaceCharWidth * ((count_digits <= 3 ? 4 : count_digits + 1) + 1);

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

        file->GutterBB = ImRect(canvas_pos.x + ImGui::GetScrollX(),
                                canvas_pos.y + ImGui::GetScrollY(),
                                canvas_pos.x + ImGui::GetScrollX() + file->GutterWidth,
                                canvas_pos.y + ImGui::GetWindowSize().y + ImGui::GetScrollY());

        file->ContentBB = ImRect(file->GutterBB.Max.x + gn.Style.ContentPadding,
                                 file->GutterBB.Min.y,
                                 file->GutterBB.Max.x + ImGui::GetWindowSize().x - file->GutterWidth,
                                 file->GutterBB.Max.y);

        file->Buffer.GetLineContent(&file->Cursor.CurrentLine, file->Cursor.LineIdx);

        RenderGutter(file);
        RenderContent(file);
        RenderCursor(file);

        if(ImGui::IsWindowFocused())
        {
            g.IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

            if(UpdateKeyboardInputs(file))
            {
                // Recalculate the maximum content width based on the largest line
                ImString& line_text_buf = gn.TextBuffer;
                file->Buffer.GetLineContent(&line_text_buf, file->Buffer.GetWidestLineIndex(gn.Settings.IndentSize));
                file->LineMaxWidth = ImCalcTextWidth(line_text_buf);
            }

            UpdateMouseInputs(file);

            RenderTokenAndScopesInspector(file);
        }
        else
            g.IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if(ImGui::GetScrollMaxX() > 0.0f)
            RenderScrollbar(file, ImGuiAxis_X);

        if(ImGui::GetScrollMaxY() > 0.0f)
            RenderScrollbar(file, ImGuiAxis_Y);

        /*
          The file content (text) is displayed within a child window, which acts as an independent area
          with its own scrolling mechanism and other functional features.To access the functions and settings
          of this child window from outside the "BeginChild/EndChild" block, we must store a pointer to its instance.
          This pointer can subsequently be used to interact with the child window from anywhere in the code.
         */
        gn.ChildWindow = g.CurrentWindow;
        ImGui::EndChild();

        RenderFileInfo(file);

        if(file->Config.CursorLineIdx != 0 || file->Config.CursorColumnIdx != 0)
        {
            file->SetCursorPosition(file->Config.CursorLineIdx, file->Config.CursorColumnIdx, ImNotepadCursorAlignmentFlags_AlignLeftTop);
            file->Config.CursorLineIdx = 0;
            file->Config.CursorColumnIdx = 0;
        }

        if(file->Config.ScrollX != 0.0f || file->Config.ScrollY != 0.0f)
        {
            ImGui::SetScrollX(gn.ChildWindow, file->Config.ScrollX);
            ImGui::SetScrollY(gn.ChildWindow, file->Config.ScrollY);
            file->Config.ScrollX = 0.0f;
            file->Config.ScrollY = 0.0f;
        }

        ImGui::PopStyleVar();

        if(!file->IsOpened)
        {
            ImGui::SetScrollX(gn.ChildWindow, 0.0f);
            ImGui::SetScrollY(gn.ChildWindow, 0.0f);
        }
    }

    gn.RootWindow = g.CurrentWindow;
    ImGui::End();
}
#endif // #ifndef IMGUI_DISABLE
