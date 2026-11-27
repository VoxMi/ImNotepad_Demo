//-----------------------------------------------------------------------------
// Copyright (c) 2023 Cameron DaCamara
// Copyright (c) 2026 VoxMi
//
// Portions of this code are based on the original work by Cameron DaCamara:
// https://github.com/cdacamar/fredbuf
// This file has been significantly modified by VoxMi to include UTF-8
// support, custom line metrics and Dear ImGui integration.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//-----------------------------------------------------------------------------

#pragma once

#include <forward_list>
#include <memory>
#include <vector>
#include <stdint.h>

#include "imstring/imstring.h"

// This is a C++ implementation of the textbuf data structure described in
// https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation. The differences are
// that this version is based on immutable data structures to achieve fast undo/redo.

// The concept for the RB tree is borrowed from
// https://bartoszmilewski.com/2013/11/25/functional-data-structures-in-c-trees/ (Bartosz Milewski)
// but some other pieces were assembled together from other sources, in particular deletion was
// gathered from https://github.com/dotnwat/persistent-rbtree after having implemented the deletion
// described by Bartosz, it was discovered that the algorithm was not always preserving the RB
// invariants.

#ifdef _MSC_VER
#pragma warning (disable: 26812)
#endif

namespace FredBuf
{
    typedef ImString        String;
    typedef ImStringView    StringView;

    enum EndOfLineSequence
    {
        EndOfLineSequence_LF,
        EndOfLineSequence_CR,
        EndOfLineSequence_CRLF,
    };

    struct Range
    {
        size_t first;
        size_t last; // Does not include LF

        Range()                             : first(0), last(0) {}
        Range(size_t _first, size_t _last)  : first(_first), last(_last) {}

        size_t length() const { return last - first; }
    };

    struct Position
    {
        // 1-based
        size_t line;
        size_t column;
    };

    struct OwningSnapshot;      // Owning snapshot owns its own buffer data (performs a lightweight copy) so that even if the original tree is destroyed, the owning snapshot can still reference the underlying text.
    struct ReferenceSnapshot;   // Reference snapshot owns no data and is only valid for as long as the original tree buffers are valid.

    struct BufferCursor
    {
        size_t line;    // Relative line in the current buffer
        size_t column;  // Column into the current line

        BufferCursor()                              : line(0), column(0) {}
        BufferCursor(size_t _line, size_t _column)  : line(_line), column(_column) {}

        bool operator==(const BufferCursor& other) const { return line == other.line && column == other.column; }
    };

    // Line length tracking for UTF-8 characters, excluding byte fields, but taking into account the size of tabs
    struct LineMetrics
    {
        size_t cp_max_count;               // Max codepoints in a single line
        size_t cp_max_count_line_index;    // Absolute line number (0-based) for the max line
        size_t cp_prefix_length;           // Codepoints in the first line of the node
        size_t cp_suffix_length;           // Codepoints in the last line of the node

        size_t tab_count_in_max_line;      // Number of tabs in the line with max codepoints
        size_t prefix_tab_count;           // Tabs in the first line of the node
        size_t suffix_tab_count;           // Tabs in the last line of the node

        size_t visual_max_width;            // Max visual width of a single line (codepoints + tab expansion)
        size_t visual_max_width_line_index; // Line index (0-based, relative to the piece) where visual_max_width is reached
    };

    struct Piece
    {
        size_t          index; // Index into a buffer in FredBuf. This could be an immutable buffer or the mutable buffer.
        BufferCursor    first;
        BufferCursor    last;
        size_t          length;
        size_t          newline_count;
        LineMetrics     metrics; // Metrics for a specific piece of text (local)
    };

    struct NodeData
    {
        Piece       piece;
        size_t      left_subtree_length;
        size_t      left_subtree_lf_count;
        size_t      tab_size;
        LineMetrics subtree_metrics; // Metrics for the entire subtree (aggregated)

        NodeData(const Piece& p, size_t _tab_size) : piece(p), left_subtree_length(0), left_subtree_lf_count(0), tab_size(_tab_size),
                                   subtree_metrics
                                   {
                                        p.metrics.cp_max_count,
                                        p.metrics.cp_max_count_line_index,
                                        p.metrics.cp_prefix_length,
                                        p.metrics.cp_suffix_length,
                                        p.metrics.tab_count_in_max_line,
                                        p.metrics.prefix_tab_count,
                                        p.metrics.suffix_tab_count,
                                        p.metrics.visual_max_width,
                                        p.metrics.visual_max_width_line_index
                                   } {}
    };

    struct RedBlackTree
    {
        enum Color
        {
            Red,
            Black,
            DoubleBlack
        };

        struct Node;
        typedef std::shared_ptr<const Node> NodePtr;

        NodePtr root_node;

        struct Node
        {
            Color       color;
            NodePtr     left;
            NodeData    data;
            NodePtr     right;

            Node(Color c, const NodePtr& lft, const NodeData& data, const NodePtr& rgt) : color(c), left(lft), data(data), right(rgt) {}
        };

        RedBlackTree() {};
        RedBlackTree(const NodePtr& node)                                                               : root_node(node) {};
        RedBlackTree(Color c, const RedBlackTree& lft, const NodeData& val, const RedBlackTree& rgt)    : root_node(std::make_shared<Node>(c, lft.root_node, attribute(val, lft, rgt), rgt.root_node)) {}

        NodeData            attribute(const NodeData& data, const RedBlackTree& left, const RedBlackTree& right);

        // Queries
        const Node*         root_ptr() const    { return root_node.get(); };
        bool                is_empty() const    { return !root_node; };
        const NodeData&     root() const        { assert(!is_empty()); return root_node->data; };
        RedBlackTree        left() const        { assert(!is_empty()); return RedBlackTree(root_node->left); };
        RedBlackTree        right() const       { assert(!is_empty()); return RedBlackTree(root_node->right); };
        Color               root_color() const  { assert(!is_empty()); return root_node->color; };

        // Helpers
        bool                operator==(const RedBlackTree& other) const { return root_node == other.root_node; }

        // Mutators
        RedBlackTree        insert(const NodeData& x, size_t at) const  { RedBlackTree t = ins(x, at, 0); return RedBlackTree(Color::Black, t.left(), t.root(), t.right()); }
        RedBlackTree        remove(size_t at) const                     { RedBlackTree t = rem(*this, at, 0); if(t.is_empty()) return RedBlackTree(); return RedBlackTree(Color::Black, t.left(), t.root(), t.right()); };

        // Removal
        static RedBlackTree fuse(const RedBlackTree& left, const RedBlackTree& right);
        static RedBlackTree balance(const RedBlackTree& node);
        static RedBlackTree balance_left(const RedBlackTree& left);
        static RedBlackTree balance_right(const RedBlackTree& right);
        static RedBlackTree remove_left(const RedBlackTree& root, size_t at, size_t total);
        static RedBlackTree remove_right(const RedBlackTree& root, size_t at, size_t total);
        static RedBlackTree rem(const RedBlackTree& root, size_t at, size_t total);

        // Insertion
        RedBlackTree        ins(const NodeData& x, size_t at, size_t total_offset) const;
        static RedBlackTree balance(Color c, const RedBlackTree& lft, const NodeData& x, const RedBlackTree& rgt);

        bool                doubled_left() const    { return !is_empty() && root_color() == Color::Red && !left().is_empty() && left().root_color() == Color::Red; }
        bool                doubled_right() const   { return !is_empty() && root_color() == Color::Red && !right().is_empty() && right().root_color() == Color::Red; }

        // General
        RedBlackTree        paint(Color c) const    { assert(!is_empty()); return RedBlackTree(c, left(), root(), right()); }
    };

    struct UndoRedoEntry
    {
        RedBlackTree        root;
        size_t              op_offset;      // The byte offset in the document where the edit operation (insertion, deletion, or replacement) occurred
        size_t              old_len;        // The number of bytes at position op_offset before the operation
        size_t              new_len;        // The number of bytes at position op_offset after the operation
        size_t              sel_start;      // Start byte index of the text selection at the moment this history entry was created
        size_t              sel_end;        // End byte index of the text selection at the moment this history entry was created
        uint64_t            version_id;     // A unique, monotonically increasing identifier for the document state
        EndOfLineSequence   eol_sequence;   // The active End-of-Line sequence (LF, CR, or CRLF) at the time this state was recorded
    };

    // We need the ability to 'release' old entries in this stack
    typedef std::forward_list<UndoRedoEntry>    UndoStack;
    typedef std::forward_list<UndoRedoEntry>    RedoStack;

    typedef std::vector<size_t>                 LineStarts;

    struct NodePosition
    {
        const NodeData* node;           // Piece Index
        size_t          remainder;      // Remainder in current piece
        size_t          start_offset;   // Node start offset in document
        size_t          line;           // The line (relative to the document) where this node starts

        NodePosition()                                                                              : node(nullptr), remainder(0), start_offset(0), line(0) {}
        NodePosition(const NodeData* _node, size_t _remainder, size_t _start_offset, size_t _line)  : node(_node), remainder(_remainder), start_offset(_start_offset), line(_line) {}
    };

    struct CharBuffer
    {
        ImString    buffer;
        LineStarts  line_starts;

        CharBuffer() {}
        CharBuffer(ImString buf, LineStarts starts)                     : buffer(std::move(buf)), line_starts(std::move(starts)) {}
        CharBuffer(const char* data, size_t size, LineStarts starts)    : buffer(data, size), line_starts(std::move(starts)) {}
    };

    typedef std::shared_ptr<const CharBuffer>   BufferReference;
    typedef std::vector<BufferReference>        Buffers;

    struct BufferCollection
    {
        Buffers             orig_buffers;
        CharBuffer          mod_buffer;

        BufferCollection() {}
        BufferCollection(Buffers buffers)   : orig_buffers(buffers) {}

        const CharBuffer*   buffer_at(size_t index) const                                   { if(index == SIZE_MAX) return &mod_buffer; return orig_buffers[index].get(); };
        size_t              buffer_offset(size_t index, const BufferCursor& cursor) const   { const LineStarts& starts = buffer_at(index)->line_starts; return starts[cursor.line] + cursor.column; };
    };

    struct UndoRedoResult
    {
        bool                success;
        size_t              op_offset;

        // On redo: the original direction of the operation is preserved.
        // On undo: old_len and new_len are swapped relative to the original operation:
        // - If the original operation was an insertion (old=0, new=N), undo becomes a deletion (old=N, new=0).
        // - If the original operation was a deletion (old=N, new=0), undo becomes an insertion (old=0, new=N).
        // - If the original operation was a replacement(old=M, new=N), undo becomes a replacement(old=N, new=M).
        size_t              old_len;
        size_t              new_len;

        // Usage logic:
        // - If both values ​​are zero, this cancels the need to move the cursor.
        // - If both values ​​are equal, this signals the need to move the cursor only.
        // - If the values ​​are not equal and nonzero, this signals the need to move the cursor and set the selection within the specified range.
        size_t              sel_start;
        size_t              sel_end;

        uint64_t            version_id;
        EndOfLineSequence   eol_sequence;

        size_t op_size() const { return old_len > new_len ? old_len : new_len; }
    };

    struct BufferMeta
    {
        size_t              lf_count;
        size_t              total_content_length;
        size_t              cp_max_count;
        size_t              cp_max_count_line_index;
        EndOfLineSequence   eol_sequence;

        BufferMeta() : lf_count(0), total_content_length(0), cp_max_count(0), cp_max_count_line_index(0), eol_sequence(EndOfLineSequence_LF) {}

        void compute_from(const RedBlackTree& root);
    };

    struct Tree
    {
        BufferCollection    buffers;
        RedBlackTree        root;
        LineStarts          scratch_starts;
        BufferCursor        last_insert;
        size_t              end_last_insert;
        BufferMeta          meta;
        UndoStack           undo_stack;
        RedoStack           redo_stack;
        size_t              version_id; // For each change (insert or remove), the version ID is incremented by one
        size_t              tab_size;

        explicit Tree()                                                     : end_last_insert(SIZE_MAX), version_id(0), tab_size(4) {}
        explicit Tree(Buffers&& _buffers, EndOfLineSequence _eol_sequence)  : buffers(std::move(_buffers)), end_last_insert(SIZE_MAX), version_id(0), tab_size(4) { meta.eol_sequence = _eol_sequence; build_tree(); }

        // Interface.
        void                build_tree(); // Initialization after populating initial immutable buffers from ctor.

        // Manipulation.
        // When mutating the tree nodes are saved by default into the undo stack. This allows callers to suppress this behavior.
        size_t              insert(size_t offset, StringView txt, size_t sel_start = 0, size_t sel_end = 0, bool suppress_history = false);
        void                remove(size_t offset, size_t count, size_t sel_start = 0, size_t sel_end = 0, bool suppress_history = false);
        size_t              replace(size_t offset, size_t count, StringView txt, size_t sel_start = 0, size_t sel_end = 0);
        void                set_tab_size(size_t new_tab_size);
        void                push_eol_sequence(EndOfLineSequence new_eol, size_t offset, size_t sel_start = 0, size_t sel_end = 0);
        UndoRedoResult      try_undo();
        UndoRedoResult      try_redo();

        // Direct history manipulation.
        void                commit_head(size_t offset, size_t sel_start = 0, size_t sel_end = 0)    { append_undo(root, offset, 0, 0, sel_start, sel_end); }; // This will commit the current node to the history. The offset provided will be the undo point later.
        RedBlackTree        head() const                                                            { return root; };
        void                snap_to(const RedBlackTree& new_root)                                   { root = new_root; compute_buffer_meta(); }; // Snaps the tree back to the specified root. This needs to be called with a root that is derived from the set of buffers based on its creation.

        // Queries.
        Range               get_line_content(String* buf, size_t line) const;
        void                get_text_in_range(String* buf, size_t start, size_t end) const;
        Range               get_line_range(size_t line) const;
        size_t              get_widest_line_index(size_t tab_size) const; // Lazy computation of visual metrics based on current tab_size
        size_t              get_version_id() const                                                  { return version_id; }
        char                at(size_t offset) const                                                 { return char_at(&buffers, root, offset); };
        size_t              line_at(size_t offset) const                                            { if(is_empty()) return 1; NodePosition result = node_at(&buffers, root, offset); return result.line; };
        Position            get_position_at(size_t offset) const                                    { if(offset > length()) offset = length(); size_t line = line_at(offset); return { line, offset - get_line_range(line).first + 1}; }

        size_t              length() const                                                          { return meta.total_content_length; }
        bool                is_empty() const                                                        { return meta.total_content_length == 0; }
        size_t              line_feed_count() const                                                 { return meta.lf_count; }
        size_t              line_count() const                                                      { return meta.lf_count + 1; }
        EndOfLineSequence   eol_sequence() const                                                    { return meta.eol_sequence; }

        OwningSnapshot      owning_snap() const;
        ReferenceSnapshot   ref_snap() const;

        void                internal_insert(size_t offset, StringView txt);
        void                internal_remove(size_t offset, size_t count);

        typedef size_t(*Accumulator)(const BufferCollection*, const Piece&, size_t);

        template <Accumulator accumulate>
        static void         line_start(size_t* offset, const BufferCollection* buffers, const RedBlackTree& node, size_t line);
        static size_t       accumulate_value(const BufferCollection* buffers, const Piece& piece, size_t index);
        static void         populate_from_node(String* buf, const BufferCollection* buffers, const RedBlackTree& node);
        static void         populate_from_node(String* buf, const BufferCollection* buffers, const RedBlackTree& node, size_t line_index);
        static size_t       line_feed_count(const BufferCollection* buffers, size_t index, const BufferCursor& start, const BufferCursor& end);
        static NodePosition node_at(const BufferCollection* buffers, RedBlackTree node, size_t off);
        static BufferCursor buffer_position(const BufferCollection* buffers, const Piece& piece, size_t remainder);
        static char         char_at(const BufferCollection* buffers, const RedBlackTree& node, size_t offset);

        // Direct mutations.
        void                assemble_line(String* buf, const RedBlackTree& node, size_t line) const;
        Piece               build_piece(StringView txt);
        void                combine_pieces(NodePosition existing_piece, Piece new_piece);
        void                remove_node_range(NodePosition first, size_t length);
        void                append_undo(const RedBlackTree& old_root, size_t op_offset, size_t old_len, size_t new_len, size_t sel_start, size_t sel_end);
        void                append_undo(const RedBlackTree& old_root, size_t op_offset, size_t old_len, size_t new_len, size_t sel_start, size_t sel_end, EndOfLineSequence line_ending);
        void                compute_buffer_meta() { meta.compute_from(root); };
    };

    struct OwningSnapshot
    {
        RedBlackTree        root;
        BufferMeta          meta;
        BufferCollection    buffers; // This should be fairly lightweight. The original buffers will retain the majority of the memory consumption.

        explicit OwningSnapshot(const Tree* tree)                           : root(tree->root), meta(tree->meta), buffers(tree->buffers) {};
        explicit OwningSnapshot(const Tree* tree, const RedBlackTree& dt)   : root(dt), meta(tree->meta), buffers(tree->buffers) { meta.compute_from(dt); };

        // Queries.
        void                get_line_content(String* buf, size_t line) const;
        size_t              line_at(size_t offset) const;
        Range               get_line_range(size_t line) const;

        bool                is_empty() const                                { return meta.total_content_length == 0; }
        size_t              line_count() const                              { return meta.lf_count + 1; }
    };

    struct ReferenceSnapshot
    {
        RedBlackTree            root;
        BufferMeta              meta;
        const BufferCollection* buffers; // A reference to the underlying tree buffers.

        explicit ReferenceSnapshot(const Tree* tree)                            : root(tree->root), meta(tree->meta), buffers(&tree->buffers) {}
        explicit ReferenceSnapshot(const Tree* tree, const RedBlackTree& dt)    : root(dt), meta(tree->meta), buffers(&tree->buffers) { meta.compute_from(dt);}

        ReferenceSnapshot(const ReferenceSnapshot& other)                       : root(other.root), meta(other.meta), buffers(other.buffers) {}
        ReferenceSnapshot& operator=(const ReferenceSnapshot& other)            { root = other.root; meta = other.meta; buffers = other.buffers; }

        // Queries.
        void                get_line_content(String* buf, size_t line) const;
        size_t              line_at(size_t offset) const;
        Range               get_line_range(size_t line) const;

        bool                is_empty() const                                { return meta.total_content_length == 0; }
        size_t              line_count() const                              { return meta.lf_count + 1; }
    };

    struct TreeBuilder
    {
        Buffers     buffers;
        LineStarts  scratch_starts;
        size_t      total_lf_count;
        size_t      total_cr_count;
        size_t      total_crlf_count;

        TreeBuilder() : total_lf_count(0), total_cr_count(0), total_crlf_count(0) {}

        void accept(StringView txt);
        Tree create();
    };

    struct TreeWalker
    {
        enum Direction
        {
            Left,
            Center,
            Right
        };

        struct StackEntry
        {
            RedBlackTree    node;
            Direction       dir;

            StackEntry(RedBlackTree _node) : node(_node), dir(Direction::Left) {}
        };

        const BufferCollection* buffers;
        RedBlackTree            root;
        BufferMeta              meta;
        std::vector<StackEntry> stack;
        size_t                  total_offset;
        const char*             first_ptr;
        const char*             last_ptr;

        TreeWalker(const Tree* tree, size_t offset = 0);
        TreeWalker(const OwningSnapshot* snap, size_t offset = 0);
        TreeWalker(const ReferenceSnapshot* snap, size_t offset = 0);
        TreeWalker(const TreeWalker&) = delete;

        char                current();
        char                next();
        void                seek(size_t offset);
        bool                exhausted() const;

        size_t              remaining() const   { return meta.total_content_length - total_offset;}
        size_t              offset() const      { return total_offset; }

        TreeWalker&         operator++()        { return *this; } // For Iterator-like behavior.
        char                operator*()         { return next(); }

        void                populate_ptrs();
        void                fast_forward_to(size_t offset);
    };

    struct ReverseTreeWalker
    {
        enum Direction
        {
            Left,
            Center,
            Right
        };

        struct StackEntry
        {
            RedBlackTree    node;
            Direction       dir;

            StackEntry(RedBlackTree _node) : node(_node), dir(Direction::Right) {}
        };

        const BufferCollection* buffers;
        RedBlackTree            root;
        BufferMeta              meta;
        std::vector<StackEntry> stack;
        size_t                  total_offset;
        const char*             first_ptr;
        const char*             last_ptr;

        ReverseTreeWalker(const Tree* tree, size_t offset = 0);
        ReverseTreeWalker(const OwningSnapshot* snap, size_t offset = 0);
        ReverseTreeWalker(const ReferenceSnapshot* snap, size_t offset = 0);
        ReverseTreeWalker(const TreeWalker&) = delete;

        char                current();
        char                next();
        void                seek(size_t offset);
        bool                exhausted() const;

        size_t              remaining() const   { return total_offset + 1; };
        size_t              offset() const      { return total_offset; }

        ReverseTreeWalker&  operator++()        { return *this; } // For Iterator-like behavior.
        char                operator*()         { return next(); }

        void                populate_ptrs();
        void                fast_forward_to(size_t offset);
    };
}
