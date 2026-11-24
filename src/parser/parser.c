/**
 * @file parser.c
 * @brief Recursive descent parser of IFJ26 (tokens -> AST).
 *
 * Reads all tokens up to the end of the file, so lexical errors are reported, but it builds an empty program.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "parser_internal.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
//           PRIVATE STRUCTURES           //
// ========================================== //

/** Receiver of an `extension (name: Type)` block, copied into every method. */
typedef struct {
    bool present;
    char *name;
    ENUM_DATA_TYPE type;
    SOURCE_LOCATION location;
} t_receiver;

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/** Lexer result codes and parser result codes share the same numbers, but are separate types. */
static t_parse_rc lex_rc(t_lex_rc rc) {
    switch (rc) {
    case LEX_OK:
        return PARSE_OK;
    case LEX_ERR_LEXICAL:
        return PARSE_ERR_LEXICAL;
    default:
        return PARSE_ERR_INTERNAL;
    }
}

// ---------- helpers shared with parser_stmt.c / parser_expr.c ---------- //

// strdup is POSIX, not C11
char *parser_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy != NULL) {
        memcpy(copy, s, n);
    }
    return copy;
}

/*
 * AI use
 * @note same as in lexer
 */

#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
t_parse_rc parser_syntax_error(t_parser *p, const char *fmt, ...) {
    fprintf(stderr, "Syntax error at %d:%d: ", p->cur.line, p->cur.column);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
    return PARSE_ERR_SYNTAX;
}

/*
 * END AI
 */

// ---------- token access ---------- //

/** Moves to the next token. The previous token and its string are released. */
t_parse_rc parser_advance(t_parser *p) {
    p->after_block = false;
    if (p->has_next) {
        token_free(&p->cur);
        p->cur = p->next; // ownership of the string moves with the struct
        token_init(&p->next);
        p->has_next = false;
        return PARSE_OK;
    }
    return lex_rc(lexer_next(&p->lexer, &p->cur));
}

/**
 * Returns the type of the token AFTER the current one without consuming anything.
 *
 * Do not call lexer_mode() while a token is buffered here, the lexer is already
 * one token ahead (parse_interpolated checks this).
 */
t_parse_rc parser_peek_type(t_parser *p, t_token_type *out) {
    if (!p->has_next) {
        TRY(lex_rc(lexer_next(&p->lexer, &p->next)));
        p->has_next = true;
    }
    *out = p->next.type;
    return PARSE_OK;
}

/** Current token must be `type`, otherwise syntax error. Consumes it. */
t_parse_rc parser_expect(t_parser *p, t_token_type type, const char *what) {
    if (p->cur.type != type) {
        return parser_syntax_error(p, "expected %s, got %s", what, token_type_name(p->cur.type));
    }
    return parser_advance(p);
}

/** Enters one more nesting level (call parser_leave() when done). */
t_parse_rc parser_enter(t_parser *p) {
    if (++p->depth > PARSER_MAX_DEPTH) {
        return parser_syntax_error(p, "nesting is too deep");
    }
    return PARSE_OK;
}

void parser_leave(t_parser *p) {
    p->depth--;
}

/**
 * Ends a statement: ';' or EOL. Nothing is needed when the statement itself ended
 * with a block (its DEDENT is the end) or when the enclosing block / file ends.
 */
t_parse_rc parser_end_statement(t_parser *p) {
    if (p->cur.type == TOKEN_SEMICOLON) {
        TRY(parser_advance(p));
        if (p->cur.type == TOKEN_EOL) { // tolerate a trailing ';'
            TRY(parser_advance(p));
        }
        return PARSE_OK;
    }
    if (p->cur.type == TOKEN_EOL) {
        return parser_advance(p);
    }
    if (p->after_block || p->cur.type == TOKEN_DEDENT || p->cur.type == TOKEN_EOF) {
        return PARSE_OK;
    }
    return parser_syntax_error(p, "expected end of statement, got %s", token_type_name(p->cur.type));
}

/**
 * Called right after a ',' inside parentheses (current token = the ',').
 * Consumes it. A trailing comma is only legal when ')' is on a LATER line
 * (the lexer ignores newlines inside parentheses, so we compare line numbers).
 */
t_parse_rc parser_skip_comma(t_parser *p, bool *list_ends) {
    int comma_line = p->cur.line;
    TRY(parser_advance(p));
    *list_ends = false;
    if (p->cur.type == TOKEN_RIGHT_PAREN) {
        if (p->cur.line <= comma_line) {
            return parser_syntax_error(p, "trailing comma must be followed by a line break before ')'");
        }
        *list_ends = true;
    }
    return PARSE_OK;
}

/** Handles `EOL else` (else on its own line below an inline branch). */
t_parse_rc parser_skip_eol_before_else(t_parser *p) {
    if (p->cur.type == TOKEN_EOL) {
        t_token_type next;
        TRY(parser_peek_type(p, &next));
        if (next == TOKEN_KW_ELSE) {
            return parser_advance(p);
        }
    }
    return PARSE_OK;
}

t_parse_rc parse_type(t_parser *p, ENUM_DATA_TYPE *out) {
    switch (p->cur.type) {
    case TOKEN_KW_INT:
        *out = TYPE_INT;
        break;
    case TOKEN_KW_DOUBLE:
        *out = TYPE_DOUBLE;
        break;
    case TOKEN_KW_STRING:
        *out = TYPE_STRING;
        break;
    case TOKEN_KW_UNIT:
        *out = TYPE_UNIT;
        break;
    case TOKEN_KW_BOOLEAN:
         *out = TYPE_BOOLEAN;
         break;
    default:
        return parser_syntax_error(p, "expected a type, got %s", token_type_name(p->cur.type));
    }
    return parser_advance(p);
}

// ---------- functions and program ---------- //

/** import ifj26.*  EOL */
static t_parse_rc parse_prolog(t_parser *p) {
    TRY(parser_expect(p, TOKEN_KW_IMPORT, "the prolog 'import ifj26.*'"));
    if (p->cur.type != TOKEN_ID || strcmp(p->cur.value.str_val, "ifj26") != 0) {
        return parser_syntax_error(p, "the prolog must be exactly 'import ifj26.*'");
    }
    TRY(parser_advance(p));
    TRY(parser_expect(p, TOKEN_DOT, "'.' in the prolog"));
    TRY(parser_expect(p, TOKEN_MULTIPLY, "'*' in the prolog"));

    if (p->cur.type == TOKEN_EOL) {
        return parser_advance(p);
    }
    if (p->cur.type != TOKEN_EOF) {
        return parser_syntax_error(p, "unexpected %s after the prolog", token_type_name(p->cur.type));
    }
    return PARSE_OK;
}

/** name : type */
static t_parse_rc parse_parameter(t_parser *p, AST_FUNCTION_DEF *fn) {
    t_parse_rc rc;
    SOURCE_LOCATION loc = loc_of(&p->cur);
    ENUM_DATA_TYPE type;
    char *name;

    if (p->cur.type != TOKEN_ID) {
        return parser_syntax_error(p, "expected a parameter name, got %s",
                                   token_type_name(p->cur.type));
    }
    name = parser_strdup(p->cur.value.str_val);
    if (name == NULL) {
        return PARSE_ERR_INTERNAL;
    }

    CHECK(parser_advance(p));
    CHECK(parser_expect(p, TOKEN_COLON, "':' after the parameter name"));
    CHECK(parse_type(p, &type));
    rc = ast_rc(ast_function_def_add_parameter(fn, name, type, loc));

fail:
    free(name);
    return rc;
}

/** '(' [param {',' param} [',']] ')'  - one parameter list */
static t_parse_rc parse_param_list(t_parser *p, AST_FUNCTION_DEF *fn) {
    TRY(ast_rc(ast_function_def_add_param_list(fn)));
    TRY(parser_advance(p)); // '('

    if (p->cur.type != TOKEN_RIGHT_PAREN) {
        for (;;) {
            TRY(parse_parameter(p, fn));
            if (p->cur.type != TOKEN_COMMA) {
                break;
            }
            bool ends;
            TRY(parser_skip_comma(p, &ends));
            if (ends) {
                break;
            }
        }
    }
    return parser_expect(p, TOKEN_RIGHT_PAREN, "')'");
}

/**
 * def name {paramlist} [: type] = body
 *
 * Zero parameter lists (`def answer: Int = 42`) means param_list_count == 0,
 * `def f()` gives one empty list.
 */
static t_parse_rc parse_function(t_parser *p, bool is_main, const t_receiver *recv,
                                 AST_FUNCTION_DEF **out) {
    t_parse_rc rc;
    AST_FUNCTION_DEF *fn = NULL;

    if (p->cur.type != TOKEN_KW_DEF) {
        return parser_syntax_error(p, "expected 'def', got %s", token_type_name(p->cur.type));
    }
    TRY(parser_advance(p));
    if (p->cur.type != TOKEN_ID) {
        return parser_syntax_error(p, "expected a function name, got %s", token_type_name(p->cur.type));
    }

    fn = ast_function_def_create(p->cur.value.str_val, loc_of(&p->cur));
    if (fn == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    fn->is_main = is_main;
    CHECK(parser_advance(p));

    if (recv->present) {
        CHECK(ast_rc(ast_function_def_set_receiver(fn, recv->name, recv->type, recv->location)));
    }
    while (p->cur.type == TOKEN_LEFT_PAREN) {
        CHECK(parse_param_list(p, fn));
    }
    if (p->cur.type == TOKEN_COLON) {
        CHECK(parser_advance(p));
        CHECK(parse_type(p, &fn->declared_return_type));
    }
    CHECK(parser_expect(p, TOKEN_ASSIGN, "'=' before the function body"));

    if (p->cur.type == TOKEN_EOL) {
        CHECK(parse_indented_block(p, &fn->body));
    } else {
        CHECK(parser_inline_expr_block(p, &fn->body));
        CHECK(parser_end_statement(p));
    }

    *out = fn;
    return PARSE_OK;

fail:
    ast_function_def_destroy(fn);
    return rc;
}

/** Parses one function and appends it to the program (consumed in every case). */
static t_parse_rc parse_and_add_function(t_parser *p, AST_PROGRAM *program, bool is_main, const t_receiver *recv) {
    AST_FUNCTION_DEF *fn = NULL;
    TRY(parse_function(p, is_main, recv, &fn));
    return ast_rc(ast_program_add_function(program, fn));
}

/** `extension` is not a lexer keyword, so it arrives as an ID. */
static bool is_extension_keyword(const t_token *tok) {
    return tok->type == TOKEN_ID && strcmp(tok->value.str_val, "extension") == 0;
}

/** extension (name: Type)  followed by one method, or by an indented block of methods */
static t_parse_rc parse_extension(t_parser *p, AST_PROGRAM *program) {
    t_parse_rc rc;
    t_receiver recv = {false, NULL, TYPE_UNSPECIFIED, {0, 0}};

    TRY(parser_advance(p)); // 'extension'
    TRY(parser_expect(p, TOKEN_LEFT_PAREN, "'(' after 'extension'"));
    if (p->cur.type != TOKEN_ID) {
        return parser_syntax_error(p, "expected the receiver name, got %s", token_type_name(p->cur.type));
    }
    recv.location = loc_of(&p->cur);
    recv.name = parser_strdup(p->cur.value.str_val);
    if (recv.name == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    recv.present = true;

    CHECK(parser_advance(p));
    CHECK(parser_expect(p, TOKEN_COLON, "':' after the receiver name"));
    CHECK(parse_type(p, &recv.type));
    CHECK(parser_expect(p, TOKEN_RIGHT_PAREN, "')'"));

    if (p->cur.type == TOKEN_EOL) {
        CHECK(parser_advance(p));
        if (p->cur.type != TOKEN_INDENT) {
            rc = parser_syntax_error(p, "expected an indented block of methods, got %s", token_type_name(p->cur.type));
            goto fail;
        }
        CHECK(parser_advance(p));
        while (p->cur.type != TOKEN_DEDENT) {
            CHECK(parse_and_add_function(p, program, false, &recv)); // errors on non-'def'
        }
        CHECK(parser_advance(p)); // DEDENT
    } else {
        CHECK(parse_and_add_function(p, program, false, &recv));
    }
    rc = PARSE_OK;

fail:
    free(recv.name);
    return rc;
}

static t_parse_rc parse_program(t_parser *p, AST_PROGRAM *program) {
    static const t_receiver NO_RECEIVER = {false, NULL, TYPE_UNSPECIFIED, {0, 0}};

    TRY(parse_prolog(p));

    for (;;) {
        switch (p->cur.type) {
        case TOKEN_EOF:
            return PARSE_OK;

        case TOKEN_ANNOT_MAIN:
            TRY(parser_advance(p));
            if (p->cur.type == TOKEN_EOL) { // @main on its own line
                TRY(parser_advance(p));
            }
            TRY(parse_and_add_function(p, program, true, &NO_RECEIVER));
            break;

        case TOKEN_KW_DEF:
            TRY(parse_and_add_function(p, program, false, &NO_RECEIVER));
            break;

        default:
            if (is_extension_keyword(&p->cur)) {
                TRY(parse_extension(p, program));
                break;
            }
            return parser_syntax_error(p, "expected a function definition, got %s", token_type_name(p->cur.type));
        }
    }
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

t_parse_rc parser_parse(FILE *in, AST_PROGRAM **out_program) {
    if (in == NULL || out_program == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out_program = NULL;

    t_parser p;
    t_parse_rc rc = lex_rc(lexer_init(&p.lexer, in));
    if (rc != PARSE_OK) {
        return rc;
    }
    token_init(&p.cur); // lexer_next frees the old token first, so it must start zeroed
    token_init(&p.next);
    p.has_next = false;
    p.after_block = false;
    p.depth = 0;

    AST_PROGRAM *program = ast_program_create();
    if (program == NULL) {
        lexer_destroy(&p.lexer);
        return PARSE_ERR_INTERNAL;
    }

    rc = parser_advance(&p); // load the first token
    if (rc == PARSE_OK) {
        rc = parse_program(&p, program);
    }

    token_free(&p.cur);
    token_free(&p.next);
    lexer_destroy(&p.lexer);

    if (rc != PARSE_OK) {
        ast_program_destroy(program);
        return rc;
    }
    *out_program = program;
    return PARSE_OK;
}