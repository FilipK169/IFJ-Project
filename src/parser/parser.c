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
        *out = TYPE_INT;     break;
    case TOKEN_KW_DOUBLE:
        *out = TYPE_DOUBLE;  break;
    case TOKEN_KW_STRING:
        *out = TYPE_STRING;  break;
    case TOKEN_KW_UNIT:
        *out = TYPE_UNIT;    break;
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

static t_parse_rc parse_program(t_parser *p, AST_PROGRAM *program) {
    (void)program; // temp

    TRY(parse_prolog(p));

    //temp
    while (p->cur.type != TOKEN_EOF) {
        TRY(parser_advance(p));
    }
    return PARSE_OK;
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

    // Step 1 only: walk through all tokens until EOF
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