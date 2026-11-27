; tree-sitter-cpp highlights.scm
; Highlight queries for the C++ programming language
; NOTE: The order of priorities is reversed - what is at the top has a low priority.

; ============================================================================
; Comments
; ============================================================================

(comment) @comment

; ============================================================================
; Identifiers
; ============================================================================

(identifier) @variable

((identifier)
  @keyword (#any-of? @keyword "static_cast" "dynamic_cast"))

(this) @variable.builtin
(null) @constant.builtin

((identifier) @constant.builtin
  (#any-of? @constant.builtin
    "stderr" "stdin" "stdout"
    "__FILE__" "__LINE__" "__DATE__" "__TIME__"
    "__STDC__" "__STDC_VERSION__" "__STDC_HOSTED__"
    "__cplusplus" "__OBJC__" "__ASSEMBLER__"
    "__BASE_FILE__" "__FILE_NAME__" "__INCLUDE_LEVEL__"
    "__TIMESTAMP__" "__clang__" "__clang_major__"
    "__clang_minor__" "__clang_patchlevel__"
    "__clang_version__" "__clang_literal_encoding__"
    "__clang_wide_literal_encoding__"
    "__FUNCTION__" "__func__" "__PRETTY_FUNCTION__"
    "__VA_ARGS__" "__VA_OPT__"))

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
  (raw_string_literal)
] @string

(escape_sequence) @string.escape

; ============================================================================
; Declarations and definitions
; ============================================================================

(namespace_identifier) @module
(statement_identifier) @label

(declaration
  type: (type_identifier) @_type
  declarator: (identifier) @label
  (#eq? @_type "__label__"))

; ============================================================================
; Preprocessor
; ============================================================================

(preproc_def
  name: (_) @constant.macro)

(preproc_def
  (preproc_arg) @constant.macro)

(preproc_defined) @function.macro

(preproc_defined
  (identifier) @constant.macro)

(preproc_def
  value: (preproc_arg) @constant.macro.body)

(preproc_if
  condition: (identifier) @constant.macro)
  
(preproc_elif
  condition: (identifier) @constant.macro)

(preproc_if
  (unary_expression
    (identifier) @constant.macro))
(preproc_elif
  (unary_expression
    (identifier) @constant.macro))

(preproc_if
  (parenthesized_expression
    (identifier) @constant.macro))
(preproc_elif
  (parenthesized_expression
    (identifier) @constant.macro))

(preproc_if
  (binary_expression
    (identifier) @constant.macro))
(preproc_elif
  (binary_expression
    (identifier) @constant.macro))

(preproc_ifdef
  name: (identifier) @constant.macro)

(preproc_elifdef
  name: (identifier) @constant.macro)

(preproc_function_def
  name: (identifier) @function.macro)
  
(preproc_function_def
  value: (preproc_arg) @function.macro.body)

(preproc_call
  directive: (preproc_directive) @keyword.directive
  (preproc_arg) @attribute
  (#eq? @keyword.directive "#pragma"))

(preproc_call
  directive: (preproc_directive) @keyword.directive
  (preproc_arg) @constant.macro
  (#eq? @keyword.directive "#undef"))

(preproc_call
  directive: (preproc_directive) @keyword.directive
  (preproc_arg) @string
  (#any-of? @keyword.directive "#error" "#warning" "#import" "#using"))

; ============================================================================
; Field access
; ============================================================================

(field_identifier) @variable.member

; ============================================================================
; Annotations
; ============================================================================

[
  "__attribute__"
  "__declspec"
  "__based"
  "__cdecl"
  "__clrcall"
  "__stdcall"
  "__fastcall"
  "__thiscall"
  "__vectorcall"
  (ms_pointer_modifier)
  (attribute_declaration)
] @attribute

(attribute_specifier) @attribute

(attribute_specifier
  (argument_list
    (identifier) @attribute))

(attribute
  prefix: (identifier) @attribute)

(attribute
  name: (identifier) @attribute)

(ms_declspec_modifier
  (identifier) @attribute)

; ============================================================================
; Keywords
; ============================================================================

[
  "asm"
  "__asm__"
  "constexpr"
  "decltype"
  "default"
  "delete"  
  "explicit"
  "friend"
  "goto"
  "new"
  "override"
  "using"
  "requires"
  "sizeof"
] @keyword

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
  "co_await"
  "co_yield"
  "co_return"
] @keyword.coroutine

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
] @keyword.directive

[
  "catch"
  "noexcept"
  "throw"
  "try"
] @keyword.exception

[
  "explicit"
  "final"  
  "friend"
  "override"
  "public"
  "private"
  "protected"
  "virtual"
  "volatile"
  (type_qualifier)
  (storage_class_specifier)
] @keyword.modifier

[
  "and"
  "and_eq"
  "bitand"
  "bitor"
  "compl"  
  "not"  
  "not_eq"
  "offsetof"
  "or"
  "or_eq"   
  "xor"
  "xor_eq"
] @keyword.operator


[
  "class"
  "concept"
  "enum"
  "namespace"
  "requires"
  "struct"  
  "template"
  "typedef"
  "typename"  
  "union"
  "using"
] @keyword.type

"#include" @keyword.import
"return" @keyword.return

; ============================================================================
; Operators
; ============================================================================

[
  "."
  ".*"
  "->*"
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
  "<=>"
] @operator

; ============================================================================
; Punctuation
; ============================================================================

"..." @punctuation.special

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
  ":"
  "::"
  ";"
  (raw_string_delimiter)
] @punctuation.delimiter

; ============================================================================
; Types
; ============================================================================

[
  (auto)
  (type_identifier)
  (type_descriptor)
] @type

(conditional_expression ":" @operator)

(type_definition
  declarator: (type_identifier) @type.definition)

[
  (primitive_type)
  (sized_type_specifier)
] @type.builtin

(enumerator
  name: (identifier) @constant)

; ============================================================================
; Functions
; ============================================================================

(function_definition
  name: (identifier) @function)

(template_function
  name: (identifier) @function)

(template_method
  name: (field_identifier) @function)

(function_declarator
  declarator: (identifier) @function)

(function_declarator
  declarator: (qualified_identifier
    name: (identifier) @function))

(function_declarator
  declarator: (field_identifier) @function)
  
(operator_name
  (identifier)? @operator) @function

(destructor_name "~" @constructor
  (identifier) @constructor)

(call_expression
  function: (identifier) @function.call)
  
(call_expression
  function: (field_expression
    field: (field_identifier) @function))

; Support up to 4 levels of nesting of qualifiers
; i.e. a::b::c::d::func();
(call_expression
  function: (qualified_identifier
    name: (identifier) @function))

(call_expression
  (qualified_identifier
    (identifier) @function.call))

(call_expression
  (qualified_identifier
    (qualified_identifier
      (identifier) @function.call)))

(call_expression
  (qualified_identifier
    (qualified_identifier
      (qualified_identifier
        (identifier) @function.call))))

(call_expression
  (qualified_identifier
    (qualified_identifier
      (qualified_identifier
        (qualified_identifier
          (identifier) @function.call)))))
		  
"static_assert" @function.builtin
