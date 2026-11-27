/**
 * @file parser_expr.c
 * @brief Expressions of IFJ26: operators by priority, calls, members,
 *        interpolated strings and if-expressions.
 *
 *   expr       := || < && < (== !=) < (< <= > >=) < (+ -) < (* /) < unary (+ - !) < postfix
 *   postfix    := primary { '.' ID [ { '(' args ')' } ] }
 *   primary    := literal | ID [ { '(' args ')' } ] | '(' [expr] ')' | interpolated | if-expr
 *
 * Ownership rule (see ast.c): every ast_*_create_* / ast_*_add_* function
 * consumes the children passed to it, even when it fails. After handing a
 * node over, the parser must NOT free it again.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Ondřej Vondra (x274921)
 */

#include "parser_internal.h"

#include <stdlib.h>
#include <string.h>

// ========================================== //
//             PRIVATE STRUCTURES             //
// ========================================== //

/* One row of the binary operator table. */
typedef struct
{
    t_token_type token;
    ENUM_BINARY_OPERATOR op;
    int level; // 0 = lowest priority
} t_binop;

static const t_binop BINOPS[] = {
    {TOKEN_OR, BINARY_OR, 0},        {TOKEN_AND, BINARY_AND, 1},
    {TOKEN_EQUAL, BINARY_EQ, 2},     {TOKEN_NOT_EQUAL, BINARY_NE, 2},
    {TOKEN_LESS, BINARY_LT, 3},      {TOKEN_LESS_EQUAL, BINARY_LE, 3},
    {TOKEN_GREATER, BINARY_GT, 3},   {TOKEN_GREATER_EQUAL, BINARY_GE, 3},
    {TOKEN_PLUS, BINARY_ADD, 4},     {TOKEN_MINUS, BINARY_SUB, 4},
    {TOKEN_MULTIPLY, BINARY_MUL, 5}, {TOKEN_DIVIDE, BINARY_DIV, 5},
};

#define BINOP_COUNT (sizeof(BINOPS) / sizeof(BINOPS[0]))
#define BINOP_LEVELS 6 // levels 0..5, level 6 is the unary operators

// ========================================== //
//      DEFINITION OF PRIVATE FUNCTIONS       //
// ========================================== //

/* Arguments of one or more `( ... )` lists, e.g. f(1)(2). Current token is '('. */
static t_parse_rc parse_call_arg_lists(t_parser *p, AST_EXPRESSION *call)
{
    while (p->cur.type == TOKEN_LEFT_PAREN)
    {
        TRY(ast_rc(ast_expression_call_add_argument_list(call)));
        TRY(parser_advance(p));

        if (p->cur.type != TOKEN_RIGHT_PAREN)
        {
            for (;;)
            {
                AST_EXPRESSION *arg = NULL;
                TRY(parse_expression(p, &arg));
                TRY(ast_rc(ast_expression_call_add_argument(call, arg)));

                if (p->cur.type != TOKEN_COMMA)
                {
                    break;
                }
                bool ends;
                TRY(parser_skip_comma(p, &ends));
                if (ends)
                {
                    break;
                }
            }
        }
        TRY(parser_expect(p, TOKEN_RIGHT_PAREN, "')'"));
    }
    return PARSE_OK;
}

/** Identifier: a variable reference, or a call if '(' follows. */
static t_parse_rc parse_identifier(t_parser *p, AST_EXPRESSION **out)
{
    t_parse_rc rc;
    AST_EXPRESSION *e = ast_expression_create_var_ref(p->cur.value.str_val, loc_of(&p->cur));
    if (e == NULL)
    {
        return PARSE_ERR_INTERNAL;
    }

    CHECK(parser_advance(p));
    if (p->cur.type == TOKEN_LEFT_PAREN)
    {
        CHECK(ast_rc(ast_expression_var_ref_to_call(e))); // keeps the name
        CHECK(parse_call_arg_lists(p, e));
    }
    *out = e;
    return PARSE_OK;

fail:
    ast_expression_destroy(e);
    return rc;
}

/* `()` is the Unit literal, `( expr )` is just grouping. */
static t_parse_rc parse_parenthesized(t_parser *p, AST_EXPRESSION **out)
{
    t_parse_rc rc;
    AST_EXPRESSION *e = NULL;
    SOURCE_LOCATION loc = loc_of(&p->cur);

    TRY(parser_advance(p)); /* '(' */
    if (p->cur.type == TOKEN_RIGHT_PAREN)
    {
        e = ast_expression_create_unit(loc);
        if (e == NULL)
        {
            return PARSE_ERR_INTERNAL;
        }
        CHECK(parser_advance(p));
    }
    else
    {
        CHECK(parse_expression(p, &e));
        CHECK(parser_expect(p, TOKEN_RIGHT_PAREN, "')'"));
    }
    *out = e;
    return PARSE_OK;

fail:
    ast_expression_destroy(e);
    return rc;
}

/**
 * s"..." / s"""...""". The lexer gives: TEXT { ID TEXT | EXPR_BEGIN ... EXPR_END TEXT }.
 * There is no END token: a TEXT part is the last one when the lexer is NOT in
 * LEX_MODE_DOLLAR right after producing it (i.e. it stopped at the closing quote).
 */
static t_parse_rc parse_interpolated(t_parser *p, AST_EXPRESSION **out)
{
    t_parse_rc rc;
    AST_EXPRESSION *str = ast_expression_create_interpolated(loc_of(&p->cur));
    if (str == NULL)
    {
        return PARSE_ERR_INTERNAL;
    }

    for (;;)
    {
        if (p->cur.type != TOKEN_ISTR_TEXT)
        {
            rc = parser_syntax_error(p, "expected text inside an interpolated string, got %s",
                                     token_type_name(p->cur.type));
            goto fail;
        }
        if (p->has_next)
        { // lexer_mode() would be one token too far
            rc = PARSE_ERR_INTERNAL;
            goto fail;
        }
        bool more = (lexer_mode(&p->lexer) == LEX_MODE_DOLLAR);

        if (p->cur.value.str_val[0] != '\0')
        {
            CHECK(ast_rc(ast_expression_interpolated_add_literal(str, p->cur.value.str_val)));
        }
        CHECK(parser_advance(p));
        if (!more)
        {
            break;
        }

        if (p->cur.type == TOKEN_ISTR_ID)
        {
            AST_EXPRESSION *ref =
                ast_expression_create_var_ref(p->cur.value.str_val, loc_of(&p->cur));
            if (ref == NULL)
            {
                rc = PARSE_ERR_INTERNAL;
                goto fail;
            }
            CHECK(ast_rc(ast_expression_interpolated_add_expression(str, ref)));
            CHECK(parser_advance(p));
        }
        else if (p->cur.type == TOKEN_ISTR_EXPR_BEGIN)
        {
            AST_EXPRESSION *part = NULL;
            CHECK(parser_advance(p));
            CHECK(parse_expression(p, &part));
            CHECK(ast_rc(ast_expression_interpolated_add_expression(str, part)));
            if (p->cur.type != TOKEN_ISTR_EXPR_END)
            {
                rc = parser_syntax_error(p, "expected '}', got %s", token_type_name(p->cur.type));
                goto fail;
            }
            CHECK(parser_advance(p));
        }
        else
        {
            rc = parser_syntax_error(p, "unexpected %s inside an interpolated string",
                                     token_type_name(p->cur.type));
            goto fail;
        }
    }

    // s"" has no parts yet: give code generation one (empty) literal to work with
    if (str->data.interpolated.part_count == 0)
    {
        CHECK(ast_rc(ast_expression_interpolated_add_literal(str, "")));
    }
    *out = str;
    return PARSE_OK;

fail:
    ast_expression_destroy(str);
    return rc;
}

/** `if c then a else b` used as an expression (else is mandatory). */
static t_parse_rc parse_if_expression(t_parser *p, AST_EXPRESSION **out)
{
    t_parse_rc rc;
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *cond = NULL;
    AST_BLOCK *then_block = NULL;
    AST_BLOCK *else_block = NULL;
    AST_EXPRESSION *result;

    CHECK(parser_advance(p)); // 'if'
    CHECK(parse_expression(p, &cond));
    CHECK(parser_expect(p, TOKEN_KW_THEN, "'then'"));
    CHECK(parse_branch(p, &then_block, true));
    CHECK(parser_skip_eol_before_else(p));

    if (p->cur.type != TOKEN_KW_ELSE)
    {
        rc = parser_syntax_error(p,
                                 "expected 'else' (an if expression needs an else branch), got %s",
                                 token_type_name(p->cur.type));
        goto fail;
    }
    CHECK(parser_advance(p));

    if (p->cur.type == TOKEN_KW_IF)
    { // else if ...
        SOURCE_LOCATION nested_loc = loc_of(&p->cur);
        AST_EXPRESSION *nested = NULL;
        AST_STATEMENT *stmt;
        CHECK(parser_enter(p));
        CHECK(parse_if_expression(p, &nested));
        parser_leave(p);
        stmt = ast_statement_create_expr(nested, nested_loc);
        if (stmt == NULL)
        {
            rc = PARSE_ERR_INTERNAL;
            goto fail;
        }
        CHECK(parser_block_from_statement(stmt, &else_block));
    }
    else
    {
        CHECK(parse_branch(p, &else_block, true));
    }

    result = ast_expression_create_if(cond, then_block, else_block, loc); // consumes all 3
    if (result == NULL)
    {
        return PARSE_ERR_INTERNAL;
    }
    *out = result;
    return PARSE_OK;

fail:
    ast_expression_destroy(cond);
    ast_block_destroy(then_block);
    ast_block_destroy(else_block);
    return rc;
}

static t_parse_rc parse_primary(t_parser *p, AST_EXPRESSION **out)
{
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *e = NULL;

    switch (p->cur.type)
    {
    case TOKEN_INT_LIT:
        e = ast_expression_create_int(p->cur.value.int_val, loc);
        break;
    case TOKEN_DOUBLE_LIT:
        e = ast_expression_create_double(p->cur.value.double_val, loc);
        break;
    case TOKEN_STRING_LIT:
        e = ast_expression_create_string(p->cur.value.str_val, strlen(p->cur.value.str_val), loc);
        break;
    case TOKEN_KW_TRUE:
        e = ast_expression_create_bool(true, loc);
        break;
    case TOKEN_KW_FALSE:
        e = ast_expression_create_bool(false, loc);
        break;
    case TOKEN_ID:
        return parse_identifier(p, out);
    case TOKEN_LEFT_PAREN:
        return parse_parenthesized(p, out);
    case TOKEN_ISTR_TEXT:
        return parse_interpolated(p, out);
    case TOKEN_KW_IF:
        return parse_if_expression(p, out);
    default:
        return parser_syntax_error(p, "unexpected %s, expected an expression",
                                   token_type_name(p->cur.type));
    }

    // a literal: the node copied what it needs, now it is safe to move on
    if (e == NULL)
    {
        return PARSE_ERR_INTERNAL;
    }
    t_parse_rc rc = parser_advance(p);
    if (rc != PARSE_OK)
    {
        ast_expression_destroy(e);
        return rc;
    }
    *out = e;
    return PARSE_OK;
}

/** primary { '.' ID [ '(' args ')' ... ] } : members and method calls */
static t_parse_rc parse_postfix(t_parser *p, AST_EXPRESSION **out)
{
    t_parse_rc rc;
    AST_EXPRESSION *e = NULL;

    TRY(parse_primary(p, &e));

    while (p->cur.type == TOKEN_DOT)
    {
        char *name;
        SOURCE_LOCATION loc;
        AST_EXPRESSION *made;

        CHECK(parser_advance(p)); // '.'
        if (p->cur.type != TOKEN_ID)
        {
            rc = parser_syntax_error(p, "expected a member name after '.', got %s",
                                     token_type_name(p->cur.type));
            goto fail;
        }
        loc = loc_of(&p->cur);
        name = parser_strdup(p->cur.value.str_val); // advance frees the token's string
        if (name == NULL)
        {
            rc = PARSE_ERR_INTERNAL;
            goto fail;
        }
        rc = parser_advance(p);
        if (rc != PARSE_OK)
        {
            free(name);
            goto fail;
        }

        // x.name(...) is a call with receiver, x.name is a member access
        if (p->cur.type == TOKEN_LEFT_PAREN)
        {
            made = ast_expression_create_call(name, e, loc);
        }
        else
        {
            made = ast_expression_create_member(e, name, loc);
        }
        free(name);
        e = made; // the old `e` was consumed (also on failure)
        if (e == NULL)
        {
            return PARSE_ERR_INTERNAL;
        }
        if (e->type == EXPR_CALL)
        {
            CHECK(parse_call_arg_lists(p, e));
        }
    }

    *out = e;
    return PARSE_OK;

fail:
    ast_expression_destroy(e);
    return rc;
}

/* Prefix + - ! (right associative, highest priority). */
static t_parse_rc parse_unary(t_parser *p, AST_EXPRESSION **out)
{
    ENUM_UNARY_OPERATOR op;

    switch (p->cur.type)
    {
    case TOKEN_MINUS:
        op = UNARY_MINUS;
        break;
    case TOKEN_PLUS:
        op = UNARY_PLUS;
        break;
    case TOKEN_NOT:
        op = UNARY_NOT;
        break;
    default:
        return parse_postfix(p, out);
    }

    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *operand = NULL;
    TRY(parser_advance(p));
    TRY(parser_enter(p));
    TRY(parse_unary(p, &operand));
    parser_leave(p);

    AST_EXPRESSION *e = ast_expression_create_unary(op, operand, loc); // consumes operand
    if (e == NULL)
    {
        return PARSE_ERR_INTERNAL;
    }
    *out = e;
    return PARSE_OK;
}

static const t_binop *find_binop(t_token_type type, int level)
{
    for (size_t i = 0; i < BINOP_COUNT; i++)
    {
        if (BINOPS[i].token == type && BINOPS[i].level == level)
        {
            return &BINOPS[i];
        }
    }
    return NULL;
}

/* One precedence level of binary operators; the level above is parsed for the operands. */
static t_parse_rc parse_binary(t_parser *p, int level, AST_EXPRESSION **out)
{
    if (level == BINOP_LEVELS)
    {
        return parse_unary(p, out);
    }

    AST_EXPRESSION *left = NULL;
    TRY(parse_binary(p, level + 1, &left));

    for (;;)
    {
        const t_binop *info = find_binop(p->cur.type, level);
        if (info == NULL)
        {
            break;
        }

        SOURCE_LOCATION loc = loc_of(&p->cur); // location of the operator
        AST_EXPRESSION *right = NULL;
        t_parse_rc rc = parser_advance(p);
        if (rc == PARSE_OK)
        {
            rc = parse_binary(p, level + 1, &right);
        }
        if (rc != PARSE_OK)
        {
            ast_expression_destroy(left);
            return rc;
        }

        left = ast_expression_create_binary(info->op, left, right, loc); // consumes both
        if (left == NULL)
        {
            return PARSE_ERR_INTERNAL;
        }

        // == != and < <= > >= have no associativity: a < b < c is an error
        bool non_associative = (level == 2 || level == 3);
        if (non_associative && find_binop(p->cur.type, level) != NULL)
        {
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

t_parse_rc parse_expression(t_parser *p, AST_EXPRESSION **out)
{
    TRY(parser_enter(p));
    t_parse_rc rc = parse_binary(p, 0, out);
    parser_leave(p);
    return rc;
}