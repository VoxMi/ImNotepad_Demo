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

#include "fredbuf.h"

namespace FredBuf
{
    //-----------------------------------------------------------------------------
    // [SECTION] ScopeGuard
    //-----------------------------------------------------------------------------

    template <typename F>
    struct ScopeGuard : F
    {
        ~ScopeGuard() { static_cast<F&>(*this)(); }

        // Non-copyable
        ScopeGuard(const ScopeGuard&)               = delete;
        ScopeGuard& operator=(const ScopeGuard&)    = delete;

        // Non-movable
        ScopeGuard(ScopeGuard&&)                    = delete;
        ScopeGuard& operator=(ScopeGuard&&)         = delete;
    };

    // Deduction guide
    template <typename F>
    ScopeGuard(F)->ScopeGuard<F>;

    //-----------------------------------------------------------------------------
    // Misc Helpers/Utilites
    //-----------------------------------------------------------------------------

    // Global queries
    static size_t tree_length(const RedBlackTree& root)                         { return root.is_empty() ? 0 : root.root().left_subtree_length + root.root().piece.length + tree_length(root.right()); }
    static size_t tree_lf_count(const RedBlackTree& root)                       { return root.is_empty() ? 0 : root.root().left_subtree_lf_count + root.root().piece.newline_count + tree_lf_count(root.right()); }
    static size_t tree_cp_max_count(const RedBlackTree& root)                   { return root.is_empty() ? 0 : root.root().subtree_metrics.cp_max_count; }
    static size_t tree_cp_max_count_line_index(const RedBlackTree& root)        { return root.is_empty() ? 0 : root.root().subtree_metrics.cp_max_count_line_index; }
    static size_t tree_cp_prefix_length(const RedBlackTree& root)               { return root.is_empty() ? 0 : root.root().subtree_metrics.cp_prefix_length; }
    static size_t tree_cp_suffix_length(const RedBlackTree& root)               { return root.is_empty() ? 0 : root.root().subtree_metrics.cp_suffix_length; }
    static size_t tree_tab_count_in_max_line(const RedBlackTree& root)          { return root.is_empty() ? 0 : root.root().subtree_metrics.tab_count_in_max_line; }
    static size_t tree_prefix_tab_count(const RedBlackTree& root)               { return root.is_empty() ? 0 : root.root().subtree_metrics.prefix_tab_count; }
    static size_t tree_suffix_tab_count(const RedBlackTree& root)               { return root.is_empty() ? 0 : root.root().subtree_metrics.suffix_tab_count; }
    static size_t tree_visual_max_width(const RedBlackTree& root)               { return root.is_empty() ? 0 : root.root().subtree_metrics.visual_max_width; }
    static size_t tree_visual_max_width_line_index(const RedBlackTree& root)    { return root.is_empty() ? 0 : root.root().subtree_metrics.visual_max_width_line_index; }

    static void compute_line_metrics(LineMetrics* metrics, ImStringView txt, size_t tab_size)
    {
        LineMetrics tmp_metrics = { 0 };
        size_t curent_cp_count = 0;
        size_t curent_tab_count = 0;
        size_t curent_line_index = 0;
        bool found_newline = false;

        auto calc_visual_length = [&](size_t cp_count, size_t tab_count) -> size_t { return cp_count + tab_count * (tab_size > 0 ? tab_size - 1 : 0); };

        for(size_t i = 0; i < txt.size(); ++i)
        {
            unsigned char c = (unsigned char)(txt[i]);
            if((c & 0xC0) != 0x80)
            {
                if(c == '\t')
                {
                    curent_cp_count++;
                    curent_tab_count++;
                }
                else if(c != '\n')
                    curent_cp_count++;
            }
            if(c == '\n')
            {
                size_t visual_length = calc_visual_length(curent_cp_count, curent_tab_count);
                if(visual_length > tmp_metrics.visual_max_width)
                {
                    tmp_metrics.cp_max_count = curent_cp_count;
                    tmp_metrics.cp_max_count_line_index = curent_line_index;
                    tmp_metrics.tab_count_in_max_line = curent_tab_count;
                    tmp_metrics.visual_max_width = visual_length;
                    tmp_metrics.visual_max_width_line_index = curent_line_index;
                }

                if(!found_newline)
                {
                    tmp_metrics.cp_prefix_length = curent_cp_count;
                    tmp_metrics.prefix_tab_count = curent_tab_count;
                }

                found_newline = true;
                curent_line_index++;
                curent_cp_count = 0;
                curent_tab_count = 0;
                tmp_metrics.cp_suffix_length = 0;
                tmp_metrics.suffix_tab_count = 0;
            }
            else
            {
                tmp_metrics.cp_suffix_length = curent_cp_count;
                tmp_metrics.suffix_tab_count = curent_tab_count;

                if(!found_newline)
                {
                    tmp_metrics.cp_prefix_length = curent_cp_count;
                    tmp_metrics.prefix_tab_count = curent_tab_count;
                }
            }
        }

        size_t visual_length = calc_visual_length(curent_cp_count, curent_tab_count);
        if(visual_length > tmp_metrics.visual_max_width)
        {
            tmp_metrics.cp_max_count = curent_cp_count;
            tmp_metrics.cp_max_count_line_index = curent_line_index;
            tmp_metrics.tab_count_in_max_line = curent_tab_count;
            tmp_metrics.visual_max_width = visual_length;
            tmp_metrics.visual_max_width_line_index = curent_line_index;
        }

        if(!found_newline)
        {
            tmp_metrics.cp_prefix_length = curent_cp_count;
            tmp_metrics.cp_suffix_length = curent_cp_count;
            tmp_metrics.prefix_tab_count = curent_tab_count;
            tmp_metrics.suffix_tab_count = curent_tab_count;
        }

        *metrics = tmp_metrics;
    }

    static void compute_piece_metrics(Piece* piece, const BufferCollection* buffers, size_t tab_size)
    {
        const CharBuffer* buf = buffers->buffer_at(piece->index);
        size_t first = buf->line_starts[piece->first.line] + piece->first.column;
        size_t last = buf->line_starts[piece->last.line] + piece->last.column;
        ImStringView txt(buf->buffer.data() + first, last - first);
        compute_line_metrics(&piece->metrics, txt, tab_size);
    }

    static void populate_line_starts(LineStarts* starts, StringView buf)
    {
        starts->clear();
        starts->push_back(0);

        for(size_t i = 0; i < buf.size(); ++i)
        {
            if(buf[i] == '\n')
                starts->push_back(i + 1);
        }
    }

    static Piece trim_piece_right(const Tree* tree, const Piece& piece, const BufferCursor& pos)
    {
        size_t orig_end_offset = tree->buffers.buffer_offset(piece.index, piece.last);

        size_t new_end_offset = tree->buffers.buffer_offset(piece.index, pos);
        size_t new_lf_count = tree->line_feed_count(&tree->buffers, piece.index, piece.first, pos);

        size_t len_delta = orig_end_offset - new_end_offset;
        size_t new_len = piece.length - len_delta;

        Piece new_piece = piece;
        new_piece.last = pos;
        new_piece.newline_count = new_lf_count;
        new_piece.length = new_len;
        compute_piece_metrics(&new_piece, &tree->buffers, tree->tab_size);
        return new_piece;
    }

    static Piece trim_piece_left(const Tree* tree, const Piece& piece, const BufferCursor& pos)
    {
        size_t orig_start_offset = tree->buffers.buffer_offset(piece.index, piece.first);

        size_t new_start_offset = tree->buffers.buffer_offset(piece.index, pos);
        size_t new_lf_count = tree->line_feed_count(&tree->buffers, piece.index, pos, piece.last);

        size_t len_delta = new_start_offset - orig_start_offset;
        size_t new_len = piece.length - len_delta;

        Piece new_piece = piece;
        new_piece.first = pos;
        new_piece.newline_count = new_lf_count;
        new_piece.length = new_len;
        compute_piece_metrics(&new_piece, &tree->buffers, tree->tab_size);
        return new_piece;
    }

    struct ShrinkResult
    {
        Piece left;
        Piece right;

        ShrinkResult(Piece _left, Piece _right) : left(_left), right(_right) {}
    };

    static ShrinkResult shrink_piece(const Tree* tree, const Piece& piece, const BufferCursor& first, const BufferCursor& last)
    {
        Piece left = trim_piece_right(tree, piece, first);
        Piece right = trim_piece_left(tree, piece, last);

        return ShrinkResult(left, right);
    }

    struct LineWidestResult
    {
        size_t length;
        size_t index;
    };

    static LineWidestResult compute_line_widest(const RedBlackTree& node, size_t tab_size, size_t line_offset)
    {
        LineWidestResult result = { 0, 0 };

        if(node.is_empty())
            return result;

        auto calc_visual_length = [&](size_t cp_count, size_t tab_count) -> size_t { return cp_count + tab_count * (tab_size > 0 ? tab_size - 1 : 0); };

        // 1. Left subtree
        if(!node.left().is_empty())
        {
            LineWidestResult left_best_candidate = compute_line_widest(node.left(), tab_size, line_offset);
            if(left_best_candidate.length > result.length)
                result = left_best_candidate;
        }

        // 2. Boundary Left/Current
        size_t left_cur_length = calc_visual_length(
            tree_cp_suffix_length(node.left()) + node.root().piece.metrics.cp_prefix_length,
            tree_suffix_tab_count(node.left()) + node.root().piece.metrics.prefix_tab_count);
        size_t left_cur_index = line_offset + tree_lf_count(node.left());
        if(left_cur_length > result.length)
            result = { left_cur_length, left_cur_index + 1 };

        // 3. Max inside current piece
        size_t cur_length = calc_visual_length(
            node.root().piece.metrics.cp_max_count,
            node.root().piece.metrics.tab_count_in_max_line);
        size_t cur_index = line_offset + tree_lf_count(node.left()) + node.root().piece.metrics.cp_max_count_line_index;
        if(cur_length > result.length)
            result = { cur_length, cur_index + 1 };

        // 4. Boundary Current/Right
        size_t cur_right_length = calc_visual_length(
            node.root().piece.metrics.cp_suffix_length + tree_cp_prefix_length(node.right()),
            node.root().piece.metrics.suffix_tab_count + tree_prefix_tab_count(node.right()));
        size_t cur_right_index = line_offset + tree_lf_count(node.left()) + node.root().piece.newline_count;
        if(cur_right_length > result.length)
            result = { cur_right_length, cur_right_index + 1 };

        // 5. When current piece has NO newlines, the line passing through it spans all three parts: left_suffix + entire_current_piece + right_prefix.
        //    Steps 2 and 4 only see partial overlaps and underestimate the real length.
        if(node.root().piece.newline_count == 0)
        {
            size_t full_length = calc_visual_length(
                tree_cp_suffix_length(node.left()) + node.root().piece.metrics.cp_prefix_length + tree_cp_prefix_length(node.right()),
                tree_suffix_tab_count(node.left()) + node.root().piece.metrics.prefix_tab_count + tree_prefix_tab_count(node.right()));

            size_t full_index = line_offset + tree_lf_count(node.left());
            if(full_length > result.length)
                result = { full_length, full_index + 1 };
        }

        // 6. Right subtree
        if(!node.right().is_empty())
        {
            size_t right_line_offset = line_offset + tree_lf_count(node.left()) + node.root().piece.newline_count;
            LineWidestResult right_best_candidate = compute_line_widest(node.right(), tab_size, right_line_offset);
            if(right_best_candidate.length > result.length)
                result = right_best_candidate;
        }

        return result;
    }

    static RedBlackTree recompute_metrics_recursive(const RedBlackTree& node, const BufferCollection* buffers, size_t tab_size)
    {
        if(node.is_empty())
            return RedBlackTree();

        // 1. Recursively count children (bottom-up)
        RedBlackTree new_left = recompute_metrics_recursive(node.left(), buffers, tab_size);
        RedBlackTree new_right = recompute_metrics_recursive(node.right(), buffers, tab_size);

        // 2. Recalculate the metrics of the current chunk with the new tab_size
        NodeData new_data = node.root();
        new_data.tab_size = tab_size;
        compute_piece_metrics(&new_data.piece, buffers, tab_size);

        // 3. Reassemble the node (attribute will automatically recalculate subtree_metrics)
        return RedBlackTree(node.root_color(), new_left, new_data, new_right);
    }

    //-----------------------------------------------------------------------------
    // [SECTION] RedBlackTree
    //-----------------------------------------------------------------------------

    NodeData RedBlackTree::attribute(const NodeData& data, const RedBlackTree& left, const RedBlackTree& right)
    {
        NodeData new_data = data;
        new_data.left_subtree_length = tree_length(left);
        new_data.left_subtree_lf_count = tree_lf_count(left);

        size_t left_subtree_lf_count = new_data.left_subtree_lf_count;
        size_t newline_count = data.piece.newline_count;
        size_t tab_size = data.tab_size;

        size_t left_cp_suffix_length = tree_cp_suffix_length(left);
        size_t left_suffix_tab_count = tree_suffix_tab_count(left);
        size_t right_cp_prefix_length = tree_cp_prefix_length(right);
        size_t right_cp_suffix_length = tree_cp_suffix_length(right);
        size_t right_prefix_tab_count = tree_prefix_tab_count(right);
        size_t right_suffix_tab_count = tree_suffix_tab_count(right);
        size_t visual_max_width = tree_visual_max_width(left);
        size_t visual_max_width_line_index = tree_visual_max_width_line_index(left);

        auto calc_visual_length = [&](size_t cp_count, size_t tab_count) -> size_t  { return cp_count + tab_count * (tab_size > 0 ? tab_size - 1 : 0); };
        auto update_visual_max  = [&](size_t _visual_width, size_t _line_index)     { if(_visual_width > visual_max_width) { visual_max_width = _visual_width; visual_max_width_line_index = _line_index; } };

        // 1. Boundary Left/Current
        size_t visual_length = calc_visual_length(left_cp_suffix_length + data.piece.metrics.cp_prefix_length, left_suffix_tab_count + data.piece.metrics.prefix_tab_count);
        update_visual_max(visual_length, left_subtree_lf_count);

        if(newline_count > 0)
        {
            // 2. Max inside current piece
            visual_length = calc_visual_length(data.piece.metrics.cp_max_count, data.piece.metrics.tab_count_in_max_line);
            update_visual_max(visual_length, left_subtree_lf_count + data.piece.metrics.cp_max_count_line_index);

            // 3. Boundary Current/Right
            visual_length = calc_visual_length(data.piece.metrics.cp_suffix_length + right_cp_prefix_length, data.piece.metrics.suffix_tab_count + right_prefix_tab_count);
            update_visual_max(visual_length, left_subtree_lf_count + newline_count);
        }
        // No newlines in current piece: merge all three parts into one line
        else
        {
            visual_length = calc_visual_length(left_cp_suffix_length + data.piece.metrics.cp_prefix_length + right_cp_prefix_length, left_suffix_tab_count + data.piece.metrics.prefix_tab_count + right_prefix_tab_count);
            update_visual_max(visual_length, left_subtree_lf_count);
        }

        // 4. Max from right subtree
        if(!right.is_empty())
            update_visual_max(right.root().subtree_metrics.visual_max_width, left_subtree_lf_count + newline_count + right.root().subtree_metrics.visual_max_width_line_index);

        new_data.subtree_metrics.visual_max_width = visual_max_width;
        new_data.subtree_metrics.visual_max_width_line_index = visual_max_width_line_index;

        // Calculate prefix (very first line of the combined node)
        if(left_subtree_lf_count > 0)
        {
            new_data.subtree_metrics.cp_prefix_length = left.root().subtree_metrics.cp_prefix_length;
            new_data.subtree_metrics.prefix_tab_count = left.root().subtree_metrics.prefix_tab_count;
        }
        else if(newline_count > 0)
        {
            new_data.subtree_metrics.cp_prefix_length = left_cp_suffix_length + data.piece.metrics.cp_prefix_length;
            new_data.subtree_metrics.prefix_tab_count = left_suffix_tab_count + data.piece.metrics.prefix_tab_count;
        }
        // No newlines anywhere: the entire combined node is a single line
        else
        {
            new_data.subtree_metrics.cp_prefix_length = left_cp_suffix_length + data.piece.metrics.cp_prefix_length + right_cp_prefix_length;
            new_data.subtree_metrics.prefix_tab_count = left_suffix_tab_count + data.piece.metrics.prefix_tab_count + right_prefix_tab_count;
        }

        // Calculate suffix (very last line of the combined node)
        if(tree_lf_count(right) > 0)
        {
            new_data.subtree_metrics.cp_suffix_length = right.root().subtree_metrics.cp_suffix_length;
            new_data.subtree_metrics.suffix_tab_count = right.root().subtree_metrics.suffix_tab_count;
        }
        else if(newline_count > 0)
        {
            new_data.subtree_metrics.cp_suffix_length = data.piece.metrics.cp_suffix_length + right_cp_suffix_length;
            new_data.subtree_metrics.suffix_tab_count = data.piece.metrics.suffix_tab_count + right_suffix_tab_count;
        }
        // No newlines anywhere: the entire combined node is a single line
        else
        {
            new_data.subtree_metrics.cp_suffix_length = left_cp_suffix_length + data.piece.metrics.cp_prefix_length + right_cp_suffix_length;
            new_data.subtree_metrics.suffix_tab_count = left_suffix_tab_count + data.piece.metrics.prefix_tab_count + right_suffix_tab_count;
        }

        return new_data;
    }

    RedBlackTree RedBlackTree::fuse(const RedBlackTree& left, const RedBlackTree& right)
    {
        // match: (left, right)
        // case: (None, r)
        if(left.is_empty())
            return right;
        if(right.is_empty())
            return left;

        // match: (left.color, right.color)
        // case: (B, R)
        if(left.root_color() == Color::Black && right.root_color() == Color::Red)
        {
            return RedBlackTree(Color::Red,
                                fuse(left, right.left()),
                                right.root(),
                                right.right());
        }
        // case: (R, B)
        if(left.root_color() == Color::Red && right.root_color() == Color::Black)
        {
            return RedBlackTree(Color::Red,
                                left.left(),
                                left.root(),
                                fuse(left.right(), right));
        }
        // case: (R, R)
        if(left.root_color() == Color::Red && right.root_color() == Color::Red)
        {
            RedBlackTree fused = fuse(left.right(), right.left());
            if(!fused.is_empty() && fused.root_color() == Color::Red)
            {
                RedBlackTree new_left = RedBlackTree(Color::Red,
                                                     left.left(),
                                                     left.root(),
                                                     fused.left());

                RedBlackTree new_right = RedBlackTree(Color::Red,
                                                      fused.right(),
                                                      right.root(),
                                                      right.right());

                return RedBlackTree(Color::Red,
                                    new_left,
                                    fused.root(),
                                    new_right);
            }

            RedBlackTree new_right = RedBlackTree(Color::Red,
                                                  fused,
                                                  right.root(),
                                                  right.right());

            return RedBlackTree(Color::Red,
                                left.left(),
                                left.root(),
                                new_right);
        }
        // case: (B, B)
        assert(left.root_color() == right.root_color() && left.root_color() == Color::Black);
        RedBlackTree fused = fuse(left.right(), right.left());
        if(!fused.is_empty() && fused.root_color() == Color::Red)
        {
            RedBlackTree new_left = RedBlackTree(Color::Black,
                                                 left.left(),
                                                 left.root(),
                                                 fused.left());

            RedBlackTree new_right = RedBlackTree(Color::Black,
                                                  fused.right(),
                                                  right.root(),
                                                  right.right());

            return RedBlackTree(Color::Red,
                                new_left,
                                fused.root(),
                                new_right);
        }

        RedBlackTree new_right = RedBlackTree(Color::Black,
                                              fused,
                                              right.root(),
                                              right.right());

        RedBlackTree new_node = RedBlackTree(Color::Red,
                                             left.left(),
                                             left.root(),
                                             new_right);

        return balance_left(new_node);
    }

    RedBlackTree RedBlackTree::balance(const RedBlackTree& node)
    {
        // Two red children
        if(!node.left().is_empty() && node.left().root_color() == Color::Red && !node.right().is_empty() && node.right().root_color() == Color::Red)
        {
            RedBlackTree l = node.left().paint(Color::Black);
            RedBlackTree r = node.right().paint(Color::Black);

            return RedBlackTree(Color::Red,
                                l,
                                node.root(),
                                r);
        }

        assert(node.root_color() == Color::Black);
        return balance(node.root_color(), node.left(), node.root(), node.right());
    }

    RedBlackTree RedBlackTree::balance_left(const RedBlackTree& left)
    {
        // match: (color_l, color_r, color_r_l)
        // case: (Some(R), ..)
        if(!left.left().is_empty() && left.left().root_color() == Color::Red)
        {
            return RedBlackTree(Color::Red,
                                left.left().paint(Color::Black),
                                left.root(),
                                left.right());
        }
        // case: (_, Some(B), _)
        if(!left.right().is_empty() && left.right().root_color() == Color::Black)
        {
            RedBlackTree new_left = RedBlackTree(Color::Black,
                                                 left.left(),
                                                 left.root(),
                                                 left.right().paint(Color::Red));
            return balance(new_left);
        }
        // case: (_, Some(R), Some(B))
        if(!left.right().is_empty() && left.right().root_color() == Color::Red && !left.right().left().is_empty() && left.right().left().root_color() == Color::Black)
        {
            RedBlackTree unbalanced_new_right = RedBlackTree(Color::Black,
                                                             left.right().left().right(),
                                                             left.right().root(),
                                                             left.right().right().paint(Color::Red));

            RedBlackTree new_right = balance(unbalanced_new_right);
            RedBlackTree new_left = RedBlackTree(Color::Black,
                                                 left.left(),
                                                 left.root(),
                                                 left.right().left().left());

            return RedBlackTree(Color::Red,
                                new_left,
                                left.right().left().root(),
                                new_right);
        }

        assert(!"impossible");
        return left;
    }

    RedBlackTree RedBlackTree::balance_right(const RedBlackTree& right)
    {
        // match: (color_l, color_l_r, color_r)
        // case: (.., Some(R))
        if(!right.right().is_empty() && right.right().root_color() == Color::Red)
        {
            return RedBlackTree(Color::Red,
                                right.left(),
                                right.root(),
                                right.right().paint(Color::Black));
        }
        // case: (Some(B), ..)
        if(!right.left().is_empty() && right.left().root_color() == Color::Black)
        {
            RedBlackTree new_right = RedBlackTree(Color::Black,
                                                  right.left().paint(Color::Red),
                                                  right.root(),
                                                  right.right());
            return balance(new_right);
        }
        // case: (Some(R), Some(B), _)
        if(!right.left().is_empty() && right.left().root_color() == Color::Red && !right.left().right().is_empty() && right.left().right().root_color() == Color::Black)
        {
            RedBlackTree unbalanced_new_left = RedBlackTree(Color::Black,
                                                            // Note: Because 'left' is red, it must have a left child.
                                                            right.left().left().paint(Color::Red),
                                                            right.left().root(), right.left().right().left());
            RedBlackTree new_left = balance(unbalanced_new_left);
            RedBlackTree new_right = RedBlackTree(Color::Black,
                                                  right.left().right().right(),
                                                  right.root(),
                                                  right.right());
            return RedBlackTree(Color::Red,
                                new_left,
                                right.left().right().root(),
                                new_right);
        }

        assert(!"impossible");
        return right;
    }

    RedBlackTree RedBlackTree::remove_left(const RedBlackTree& root, size_t at, size_t total)
    {
        RedBlackTree new_left = rem(root.left(), at, total);
        RedBlackTree new_node = RedBlackTree(Color::Red,
                                             new_left, root.root(),
                                             root.right());
        // In this case, the root was a red node and must've had at least two children.
        if(!root.left().is_empty() && root.left().root_color() == Color::Black)
            return balance_left(new_node);
        return new_node;
    }

    RedBlackTree RedBlackTree::remove_right(const RedBlackTree& root, size_t at, size_t total)
    {
        const NodeData& y = root.root();
        RedBlackTree new_right = rem(root.right(), at, total + y.left_subtree_length + y.piece.length);
        RedBlackTree new_node = RedBlackTree(Color::Red,
                                             root.left(),
                                             root.root(),
                                             new_right);
        // In this case, the root was a red node and must've had at least two children.
        if(!root.right().is_empty() && root.right().root_color() == Color::Black)
            return balance_right(new_node);
        return new_node;
    }

    RedBlackTree RedBlackTree::rem(const RedBlackTree& root, size_t at, size_t total)
    {
        if(root.is_empty())
            return RedBlackTree();

        const NodeData& y = root.root();
        if(at < total + y.left_subtree_length)
            return remove_left(root, at, total);
        if(at == total + y.left_subtree_length)
            return fuse(root.left(), root.right());

        return remove_right(root, at, total);
    }

    RedBlackTree RedBlackTree::ins(const NodeData& x, size_t at, size_t total_offset) const
    {
        if(is_empty())
            return RedBlackTree(Color::Red, RedBlackTree(), x, RedBlackTree());

        const NodeData& y = root();
        if(at < total_offset + y.left_subtree_length + y.piece.length)
            return balance(root_color(), left().ins(x, at, total_offset), y, right());

        return balance(root_color(), left(), y, right().ins(x, at, total_offset + y.left_subtree_length + y.piece.length));
    }

    RedBlackTree RedBlackTree::balance(Color c, const RedBlackTree& lft, const NodeData& x, const RedBlackTree& rgt)
    {
        if(c == Color::Black && lft.doubled_left())
        {
            return RedBlackTree(Color::Red,
                                lft.left().paint(Color::Black),
                                lft.root(),
                                RedBlackTree(Color::Black,
                                             lft.right(),
                                             x,
                                             rgt));
        }
        else if(c == Color::Black && lft.doubled_right())
        {
            return RedBlackTree(Color::Red,
                                RedBlackTree(Color::Black,
                                             lft.left(),
                                             lft.root(),
                                             lft.right().left()),
                                lft.right().root(),
                                RedBlackTree(Color::Black,
                                             lft.right().right(),
                                             x,
                                             rgt));
        }
        else if(c == Color::Black && rgt.doubled_left())
        {
            return RedBlackTree(Color::Red,
                                RedBlackTree(Color::Black,
                                             lft,
                                             x,
                                             rgt.left().left()),
                                rgt.left().root(),
                                RedBlackTree(Color::Black,
                                             rgt.left().right(),
                                             rgt.root(),
                                             rgt.right()));
        }
        else if(c == Color::Black && rgt.doubled_right())
        {
            return RedBlackTree(Color::Red,
                                RedBlackTree(Color::Black,
                                             lft,
                                             x,
                                             rgt.left()),
                                rgt.root(),
                                rgt.right().paint(Color::Black));
        }

        return RedBlackTree(c, lft, x, rgt);
    }

    //-----------------------------------------------------------------------------
    // [SECTION] BufferMeta
    //-----------------------------------------------------------------------------

    void BufferMeta::compute_from(const RedBlackTree& root)
    {
        lf_count = tree_lf_count(root);
        total_content_length = tree_length(root);
        cp_max_count = tree_cp_max_count(root);
        cp_max_count_line_index = tree_cp_max_count_line_index(root);
    }

    //-----------------------------------------------------------------------------
    // [SECTION] Tree
    //-----------------------------------------------------------------------------

    void Tree::build_tree()
    {
        buffers.mod_buffer.line_starts.clear();
        buffers.mod_buffer.buffer.clear();
        buffers.mod_buffer.line_starts.push_back(0); // In order to maintain the invariant of other buffers, the mod_buffer needs a single line-start of 0.

        const size_t buf_count = buffers.orig_buffers.size();

        size_t offset = 0;
        for(size_t i = 0; i < buf_count; ++i)
        {
            const CharBuffer& buf = *buffers.orig_buffers[i];
            assert(!buf.line_starts.empty());
            // If this immutable buffer is empty, we can avoid creating a piece for it altogether.
            if(buf.buffer.empty())
                continue;
            size_t last_line = buf.line_starts.size() - 1;

            // Create a new node that spans this buffer and retains an index to it.
            // Insert the node into the balanced tree.
            Piece piece;
            piece.index = i;
            piece.first = BufferCursor(0, 0);
            piece.last.line = last_line;
            piece.last.column = buf.buffer.size() - buf.line_starts[last_line];
            piece.length = buf.buffer.size();
            piece.newline_count = last_line; // Note: the number of newlines

            compute_line_metrics(&piece.metrics, buf.buffer.data(), tab_size);

            root = root.insert({ piece, tab_size }, offset);
            offset = offset + piece.length;
        }

        compute_buffer_meta();
    }

    size_t Tree::insert(size_t offset, StringView txt, size_t sel_start, size_t sel_end, bool suppress_history)
    {
        if(txt.empty())
            return 0;

        // Actual length after CRLF -> LF normalization
        size_t normalized_len = txt.size();
        for(size_t i = 0; i + 1 < txt.size(); ++i)
        {
            if(txt[i] == '\r' && txt[i + 1] == '\n')
                normalized_len--;
        }

        if(!suppress_history)
        {
            // New operation: create a record
            if(end_last_insert != offset || root.is_empty())
                append_undo(root, offset, 0, normalized_len, sel_start, sel_end);
            // Grouping with the previous clean insertion (the user continues typing)
            else if(!undo_stack.empty() && undo_stack.front().old_len == 0)
                undo_stack.front().new_len += normalized_len;
            // The previous operation was a deletion/replacement: no grouping is required
            else
                append_undo(root, offset, 0, normalized_len, sel_start, sel_end);
        }

        internal_insert(offset, txt);
        // Updating end_last_insert to account for normalization (the internal function is unaware of this)
        end_last_insert = offset + normalized_len;
        version_id++;

        return normalized_len; // Return the actual length of the inserted text
    }

    void Tree::remove(size_t offset, size_t count, size_t sel_start, size_t sel_end, bool suppress_history)
    {
        if(count == 0 || root.is_empty())
            return;

        if(!suppress_history)
        {
            append_undo(root, offset, count, 0, sel_start, sel_end);
            end_last_insert = SIZE_MAX; // A reset is required so that the next insertion is not grouped with this one.
        }

        internal_remove(offset, count);
        version_id++;
    }

    size_t Tree::replace(size_t offset, size_t count, StringView txt, size_t sel_start, size_t sel_end)
    {
        if(count == 0 || txt.empty())
            return 0;

        // Actual length after CRLF -> LF normalization
        size_t normalized_len = txt.size();
        for(size_t i = 0; i + 1 < txt.size(); ++i)
        {
            if(txt[i] == '\r' && txt[i + 1] == '\n')
                normalized_len--;
        }

        append_undo(root, offset, count, normalized_len, sel_start, sel_end);
        end_last_insert = SIZE_MAX;

        internal_remove(offset, count);
        internal_insert(offset, txt);

        end_last_insert = offset + normalized_len;
        version_id++;

        return normalized_len; // Return the actual length of the inserted text
    }

    void Tree::set_tab_size(size_t new_tab_size)
    {
        if(new_tab_size == 0 || tab_size == new_tab_size)
            return;
        tab_size = new_tab_size;
        root = recompute_metrics_recursive(root, &buffers, tab_size); // O(N), but called rarely (only when changing settings)
        compute_buffer_meta();
    }

    void Tree::push_eol_sequence(EndOfLineSequence new_eol, size_t offset, size_t sel_start, size_t sel_end)
    {
        if(meta.eol_sequence == new_eol)
            return;
        append_undo(root, offset, 0, 0, sel_start, sel_end);
        meta.eol_sequence = new_eol;
        version_id++;
    }

    UndoRedoResult Tree::try_undo()
    {
        if(undo_stack.empty())
            return UndoRedoResult();

        UndoRedoEntry node = undo_stack.front();

        UndoRedoEntry new_entry;
        new_entry.root          = root;
        new_entry.op_offset     = node.op_offset;
        new_entry.old_len       = node.old_len;
        new_entry.new_len       = node.new_len;
        new_entry.sel_start     = node.sel_start;
        new_entry.sel_end       = node.sel_end;
        new_entry.version_id    = version_id;
        new_entry.eol_sequence  = meta.eol_sequence;

        redo_stack.push_front(new_entry);
        undo_stack.pop_front();

        root = node.root;
        meta.eol_sequence = node.eol_sequence;
        version_id = node.version_id;
        compute_buffer_meta();

        UndoRedoResult result;
        result.success      = true;
        result.op_offset    = node.op_offset;
        // Swapping old_len and new_len
        result.old_len      = node.new_len;
        result.new_len      = node.old_len;
        result.sel_start    = node.sel_start;
        result.sel_end      = node.sel_end;
        result.version_id   = node.version_id;
        result.eol_sequence = node.eol_sequence;
        return result;
    }

    UndoRedoResult Tree::try_redo()
    {
        if(redo_stack.empty())
            return UndoRedoResult();

        UndoRedoEntry node = redo_stack.front();

        UndoRedoEntry new_entry;
        new_entry.root          = root;
        new_entry.op_offset     = node.op_offset;
        new_entry.old_len       = node.old_len;
        new_entry.new_len       = node.new_len;
        new_entry.sel_start     = node.sel_start;
        new_entry.sel_end       = node.sel_end;
        new_entry.version_id    = version_id;
        new_entry.eol_sequence  = meta.eol_sequence;

        undo_stack.push_front(new_entry);
        redo_stack.pop_front();

        root = node.root;
        meta.eol_sequence = node.eol_sequence;
        version_id = node.version_id;
        compute_buffer_meta();

        UndoRedoResult result;
        result.success  = true;
        result.op_offset    = node.op_offset;
        result.old_len      = node.old_len;
        result.new_len      = node.new_len;
        result.sel_start    = node.sel_start;
        result.sel_end      = node.sel_end;
        result.version_id   = node.version_id;
        result.eol_sequence = node.eol_sequence;

        return result;
    }

    Range Tree::get_line_content(String* buf, size_t line) const
    {
        buf->clear(); // Reset the buffer

        Range range;
        if(line == 0 || root.is_empty())
            return range;

        size_t line_offset = 0;
        line_start<&accumulate_value>(&line_offset, &buffers, root, line);

        range.first = line_offset;
        range.last = range.first;

        TreeWalker walker = TreeWalker(this, line_offset);
        while(!walker.exhausted())
        {
            char c = walker.next();
            if(c == '\n')
                break;
            buf->push_back(c);
            range.last++;
        }

        return range;
    }

    void Tree::get_text_in_range(String* buf, size_t start, size_t end) const
    {
        buf->clear(); // Reset the buffer

        if(start >= end || start >= meta.total_content_length)
            return;

        end = std::min(end, meta.total_content_length);
        const size_t length = end - start;
        buf->reserve(length);

        TreeWalker walker(this, start);
        for(size_t i = 0; i < length && !walker.exhausted(); ++i)
            buf->push_back(walker.next());

        return;
    }

    Range Tree::get_line_range(size_t line) const
    {
        Range range;
        line_start<&accumulate_value>(&range.first, &buffers, root, line);
        line_start<&accumulate_value>(&range.last, &buffers, root, line + 1);
        return range;
    }

    size_t Tree::get_widest_line_index(size_t tab_size) const
    {
        if(root.is_empty())
            return 1;
        return root.root().subtree_metrics.visual_max_width_line_index + 1;
    }

    OwningSnapshot Tree::owning_snap() const
    {
        return OwningSnapshot(this);
    }

    ReferenceSnapshot Tree::ref_snap() const
    {
        return ReferenceSnapshot(this);
    }

    void Tree::internal_insert(size_t offset, StringView txt)
    {
        assert(!txt.empty());

        ScopeGuard guard { [&] { compute_buffer_meta(); } };

        if(root.is_empty())
        {
            Piece piece = build_piece(txt);
            root = root.insert({ piece, tab_size }, 0);
            return;
        }

        NodePosition result = node_at(&buffers, root, offset);
        // If the offset is beyond the buffer, just select the last node.
        if(result.node == nullptr)
        {
            size_t off = 0;
            if(meta.total_content_length != 0)
                off = off + (meta.total_content_length - 1);
            result = node_at(&buffers, root, off);
        }

        // There are 3 cases:
        // 1. We are inserting at the beginning of an existing node.
        // 2. We are inserting at the end of an existing node.
        // 3. We are inserting in the middle of the node.
        const NodeData* node = result.node;
        size_t remainder = result.remainder;
        size_t node_start_offset = result.start_offset;

        assert(node != nullptr);
        BufferCursor insert_pos = buffer_position(&buffers, node->piece, remainder);
        // Case #1.
        if(node_start_offset == offset)
        {
            // There's a bonus case here.  If our last insertion point was the same as this piece's
            // last and it inserted into the mod buffer, then we can simply 'extend' this piece by
            // the following process:
            // 1. Fetch the previous node (if we can) and compare.
            // 2. Build the new piece.
            // 3. Remove the old piece.
            // 4. Extend the old piece's length to the length of the newly created piece.
            // 5. Re-insert the new piece.
            if(offset != 0)
            {
                NodePosition prev_node_result = node_at(&buffers, root, offset - 1);
                if(prev_node_result.node->piece.index == SIZE_MAX && prev_node_result.node->piece.last == last_insert)
                {
                    Piece new_piece = build_piece(txt);
                    combine_pieces(prev_node_result, new_piece);
                    return;
                }
            }
            Piece piece = build_piece(txt);
            root = root.insert({ piece, tab_size }, offset);
            return;
        }

        const bool inside_node = offset < node_start_offset + node->piece.length;

        // Case #2.
        if(!inside_node)
        {
            // There's a bonus case here.  If our last insertion point was the same as this piece's
            // last and it inserted into the mod buffer, then we can simply 'extend' this piece by
            // the following process:
            // 1. Build the new piece.
            // 2. Remove the old piece.
            // 3. Extend the old piece's length to the length of the newly created piece.
            // 4. Re-insert the new piece.
            if(node->piece.index == SIZE_MAX && node->piece.last == last_insert)
            {
                Piece new_piece = build_piece(txt);
                combine_pieces(result, new_piece);
                return;
            }
            // Insert the new piece at the end.
            Piece piece = build_piece(txt);
            root = root.insert({ piece, tab_size }, offset);
            return;
        }

        // Case #3.
        // The basic approach here is to split the existing node into two pieces
        // and insert the new piece in between them.
        size_t new_len_right = buffers.buffer_offset(node->piece.index, node->piece.last) - buffers.buffer_offset(node->piece.index, insert_pos);
        Piece new_piece_right = node->piece;
        new_piece_right.first = insert_pos;
        new_piece_right.length = new_len_right;
        new_piece_right.newline_count = line_feed_count(&buffers, node->piece.index, insert_pos, node->piece.last);

        compute_piece_metrics(&new_piece_right, &buffers, tab_size);

        // Remove the original node tail.
        Piece new_piece_left = trim_piece_right(this, node->piece, insert_pos);

        Piece new_piece = build_piece(txt);

        // Remove the original node.
        root = root.remove(node_start_offset);
        // Insert the left.
        root = root.insert({ new_piece_left, tab_size }, node_start_offset);
        // Insert the new mid.
        node_start_offset = node_start_offset + new_piece_left.length;
        root = root.insert({ new_piece, tab_size }, node_start_offset);
        // Insert remainder.
        node_start_offset = node_start_offset + new_piece.length;
        root = root.insert({ new_piece_right, tab_size }, node_start_offset);
    }

    void Tree::internal_remove(size_t offset, size_t count)
    {
        assert(count != 0 && !root.is_empty());

        ScopeGuard guard { [&] { compute_buffer_meta(); } };

        NodePosition first = node_at(&buffers, root, offset);
        NodePosition last = node_at(&buffers, root, offset + count);
        const NodeData* first_node = first.node;
        const NodeData* last_node = last.node;

        BufferCursor start_split_pos = buffer_position(&buffers, first_node->piece, first.remainder);

        // Simple case: the range of characters we want to delete are
        // held directly within this node.  Remove the node, resize it
        // then add it back.
        if(first_node == last_node)
        {
            BufferCursor end_split_pos = buffer_position(&buffers, first_node->piece, last.remainder);
            // We're going to shrink the node starting from the beginning.
            if(first.start_offset == offset)
            {
                // Delete the entire node.
                if(count == first_node->piece.length)
                {
                    root = root.remove(first.start_offset);
                    return;
                }
                // Shrink the node.
                Piece new_piece = trim_piece_left(this, first_node->piece, end_split_pos);
                // Remove the old one and update.
                root = root.remove(first.start_offset).insert({ new_piece, tab_size }, first.start_offset);
                return;
            }

            // Trim the tail of this piece.
            if(first.start_offset + first_node->piece.length == offset + count)
            {
                Piece new_piece = trim_piece_right(this, first_node->piece, start_split_pos);
                // Remove the old one and update.
                root = root.remove(first.start_offset).insert({ new_piece, tab_size }, first.start_offset);
                return;
            }

            // The removed buffer is somewhere in the middle.  Trim it in both directions.
            ShrinkResult new_piece = shrink_piece(this, first_node->piece, start_split_pos, end_split_pos);
            root = root.remove(first.start_offset)
                // Note: We insert right first so that the 'left' will be inserted to the right node's left.
                .insert({ new_piece.right, tab_size }, first.start_offset)
                .insert({ new_piece.left, tab_size }, first.start_offset);
            return;
        }

        // Traverse nodes and delete all nodes within the offset range. First we will build the
        // partial pieces for the nodes that will eventually make up this range.
        // There are four cases here:
        // 1. The entire first node is deleted as well as all of the last node.
        // 2. Part of the first node is deleted and all of the last node.
        // 3. Part of the first node is deleted and part of the last node.
        // 4. The entire first node is deleted and part of the last node.

        Piece new_first = trim_piece_right(this, first_node->piece, start_split_pos);
        if(last_node == nullptr)
            remove_node_range(first, count);
        else
        {
            BufferCursor end_split_pos = buffer_position(&buffers, last_node->piece, last.remainder);
            Piece new_last = trim_piece_left(this, last_node->piece, end_split_pos);
            remove_node_range(first, count);
            // There's an edge case here where we delete all the nodes up to 'last' but
            // last itself remains untouched. The test of 'remainder' in 'last' can identify
            // this scenario to avoid inserting a duplicate of 'last'.
            if(last.remainder != 0 && new_last.length != 0)
                root = root.insert({ new_last, tab_size }, first.start_offset);
        }

        if(new_first.length != 0)
            root = root.insert({ new_first, tab_size }, first.start_offset);
    }

    template <Tree::Accumulator accumulate>
    void Tree::line_start(size_t* offset, const BufferCollection* buffers, const RedBlackTree& node, size_t line)
    {
        if(node.is_empty())
            return;

        assert(line != 0);

        size_t line_index = line - 1;
        if(node.root().left_subtree_lf_count >= line_index)
            line_start<accumulate>(offset, buffers, node.left(), line);
        // The desired line is directly within the node.
        else if(node.root().left_subtree_lf_count + node.root().piece.newline_count >= line_index)
        {
            line_index -= node.root().left_subtree_lf_count;
            size_t len = node.root().left_subtree_length;
            if(line_index != 0)
                len = len + (*accumulate)(buffers, node.root().piece, line_index - 1);
            *offset = *offset + len;
        }
        // Assemble the LHS and RHS.
        else
        {
            // This case implies that 'left_subtree_lf_count' is strictly < line_index.
            // The content is somewhere in the middle.
            line_index -= node.root().left_subtree_lf_count + node.root().piece.newline_count;
            *offset = *offset + node.root().left_subtree_length + node.root().piece.length;
            line_start<accumulate>(offset, buffers, node.right(), line_index + 1);
        }
    }

    // Fetches the length of the piece starting from the first line to 'index' or to the end of the piece.
    size_t Tree::accumulate_value(const BufferCollection* buffers, const Piece& piece, size_t index)
    {
        const CharBuffer* buffer = buffers->buffer_at(piece.index);
        const LineStarts& line_starts = buffer->line_starts;

        // Extend it so we can capture the entire line content including newline.
        size_t expected_start = piece.first.line + (index + 1);
        size_t first = line_starts[piece.first.line] + piece.first.column;
        if(expected_start > piece.last.line)
        {
            size_t last = line_starts[piece.last.line] + piece.last.column;
            return last - first;
        }

        size_t last = line_starts[expected_start];
        return last - first;
    }

    void Tree::populate_from_node(String* buf, const BufferCollection* buffers, const RedBlackTree& node)
    {
        const String& buffer = buffers->buffer_at(node.root().piece.index)->buffer;
        size_t old_buf_size = buf->size();
        // We know we want the first line (index 0).
        size_t accumulated_value = accumulate_value(buffers, node.root().piece, node.root().piece.first.line);
        size_t start_offset = buffers->buffer_offset(node.root().piece.index, node.root().piece.first);

        const char* first = buffer.data() + start_offset;
        const char* last = first + accumulated_value;

        buf->resize(buf->size() + std::distance(first, last));
        std::copy(first, last, buf->data() + old_buf_size);
    }

    void Tree::populate_from_node(String* buf, const BufferCollection* buffers, const RedBlackTree& node, size_t line_index)
    {
        size_t accumulated_value = accumulate_value(buffers, node.root().piece, line_index);

        size_t prev_accumulated_value = 0;
        if(line_index != 0)
            prev_accumulated_value = accumulate_value(buffers, node.root().piece, line_index - 1);

        const String& buffer = buffers->buffer_at(node.root().piece.index)->buffer;
        size_t start_offset = buffers->buffer_offset(node.root().piece.index, node.root().piece.first);

        const char* first = buffer.data() + start_offset + prev_accumulated_value;
        const char* last = buffer.data() + start_offset + accumulated_value;
        size_t old_buf_size = buf->size();

        buf->resize(buf->size() + std::distance(first, last));
        std::copy(first, last, buf->data() + old_buf_size);
    }

    size_t Tree::line_feed_count(const BufferCollection* buffers, size_t index, const BufferCursor& start, const BufferCursor& end)
    {
        // If the end position is the beginning of a new line, then we can just return the difference in lines.
        if(end.column == 0)
            return end.line - start.line;

        const LineStarts& starts = buffers->buffer_at(index)->line_starts;
        // It means, there is no LF after end.
        if(end.line == starts.size() - 1)
            return end.line - start.line;

        // Due to the check above, we know that there's at least one more line after 'end.line'.
        size_t next_start_offset = starts[end.line + 1];
        size_t end_offset = starts[end.line] + end.column;

        // There are more than 1 character after end, which means it can't be LF.
        if(next_start_offset > end_offset + 1)
            return end.line - start.line;

        // This must be the case.  next_start_offset is a line down, so it is !possible for end_offset to be < it at this point.
        assert(end_offset + 1 == next_start_offset);
        return end.line - start.line;
    }

    NodePosition Tree::node_at(const BufferCollection* buffers, RedBlackTree node, size_t off)
    {
        size_t node_start_offset = 0;
        size_t newline_count = 0;

        while(!node.is_empty())
        {
            if(node.root().left_subtree_length > off)
                node = node.left();
            else if(node.root().left_subtree_length + node.root().piece.length > off)
            {
                node_start_offset += node.root().left_subtree_length;
                newline_count += node.root().left_subtree_lf_count;

                // Now we find the line within this piece.
                size_t remainder = off - node.root().left_subtree_length;
                BufferCursor pos = buffer_position(buffers, node.root().piece, remainder);

                // Note: since buffer_position will return us a newline relative to the buffer itself, we need
                // to retract it by the starting line of the piece to get the real difference.
                newline_count += (pos.line - node.root().piece.first.line);
                return NodePosition(&node.root(), remainder, node_start_offset, newline_count + 1);
            }
            else
            {
                // If there are no more nodes to traverse to, return this final node.
                if(node.right().is_empty())
                {
                    size_t offset_amount = node.root().left_subtree_length;
                    node_start_offset += offset_amount;
                    newline_count += node.root().left_subtree_lf_count + node.root().piece.newline_count;

                    // Now we find the line within this piece.
                    size_t remainder = node.root().piece.length;
                    return NodePosition(&node.root(), remainder, node_start_offset, newline_count + 1);
                }

                size_t offset_amount = node.root().left_subtree_length + node.root().piece.length;
                off = off - offset_amount;
                node_start_offset += offset_amount;
                newline_count += node.root().left_subtree_lf_count + node.root().piece.newline_count;
                node = node.right();
            }
        }

        return NodePosition();
    }

    BufferCursor Tree::buffer_position(const BufferCollection* buffers, const Piece& piece, size_t remainder)
    {
        const LineStarts& starts = buffers->buffer_at(piece.index)->line_starts;
        size_t start_offset = starts[piece.first.line] + piece.first.column;
        size_t offset = start_offset + remainder;

        // Binary search for 'offset' between start and ending offset.
        size_t low = piece.first.line;
        size_t high = piece.last.line;

        size_t mid = 0;
        size_t mid_start = 0;
        size_t mid_stop = 0;

        while(low <= high)
        {
            mid = low + ((high - low) / 2);
            mid_start = starts[mid];

            if(mid == high)
                break;

            mid_stop = starts[mid + 1];

            if(offset < mid_start)
                high = mid - 1;
            else if(offset >= mid_stop)
                low = mid + 1;
            else
                break;
        }

        return BufferCursor(mid, offset - mid_start);
    }

    char Tree::char_at(const BufferCollection* buffers, const RedBlackTree& node, size_t offset)
    {
        NodePosition result = node_at(buffers, node, offset);
        if(result.node == nullptr)
            return '\0';

        const CharBuffer* buffer = buffers->buffer_at(result.node->piece.index);
        size_t buf_offset = buffers->buffer_offset(result.node->piece.index, result.node->piece.first);
        const char* p = buffer->buffer.data() + buf_offset + result.remainder;
        return *p;
    }

    void Tree::assemble_line(String* buf, const RedBlackTree& node, size_t line) const
    {
        if(node.is_empty())
            return;

        size_t line_offset = 0;
        line_start<&accumulate_value>(&line_offset, &buffers, node, line);

        TreeWalker walker = TreeWalker(this, line_offset);
        while(!walker.exhausted())
        {
            char c = walker.next();
            if(c == '\n')
                break;
            buf->push_back(c);
        }
    }

    Piece Tree::build_piece(StringView txt)
    {
        size_t start_offset = buffers.mod_buffer.buffer.size();

        String tmp;
        tmp.reserve(txt.size());
        for(size_t i = 0; i < txt.size(); ++i)
        {
            if(txt[i] == '\r')
            {
                if(i + 1 < txt.size() && txt[i + 1] == '\n')
                {
                    tmp.push_back('\n');
                    i++;
                }
                else
                    tmp.push_back('\n');
            }
            else
                tmp.push_back(txt[i]);
        }

        StringView txt_normalized(tmp.data(), tmp.size());
        populate_line_starts(&scratch_starts, txt_normalized);

        BufferCursor start = last_insert;
        // Offset the new starts relative to the existing buffer.
        for(size_t& new_start : scratch_starts)
            new_start = new_start + start_offset;

        // Append new starts.
        // Note: we can drop the first start because the algorithm always adds an empty start.
        size_t new_starts_end = scratch_starts.size();
        buffers.mod_buffer.line_starts.reserve(buffers.mod_buffer.line_starts.size() + new_starts_end);
        for(size_t i = 1; i < new_starts_end; ++i)
            buffers.mod_buffer.line_starts.push_back(scratch_starts[i]);

        size_t old_size = buffers.mod_buffer.buffer.size();

        buffers.mod_buffer.buffer.resize(buffers.mod_buffer.buffer.size() + txt_normalized.size());
        char* insert_at = buffers.mod_buffer.buffer.data() + old_size;
        std::copy(txt_normalized.data(), txt_normalized.data() + txt_normalized.size(), insert_at);

        // Build the new piece for the inserted buffer.
        size_t end_offset = buffers.mod_buffer.buffer.size();
        size_t end_index = buffers.mod_buffer.line_starts.size() - 1;
        size_t end_col = end_offset - buffers.mod_buffer.line_starts[end_index];
        BufferCursor end_pos(end_index, end_col);

        Piece piece;
        piece.index = SIZE_MAX;
        piece.first = start;
        piece.last = end_pos;
        piece.length = end_offset - start_offset;
        piece.newline_count = line_feed_count(&buffers, SIZE_MAX, start, end_pos);

        compute_line_metrics(&piece.metrics, txt_normalized, tab_size);

        // Update the last insertion.
        last_insert = end_pos;
        return piece;
    }

    void Tree::combine_pieces(NodePosition existing, Piece new_piece)
    {
        assert(existing.node->piece.index == SIZE_MAX);          // This transformation is only valid under the following conditions.
        assert(existing.node->piece.last == new_piece.first);    // This assumes that the piece was just built.

        Piece old_piece = existing.node->piece;
        new_piece.first = old_piece.first;
        new_piece.newline_count = new_piece.newline_count + old_piece.newline_count;
        new_piece.length = new_piece.length + old_piece.length;

        compute_piece_metrics(&new_piece, &buffers, tab_size);

        root = root.remove(existing.start_offset).insert({ new_piece, tab_size }, existing.start_offset);
    }

    void Tree::remove_node_range(NodePosition first, size_t length)
    {
        // Remove pieces until we reach the desired length.
        size_t deleted_len = 0;
        // Because we could be deleting content in the range starting at 'first' where the piece
        // length could be much larger than 'length', we need to adjust 'length' to contain the
        // delta in length within the piece to the end where 'length' starts:
        // "abcd"  "efg"
        //     ^     ^
        //     |_____|
        //      length to delete = 3
        // P1 length: 4
        // P2 length: 3 (though this length does !matter)
        // We're going to remove all of 'P1' and 'P2' in this range and the caller will re-insert
        // these pieces with the correct lengths.  If we fail to adjust 'length' we will delete P1
        // and believe that the entire range was deleted.
        assert(first.node != nullptr);
        size_t total_length = first.node->piece.length;
        // (total - remainder) is the section of 'length' where 'first' intersects.
        length = length - (total_length - first.remainder) + total_length;
        size_t delete_at_offset = first.start_offset;
        while(deleted_len < length && first.node != nullptr)
        {
            deleted_len = deleted_len + first.node->piece.length;
            root = root.remove(delete_at_offset);
            first = node_at(&buffers, root, delete_at_offset);
        }
    }

    void Tree::append_undo(const RedBlackTree& old_root, size_t op_offset, size_t old_len, size_t new_len, size_t sel_start, size_t sel_end)
    {
        if(!redo_stack.empty())
            redo_stack.clear();

        UndoRedoEntry new_entry;
        new_entry.root          = old_root;
        new_entry.op_offset     = op_offset;
        new_entry.old_len       = old_len;
        new_entry.new_len       = new_len;
        new_entry.sel_start     = sel_start;
        new_entry.sel_end       = sel_end;
        new_entry.version_id    = version_id;
        new_entry.eol_sequence  = meta.eol_sequence;

        undo_stack.push_front(new_entry);
    }

    void Tree::append_undo(const RedBlackTree& old_root, size_t op_offset, size_t old_len, size_t new_len, size_t sel_start, size_t sel_end, EndOfLineSequence eol_sequence)
    {
        if(!redo_stack.empty())
            redo_stack.clear();

        UndoRedoEntry new_entry;
        new_entry.root          = old_root;
        new_entry.op_offset     = op_offset;
        new_entry.old_len       = old_len;
        new_entry.new_len       = new_len;
        new_entry.sel_start     = sel_start;
        new_entry.sel_end       = sel_end;
        new_entry.version_id    = version_id;
        new_entry.eol_sequence  = eol_sequence;

        undo_stack.push_front(new_entry);
    }

    //-----------------------------------------------------------------------------
    // [SECTION] OwningSnapshot
    //-----------------------------------------------------------------------------

    void OwningSnapshot::get_line_content(String* buf, size_t line) const
    {
        buf->clear(); // Reset the buffer

        if(line == 0 || root.is_empty())
            return;

        size_t line_offset = 0;
        Tree::line_start<&Tree::accumulate_value>(&line_offset, &buffers, root, line);

        TreeWalker walker = TreeWalker(this, line_offset);
        while(!walker.exhausted())
        {
            char c = walker.next();
            if(c == '\n')
                break;
            buf->push_back(c);
        }
    }

    size_t OwningSnapshot::line_at(size_t offset) const
    {
        if(is_empty())
            return 1;
        NodePosition result = Tree::node_at(&buffers, root, offset);
        return result.line;
    }

    Range OwningSnapshot::get_line_range(size_t line) const
    {
        Range range;
        Tree::line_start<&Tree::accumulate_value>(&range.first, &buffers, root, line);
        Tree::line_start<&Tree::accumulate_value>(&range.last, &buffers, root, line + 1);
        return range;
    }

    //-----------------------------------------------------------------------------
    // [SECTION] OwningSnapshot
    //-----------------------------------------------------------------------------

    void ReferenceSnapshot::get_line_content(String* buf, size_t line) const
    {
        buf->clear(); // Reset the buffer

        if(line == 0 || root.is_empty())
            return;

        size_t line_offset = 0;
        Tree::line_start<&Tree::accumulate_value>(&line_offset, buffers, root, line);
        TreeWalker walker = TreeWalker(this, line_offset);

        while(!walker.exhausted())
        {
            char c = walker.next();
            if(c == '\n')
                break;
            buf->push_back(c);
        }
    }

    size_t ReferenceSnapshot::line_at(size_t offset) const
    {
        if(is_empty())
            return 1;
        NodePosition result = Tree::node_at(buffers, root, offset);
        return result.line;
    }

    Range ReferenceSnapshot::get_line_range(size_t line) const
    {
        Range range;
        Tree::line_start<&Tree::accumulate_value>(&range.first, buffers, root, line);
        Tree::line_start<&Tree::accumulate_value>(&range.last, buffers, root, line + 1);
        return range;
    }

    //-----------------------------------------------------------------------------
    // [SECTION] TreeBuilder
    //-----------------------------------------------------------------------------

    void TreeBuilder::accept(StringView txt)
    {
        size_t crlf_count = 0;
        size_t lf_count = 0;
        size_t cr_count = 0;

        for(size_t i = 0; i < txt.size(); ++i)
        {
            if(txt[i] == '\r')
            {
                if(i + 1 < txt.size() && txt[i + 1] == '\n')
                {
                    crlf_count++;
                    i++;
                }
                else
                    cr_count++;
            }
            else if(txt[i] == '\n')
                lf_count++;
        }

        // Accumulate totals across multiple accept() calls for accurate document-wide detection
        total_crlf_count += crlf_count;
        total_lf_count += lf_count;
        total_cr_count += cr_count;

        String tmp;
        tmp.reserve(txt.size());
        for(size_t i = 0; i < txt.size(); ++i)
        {
            if(txt[i] == '\r')
            {
                if(i + 1 < txt.size() && txt[i + 1] == '\n')
                {
                    tmp.push_back('\n');
                    i++;
                }
                else
                    tmp.push_back('\n');
            }
            else
                tmp.push_back(txt[i]);
        }

        StringView txt_normalized(tmp.data(), tmp.size());
        populate_line_starts(&scratch_starts, txt_normalized);
        buffers.push_back(std::make_shared<CharBuffer>(txt_normalized.data(), txt_normalized.size(), scratch_starts));
    }

    Tree TreeBuilder::create()
    {
        // Finalize eol_sequence based on totals across all accept() calls
        size_t max_count = 0;
        EndOfLineSequence detected_eol_sequence = EndOfLineSequence_LF;
#if defined(_WIN32)
        // Priority Windows: CRLF > LF > CR
        if(total_cr_count == 0 && total_lf_count == 0 && total_crlf_count == 0)
            detected_eol_sequence = EndOfLineSequence_CRLF;
        else
        {
            if(total_cr_count > max_count)      { max_count = total_cr_count;   detected_eol_sequence = EndOfLineSequence_CR; }
            if(total_lf_count >= max_count)     { max_count = total_lf_count;   detected_eol_sequence = EndOfLineSequence_LF; }
            if(total_crlf_count >= max_count)   { max_count = total_crlf_count; detected_eol_sequence = EndOfLineSequence_CRLF; }
        }
#elif defined(__linux__)
        // Priority Linux: LF > CRLF > CR
        if(total_cr_count == 0 && total_lf_count == 0 && total_crlf_count == 0)
            detected_eol_sequence = EndOfLineSequence_LF;
        else
        {
            if(total_cr_count > max_count)      { max_count = total_cr_count;   detected_eol_sequence = EndOfLineSequence_CR; }
            if(total_crlf_count >= max_count)   { max_count = total_crlf_count; detected_eol_sequence = EndOfLineSequence_CRLF; }
            if(total_lf_count >= max_count)     { max_count = total_lf_count;   detected_eol_sequence = EndOfLineSequence_LF; }
        }
#elif defined(__APPLE__)
        // Priority macOS: CR > LF > CRLF
        if(total_cr_count == 0 && total_lf_count == 0 && total_crlf_count == 0)
            detected_eol_sequence = EndOfLineSequence_CR;
        else
        {
            if(total_crlf_count > max_count)    { max_count = total_crlf_count; detected_eol_sequence = EndOfLineSequence_CRLF; }
            if(total_lf_count >= max_count)     { max_count = total_lf_count;   detected_eol_sequence = EndOfLineSequence_LF; }
            if(total_cr_count >= max_count)     { max_count = total_cr_count;   detected_eol_sequence = EndOfLineSequence_CR; }
        }
#else
        // Priority deault (POSIX/Unix): LF > CRLF > CR
        if(total_cr_count == 0 && total_lf_count == 0 && total_crlf_count == 0)
            detected_eol_sequence = EndOfLineSequence_LF;
        else
        {
            if(total_cr_count > max_count)      { max_count = total_cr_count;   detected_eol_sequence = EndOfLineSequence_CR; }
            if(total_crlf_count >= max_count)   { max_count = total_crlf_count; detected_eol_sequence = EndOfLineSequence_CRLF; }
            if(total_lf_count >= max_count)     { max_count = total_lf_count;   detected_eol_sequence = EndOfLineSequence_LF; }
        }
#endif
        return Tree(std::move(buffers), detected_eol_sequence);
    }

    //-----------------------------------------------------------------------------
    // [SECTION] TreeWalker
    //-----------------------------------------------------------------------------

    TreeWalker::TreeWalker(const Tree* tree, size_t offset) :
        buffers(&tree->buffers),
        root(tree->root),
        meta(tree->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    TreeWalker::TreeWalker(const OwningSnapshot* snap, size_t offset) :
        buffers(&snap->buffers),
        root(snap->root),
        meta(snap->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    TreeWalker::TreeWalker(const ReferenceSnapshot* snap, size_t offset) :
        buffers(snap->buffers),
        root(snap->root),
        meta(snap->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    char TreeWalker::current()
    {
        if(first_ptr == last_ptr)
        {
            populate_ptrs();
            // If this is exhausted, we're done.
            if(exhausted())
                return '\0';
        }
        return *first_ptr;
    }

    char TreeWalker::next()
    {
        if(first_ptr == last_ptr)
        {
            populate_ptrs();
            // If this is exhausted, we're done.
            if(exhausted())
                return '\0';
            // Catchall.
            if(first_ptr == last_ptr)
                return next();
        }
        total_offset = total_offset + 1;
        return *first_ptr++;
    }

    void TreeWalker::seek(size_t offset)
    {
        stack.clear();
        stack.push_back(root);
        total_offset = offset;
        fast_forward_to(offset);
    }

    bool TreeWalker::exhausted() const
    {
        if(stack.empty())
            return true;
        // If we have !exhausted the pointers, we're still active.
        if(first_ptr != last_ptr)
            return false;
        // If there's more than one entry on the stack, we're still active.
        if(stack.size() > 1)
            return false;
        // Now, if there's exactly one entry and that entry itself is exhausted (no right subtree) we're done.
        const StackEntry& entry = stack.back();
        // We descended into a null child, we're done.
        if(entry.node.is_empty())
            return true;
        if(entry.dir == Direction::Right && entry.node.right().is_empty())
            return true;
        return false;
    }

    void TreeWalker::populate_ptrs()
    {
        if(exhausted())
            return;
        if(stack.back().node.is_empty())
        {
            stack.pop_back();
            populate_ptrs();
            return;
        }

        StackEntry& entry = stack.back();
        if(entry.dir == Direction::Left)
        {
            if(!entry.node.left().is_empty())
            {
                RedBlackTree left = entry.node.left();
                stack.back().dir = Direction::Center; // Change the dir for when we pop back.
                stack.push_back(left);
                populate_ptrs();
                return;
            }
            // Otherwise, let's visit the center, we can actually fallthrough.
            stack.back().dir = Direction::Center;
            entry.dir = Direction::Center;
        }

        if(entry.dir == Direction::Center)
        {
            const Piece& piece = entry.node.root().piece;
            const CharBuffer* buffer = buffers->buffer_at(piece.index);
            size_t first_offset = buffers->buffer_offset(piece.index, piece.first);
            size_t last_offset = buffers->buffer_offset(piece.index, piece.last);
            first_ptr = buffer->buffer.data() + first_offset;
            last_ptr = buffer->buffer.data() + last_offset;
            stack.back().dir = Direction::Right; // Change this direction.
            return;
        }

        assert(entry.dir == Direction::Right);
        RedBlackTree right = entry.node.right();
        stack.pop_back();
        stack.push_back(right);
        populate_ptrs();
    }

    void TreeWalker::fast_forward_to(size_t offset)
    {
        RedBlackTree node = root;
        while(!node.is_empty())
        {
            if(node.root().left_subtree_length > offset)
            {
                stack.back().dir = Direction::Center; // For when we revisit this node.
                node = node.left();
                stack.push_back(node);
            }
            // It is inside this node.
            else if(node.root().left_subtree_length + node.root().piece.length > offset)
            {
                stack.back().dir = Direction::Right;
                offset = offset - node.root().left_subtree_length; // Make the offset relative to this piece.
                const Piece& piece = node.root().piece;
                const CharBuffer* buffer = buffers->buffer_at(piece.index);
                size_t first_offset = buffers->buffer_offset(piece.index, piece.first);
                size_t last_offset = buffers->buffer_offset(piece.index, piece.last);
                first_ptr = buffer->buffer.data() + first_offset + offset;
                last_ptr = buffer->buffer.data() + last_offset;
                return;
            }
            else
            {
                assert(!stack.empty());
                // This parent is no longer relevant.
                stack.pop_back();
                size_t offset_amount = node.root().left_subtree_length + node.root().piece.length;
                offset = offset - offset_amount;
                node = node.right();
                stack.push_back(node);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // [SECTION] ReverseTreeWalker
    //-----------------------------------------------------------------------------

    ReverseTreeWalker::ReverseTreeWalker(const Tree* tree, size_t offset) :
        buffers(&tree->buffers),
        root(tree->root),
        meta(tree->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    ReverseTreeWalker::ReverseTreeWalker(const OwningSnapshot* snap, size_t offset) :
        buffers(&snap->buffers),
        root(snap->root),
        meta(snap->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    ReverseTreeWalker::ReverseTreeWalker(const ReferenceSnapshot* snap, size_t offset) :
        buffers(snap->buffers),
        root(snap->root),
        meta(snap->meta),
        total_offset(offset),
        first_ptr(nullptr),
        last_ptr(nullptr)
    {
        stack.emplace_back(root);
        fast_forward_to(offset);
    }

    char ReverseTreeWalker::current()
    {
        if(first_ptr == last_ptr)
        {
            populate_ptrs();
            // If this is exhausted, we're done.
            if(exhausted())
                return '\0';
        }
        return *(first_ptr - 1);
    }

    char ReverseTreeWalker::next()
    {
        if(first_ptr == last_ptr)
        {
            populate_ptrs();
            // If this is exhausted, we're done.
            if(exhausted())
                return '\0';
            // Catchall.
            if(first_ptr == last_ptr)
                return next();
        }
        // Since size_t is unsigned, this will end up wrapping, both 'exhausted' and 'remaining' will return 'true' and '0' respectively.
        total_offset = total_offset - 1;
        // A dereference is the pointer value _before_ this actual pointer, just like STL reverse iterator models.
        return *(--first_ptr);
    }

    void ReverseTreeWalker::seek(size_t offset)
    {
        stack.clear();
        stack.push_back(root);
        total_offset = offset;
        fast_forward_to(offset);
    }

    bool ReverseTreeWalker::exhausted() const
    {
        if(stack.empty())
            return true;
        // If we have !exhausted the pointers, we're still active.
        if(first_ptr != last_ptr)
            return false;
        // If there's more than one entry on the stack, we're still active.
        if(stack.size() > 1)
            return false;
        // Now, if there's exactly one entry and that entry itself is exhausted (no right subtree) we're done.
        const StackEntry& entry = stack.back();
        // We descended into a null child, we're done.
        if(entry.node.is_empty())
            return true;
        // Do we need this check for reverse iterators?
        if(entry.dir == Direction::Left && entry.node.left().is_empty())
            return true;
        return false;
    }

    void ReverseTreeWalker::populate_ptrs()
    {
        if(exhausted())
            return;
        if(stack.back().node.is_empty())
        {
            stack.pop_back();
            populate_ptrs();
            return;
        }

        StackEntry& entry = stack.back();
        if(entry.dir == Direction::Right)
        {
            if(!entry.node.right().is_empty())
            {
                RedBlackTree right = entry.node.right();
                stack.back().dir = Direction::Center; // Change the dir for when we pop back.
                stack.push_back(right);
                populate_ptrs();
                return;
            }
            // Otherwise, let's visit the center, we can actually fallthrough.
            stack.back().dir = Direction::Center;
            entry.dir = Direction::Center;
        }

        if(entry.dir == Direction::Center)
        {
            const Piece& piece = entry.node.root().piece;
            const CharBuffer* buffer = buffers->buffer_at(piece.index);
            size_t first_offset = buffers->buffer_offset(piece.index, piece.first);
            size_t last_offset = buffers->buffer_offset(piece.index, piece.last);
            last_ptr = buffer->buffer.data() + first_offset;
            first_ptr = buffer->buffer.data() + last_offset;
            stack.back().dir = Direction::Left; // Change this direction.
            return;
        }

        assert(entry.dir == Direction::Left);
        RedBlackTree left = entry.node.left();
        stack.pop_back();
        stack.push_back(left);
        populate_ptrs();
    }

    void ReverseTreeWalker::fast_forward_to(size_t offset)
    {
        RedBlackTree node = root;
        while(!node.is_empty())
        {
            if(node.root().left_subtree_length > offset)
            {
                assert(!stack.empty());
                // This parent is no longer relevant.
                stack.pop_back();
                node = node.left();
                stack.push_back(node);
            }
            // It is inside this node.
            else if(node.root().left_subtree_length + node.root().piece.length > offset)
            {
                stack.back().dir = Direction::Left;
                // Make the offset relative to this piece.
                offset = offset - node.root().left_subtree_length;
                const Piece& piece = node.root().piece;
                const CharBuffer* buffer = buffers->buffer_at(piece.index);
                size_t first_offset = buffers->buffer_offset(piece.index, piece.first);
                last_ptr = buffer->buffer.data() + first_offset;
                // We extend offset because it is the point where we want to start and because this walker works by dereferencing 'first_ptr - 1', offset + 1 is our 'begin'.
                first_ptr = buffer->buffer.data() + first_offset + (offset + 1);
                return;
            }
            else
            {
                // For when we revisit this node.
                stack.back().dir = Direction::Center;
                size_t offset_amount = node.root().left_subtree_length + node.root().piece.length;
                offset = offset - offset_amount;
                node = node.right();
                stack.push_back(node);
            }
        }
    }
}
