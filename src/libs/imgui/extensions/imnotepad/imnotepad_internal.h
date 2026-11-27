#pragma once
#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

#include "fredbuf.h"
#include "imstring/imstring.h"
#include "tree_sitter/api.h"

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] ImNotepad Enums
//-----------------------------------------------------------------------------

enum ImNotepadCursorAlignmentFlags_
{
    ImNotepadCursorAlignmentFlags_None              = 0,
    ImNotepadCursorAlignmentFlags_AlignLeft         = 1 << 0,
    ImNotepadCursorAlignmentFlags_AlignRight        = 1 << 1,
    ImNotepadCursorAlignmentFlags_AlignTop          = 1 << 2,
    ImNotepadCursorAlignmentFlags_AlignBottom       = 1 << 3,
    ImNotepadCursorAlignmentFlags_AlignCenter       = 1 << 4,
    ImNotepadCursorAlignmentFlags_AlignLeftTop      = ImNotepadCursorAlignmentFlags_AlignLeft | ImNotepadCursorAlignmentFlags_AlignTop,
    ImNotepadCursorAlignmentFlags_AlignLeftBottom   = ImNotepadCursorAlignmentFlags_AlignLeft | ImNotepadCursorAlignmentFlags_AlignBottom,
    ImNotepadCursorAlignmentFlags_AlignRightTop     = ImNotepadCursorAlignmentFlags_AlignRight | ImNotepadCursorAlignmentFlags_AlignTop,
    ImNotepadCursorAlignmentFlags_AlignRightBottom  = ImNotepadCursorAlignmentFlags_AlignRight | ImNotepadCursorAlignmentFlags_AlignBottom,
};

typedef int ImNotepadCursorAlignmentFlags;  // -> enum ImNotepadCursorAlignmentFlags_

//-----------------------------------------------------------------------------
// [SECTION] Text helpers and storage
//-----------------------------------------------------------------------------

struct ImNotepadLine
{
    ImString        Text;
    FredBuf::Range  Range;
};

struct ImNotepadPosition
{
    size_t Line;
    size_t Column;
};

struct ImNotepadTextBuffer
{
    FredBuf::Tree         PieceTree;
    FredBuf::TreeBuilder  PieceTreeBuilder;

    ImNotepadTextBuffer()                                                                                                                       { PieceTree = PieceTreeBuilder.create(); }
    ImNotepadTextBuffer(char* src, size_t size)                                                                                                 { Assign(src, size); }

    void                        Assign(const char* src, size_t size)                                                                            { PieceTreeBuilder.accept(ImStringView(src, size)); PieceTree = PieceTreeBuilder.create(); };

    FredBuf::Range              GetLineContent(ImString* buf, size_t line)                                                                      { return PieceTree.get_line_content(buf, line); };
    void                        GetLineContent(ImNotepadLine* buf, size_t line)                                                                 { buf->Range = PieceTree.get_line_content(&buf->Text, line); }
    void                        GetTextInRange(ImString* buf, size_t start, size_t end)                                                         { PieceTree.get_text_in_range(buf, start, end); };
    char                        GetCharAt(size_t offset) const                                                                                  { return PieceTree.at(offset); };
    size_t                      GetLineIndexAt(size_t offset) const                                                                             { return PieceTree.line_at(offset); };
    FredBuf::Position           GetPositionAt(size_t offset) const                                                                              { return PieceTree.get_position_at(offset); }
    FredBuf::Range              GetLineRange(size_t line) const                                                                                 { return PieceTree.get_line_range(line); }
    size_t                      GetLineCount() const                                                                                            { return PieceTree.line_count() ;}
    size_t                      GetSize() const                                                                                                 { return PieceTree.length(); }
    size_t                      GetVersionId() const                                                                                            { return PieceTree.get_version_id(); }
    size_t                      GetWidestLineIndex(int tab_size) const                                                                          { return PieceTree.get_widest_line_index(tab_size); }
    FredBuf::EndOfLineSequence  GetEndOfLineSequence() const                                                                                    { return PieceTree.eol_sequence(); }

    void                        SetTabSize(size_t new_tab_size)                                                                                 { PieceTree.set_tab_size(new_tab_size); }
    void                        SetEndOfLineSequence(FredBuf::EndOfLineSequence new_eol, size_t offset, size_t sel_start, size_t sel_end)       { PieceTree.push_eol_sequence(new_eol, offset, sel_start, sel_end); }

    FredBuf::Range              GetWordBoundariesByByteIndex(size_t offset);

    size_t                      Insert(size_t pos, const char* text, size_t sel_start = 0, size_t sel_end = 0, bool suppress_history = false)   { return PieceTree.insert(pos, text, sel_start, sel_end, suppress_history); };
    void                        Remove(size_t pos, size_t len, size_t sel_start = 0, size_t sel_end = 0, bool suppress_history = false)         { PieceTree.remove(pos, len, sel_start, sel_end, suppress_history); };
    size_t                      Replace(size_t pos, size_t len, const char* text, size_t sel_start = 0, size_t sel_end = 0)                     { return PieceTree.replace(pos, len, text, sel_start, sel_end); };
};

//-----------------------------------------------------------------------------
// [SECTION] (ImNotepadTextCursor, ImNotepadMouseState)
//-----------------------------------------------------------------------------

struct ImNotepadTextCursor
{
    size_t          LineIdx         = 1; // Current string (line) where the cursor is located
    size_t          ColumnIdx       = 1; // Current index of a character in a string (line) without taking into account the length of UTF-8 characters
    size_t          GoalColumnIdx   = 1; // The remembered column position where the cursor should return after moving up/down

    size_t          ByteIdx         = 0; // Current global index of the character in the buffer, taking into account the length of UTF-8
    size_t          ByteIdxPrev     = 0; // Auxiliary previous value of the global character index for the text selection mechanism

    ImNotepadLine   CurrentLine;
    ImVec2          BlinkingPos;
    float           ShowTime        = 0.0f;
    bool            Show            = true;
    bool            DisableBlinking = false;
};

struct ImNotepadMouseState
{
    bool    InCanvas                = false;
    bool    DragStartedInCanvas     = false;
    float   ScrollAccX              = 0.0f;
    float   ScrollAccY              = 0.0f;
};

//-----------------------------------------------------------------------------
// [SECTION] Tree-sitter (incremental parsing and syntax highlighting)
//-----------------------------------------------------------------------------

struct ImNotepadLanguage
{
    void*               DynLibHandle    = nullptr;
    const TSLanguage*   Grammar         = nullptr;
    TSQuery*            Query           = nullptr;
    ImString            Name;
    ImString            DisplayName;

    ~ImNotepadLanguage();
};

struct ImNotepadLanguageDescriptor
{
    const char* DisplayName;
    const char* Name;
    const char* Extensions;
};

struct ImNotepadHighlightSpan
{
    const char* Name;
    size_t      StartByte;
    size_t      EndByte;
    ImU32       Color;
};

// Universal predicate argument
struct TSQueryPredicateArg
{
    TSQueryPredicateStepType Type = TSQueryPredicateStepTypeDone;
    uint32_t CaptureID;
    ImString StringVal;
};

// Pre-parsed predicate
struct TSQueryParsedPredicate
{
    const char* Name            = nullptr;
    uint32_t    NameHash        = 0;
    uint32_t    ArgsStartIndex  = 0;
    uint32_t    ArgsCount       = 0;
};

// Range of predicates for a single pattern
struct TSQueryPredicateRange
{
    uint32_t StartIndex         = 0;
    uint32_t PredicatesCount    = 0;
};

// Using a flat structure when working with ImVector.
// This avoids hidden allocations and shallow copying issues.
struct TSQueryCache
{
    TSQuery*                            Query = nullptr;
    ImVector<TSQueryParsedPredicate>    Predicates;
    ImVector<TSQueryPredicateArg>       PredicateArgs;
    ImVector<TSQueryPredicateRange>     PatternRanges;
};

struct TSInputPayload
{
    char*           ReadBuf;
    size_t          ReadBufSize;
    FredBuf::Tree*  PieceTree;
};

struct ImNotepadTreeSitter
{
    TSParser*                           Parser          = nullptr;
    TSTree*                             SyntaxTree      = nullptr;
    ImNotepadLanguage*                  Grammar         = nullptr;
    ImNotepadTextBuffer*                TextBuffer      = nullptr;
    ImVector<ImNotepadHighlightSpan>    Highlights;
    TSQueryCache                        QueryCache;
    char                                ReadBuf[256];
    bool                                IsDirty         = false;

    // Scratch buffers for storing the final color and names of each byte in the visible range
    ImVector<ImU32>                     ScratchByteColors;
    ImVector<const char*>               ScratchByteNames;
    size_t                              ScratchStartByte    = 0;
    size_t                              ScratchEndByte      = 0;
    bool                                ScratchIsValid      = false;

    ~ImNotepadTreeSitter();

    static const char*                  ReadTextChunk(void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read);
    bool                                CompareCaptureWithLiteral(const TSQueryMatch& match, uint32_t capture_id, FredBuf::Tree* piece_tree, const char* literal, size_t literal_len);
    bool                                CompareCaptures(const TSQueryMatch& match, uint32_t lhs_capture_id, uint32_t rhs_capture_id, FredBuf::Tree* piece_tree);
    TSQueryPredicateArg                 QueryPredicateArgFromStep(TSQuery* query, const TSQueryPredicateStep& step);
    bool                                QueryEvaluatePredicates(const TSQueryParsedPredicate* predicates, const TSQueryPredicateArg* cached_args, uint32_t predicate_count, const TSQueryMatch& match, FredBuf::Tree* piece_tree);
    void                                CacheQueryPredicates(ImNotepadTreeSitter& ts, TSQuery* query);
    bool                                ResolveLanguageForExtension(const char* language, const char* extension);

    void                                TreeEdit(size_t start_byte, size_t old_len, size_t new_len);
};

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadTextFile
//-----------------------------------------------------------------------------

struct ImNotepadTextFile
{
    ImString                    Name;
    ImString                    FullPathName;
    ImNotepadTextFileConfig     Config;
    ImNotepadTextBuffer         Buffer;
    ImNotepadTextCursor         Cursor;
    ImNotepadTreeSitter         TreeSitter;
    ImNotepadLanguage*          Language            = nullptr;
    FredBuf::Range              RenderedLines;
    FredBuf::Range              RenderedLinesPrev;
    ImRect                      GutterBB;
    ImRect                      ContentBB;
    float                       GutterWidth         = 0.0f;
    float                       LineMaxWidth        = 0.0f;
    size_t                      SelectionStart      = 0;
    size_t                      SelectionEnd        = 0;
    size_t                      SavedVersionId      = 0;
    bool                        HasSelection        = false;
    bool                        HasSelectionWord    = false;
    bool                        IsOpened            = false;

    ImNotepadTextFile()         { TreeSitter.TextBuffer = &Buffer; }
    ~ImNotepadTextFile()        {}

    void                        LoadContentFromMemory(const char* src, size_t size)    { Buffer.Assign(src, size); };

    FredBuf::UndoRedoResult     Undo()                  { return Buffer.PieceTree.try_undo(); }
    FredBuf::UndoRedoResult     Redo()                  { return Buffer.PieceTree.try_redo(); }

    bool                        IsDirty() const         { return SavedVersionId != Buffer.GetVersionId(); }
    void                        UpdateSavedVersionId()  { SavedVersionId = Buffer.GetVersionId(); }

    void                        SetCursorPosition(size_t line_idx, size_t column_idx, ImNotepadCursorAlignmentFlags alignment_flags = ImNotepadCursorAlignmentFlags_None);
    void                        SetCursorPositionByByteIdx(size_t byte_idx);

    void                        UpdateCursorScrollPos();
    void                        UpdateCursorScrollIntoView(ImNotepadCursorAlignmentFlags alignment_flags = ImNotepadCursorAlignmentFlags_None);
};

//-----------------------------------------------------------------------------
// [SECTION] ImNotepadFileSystem
//-----------------------------------------------------------------------------
struct ImNotepadFileSystem
{
    ImVector<ImNotepadTextFile*> TextFiles;
    ImVector<ImNotepadLanguage*> Languages;

    ~ImNotepadFileSystem()      { TextFiles.clear_delete(); Languages.clear_delete(); }

    ImNotepadTextFile*          NewFile(const char* file_name);
    ImNotepadTextFile*          TextFileRead(const char* file_name, ImNotepadTextFileConfig* file_cfg = nullptr);
    bool                        TextFileWrite(ImNotepadTextFile* file);
    void                        LoadLanguages();

    ImNotepadLanguage*          TextFileGetLanguage(ImNotepadTextFile* file);
};

//-----------------------------------------------------------------------------
// [SECTION] Token Scope Inspector
//-----------------------------------------------------------------------------

struct ImNotepadTokenScopeInspector
{
    struct CaptureInfo
    {
        const char* Name;
    };

    const char*             Name;
    ImVector<CaptureInfo>   Captures;
    ImRect                  RectBB;
    uint32_t                StartByte   = 0;
    uint32_t                EndByte     = 0;
    bool                    HasCapture  = false;

    void SetScopeBounds(ImRect rect_bb, uint32_t start_byte, uint32_t end_byte) { RectBB = rect_bb; StartByte = start_byte; EndByte = end_byte; Captures.clear(); };
    void QueryCaptures(ImNotepadTextFile* file);
};

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

struct ImNotepadContext
{
    ImGuiWindow*                    RootWindow          = nullptr;
    ImGuiWindow*                    ChildWindow         = nullptr;
    ImNotepadStyle                  Style;
    ImNotepadSettings               Settings;
    ImNotepadFileSystem             FileSystem;
    ImString                        TextBuffer;
    ImNotepadLine                   TextLineBuffer;
    ImNotepadTokenScopeInspector    TokenScopeInspector;
    ImNotepadMouseState             MouseState;
    float                           LineHeight          = 0.0f;
    float                           MonospaceCharWidth  = 0.0f;

    ImNotepadContext() { TextBuffer.reserve(512), TextLineBuffer.Text.reserve(512); FileSystem.LoadLanguages(); };
};

#ifndef GImNotepad
extern ImNotepadContext* GImNotepad;
#endif

//-----------------------------------------------------------------------------
// [SECTION] Internal API
//-----------------------------------------------------------------------------

namespace ImNotepad
{
    void Initialize(ImNotepadContext* ctx);

    void UpdateMouseWheel(ImNotepadTextFile* file);
    void UpdateMouseInputs(ImNotepadTextFile* file);
    bool UpdateKeyboardInputs(ImNotepadTextFile* file);

    void UpdateSyntaxHighlighting(ImNotepadTextFile* file);

    void RenderLineSegment(ImString* line_text, FredBuf::Range& segment_range, FredBuf::Range& trailing_whitespace_range, float pos_x, float pos_y, ImU32 text_color, size_t line_start_byte, size_t sel_start, size_t sel_end, bool has_selection);
    void RenderLine(ImNotepadTextFile* file, size_t line_idx, int& highlight_index, float pos_x, float pos_y);

    void RenderGutter(ImNotepadTextFile* file);
    void RenderContent(ImNotepadTextFile* file);
    void RenderScrollbar(ImNotepadTextFile* file, ImGuiAxis axis);
    void RenderCursor(ImNotepadTextFile* file);
    void RenderFileInfo(ImNotepadTextFile* file);
    void RenderTokenAndScopesInspector(ImNotepadTextFile* file);

    void RenderFileContent(ImNotepadTextFile* file);
}
#endif // #ifndef IMGUI_DISABLE
