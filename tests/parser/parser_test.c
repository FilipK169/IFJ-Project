#include "ast.h"
#include "parser.h"

#include <stdio.h>

static void dump_expr(const AST_EXPRESSION *e);
static void dump_block(const AST_BLOCK *b);

/** Name of a data type. "?" means the type is not written. */
static const char *type_name(ENUM_DATA_TYPE t)
{
    switch (t)
    {
    case TYPE_INT:
        return "Int";
    case TYPE_DOUBLE:
        return "Double";
    case TYPE_STRING:
        return "String";
    case TYPE_BOOLEAN:
        return "Boolean";
    case TYPE_UNIT:
        return "Unit";
    default:
        return "?";
    }
}

/** Prints the condition and the branches of an if. */
static void dump_if(const AST_IF *n, bool is_statement)
{
    dump_expr(n->condition);
    dump_block(n->then_block);
    if (n->else_block != NULL || !is_statement)
    {
        dump_block(n->else_block);
    }
}

static void dump_expr(const AST_EXPRESSION *e)
{
    if (e == NULL)
    {
        return;
    }
    switch (e->type)
    {
    case EXPR_VAR_REF:
        printf("var %s ", e->data.var_ref.name);
        break;
    case EXPR_INT_LITERAL:
        printf("int %lld ", (long long)e->data.int_literal);
        break;
    case EXPR_DOUBLE_LITERAL:
        printf("double %g ", e->data.double_literal);
        break;
    case EXPR_STRING_LITERAL:
        printf("string \"%s\" ", e->data.string_literal.value);
        break;
    case EXPR_UNIT_LITERAL:
        printf("unit ");
        break;
    case EXPR_BOOL_LITERAL:
        printf("bool %s ", e->data.bool_literal ? "true" : "false");
        break;
    case EXPR_UNARY:
        printf("unary %d ", (int)e->data.unary.op);
        dump_expr(e->data.unary.operand);
        break;
    case EXPR_BINARY:
        printf("binary %d ", (int)e->data.binary.op);
        dump_expr(e->data.binary.left);
        dump_expr(e->data.binary.right);
        break;
    case EXPR_CALL:
    {
        const AST_CALL *c = &e->data.call;
        printf("call %s (%zu list%s) ", c->name, c->argument_list_count,
               c->argument_list_count == 1 ? "" : "s");
        if (c->receiver != NULL)
        {
            printf("receiver: ");
            dump_expr(c->receiver);
        }
        for (size_t i = 0; i < c->argument_list_count; i++)
        {
            printf("args[%zu]: ", i);
            for (size_t j = 0; j < c->argument_lists[i].argument_count; j++)
            {
                dump_expr(c->argument_lists[i].arguments[j]);
            }
        }
        break;
    }
    case EXPR_MEMBER:
        printf("member .%s ", e->data.member.member_name);
        dump_expr(e->data.member.receiver);
        break;
    case EXPR_INTERPOLATED:
    {
        const AST_INTERPOLATED *s = &e->data.interpolated;
        printf("interpolated ");
        for (size_t i = 0; i < s->part_count; i++)
        {
            if (s->parts[i].is_literal)
            {
                printf("text \"%s\" ", s->parts[i].text);
            }
            else
            {
                dump_expr(s->parts[i].expression);
            }
        }
        break;
    }
    case EXPR_IF:
        printf("if-expr ");
        dump_if(&e->data.if_expr, false);
        break;
    }
}

static void dump_stmt(const AST_STATEMENT *s)
{
    switch (s->type)
    {
    case STMT_VAR_DECL:
    {
        const AST_VAR_DECL *d = &s->data.var_decl;
        printf("%s %s : %s ", d->is_mutable ? "var" : "val", d->name, type_name(d->declared_type));
        dump_expr(d->initializer);
        break;
    }
    case STMT_ASSIGNMENT:
        printf("assign %s ", s->data.assignment.name);
        dump_expr(s->data.assignment.value);
        break;
    case STMT_RETURN:
        printf("return ");
        dump_expr(s->data.return_stmt.value);
        break;
    case STMT_IF:
        printf("if ");
        dump_if(&s->data.if_stmt, true);
        break;
    case STMT_WHILE:
        printf("while ");
        dump_expr(s->data.while_stmt.condition);
        dump_block(s->data.while_stmt.body);
        break;
    case STMT_FOR:
    {
        const AST_FOR *f = &s->data.for_stmt;
        printf("for %s %s ", f->var_name, f->is_exclusive ? "until" : "to");
        dump_expr(f->from);
        dump_expr(f->to);
        if (f->step != NULL)
        {
            printf("by: ");
            dump_expr(f->step);
        }
        for (size_t i = 0; i < f->guard_count; i++)
        {
            printf("guard: ");
            dump_expr(f->guards[i]);
        }
        dump_block(f->body);
        break;
    }
    case STMT_EXPR:
        printf("expr ");
        dump_expr(s->data.expr_stmt.expression);
        break;
    }
}

static void dump_block(const AST_BLOCK *b)
{
    printf("block ");
    if (b == NULL)
    {
        return;
    }
    for (size_t i = 0; i < b->statement_count; i++)
    {
        dump_stmt(b->statements[i]);
    }
}

static void dump_function(const AST_FUNCTION_DEF *f)
{
    printf("def %s ", f->name);
    if (f->is_main)
    {
        printf("@main ");
    }
    if (f->receiver != NULL)
    {
        printf("receiver(%s: %s) ", f->receiver->name, type_name(f->receiver->type));
    }
    printf("lists=%zu ", f->param_list_count);
    for (size_t i = 0; i < f->param_list_count; i++)
    {
        const AST_PARAM_LIST *l = &f->param_lists[i];
        printf("[");
        for (size_t j = 0; j < l->parameter_count; j++)
        {
            printf("%s%s:%s", j > 0 ? "," : "", l->parameters[j].name,
                   type_name(l->parameters[j].type));
        }
        printf("] ");
    }
    printf("-> %s ", type_name(f->declared_return_type));
    dump_block(f->body);
    printf("\n");
}

static void dump_program(const AST_PROGRAM *p)
{
    for (size_t i = 0; i < p->function_count; i++)
    {
        dump_function(p->functions[i]);
    }
}

int main(void)
{
    AST_PROGRAM *program = NULL;
    t_parse_rc rc = parser_parse(stdin, &program);
    if (rc != PARSE_OK)
    {
        return (int)rc;
    }
    dump_program(program);
    ast_program_destroy(program);
    return 0;
}