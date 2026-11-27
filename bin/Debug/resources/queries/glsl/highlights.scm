; tree-sitter-glsl highlights.scm
; Highlight queries for the GLSL programming language
; NOTE: The order of priorities is reversed - what is at the top has a low priority.

; ============================================================================
; Comments
; ============================================================================

(comment) @comment

; ============================================================================
; Identifiers
; ============================================================================

(identifier) @variable
(field_identifier) @property

; ============================================================================
; Literals
; ============================================================================

(number_literal) @number
(char_literal) @character

[
  (true)
  (false)
] @boolean

[
  (string_literal)
  (system_lib_string)
] @string

; ============================================================================
; Declarations and definitions
; ============================================================================

; ============================================================================
; Preprocessor
; ============================================================================

(preproc_def
  name: (_) @constant.macro)

(preproc_def
  value: (preproc_arg) @constant.macro.body)

(preproc_ifdef
  name: (identifier) @constant.macro)

(preproc_elifdef
  name: (identifier) @constant.macro)

(preproc_defined) @function.macro

(preproc_defined
  (identifier) @constant.macro)

(preproc_function_def
  name: (identifier) @function.macro)
  
(preproc_function_def
  value: (preproc_arg) @function.macro.body)

(preproc_call
  directive: (preproc_directive) @keyword.directive
  (preproc_arg) @string
  (#any-of? @keyword.directive "#error" "#warning" "#import" "#using"))

; ============================================================================
; Field access
; ============================================================================

; ============================================================================
; Annotations
; ============================================================================

(attribute
  name: (identifier) @attribute)

; ============================================================================
; Keywords
; ============================================================================

[
  "default"
] @keyword

[
  "#define"
  "#elif"
  "#elifdef"
  "#elifndef"
  "#else"
  "#endif"
  "#if"
  "#ifdef"
  "#ifndef"
  "#include"
  (preproc_directive)
] @keyword.directive

[
  "if"
  "else"
  "case"
  "switch"
] @keyword.conditional

[
  "while"
  "for"
  "do"
  "continue"
  "break"
] @keyword.repeat

[
  "in"
  "out"
  "inout"
  "uniform"
  "shared"
  "layout"
  "attribute"
  "varying"
  "buffer"
  "coherent"
  "readonly"
  "writeonly"
  "precision"
  "highp"
  "mediump"
  "lowp"
  "centroid"
  "sample"
  "patch"
  "smooth"
  "flat"
  "noperspective"
  "invariant"
  "precise"
  (type_qualifier)
  (storage_class_specifier)
] @keyword.modifier

"struct" @keyword.type
"return" @keyword.return

"#include" @keyword.import
"subroutine" @keyword.function
;(extension_storage_class) @keyword.modifier

; ============================================================================
; Operators
; ============================================================================

[
  "."
  "~"
  "-"
  "--"
  "-="
  "->"
  "="
  "!"
  "!="
  "|"
  "|="
  "||"
  "^"
  "^="
  "&"
  "&="
  "&&"
  "+"
  "++"
  "+="
  "*"
  "*="
  "/"
  "/="
  "%"
  "%="
  "<<"
  "<<="
  ">>"
  ">>="
  "<"
  "=="
  ">"
  "<="
  ">="
  "?"
] @operator

; ============================================================================
; Punctuation
; ============================================================================

[
  "{"
  "}"
  "("
  ")"
  "["
  "]"
  "[["
  "]]"
] @punctuation.bracket

[
  ","
  ";"
  ":"
  "."
  "->"
] @punctuation.delimiter

; ============================================================================
; Types
; ============================================================================

[
  (type_identifier)
  (type_descriptor)
] @type

(primitive_type) @type.builtin

; ============================================================================
; Functions
; ============================================================================

(call_expression
  function: (identifier) @function.call)

(function_declarator
  declarator: (identifier) @function)

; ============================================================================
; Other
; ============================================================================

((identifier) @keyword.modifier
  (#eq? @keyword.modifier "writeonly"))

(declaration
  type: (type_identifier) @keyword.modifier
  (#eq? @keyword.modifier "readonly"))
  
