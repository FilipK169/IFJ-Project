/**
 * @file parser_expr.c
 * @brief Expressions of the IFJ26 base language.
 *
 *   expression := eq
 *   eq         := rel (('==' | '!=') rel)?          non-associative
 *   rel        := add (('<' | '<=' | '>' | '>=') add)?  non-associative
 *   add        := mul (('+' | '-') mul)*
 *   mul        := unary (('*' | '/') unary)*
 *   unary      := ('+' | '-') unary | primary       right associative
 *   primary    := INT | DOUBLE | STRING | ID | '(' [expr] ')'
 *
 *   rhs        := call | expression
 *   call       := ID '(' [term {',' term} [',']] ')'
 *   term       := INT | DOUBLE | STRING | ID | '()'
 *
 * In the base language calls can only appear at the top of an rhs (statement,
 * initializer, assignment, return value, one-line function body). Calls do not
 * nest inside expressions and are not allowed as arguments.
 *
 * Ownership rule (see ast.c): every ast_*_create_* / ast_*_add_* function
 * consumes the children passed to it, even when it fails.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#include "parser_internal.h"

#include <stdlib.h>
#include <string.h>

// ========================================== //
//           PRIVATE STRUCTURES           //
// ========================================== //

/** One row of the binary operator table. */
typedef struct {
    t_token_type token;
    ENUM_BINARY_OPERATOR op;
    int level; /* 0 = lowest priority, BINOP_LEVELS = unary */
} t_binop;

static const t_binop BINOPS[] = {
    {TOKEN_EQUAL, BINARY_EQ, 0},
    {TOKEN_NOT_EQUAL, BINARY_NE, 0},
    {TOKEN_LESS, BINARY_LT, 1},
    {TOKEN_LESS_EQUAL, BINARY_LE, 1},
    {TOKEN_GREATER, BINARY_GT, 1},
    {TOKEN_GREATER_EQUAL, BINARY_GE, 1},
    {TOKEN_PLUS, BINARY_ADD, 2},
    {TOKEN_MINUS, BINARY_SUB, 2},
    {TOKEN_MULTIPLY, BINARY_MUL, 3},
    {TOKEN_DIVIDE, BINARY_DIV, 3},
};

#define BINOP_COUNT (sizeof(BINOPS) / sizeof(BINOPS[0]))
#define BINOP_LEVELS 4  /* levels 0..3, level 4 is unary */

// ========================================== //
//    DEFINITION OF PRIVATE FUNCTIONS     //
// ========================================== //

/** Literal, ID, or `()`. This is the only form allowed as a call argument. */
static t_parse_rc parse_term(t_parser *p, AST_EXPRESSION **out) {
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *e = NULL;
    t_parse_rc rc;

    switch (p->cur.type) {
    case TOKEN_INT_LIT:
        e = ast_expression_create_int(p->cur.value.int_val, loc);
        break;
    case TOKEN_DOUBLE_LIT:
        e = ast_expression_create_double(p->cur.value.double_val, loc);
        break;
    case TOKEN_STRING_LIT:
        e = ast_expression_create_string(p->cur.value.str_val,
                                         strlen(p->cur.value.str_val), loc);
        break;
    case TOKEN_ID:
        e = ast_expression_create_var_ref(p->cur.value.str_val, loc);
        break;
    case TOKEN_LEFT_PAREN:
        /* only `()` — `(expr)` here would require FUNEXP */
        TRY(parser_advance(p));
        if (p->cur.type != TOKEN_RIGHT_PAREN) {
            return parser_syntax_error(p,
                "only '()' is allowed as a parenthesised term, got '%s'",
                token_type_name(p->cur.type));
        }
        e = ast_expression_create_unit(loc);
        if (e == NULL) {
            return PARSE_ERR_INTERNAL;
        }
        rc = parser_advance(p);
        if (rc != PARSE_OK) {
            ast_expression_destroy(e);
            return rc;
        }
        *out = e;
        return PARSE_OK;
    default:
        return parser_syntax_error(p, "expected a term, got %s",
                                   token_type_name(p->cur.type));
    }

    if (e == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    rc = parser_advance(p);
    if (rc != PARSE_OK) {
        ast_expression_destroy(e);
        return rc;
    }
    *out = e;
    return PARSE_OK;
}

/** One argument list `( [term {',' term} [',']] )`. Current token is '('. */
static t_parse_rc parse_call_arg_list(t_parser *p, AST_EXPRESSION *call) {
    TRY(ast_rc(ast_expression_call_add_argument_list(call)));
    TRY(parser_advance(p)); /* '(' */

    if (p->cur.type != TOKEN_RIGHT_PAREN) {
        for (;;) {
            AST_EXPRESSION *arg = NULL;
            TRY(parse_term(p, &arg));
            TRY(ast_rc(ast_expression_call_add_argument(call, arg)));

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

/** `ID ( args )` — a call expression. Current token is ID. */
static t_parse_rc parse_call_expression(t_parser *p, AST_EXPRESSION **out) {
    t_parse_rc rc;
    AST_EXPRESSION *call = ast_expression_create_var_ref(p->cur.value.str_val, loc_of(&p->cur));
    if (call == NULL) {
        return PARSE_ERR_INTERNAL;
    }

    CHECK(parser_advance(p));                       /* ID */
    CHECK(ast_rc(ast_expression_var_ref_to_call(call)));
    CHECK(parse_call_arg_list(p, call));
    *out = call;
    return PARSE_OK;

fail:
    ast_expression_destroy(call);
    return rc;
}

/** A bare ID inside an expression: always a variable reference. */
static t_parse_rc parse_identifier(t_parser *p, AST_EXPRESSION **out) {
    AST_EXPRESSION *e = ast_expression_create_var_ref(p->cur.value.str_val, loc_of(&p->cur));
    if (e == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    t_parse_rc rc = parser_advance(p);
    if (rc != PARSE_OK) {
        ast_expression_destroy(e);
        return rc;
    }
    *out = e;
    return PARSE_OK;
}

/** `()` is the Unit literal; `( expr )` is grouping. */
static t_parse_rc parse_parenthesized(t_parser *p, AST_EXPRESSION **out) {
    t_parse_rc rc;
    AST_EXPRESSION *e = NULL;
    SOURCE_LOCATION loc = loc_of(&p->cur);

    TRY(parser_advance(p)); /* '(' */
    if (p->cur.type == TOKEN_RIGHT_PAREN) {
        e = ast_expression_create_unit(loc);
        if (e == NULL) {
            return PARSE_ERR_INTERNAL;
        }
        CHECK(parser_advance(p));
    } else {
        CHECK(parse_expression(p, &e));
        CHECK(parser_expect(p, TOKEN_RIGHT_PAREN, "')'"));
    }
    *out = e;
    return PARSE_OK;

fail:
    ast_expression_destroy(e);
    return rc;
}

static t_parse_rc parse_primary(t_parser *p, AST_EXPRESSION **out) {
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *e = NULL;

    switch (p->cur.type) {
    case TOKEN_INT_LIT:
        e = ast_expression_create_int(p->cur.value.int_val, loc);
        break;
    case TOKEN_DOUBLE_LIT:
        e = ast_expression_create_double(p->cur.value.double_val, loc);
        break;
    case TOKEN_STRING_LIT:
        e = ast_expression_create_string(p->cur.value.str_val, strlen(p->cur.value.str_val), loc);
        break;
    case TOKEN_ID:
        return parse_identifier(p, out);
    case TOKEN_LEFT_PAREN:
        return parse_parenthesized(p, out);
    default:
        return parser_syntax_error(p, "unexpected %s, expected an expression", token_type_name(p->cur.type));
    }

    if (e == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    t_parse_rc rc = parser_advance(p);
    if (rc != PARSE_OK) {
        ast_expression_destroy(e);
        return rc;
    }
    *out = e;
    return PARSE_OK;
}

/** Unary `+` and `-` (right associative, highest priority). */
static t_parse_rc parse_unary(t_parser *p, AST_EXPRESSION **out) {
    ENUM_UNARY_OPERATOR op;

    switch (p->cur.type) {
    case TOKEN_MINUS: op = UNARY_MINUS; break;
    case TOKEN_PLUS:  op = UNARY_PLUS;  break;
    default:
        return parse_primary(p, out);
    }

    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *operand = NULL;
    TRY(parser_advance(p));
    TRY(parser_enter(p));
    TRY(parse_unary(p, &operand));
    parser_leave(p);

    AST_EXPRESSION *e = ast_expression_create_unary(op, operand, loc);
    if (e == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = e;
    return PARSE_OK;
}

static const t_binop *find_binop(t_token_type type, int level) {
    for (size_t i = 0; i < BINOP_COUNT; i++) {
        if (BINOPS[i].token == type && BINOPS[i].level == level) {
            return &BINOPS[i];
        }
    }
    return NULL;
}

/** One precedence level; the level above is parsed for the operands. */
static t_parse_rc parse_binary(t_parser *p, int level, AST_EXPRESSION **out) {
    if (level == BINOP_LEVELS) {
        return parse_unary(p, out);
    }

    AST_EXPRESSION *left = NULL;
    TRY(parse_binary(p, level + 1, &left));

    for (;;) {
        const t_binop *info = find_binop(p->cur.type, level);
        if (info == NULL) {
            break;
        }

        SOURCE_LOCATION loc = loc_of(&p->cur);
        AST_EXPRESSION *right = NULL;
        t_parse_rc rc = parser_advance(p);
        if (rc == PARSE_OK) {
            rc = parse_binary(p, level + 1, &right);
        }
        if (rc != PARSE_OK) {
            ast_expression_destroy(left);
            return rc;
        }

        left = ast_expression_create_binary(info->op, left, right, loc);
        if (left == NULL) {
            return PARSE_ERR_INTERNAL;
        }

        /* == != and < <= > >= have no associativity: a < b < c is an error */
        bool non_associative = (level == 0 || level == 1);
        if (non_associative && find_binop(p->cur.type, level) != NULL) {
            ast_expression_destroy(left);
            return parser_syntax_error(p, "relational operators cannot be chained");
        }
    }

    *out = left;
    return PARSE_OK;
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

t_parse_rc parse_expression(t_parser *p, AST_EXPRESSION **out) {
    TRY(parser_enter(p));
    t_parse_rc rc = parse_binary(p, 0, out);
    parser_leave(p);
    return rc;
}

t_parse_rc parse_rhs(t_parser *p, AST_EXPRESSION **out) {
    if (p->cur.type == TOKEN_ID) {
        t_token_type next;
        TRY(parser_peek_type(p, &next));
        if (next == TOKEN_LEFT_PAREN) {
            return parse_call_expression(p, out);
        }
    }
    return parse_expression(p, out);
}