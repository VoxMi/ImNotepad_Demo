#include "imnotepad.h"
#include "imnotepad_colorschemes.h"

#ifndef IMGUI_DISABLE
#include "misc/imgui_color_utils.h"

// Notes:
//  - capture names match standardized Tree-sitter captures in Neovim (https://neovim.io/doc/user/treesitter/#treesitter-highlight).
//  - color maps scopes to colors using pairs of compile-time 32-bit hashes containing the keyword name and the 32-bit ABGR-color value () (your compiler must support C++14 or a later version).

//-----------------------------------------------------------------------------
// [SECTION] Style functions
//-----------------------------------------------------------------------------

// https://github.com/catppuccin/nvim
void ImNotepad::StyleColorsCatppuccinFrappe(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.19f, 0.20f, 0.27f, 1.00f); // #303446
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.78f, 0.82f, 0.96f, 1.00f); // #c6d0f5
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.32f, 0.34f, 0.43f, 1.00f); // #51576d
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.32f, 0.34f, 0.43f, 1.00f); // #51576d
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.73f, 0.73f, 0.95f, 1.00f); // #babbf1
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.23f, 0.25f, 0.32f, 1.00f); // #3b3f52
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.38f, 0.41f, 0.50f, 1.00f); // #626880
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.38f, 0.41f, 0.50f, 1.00f); // #626880

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#ef9f76") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#f4b8e4") }, // builtin annotations (e.g. '@property' in Python)
        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#ef9f76") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#81c8be") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#f4b8e4") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#ef9f76") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#ef9f76") }, // floating-point number literals
        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#949cbb") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#949cbb") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#303446") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#303446") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#303446") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#303446") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')
        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#ca9ee6") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#ef9f76") }, // built-in constant values
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#ca9ee6") }, // constants defined by the preprocessor
        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#e5c890") }, // constructor calls and definitions
        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#a6d189") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#e78284") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#8caaee") }, // changed text (for diff files)
        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#8caaee") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#ef9f76") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#8caaee") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#f4b8e4") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#8caaee") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#8caaee") }, // method calls
        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#ca9ee6") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#ca9ee6") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#99d1db") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#ca9ee6") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#ca9ee6") }, // keywords related to debugging
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#f4b8e4") }, // various preprocessor directives and shebangs)
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#f4b8e4") }, // preprocessor definition directive)
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#ca9ee6") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#ca9ee6") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python))
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#e5c890") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#ca9ee6") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#ca9ee6") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#ca9ee6") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#ca9ee6") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#ca9ee6") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum'))
        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#85c1dc") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)
        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#e78284") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#e78284") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#c6d0f5") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#c6d0f5") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#8caaee") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#e78284") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#ef9f76") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#e5c890") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#a6d189") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#85c1dc") }, // and so forth)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#babbf1") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#babbf1") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#babbf1") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#8caaee") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#81c8be") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#a6d189") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#838ba7") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#8caaee") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#f4b8e4") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#a6d189") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#a6d189") }, // literal or verbatim text as a stand-alone block
        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#e5c890") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#f4b8e4") }, // built-in modules or namespaces
        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#99d1db") }, // symbolic operators (e.g. `+`, `*`)
        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#babbf1") }, // the key in key/value pairs
        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#949cbb") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#abb2bf") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#949cbb") }, // special symbols (e.g. `{}` in string interpolation)
        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#a6d189") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#81c8be") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#f4b8e4") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#f4b8e4") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#f4b8e4") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#f4b8e4") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#eebebe") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#8caaee") }, // URIs (e.g. hyperlinks)
        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#8caaee") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#8caaee") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#e5c890") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#81c8be") }, // XML-style tag delimiters
        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#e5c890") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#ca9ee6") }, // built-in types)
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#e5c890") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)
        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#c6d0f5") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#e78284") }, // built-in variable names (e.g. this, self))
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#babbf1") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#a6d189") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#f4b8e4") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

// https://github.com/olimorris/onedarkpro.nvim
void ImNotepad::StyleColorsOnedarkProOnedark(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.16f, 0.17f, 0.20f, 1.00f); // #282c34
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.67f, 0.70f, 0.75f, 1.00f); // #abb2bf
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.25f, 0.28f, 0.35f, 1.00f); // #414858
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.29f, 0.32f, 0.38f, 1.00f); // #495162
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.78f, 0.47f, 0.87f, 1.00f); // #c678dd
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.16f, 0.17f, 0.20f, 1.00f); // #282c34
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.36f, 0.39f, 0.44f, 1.00f); // #5c6370
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.36f, 0.39f, 0.44f, 1.00f); // #5c6370

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#c678dd") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#61afef") }, // builtin annotations (e.g. '@property' in Python)
        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#d19a66") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#98c379") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#c678dd") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#d19a66") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#d19a66") }, // floating-point number literals
        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#7f848e") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#7f848e") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#e06c75") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#56b6c2") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#61afef") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#e5c07b") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')
        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#d19a66") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#c678dd") }, // built-in constant values)
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#d19a66") }, // constants defined by the preprocessor
        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#e5c07b") }, // constructor calls and definitions
        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#abb2bf") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#abb2bf") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#abb2bf") }, // changed text (for diff files)
        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#61afef") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#e5c07b") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#61afef") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#61afef") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#61afef") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#61afef") }, // method calls
        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#c678dd") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#c678dd") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#c678dd") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#c678dd") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#c678dd") }, // keywords related to debugging
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#c678dd") }, // various preprocessor directives and shebangs)
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#c678dd") }, // preprocessor definition directive)
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#c678dd") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#c678dd") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python))
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#c678dd") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#c678dd") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#c678dd") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#c678dd") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#c678dd") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#c678dd") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum'))
        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#c678dd") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)
        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#d19a66") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#c678dd") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#abb2bf") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#abb2bf") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#e06c75") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#e06c75") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#d19a66") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#e5c07b") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#98c379") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#61afef") }, // and so forth)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#56b6c2") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#abb2bf") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#61afef") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#c678dd") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#e5c07b") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#c678dd") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#abb2bf") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#61afef") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#7f848e") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#98c379") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#98c379") }, // literal or verbatim text as a stand-alone block
        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#e5c07b") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#61afef") }, // built-in modules or namespaces
        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#56b6c2") }, // symbolic operators (e.g. `+`, `*`)
        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#e06c75") }, // the key in key/value pairs
        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#c678dd") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#abb2bf") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#abb2bf") }, // special symbols (e.g. `{}` in string interpolation)
        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#98c379") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#98c379") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#56b6c2") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#61afef") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#61afef") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#61afef") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#61afef") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#61afef") }, // URIs (e.g. hyperlinks)
        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#e06c75") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#61afef") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#e06c75") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#abb2bf") }, // XML-style tag delimiters
        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#e5c07b") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#e5c07b") }, // built-in types)
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#e5c07b") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)
        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#e06c75") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#e5c07b") }, // built-in variable names (e.g. this, self))
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#e06c75") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#e06c75") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#61afef") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

void ImNotepad::StyleColorsOnedarkProOnelight(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.98f, 0.98f, 0.98f, 1.00f); // #fafafa
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.42f, 0.42f, 0.42f, 1.00f); // #6a6a6a
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.91f, 0.91f, 0.91f, 1.00f); // #e9e9e9
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.80f, 0.80f, 0.80f, 1.00f); // #cccccc
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.60f, 0.47f, 0.81f, 1.00f); // #9a77cf
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.98f, 0.98f, 0.98f, 1.00f); // #fafafa
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); // #bebebe
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); // #bebebe

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#9a77cf") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#118dc3") }, // builtin annotations (e.g. '@property' in Python)
        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#ee9025") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#1da912") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#9a77cf") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#ee9025") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#ee9025") }, // floating-point number literals
        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#9b9fa6") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#9b9fa6") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#e05661") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#56b6c2") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#118dc3") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#eea825") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')
        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#ee9025") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#9a77cf") }, // built-in constant values)
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#ee9025") }, // constants defined by the preprocessor
        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#eea825") }, // constructor calls and definitions
        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#6a6a6a") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#6a6a6a") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#6a6a6a") }, // changed text (for diff files)
        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#118dc3") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#eea825") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#118dc3") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#118dc3") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#118dc3") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#118dc3") }, // method calls
        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#9a77cf") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#9a77cf") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#9a77cf") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#9a77cf") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#9a77cf") }, // keywords related to debugging
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#9a77cf") }, // various preprocessor directives and shebangs)
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#9a77cf") }, // preprocessor definition directive)
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#9a77cf") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#9a77cf") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python))
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#9a77cf") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#9a77cf") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#9a77cf") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#9a77cf") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#9a77cf") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#9a77cf") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum'))
        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#9a77cf") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)
        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#eea825") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#9a77cf") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#6a6a6a") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#6a6a6a") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#e05661") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#e05661") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#ee9025") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#eea825") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#1da912") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#118dc3") }, // and so forth)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#56b6c2") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#6a6a6a") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#118dc3") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#9a77cf") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#eea825") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#9a77cf") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#6a6a6a") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#118dc3") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#9b9fa6") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#1da912") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#1da912") }, // literal or verbatim text as a stand-alone block
        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#eea825") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#118dc3") }, // built-in modules or namespaces
        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#56b6c2") }, // symbolic operators (e.g. `+`, `*`)
        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#e05661") }, // the key in key/value pairs
        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#9a77cf") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#6a6a6a") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#6a6a6a") }, // special symbols (e.g. `{}` in string interpolation)
        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#1da912") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#1da912") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#56b6c2") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#61afef") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#118dc3") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#118dc3") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#118dc3") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#118dc3") }, // URIs (e.g. hyperlinks)
        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#e06c75") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#61afef") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#e06c75") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#abb2bf") }, // XML-style tag delimiters
        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#e05661") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#118dc3") }, // built-in types)
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#e05661") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)
        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#e05661") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#eea825") }, // built-in variable names (e.g. this, self))
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#e05661") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#e05661") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#118dc3") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

void ImNotepad::StyleColorsOnedarkProDark(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.00f, 0.00f, 0.00f, 1.00f); // #000000
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.67f, 0.70f, 0.75f, 1.00f); // #abb2bf
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.13f, 0.13f, 0.13f, 1.00f); // #212121
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.29f, 0.32f, 0.38f, 1.00f); // #495162
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.84f, 0.37f, 0.87f, 1.00f); // #d55fde
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.00f, 0.00f, 0.00f, 1.00f); // #000000
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.26f, 0.28f, 0.32f, 1.00f); // #434852
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.26f, 0.28f, 0.32f, 1.00f); // #434852

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#d55fde") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#61afef") }, // builtin annotations (e.g. '@property' in Python)

        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#d19a66") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#89ca78") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#d55fde") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#d19a66") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#d19a66") }, // floating-point number literals

        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#7f848e") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#7f848e") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#ef596f") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#2bbac5") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#61afef") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#e5c07b") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')

        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#d19a66") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#d55fde") }, // built-in constant values)
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#d19a66") }, // constants defined by the preprocessor

        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#e5c07b") }, // constructor calls and definitions

        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#abb2bf") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#abb2bf") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#abb2bf") }, // changed text (for diff files)

        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#61afef") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#e5c07b") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#61afef") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#61afef") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#61afef") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#61afef") }, // method calls

        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#d55fde") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#d55fde") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#d55fde") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#d55fde") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#d55fde") }, // keywords related to debugging
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#d55fde") }, // various preprocessor directives and shebangs)
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#d55fde") }, // preprocessor definition directive)
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#d55fde") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#d55fde") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python))
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#d55fde") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#d55fde") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#d55fde") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#d55fde") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#d55fde") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#d55fde") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum'))

        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#label") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)

        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#d19a66") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#c678dd") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#abb2bf") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#abb2bf") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#ef596f") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#ef596f") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#d19a66") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#e5c07b") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#89ca78") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#61afef") }, // and so forth)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#2bbac5") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#abb2bf") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#61afef") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#d55fde") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#ef596f") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#d55fde") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#abb2bf") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#61afef") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#7f848e") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#89ca78") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#89ca78") }, // literal or verbatim text as a stand-alone block

        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#e5c07b") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#61afef") }, // built-in modules or namespaces

        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#2bbac5") }, // symbolic operators (e.g. `+`, `*`)

        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#ef596f") }, // the key in key/value pairs

        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#d55fde") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#abb2bf") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#abb2bf") }, // special symbols (e.g. `{}` in string interpolation)

        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#89ca78") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#89ca78") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#2bbac5") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#61afef") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#61afef") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#61afef") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#61afef") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#61afef") }, // URIs (e.g. hyperlinks)

        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#ef596f") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#61afef") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#ef596f") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#abb2bf") }, // XML-style tag delimiters

        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#e5c07b") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#e5c07b") }, // built-in types)
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#e5c07b") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)

        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#ef596f") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#e5c07b") }, // built-in variable names (e.g. this, self))
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#ef596f") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#ef596f") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#61afef") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

// https://github.com/tomasiser/vim-code-dark
void ImNotepad::StyleColorsVimCodeDark(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // #1e1e1e
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.83f, 0.83f, 0.83f, 1.00f); // #d4d4d4
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.15f, 0.31f, 0.47f, 1.00f); // #264f78
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // #5a5a5a
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.73f, 0.73f, 0.73f, 1.00f); // #bbbbbb
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.13f, 0.13f, 0.13f, 1.00f); // #222222
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // #5a5a5a
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // #5a5a5a

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#4ec9b0") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#d7ba7d") }, // builtin annotations (e.g. '@property' in Python)
        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#569cd6") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#ce9178") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#ce9178") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#b5cea8") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#b5cea8") }, // floating-point number literals
        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#6a9955") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#6a9955") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#ffc0b9") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#8cf8f7") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#373737") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#f4d88c") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')
        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#dcdcaa") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#569cd6") }, // built-in constant values
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#4ec9b0") }, // constants defined by the preprocessor
        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#4ec9b0") }, // constructor calls and definitions
        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#d4d4d4") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#d4d4d4") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#d4d4d4") }, // changed text (for diff files)
        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#dcdcaa") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#dcdcaa") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#dcdcaa") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#dcdcaa") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#dcdcaa") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#dcdcaa") }, // method calls
        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#569cd6") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#569cd6") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#569cd6") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#569cd6") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#569cd6") }, // keywords related to debuggin
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#569cd6") }, // various preprocessor directives and shebangs
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#569cd6") }, // preprocessor definition directive
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#569cd6") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#569cd6") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#569cd6") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#569cd6") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#569cd6") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#569cd6") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#569cd6") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#569cd6") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum')
        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#9cdcfe") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)
        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#d4d4d4") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#d4d4d4") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#d4d4d4") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#d4d4d4") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#d4d4d4") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#d4d4d4") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#d4d4d4") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#d4d4d4") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#d4d4d4") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#d4d4d4") }, // and so forth
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#d19a66") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#d19a66") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#d19a66") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#d19a66") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#d19a66") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#d19a66") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#d19a66") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#d19a66") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#d19a66") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#d19a66") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#d19a66") }, // literal or verbatim text as a stand-alone block
        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#569cd6") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#d7ba7d") }, // built-in modules or namespaces
        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#d4d4d4") }, // symbolic operators (e.g. `+`, `*`)
        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#9cdcfe") }, // the key in key/value pairs
        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#d4d4d4") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#d4d4d4") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#d4d4d4") }, // special symbols (e.g. `{}` in string interpolation)
        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#ce9178") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#ce9178") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#d7ba7d") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#ce9178") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#ce9178") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#ce9178") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#ce9178") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#ce9178") }, // URIs (e.g. hyperlinks)
        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#569cd6") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#d7ba7d") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#569cd6") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#808080") }, // XML-style tag delimiters
        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#4ec9b0") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#569cd6") }, // built-in types
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#4ec9b0") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)
        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#9cdcfe") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#9cdcfe") }, // built-in variable names (e.g. this, self)
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#9cdcfe") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#d7ba7d") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#d7ba7d") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

// https://github.com/AdrianWilczynski/OneDarkPro
void ImNotepad::StyleColorsOnedarkProVisualStudio(ImNotepadStyle* dst)
{
    ImNotepadStyle* style = dst ? dst : &ImNotepad::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImNotepadCol_WindowBg]               = ImVec4(0.17f, 0.19f, 0.23f, 1.00f); // #2b303a
    colors[ImNotepadCol_TextDefaultFg]          = ImVec4(0.67f, 0.70f, 0.75f, 1.00f); // #abb2bf
    colors[ImNotepadCol_TextSelectedBg]         = ImVec4(0.25f, 0.28f, 0.34f, 1.00f); // #404856
    colors[ImNotepadCol_LineNumber]             = ImVec4(0.17f, 0.57f, 0.69f, 1.00f); // #2b91af
    colors[ImNotepadCol_CursorLineNumber]       = ImVec4(0.52f, 0.80f, 0.88f, 1.00f); // #84cbe1
    colors[ImNotepadCol_CursorLine]             = ImVec4(0.23f, 0.25f, 0.32f, 1.00f); // #2c313c
    colors[ImNotepadCol_Whitespace]             = ImVec4(0.08f, 0.28f, 0.32f, 1.00f); // #144852
    colors[ImNotepadCol_TrailingWhitespace]     = ImVec4(0.83f, 0.49f, 0.49f, 1.00f); // #d47d7e

    ImNotepadSyntaxColorMap syntax_color_map[SYNTAX_COLORMAP_SIZE_MAX] =
    {
        // Attributes
        { CONSTEXPR_KEYWORD_COLOR("attribute",                   "#abb2bf") }, // attribute annotations (e.g. Python decorators, Rust lifetimes)
        { CONSTEXPR_KEYWORD_COLOR("attribute.builtin",           "#abb2bf") }, // builtin annotations (e.g. '@property' in Python)
        // Primitives
        { CONSTEXPR_KEYWORD_COLOR("boolean",                     "#c678dd") }, // boolean literals
        { CONSTEXPR_KEYWORD_COLOR("character",                   "#98c379") }, // character literals
        { CONSTEXPR_KEYWORD_COLOR("character.special",           "#c678dd") }, // special characters (e.g. wildcards)
        { CONSTEXPR_KEYWORD_COLOR("number",                      "#d19a66") }, // numeric literals
        { CONSTEXPR_KEYWORD_COLOR("number.float",                "#d19a66") }, // floating-point number literals
        // Comments
        { CONSTEXPR_KEYWORD_COLOR("comment",                     "#7f848e") }, // line and block comments
        { CONSTEXPR_KEYWORD_COLOR("comment.documentation",       "#7f848e") }, // comments documenting code
        { CONSTEXPR_KEYWORD_COLOR("comment.error",               "#e06c75") }, // error-type comments (e.g. 'ERROR', 'FIXME', 'DEPRECATED')
        { CONSTEXPR_KEYWORD_COLOR("comment.note",                "#56b6c2") }, // note-type comments (e.g. 'NOTE', 'INFO', 'XXX')
        { CONSTEXPR_KEYWORD_COLOR("comment.todo",                "#61afef") }, // todo-type comments (e.g. 'TODO', 'WIP')
        { CONSTEXPR_KEYWORD_COLOR("comment.warning",             "#e5c07b") }, // warning-type comments (e.g. 'WARNING', 'FIX', 'HACK')
        // Constants
        { CONSTEXPR_KEYWORD_COLOR("constant",                    "#b8d7a3") }, // constant identifiers
        { CONSTEXPR_KEYWORD_COLOR("constant.builtin",            "#c678dd") }, // built-in constant values
        { CONSTEXPR_KEYWORD_COLOR("constant.macro",              "#61afef") }, // constants defined by the preprocessor
        // Constructors
        { CONSTEXPR_KEYWORD_COLOR("constructor",                 "#61afef") }, // constructor calls and definitions
        // Diffs
        { CONSTEXPR_KEYWORD_COLOR("diff.plus",                   "#98c379") }, // added text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.minus",                  "#e06c75") }, // deleted text (for diff files)
        { CONSTEXPR_KEYWORD_COLOR("diff.delta",                  "#61afef") }, // changed text (for diff files)
        // Functions
        { CONSTEXPR_KEYWORD_COLOR("function",                    "#61afef") }, // function definitions
        { CONSTEXPR_KEYWORD_COLOR("function.builtin",            "#e5c07b") }, // built-in functions
        { CONSTEXPR_KEYWORD_COLOR("function.call",               "#61afef") }, // function calls
        { CONSTEXPR_KEYWORD_COLOR("function.macro",              "#61afef") }, // preprocessor macros
        { CONSTEXPR_KEYWORD_COLOR("function.method",             "#61afef") }, // method definitions
        { CONSTEXPR_KEYWORD_COLOR("function.method.call",        "#61afef") }, // method calls
        // Keywords
        { CONSTEXPR_KEYWORD_COLOR("keyword",                     "#c678dd") }, // keywords not fitting into specific categories
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional",         "#c678dd") }, // keywords related to conditionals (e.g. 'if', 'else')
        { CONSTEXPR_KEYWORD_COLOR("keyword.conditional.ternary", "#c678dd") }, // ternary operator (e.g. '?', ':')
        { CONSTEXPR_KEYWORD_COLOR("keyword.coroutine",           "#c678dd") }, // keywords related to coroutines (e.g. 'go' in Go, 'async'/'await' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.debug",               "#e06c75") }, // keywords related to debuggin
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive",           "#56b6c2") }, // various preprocessor directives and shebangs
        { CONSTEXPR_KEYWORD_COLOR("keyword.directive.define",    "#56b6c2") }, // preprocessor definition directive
        { CONSTEXPR_KEYWORD_COLOR("keyword.exception",           "#c678dd") }, // keywords related to exceptions (e.g. 'throw', 'catch')
        { CONSTEXPR_KEYWORD_COLOR("keyword.function",            "#c678dd") }, // keywords that define a function (e.g. 'func' in Go, 'def' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.import",              "#56b6c2") }, // keywords for including or exporting modules (e.g. 'import', 'from' in Python)
        { CONSTEXPR_KEYWORD_COLOR("keyword.modifier",            "#c678dd") }, // keywords modifying other constructs (e.g. 'const', 'static', 'public')
        { CONSTEXPR_KEYWORD_COLOR("keyword.operator",            "#c678dd") }, // operators that are English words (e.g. 'and', 'or')
        { CONSTEXPR_KEYWORD_COLOR("keyword.repeat",              "#c678dd") }, // keywords related to loops (e.g. 'for', 'while')
        { CONSTEXPR_KEYWORD_COLOR("keyword.return",              "#c678dd") }, // keywords like 'return' and 'yield'
        { CONSTEXPR_KEYWORD_COLOR("keyword.type",                "#c678dd") }, // keywords describing namespaces and composite types (e.g. 'struct', 'enum')
        // Labels
        { CONSTEXPR_KEYWORD_COLOR("label",                       "#c8c8c8") }, // 'GOTO' and other labels (e.g. `label:` in C, including heredoc labels)
        // Markup (Markdown, etc.)
        { CONSTEXPR_KEYWORD_COLOR("markup.strong",               "#abb2bf") }, // bold text
        { CONSTEXPR_KEYWORD_COLOR("markup.italic",               "#abb2bf") }, // italic text
        { CONSTEXPR_KEYWORD_COLOR("markup.strikethrough",        "#abb2bf") }, // struck-through text
        { CONSTEXPR_KEYWORD_COLOR("markup.underline",            "#abb2bf") }, // underlined text (only for literal underline markup!)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading",              "#d19a66") }, // headings, titles (including markers)
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.1",            "#e06c75") }, // top-level heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.2",            "#c678dd") }, // section heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.3",            "#d19a66") }, // subsection heading
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.4",            "#e06c75") }, // and so on
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.5",            "#c678dd") }, // and so forth
        { CONSTEXPR_KEYWORD_COLOR("markup.heading.6",            "#d19a66") }, // six levels ought to be enough for anybody
        { CONSTEXPR_KEYWORD_COLOR("markup.link",                 "#61afef") }, // text references, footnotes, citations, etc.
        { CONSTEXPR_KEYWORD_COLOR("markup.link.label",           "#56b6c2") }, // link, reference descriptions
        { CONSTEXPR_KEYWORD_COLOR("markup.link.url",             "#56b6c2") }, // URL-style links
        { CONSTEXPR_KEYWORD_COLOR("markup.list",                 "#e06c75") }, // list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.checked",         "#98c379") }, // checked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.list.unchecked",       "#e06c75") }, // unchecked todo-style list markers
        { CONSTEXPR_KEYWORD_COLOR("markup.math",                 "#abb2bf") }, // math environments (e.g. $ ... $ in LaTeX)
        { CONSTEXPR_KEYWORD_COLOR("markup.quote",                "#5c6370") }, // block quotes
        { CONSTEXPR_KEYWORD_COLOR("markup.raw",                  "#98c379") }, // literal or verbatim text (e.g. inline code)
        { CONSTEXPR_KEYWORD_COLOR("markup.raw.block",            "#98c379") }, // literal or verbatim text as a stand-alone block
        // Modules
        { CONSTEXPR_KEYWORD_COLOR("module",                      "#c8c8c8") }, // modules or namespaces
        { CONSTEXPR_KEYWORD_COLOR("module.builtin",              "#d19a66") }, // built-in modules or namespaces
        // Operators
        { CONSTEXPR_KEYWORD_COLOR("operator",                    "#56b6c2") }, // symbolic operators (e.g. `+`, `*`)
        // Properties
        { CONSTEXPR_KEYWORD_COLOR("property",                    "#e06c75") }, // the key in key/value pairs
        // Punctuation
        { CONSTEXPR_KEYWORD_COLOR("punctuation.bracket",         "#56b6c2") }, // brackets (e.g. `()`, `{}`, `[]`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.delimiter",       "#56b6c2") }, // delimiters (e.g. `;`, `.`, `,`)
        { CONSTEXPR_KEYWORD_COLOR("punctuation.special",         "#56b6c2") }, // special symbols (e.g. `{}` in string interpolation)
        // Strings
        { CONSTEXPR_KEYWORD_COLOR("string",                      "#98c379") }, // string literals
        { CONSTEXPR_KEYWORD_COLOR("string.documentation",        "#98c379") }, // string documenting code (e.g. Python docstrings)
        { CONSTEXPR_KEYWORD_COLOR("string.escape",               "#ff00ff") }, // escape sequences
        { CONSTEXPR_KEYWORD_COLOR("string.regexp",               "#98c379") }, // regular expressions
        { CONSTEXPR_KEYWORD_COLOR("string.special",              "#61afef") }, // other special strings (e.g. dates)
        { CONSTEXPR_KEYWORD_COLOR("string.special.path",         "#98c379") }, // filenames
        { CONSTEXPR_KEYWORD_COLOR("string.special.symbol",       "#e06c75") }, // symbols or atoms
        { CONSTEXPR_KEYWORD_COLOR("string.special.url",          "#e06c75") }, // URIs (e.g. hyperlinks)
        // Tags (HTML, JSX, TSX, XML)
        { CONSTEXPR_KEYWORD_COLOR("tag",                         "#e06c75") }, // XML-style tag names (e.g. in XML, HTML, etc.)
        { CONSTEXPR_KEYWORD_COLOR("tag.builtin",                 "#e06c75") }, // builtin tag names (e.g. HTML5 tags)
        { CONSTEXPR_KEYWORD_COLOR("tag.attribute",               "#e06c75") }, // XML-style tag attributes
        { CONSTEXPR_KEYWORD_COLOR("tag.delimiter",               "#abb2bf") }, // XML-style tag delimiters
        // Types
        { CONSTEXPR_KEYWORD_COLOR("type",                        "#e5c07b") }, // type or class definitions and annotations
        { CONSTEXPR_KEYWORD_COLOR("type.builtin",                "#c678dd") }, // built-in types
        { CONSTEXPR_KEYWORD_COLOR("type.definition",             "#e5c07b") }, // identifiers in type definitions (e.g. typedef <type> <identifier> in C)
        // Variables
        { CONSTEXPR_KEYWORD_COLOR("variable",                    "#e06c75") }, // various variable names
        { CONSTEXPR_KEYWORD_COLOR("variable.builtin",            "#c678dd") }, // built-in variable names (e.g. this, self)
        { CONSTEXPR_KEYWORD_COLOR("variable.member",             "#e06c75") }, // object and struct fields
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter",          "#e06c75") }, // parameters of a function
        { CONSTEXPR_KEYWORD_COLOR("variable.parameter.builtin",  "#e06c75") }, // special parameters (e.g. '_', 'it')
    };

    memcpy(style->SyntaxColorMap, syntax_color_map, sizeof(syntax_color_map));
}

ImU32 ImNotepadGetSyntaxHighlightColor(ImNotepadSyntaxColorMap* color_map, const char* name, size_t name_len)
{
    ImU32 color = IM_COL32(0xAB, 0xB2, 0xBF, 0xFF); // Default color

    if(!color_map || !name || !name_len)
        return color;

    uint32_t name_hash = Hash::fnv1a_32(name, name_len);

    for(int i = 0; i < SYNTAX_COLORMAP_SIZE_MAX; i++)
    {
        if(color_map[i].NameHash == name_hash)
        {
            color = color_map[i].Value;
            break;
        }
    }

    return color;
}
#endif // #ifndef IMGUI_DISABLE
