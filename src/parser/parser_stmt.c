/**
 * @file parser_stmt.c
 * @brief Statements and blocks of IFJ26: val/var, assignment, return, if, while,
 *        expression statements, and indented blocks.
 *
 *   statement  := ('val'|'var') ID [':' type] '=' expr | ID '=' expr | 'return' [expr]
 *               | if-stmt | 'while' expr 'do' branch | expr
 *   if-stmt    := 'if' expr 'then' branch [ 'else' (if-stmt | branch) ]
 *
 * Ownership rule (see ast.c): every ast_*_create_* / ast_*_add_* function
 * consumes the children passed to it, even when it fails. After handing a
 * node over, the parser must NOT free it again.
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
//    DEFINITION OF PRIVATE FUNCTIONS     //
// ========================================== //

// ---------- blocks ---------- //

t_parse_rc parser_block_from_statement(AST_STATEMENT *stmt, AST_BLOCK **out) {
    AST_BLOCK *block = ast_block_create();
    if (block == NULL) {
        ast_statement_destroy(stmt);
        return PARSE_ERR_INTERNAL;
    }
    t_parse_rc rc = ast_rc(ast_block_add_statement(block, stmt));
    if (rc != PARSE_OK) {
        ast_block_destroy(block);
        return rc;
    }
    *out = block;
    return PARSE_OK;
}

/* One-line function body: `def f() = 2 + 3` or `def f() = print(x)`. */
t_parse_rc parser_inline_expr_block(t_parser *p, AST_BLOCK **out) {
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *expr = NULL;
    TRY(parse_expression(p, &expr));
    AST_STATEMENT *stmt = ast_statement_create_expr(expr, loc);
    if (stmt == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    return parser_block_from_statement(stmt, out);
}

/** Single-line branch made of one statement: `if c then x = 1 else x = 2`. */
static t_parse_rc inline_statement_block(t_parser *p, AST_BLOCK **out) {
    AST_STATEMENT *stmt = NULL;
    TRY(parse_statement(p, &stmt));
    return parser_block_from_statement(stmt, out);
}

/** Body after `then` / `else` / `do`: an indented block or one inline item. */
t_parse_rc parse_branch(t_parser *p, AST_BLOCK **out, bool as_expression) {
    if (p->cur.type == TOKEN_EOL) {
        return parse_indented_block(p, out);
    }
    return as_expression ? parser_inline_expr_block(p, out) : inline_statement_block(p, out);
}

/* EOL INDENT statement { separator statement } DEDENT */
t_parse_rc parse_indented_block(t_parser *p, AST_BLOCK **out) {
    t_parse_rc rc;
    AST_BLOCK *block = NULL;

    TRY(parser_expect(p, TOKEN_EOL, "end of line"));
    if (p->cur.type != TOKEN_INDENT) {
        return parser_syntax_error(p, "expected an indented block, got %s",
                                   token_type_name(p->cur.type));
    }
    TRY(parser_advance(p));

    block = ast_block_create();
    if (block == NULL) {
        return PARSE_ERR_INTERNAL;
    }

    for (;;) {
        AST_STATEMENT *stmt = NULL;
        CHECK(parse_statement(p, &stmt));
        CHECK(ast_rc(ast_block_add_statement(block, stmt)));
        CHECK(parser_end_statement(p));

        if (p->cur.type == TOKEN_DEDENT) {
            break;
        }
        if (p->cur.type == TOKEN_EOF) {
            rc = parser_syntax_error(p, "unexpected end of file inside a block");
            goto fail;
        }
    }

    CHECK(parser_advance(p)); // DEDENT
    p->after_block = true; // set AFTER advance, advance clears it
    *out = block;
    return PARSE_OK;

fail:
    ast_block_destroy(block);
    return rc;
}

// ---------- statements ---------- //

/* val / var  name [: type] = expr */
static t_parse_rc parse_var_decl(t_parser *p, AST_STATEMENT **out) {
    t_parse_rc rc;
    bool is_mutable = (p->cur.type == TOKEN_KW_VAR);
    SOURCE_LOCATION loc = loc_of(&p->cur);
    ENUM_DATA_TYPE type = TYPE_UNSPECIFIED;
    char *name = NULL;
    AST_EXPRESSION *init = NULL;
    AST_STATEMENT *stmt;

    TRY(parser_advance(p)); // val / var
    if (p->cur.type != TOKEN_ID) {
        return parser_syntax_error(p, "expected a variable name, got %s", token_type_name(p->cur.type));
    }
    name = parser_strdup(p->cur.value.str_val);
    if (name == NULL) {
        return PARSE_ERR_INTERNAL;
    }

    CHECK(parser_advance(p));
    if (p->cur.type == TOKEN_COLON) {
        CHECK(parser_advance(p));
        CHECK(parse_type(p, &type));
    }
    CHECK(parser_expect(p, TOKEN_ASSIGN, "'=' (a variable must be initialized)"));
    CHECK(parse_expression(p, &init));

    stmt = ast_statement_create_var_decl(name, is_mutable, type, init, loc);
    free(name);
    if (stmt == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = stmt;
    return PARSE_OK;

fail:
    free(name);
    return rc;
}

/* name = expr   (the caller already saw ID '=') */
static t_parse_rc parse_assignment(t_parser *p, AST_STATEMENT **out) {
    t_parse_rc rc;
    SOURCE_LOCATION loc = loc_of(&p->cur);
    char *name = parser_strdup(p->cur.value.str_val);
    AST_EXPRESSION *value = NULL;
    AST_STATEMENT *stmt;

    if (name == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    CHECK(parser_advance(p)); // the identifier
    CHECK(parser_advance(p)); // '='
    CHECK(parse_expression(p, &value));

    stmt = ast_statement_create_assignment(name, value, loc);
    free(name);
    if (stmt == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = stmt;
    return PARSE_OK;

fail:
    free(name);
    return rc;
}

/* return [expr] */
static t_parse_rc parse_return(t_parser *p, AST_STATEMENT **out) {
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *value = NULL;

    TRY(parser_advance(p)); // 'return'
    switch (p->cur.type) {
        case TOKEN_EOL:
        case TOKEN_SEMICOLON:
        case TOKEN_DEDENT:
        case TOKEN_EOF:
        case TOKEN_KW_ELSE:
            break; // plain `return`
        default:
            TRY(parse_expression(p, &value));
            break;
    }

    AST_STATEMENT *stmt = ast_statement_create_return(value, loc);
    if (stmt == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = stmt;
    return PARSE_OK;
}

/** if c then branch [else (if ... | branch)]  as a statement (else optional) */
static t_parse_rc parse_if_statement(t_parser *p, AST_STATEMENT **out) {
    t_parse_rc rc;
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *cond = NULL;
    AST_BLOCK *then_block = NULL;
    AST_BLOCK *else_block = NULL;
    AST_STATEMENT *result;

    CHECK(parser_advance(p)); // 'if'
    CHECK(parse_expression(p, &cond));
    CHECK(parser_expect(p, TOKEN_KW_THEN, "'then'"));
    CHECK(parse_branch(p, &then_block, false));
    CHECK(parser_skip_eol_before_else(p));

    if (p->cur.type == TOKEN_KW_ELSE) {
        CHECK(parser_advance(p));
        if (p->cur.type == TOKEN_KW_IF) { // else if ...
            AST_STATEMENT *nested = NULL;
            CHECK(parser_enter(p));
            CHECK(parse_if_statement(p, &nested));
            parser_leave(p);
            CHECK(parser_block_from_statement(nested, &else_block));
        } else {
            CHECK(parse_branch(p, &else_block, false));
        }
    }

    result = ast_statement_create_if(cond, then_block, else_block, loc); // consumes all
    if (result == NULL) {
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

/** while expr do BLOCK  (block only) */
static t_parse_rc parse_while(t_parser *p, AST_STATEMENT **out) {
    t_parse_rc rc;
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *cond = NULL;
    AST_BLOCK *body = NULL;
    AST_STATEMENT *result;

    CHECK(parser_advance(p)); // 'while'
    CHECK(parse_expression(p, &cond));
    CHECK(parser_expect(p, TOKEN_KW_DO, "'do'"));
    CHECK(parse_branch(p, &body, false));

    result = ast_statement_create_while(cond, body, loc);
    if (result == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = result;
    return PARSE_OK;

fail:
    ast_expression_destroy(cond);
    ast_block_destroy(body);
    return rc;
}

/* expression used as a statement (also a bare call) */
static t_parse_rc parse_expression_statement(t_parser *p, AST_STATEMENT **out) {
    SOURCE_LOCATION loc = loc_of(&p->cur);
    AST_EXPRESSION *expr = NULL;
    TRY(parse_expression(p, &expr));
    AST_STATEMENT *stmt = ast_statement_create_expr(expr, loc);
    if (stmt == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out = stmt;
    return PARSE_OK;
}

static t_parse_rc parse_statement_body(t_parser *p, AST_STATEMENT **out) {
    switch (p->cur.type) {
    case TOKEN_KW_VAL:
    case TOKEN_KW_VAR:
        return parse_var_decl(p, out);
    case TOKEN_KW_RETURN:
        return parse_return(p, out);
    case TOKEN_KW_IF:
        return parse_if_statement(p, out);
    case TOKEN_KW_WHILE:
        return parse_while(p, out);
    case TOKEN_ID: {
        // `x = ...` is an assignment; anything else starting with an ID is
        // an expression or a call
        t_token_type next;
        TRY(parser_peek_type(p, &next));
        if (next == TOKEN_ASSIGN) {
            return parse_assignment(p, out);
        }
        break;
    }
    default:
        break;
    }
    return parse_expression_statement(p, out);
}

t_parse_rc parse_statement(t_parser *p, AST_STATEMENT **out) {
    TRY(parser_enter(p));
    t_parse_rc rc = parse_statement_body(p, out);
    parser_leave(p);
    return rc;
}