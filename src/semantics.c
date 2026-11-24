/**
 * @file semantics.c
 * @brief Semantic analysis implementation.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "semantics.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
  FUNCTION_UNVISITED,
  FUNCTION_IN_PROGRESS,
  FUNCTION_DONE
} t_function_analysis_state;

typedef struct {
  AST_FUNCTION_DEF *function;
  t_function_analysis_state state;
} t_function_analysis;

typedef struct {
  t_symtable *table;

  t_function_analysis *functions;
  size_t function_count;
} t_semantics_context;

// ========================================== //
// ========= FORWARD DECLARATIONS =========== //
// ========================================== //

// ---------- Built-ins and first/second pass ---------- //

static t_semantics_result register_builtin(t_symtable *table, const char *name,
                                           ENUM_DATA_TYPE return_type,
                                           ENUM_DATA_TYPE *parameter_types,
                                           size_t parameter_count);

static t_semantics_result register_builtins(t_symtable *table);

static t_semantics_result
validate_function_header(const AST_FUNCTION_DEF *function);

static void destroy_temporary_function_symbol(t_function_symbol *symbol);

static t_semantics_result
build_function_symbol(const AST_FUNCTION_DEF *function,
                      t_function_symbol *out_symbol);

static t_semantics_result add_function(AST_FUNCTION_DEF *function,
                                       t_symtable *table);

static t_semantics_result semantics_first_pass(AST_PROGRAM *program,
                                               t_symtable *table);

static t_semantics_result semantics_second_pass(AST_PROGRAM *program,
                                                t_symtable *table);

// ---------- Function analysis ---------- //

static t_semantics_result analyze_function(AST_FUNCTION_DEF *function,
                                           t_semantics_context *context);

static t_semantics_result add_function_parameters(AST_FUNCTION_DEF *function,
                                                  t_symtable *table);

static t_semantics_result add_parameter(AST_PARAMETER *parameter,
                                        t_symtable *table);

// ---------- Function return analysis ---------- //

static bool block_always_returns(AST_BLOCK *block);

static bool block_result_is_unit(AST_BLOCK *block);

static t_semantics_result resolve_function_return(AST_FUNCTION_DEF *function,
                                                  t_semantics_context *context);

// ---------- Block and statement analysis ---------- //

static t_semantics_result analyze_block(AST_BLOCK *block,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context,
                                        bool create_new_scope);

static t_semantics_result analyze_statement(AST_STATEMENT *statement,
                                            AST_FUNCTION_DEF *function,
                                            t_semantics_context *context);

static t_semantics_result analyze_var_decl(AST_VAR_DECL *declaration,
                                           AST_FUNCTION_DEF *function,
                                           t_semantics_context *context);

static t_semantics_result analyze_assignment(AST_ASSIGNMENT *assignment,
                                             AST_FUNCTION_DEF *function,
                                             t_semantics_context *context);

static t_semantics_result analyze_return(AST_RETURN *return_statement,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context);

static t_semantics_result analyze_if(AST_IF *if_statement,
                                     AST_FUNCTION_DEF *function,
                                     t_semantics_context *context);

static t_semantics_result analyze_while(AST_WHILE *while_statement,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context);

static t_semantics_result analyze_for(AST_FOR *for_statement,
                                      AST_FUNCTION_DEF *function,
                                      t_semantics_context *context);

static t_semantics_result analyze_expr_statement(AST_EXPR_STMT *statement,
                                                 AST_FUNCTION_DEF *function,
                                                 t_semantics_context *context);

// ---------- Expression analysis ---------- //

static t_semantics_result analyze_expression(AST_EXPRESSION *expression,
                                             AST_FUNCTION_DEF *function,
                                             t_semantics_context *context);

static t_semantics_result analyze_var_ref(AST_EXPRESSION *expression,
                                          t_symtable *table);

static t_semantics_result analyze_unary(AST_EXPRESSION *expression,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context);

static t_semantics_result analyze_binary(AST_EXPRESSION *expression,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context);

static t_semantics_result analyze_call(AST_EXPRESSION *expression,
                                       AST_FUNCTION_DEF *function,
                                       t_semantics_context *context);

static t_semantics_result analyze_member(AST_EXPRESSION *expression,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context);

static t_semantics_result analyze_interpolated(AST_EXPRESSION *expression,
                                               AST_FUNCTION_DEF *function,
                                               t_semantics_context *context);

static t_semantics_result analyze_if_expression(AST_EXPRESSION *expression,
                                                AST_FUNCTION_DEF *function,
                                                t_semantics_context *context);

// ---------- Type and result helpers ---------- //

static t_semantics_result check_expression_type(AST_EXPRESSION *expression,
                                                ENUM_DATA_TYPE expected_type);

static AST_EXPRESSION *get_block_result_expression(AST_BLOCK *block);

static ENUM_DATA_TYPE get_statement_result_type(AST_STATEMENT *statement);

// ========================================== //
// ============ STATIC FUNCTIONS ============ //
// ========================================== //

static t_semantics_result register_builtin(t_symtable *table, const char *name,
                                           ENUM_DATA_TYPE return_type,
                                           ENUM_DATA_TYPE *parameter_types,
                                           size_t parameter_count) {
  t_function_parameter_list parameter_list = {
      .types = parameter_types,
      .count = parameter_count,
  };

  t_function_symbol symbol = {
      .name = (char *)name,
      .kind = FUNCTION_BUILTIN,
      .return_type = return_type,
      .parameter_lists = &parameter_list,
      .parameter_list_count = 1,
      .receiver_type = TYPE_UNSPECIFIED,
      .is_main = false,
  };

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result result = symtable_insert_function(table, &symbol, &id);

  if (result == SYMTABLE_ERR_DUPLICATE)
    return SEMANTICS_ERR_REDEFINITION;

  if (result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  return SEMANTICS_OK;
}

static t_semantics_result register_builtins(t_symtable *table) {
  if (!table)
    return SEMANTICS_ERR_INTERNAL;

  ENUM_DATA_TYPE any[] = {TYPE_ANY};
  ENUM_DATA_TYPE double_param[] = {TYPE_DOUBLE};
  ENUM_DATA_TYPE string_param[] = {TYPE_STRING};
  ENUM_DATA_TYPE substring_params[] = {
      TYPE_STRING,
      TYPE_INT,
      TYPE_INT,
  };
  ENUM_DATA_TYPE ord_params[] = {
      TYPE_STRING,
      TYPE_INT,
  };
  ENUM_DATA_TYPE int_param[] = {TYPE_INT};

  t_semantics_result result;

  result = register_builtin(table, "print", TYPE_UNIT, any, 1);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "readLine", TYPE_STRING, NULL, 0);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "readInt", TYPE_INT, NULL, 0);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "readDouble", TYPE_DOUBLE, NULL, 0);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "toStr", TYPE_STRING, any, 1);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "toInt", TYPE_INT, double_param, 1);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "strLength", TYPE_INT, string_param, 1);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "subStr", TYPE_STRING, substring_params, 3);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "ord", TYPE_INT, ord_params, 2);
  if (result != SEMANTICS_OK)
    return result;

  result = register_builtin(table, "chr", TYPE_STRING, int_param, 1);
  if (result != SEMANTICS_OK)
    return result;

    result = register_builtin(table, "readBool", TYPE_BOOLEAN, NULL, 0);
  if (result != SEMANTICS_OK)
    return result;

  return SEMANTICS_OK;
}

static t_semantics_result
validate_function_header(const AST_FUNCTION_DEF *function) {
  if (!function || !function->name)
    return SEMANTICS_ERR_INTERNAL;

  if (function->param_list_count > 0 && !function->param_lists)
    return SEMANTICS_ERR_INTERNAL;

  if (function->receiver && !function->receiver->name)
    return SEMANTICS_ERR_INTERNAL;

  // @main must not have parameters
  if (function->is_main) {
    for (size_t i = 0; i < function->param_list_count; i++) {
      if (function->param_lists[i].parameter_count != 0)
        return SEMANTICS_ERR_FUNCTION_ARGS;
    }

    if (function->declared_return_type != TYPE_UNSPECIFIED &&
        function->declared_return_type != TYPE_UNIT)
      return SEMANTICS_ERR_TYPE;
  }

  // Parameter names must be unique across all parameter lists
  for (size_t i = 0; i < function->param_list_count; i++) {
    const AST_PARAM_LIST *list = &function->param_lists[i];

    if (list->parameter_count > 0 && !list->parameters)
      return SEMANTICS_ERR_INTERNAL;

    for (size_t j = 0; j < list->parameter_count; j++) {
      const AST_PARAMETER *parameter = &list->parameters[j];

      if (!parameter->name)
        return SEMANTICS_ERR_INTERNAL;

      // Receiver and parameter share the function scope
      if (function->receiver &&
          strcmp(function->receiver->name, parameter->name) == 0)
        return SEMANTICS_ERR_REDEFINITION;

      // Check all parameters from previous parameter lists
      for (size_t previous_list = 0; previous_list < i; previous_list++) {
        const AST_PARAM_LIST *other = &function->param_lists[previous_list];

        for (size_t k = 0; k < other->parameter_count; k++) {
          if (strcmp(other->parameters[k].name, parameter->name) == 0)
            return SEMANTICS_ERR_REDEFINITION;
        }
      }

      // Check previous parameters in the current parameter list
      for (size_t k = 0; k < j; k++) {
        if (strcmp(list->parameters[k].name, parameter->name) == 0)
          return SEMANTICS_ERR_REDEFINITION;
      }
    }
  }

  return SEMANTICS_OK;
}

static void destroy_temporary_function_symbol(t_function_symbol *symbol) {
  if (!symbol)
    return;

  for (size_t i = 0; i < symbol->parameter_list_count; i++) {
    free(symbol->parameter_lists[i].types);
  }

  free(symbol->parameter_lists);

  symbol->parameter_lists = NULL;
  symbol->parameter_list_count = 0;

  // name is borrowed from AST, DONT TOUCH IT
  symbol->name = NULL;
}

static t_semantics_result
build_function_symbol(const AST_FUNCTION_DEF *function,
                      t_function_symbol *out_symbol) {
  if (!function || !out_symbol)
    return SEMANTICS_ERR_INTERNAL;

  *out_symbol = (t_function_symbol){0};

  // Borrowed from AST, symtable will deep-copy it
  out_symbol->name = function->name;

  out_symbol->kind = function->receiver ? FUNCTION_EXTENSION : FUNCTION_USER;

  out_symbol->return_type = function->declared_return_type;

  out_symbol->receiver_type =
      function->receiver ? function->receiver->type : TYPE_UNSPECIFIED;

  out_symbol->is_main = function->is_main;

  if (function->param_list_count == 0)
    return SEMANTICS_OK;

  out_symbol->parameter_lists =
      calloc(function->param_list_count, sizeof(t_function_parameter_list));

  if (!out_symbol->parameter_lists)
    return SEMANTICS_ERR_INTERNAL;

  out_symbol->parameter_list_count = function->param_list_count;

  for (size_t i = 0; i < function->param_list_count; i++) {
    const AST_PARAM_LIST *ast_list = &function->param_lists[i];
    t_function_parameter_list *symbol_list = &out_symbol->parameter_lists[i];

    symbol_list->count = ast_list->parameter_count;

    if (ast_list->parameter_count == 0)
      continue;

    symbol_list->types =
        malloc(ast_list->parameter_count * sizeof(ENUM_DATA_TYPE));

    if (!symbol_list->types) {
      destroy_temporary_function_symbol(out_symbol);
      return SEMANTICS_ERR_INTERNAL;
    }

    for (size_t j = 0; j < ast_list->parameter_count; j++)
      symbol_list->types[j] = ast_list->parameters[j].type;
  }

  return SEMANTICS_OK;
}

static t_semantics_result add_function(AST_FUNCTION_DEF *function,
                                       t_symtable *table) {
  if (!function || !table)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result = validate_function_header(function);
  if (result != SEMANTICS_OK)
    return result;

  t_function_symbol symbol = {0};

  result = build_function_symbol(function, &symbol);
  if (result != SEMANTICS_OK)
    return result;

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result =
      symtable_insert_function(table, &symbol, &id);

  destroy_temporary_function_symbol(&symbol);

  if (table_result == SYMTABLE_ERR_DUPLICATE)
    return SEMANTICS_ERR_REDEFINITION;

  if (table_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  function->function_id = id;

  return SEMANTICS_OK;
}

static t_semantics_result semantics_first_pass(AST_PROGRAM *program,
                                               t_symtable *table) {
  size_t index = 0;
  size_t main_count = 0;

  while (index < program->function_count) {
    AST_FUNCTION_DEF *function = program->functions[index];

    t_semantics_result result = add_function(function, table);
    if (result != SEMANTICS_OK)
      return result;

    if (function->is_main)
      main_count++;

    index++;
  }

  if (main_count == 0)
    return SEMANTICS_ERR_UNDEFINED;

  if (main_count > 1)
    return SEMANTICS_ERR_REDEFINITION;

  return SEMANTICS_OK;
}

static t_semantics_result semantics_second_pass(AST_PROGRAM *program,
                                                t_symtable *table) {
  if (!program || !table)
    return SEMANTICS_ERR_INTERNAL;

  size_t max_function_id = 0;

  for (size_t i = 0; i < program->function_count; i++) {
    AST_FUNCTION_DEF *function = program->functions[i];

    if (!function)
      return SEMANTICS_ERR_INTERNAL;

    if (function->function_id > max_function_id)
      max_function_id = function->function_id;
  }

  size_t function_count = max_function_id + 1;

  t_function_analysis *functions =
      calloc(function_count, sizeof(t_function_analysis));

  if (!functions)
    return SEMANTICS_ERR_INTERNAL;

  for (size_t i = 0; i < program->function_count; i++) {
    AST_FUNCTION_DEF *function = program->functions[i];

    functions[function->function_id].function = function;
    functions[function->function_id].state = FUNCTION_UNVISITED;
  }

  t_semantics_context context = {
      .table = table,
      .functions = functions,
      .function_count = function_count,
  };

  t_semantics_result result = SEMANTICS_OK;

  for (size_t i = 0; i < program->function_count; i++) {
    result = analyze_function(program->functions[i], &context);

    if (result != SEMANTICS_OK)
      break;
  }

  free(functions);

  return result;
}

static t_semantics_result analyze_function(AST_FUNCTION_DEF *function,
                                           t_semantics_context *context) {
  if (!function || !context || !context->table || !function->body)
    return SEMANTICS_ERR_INTERNAL;

  t_symbol_id id = function->function_id;

  if (id >= context->function_count)
    return SEMANTICS_ERR_INTERNAL;

  t_function_analysis *analysis = &context->functions[id];

  if (analysis->function != function)
    return SEMANTICS_ERR_INTERNAL;

  if (analysis->state == FUNCTION_DONE)
    return SEMANTICS_OK;

  if (analysis->state == FUNCTION_IN_PROGRESS)
    return SEMANTICS_ERR_TYPE;

  analysis->state = FUNCTION_IN_PROGRESS;

  t_symtable *table = context->table;

  if (symtable_enter_scope(table) != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  // Add receiver and parameters into the function scope
  t_semantics_result result = add_function_parameters(function, table);

  // Analyze statements in the function body
  if (result == SEMANTICS_OK)
    result = analyze_block(function->body, function, context, false);

  // Resolve/check function return type
  if (result == SEMANTICS_OK)
    result = resolve_function_return(function, context);

  // Always destroy the function scope, even if analysis failed
  t_symtable_result leave_result = symtable_leave_scope(table);

  if (leave_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  if (result == SEMANTICS_OK)
    analysis->state = FUNCTION_DONE;

  return result;
}

static bool block_always_returns(AST_BLOCK *block) {
  if (!block)
    return false;

  for (size_t i = 0; i < block->statement_count; i++) {
    AST_STATEMENT *statement = block->statements[i];

    if (!statement)
      return false;

    if (statement->type == STMT_RETURN)
      return true;

    if (statement->type == STMT_IF) {
      AST_IF *if_statement = &statement->data.if_stmt;

      if (if_statement->then_block && if_statement->else_block &&
          block_always_returns(if_statement->then_block) &&
          block_always_returns(if_statement->else_block))
        return true;
    }
  }

  return false;
}

static bool block_result_is_unit(AST_BLOCK *block) {
  if (!block)
    return false;

  if (block->statement_count == 0)
    return true;

  AST_STATEMENT *last = block->statements[block->statement_count - 1];

  if (!last)
    return false;

  if (last->type == STMT_IF) {
    AST_IF *if_statement = &last->data.if_stmt;

    if (!if_statement->then_block)
      return false;

    if (!block_result_is_unit(if_statement->then_block))
      return false;

    if (!if_statement->else_block)
      return true;

    return block_result_is_unit(if_statement->else_block);
  }

  return get_statement_result_type(last) == TYPE_UNIT;
}

static t_semantics_result
resolve_function_return(AST_FUNCTION_DEF *function,
                        t_semantics_context *context) {
  if (!function || !function->body || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  AST_BLOCK *body = function->body;

  ENUM_DATA_TYPE resolved_type = TYPE_UNSPECIFIED;

  bool always_returns = block_always_returns(body);

  // Explicit Unit always returns Unit.
  // Any body result is evaluated but discarded.
  if (function->declared_return_type == TYPE_UNIT) {
    resolved_type = TYPE_UNIT;
  }

  // Explicit non-Unit return type
  else if (function->declared_return_type != TYPE_UNSPECIFIED) {
    resolved_type = function->declared_return_type;

    // If every path already explicitly returns, analyze_return()
    // has checked all returned values.
    if (!always_returns) {
      AST_EXPRESSION *result_expression = get_block_result_expression(body);

      if (!result_expression)
        return SEMANTICS_ERR_TYPE;

      t_semantics_result result =
          check_expression_type(result_expression, resolved_type);

      if (result != SEMANTICS_OK)
        return result;
    }
  }

  // No explicit return type -> infer from body
  else {
    if (body->statement_count > 0) {
      AST_STATEMENT *last = body->statements[body->statement_count - 1];

      if (!last)
        return SEMANTICS_ERR_INTERNAL;

      // Statement-if has special inference rules
      if (last->type == STMT_IF) {
        if (!block_result_is_unit(body))
          return SEMANTICS_ERR_TYPE;

        resolved_type = TYPE_UNIT;
      } else {
        resolved_type = body->resolved_type;
      }
    } else {
      resolved_type = TYPE_UNIT;
    }

    if (resolved_type == TYPE_UNSPECIFIED)
      return SEMANTICS_ERR_INTERNAL;
  }

  // @main must resolve to Unit
  if (function->is_main && resolved_type != TYPE_UNIT)
    return SEMANTICS_ERR_TYPE;

  function->resolved_return_type = resolved_type;

  // Keep symbol table synchronized with inferred type
  t_function_symbol *symbol =
      symtable_get_function_by_id(context->table, function->function_id);

  if (!symbol)
    return SEMANTICS_ERR_INTERNAL;

  symbol->return_type = resolved_type;

  return SEMANTICS_OK;
}

static t_semantics_result add_function_parameters(AST_FUNCTION_DEF *function,
                                                  t_symtable *table) {
  if (!function || !table)
    return SEMANTICS_ERR_INTERNAL;

  // Add extension receiver first
  if (function->receiver) {
    t_semantics_result result = add_parameter(function->receiver, table);
    if (result != SEMANTICS_OK)
      return result;
  }

  // Add all normal parameters
  for (size_t i = 0; i < function->param_list_count; i++) {
    AST_PARAM_LIST *list = &function->param_lists[i];

    for (size_t j = 0; j < list->parameter_count; j++) {
      t_semantics_result result = add_parameter(&list->parameters[j], table);

      if (result != SEMANTICS_OK)
        return result;
    }
  }

  return SEMANTICS_OK;
}

static t_semantics_result add_parameter(AST_PARAMETER *parameter,
                                        t_symtable *table) {
  if (!parameter || !parameter->name || !table)
    return SEMANTICS_ERR_INTERNAL;

  t_variable_symbol symbol = {
      .name = parameter->name,
      .type = parameter->type,
      .is_mutable = false,
      .is_parameter = true,
  };

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result result = symtable_insert_variable(table, &symbol, &id);

  if (result == SYMTABLE_ERR_DUPLICATE)
    return SEMANTICS_ERR_REDEFINITION;

  if (result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  parameter->variable_id = id;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_block(AST_BLOCK *block,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context,
                                        bool create_new_scope) {
  if (!block || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  if (block->statement_count > 0 && !block->statements)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  if (create_new_scope) {
    if (symtable_enter_scope(table) != SYMTABLE_OK)
      return SEMANTICS_ERR_INTERNAL;
  }

  t_semantics_result result = SEMANTICS_OK;

  for (size_t i = 0; i < block->statement_count; i++) {
    AST_STATEMENT *statement = block->statements[i];

    result = analyze_statement(statement, function, context);
    if (result != SEMANTICS_OK)
      break;
  }

  if (result == SEMANTICS_OK) {
    if (block->statement_count == 0) {
      block->resolved_type = TYPE_UNIT;
    } else {
      AST_STATEMENT *last = block->statements[block->statement_count - 1];

      block->resolved_type = get_statement_result_type(last);

      if (block->resolved_type == TYPE_UNSPECIFIED)
        result = SEMANTICS_ERR_INTERNAL;
    }
  }

  if (create_new_scope) {
    if (symtable_leave_scope(table) != SYMTABLE_OK)
      return SEMANTICS_ERR_INTERNAL;
  }

  return result;
}

static t_semantics_result analyze_expr_statement(AST_EXPR_STMT *statement,
                                                 AST_FUNCTION_DEF *function,
                                                 t_semantics_context *context) {
  if (!statement || !statement->expression || !function || !context ||
      !context->table)
    return SEMANTICS_ERR_INTERNAL;

  return analyze_expression(statement->expression, function, context);
}

static t_semantics_result analyze_assignment(AST_ASSIGNMENT *assignment,
                                             AST_FUNCTION_DEF *function,
                                             t_semantics_context *context) {
  if (!assignment || !assignment->name || !assignment->value || !function ||
      !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result =
      symtable_find_variable(table, assignment->name, &id);

  if (table_result == SYMTABLE_ERR_NOT_FOUND)
    return SEMANTICS_ERR_UNDEFINED;

  if (table_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  t_variable_symbol *symbol = symtable_get_variable_by_id(table, id);

  if (!symbol)
    return SEMANTICS_ERR_INTERNAL;

  // val and function parameters cannot be assigned to
  if (!symbol->is_mutable)
    return SEMANTICS_ERR_IMMUTABLE;

  t_semantics_result result =
      analyze_expression(assignment->value, function, context);

  if (result != SEMANTICS_OK)
    return result;

  result = check_expression_type(assignment->value, symbol->type);

  if (result != SEMANTICS_OK)
    return result;

  assignment->variable_id = id;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_return(AST_RETURN *return_statement,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context) {
  if (!return_statement || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  // Any explicit return requires an explicitly declared return type
  if (function->declared_return_type == TYPE_UNSPECIFIED)
    return SEMANTICS_ERR_TYPE;

  // return without value
  if (!return_statement->value) {
    if (function->declared_return_type != TYPE_UNIT)
      return SEMANTICS_ERR_TYPE;

    return SEMANTICS_OK;
  }

  // Analyze returned expression
  t_semantics_result result =
      analyze_expression(return_statement->value, function, context);

  if (result != SEMANTICS_OK)
    return result;

  // Explicit Unit function evaluates the value but discards it
  if (function->declared_return_type == TYPE_UNIT)
    return SEMANTICS_OK;

  return check_expression_type(return_statement->value,
                               function->declared_return_type);
}

static t_semantics_result analyze_while(AST_WHILE *while_statement,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context) {
  if (!while_statement || !while_statement->condition ||
      !while_statement->body || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result =
      analyze_expression(while_statement->condition, function, context);

  if (result != SEMANTICS_OK)
    return result;

  if (while_statement->condition->resolved_type != TYPE_BOOLEAN)
    return SEMANTICS_ERR_TYPE;

  return analyze_block(while_statement->body, function, context, true);
}

static t_semantics_result analyze_for(AST_FOR *for_statement,
                                      AST_FUNCTION_DEF *function,
                                      t_semantics_context *context) {
  if (!for_statement || !for_statement->var_name || !for_statement->from ||
      !for_statement->to || !for_statement->body || !function || !context ||
      !context->table)
    return SEMANTICS_ERR_INTERNAL;

  if (for_statement->guard_count > 0 && !for_statement->guards)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  // Analyze range start
  t_semantics_result result =
      analyze_expression(for_statement->from, function, context);

  if (result != SEMANTICS_OK)
    return result;

  if (for_statement->from->resolved_type != TYPE_INT)
    return SEMANTICS_ERR_TYPE;

  // Analyze range end
  result = analyze_expression(for_statement->to, function, context);

  if (result != SEMANTICS_OK)
    return result;

  if (for_statement->to->resolved_type != TYPE_INT)
    return SEMANTICS_ERR_TYPE;

  // Optional step
  if (for_statement->step) {
    result = analyze_expression(for_statement->step, function, context);

    if (result != SEMANTICS_OK)
      return result;

    if (for_statement->step->resolved_type != TYPE_INT)
      return SEMANTICS_ERR_TYPE;

    // Literal zero step is forbidden
    if (for_statement->step->type == EXPR_INT_LITERAL &&
        for_statement->step->data.int_literal == 0)
      return SEMANTICS_ERR_OTHER;
  }

  // Scope containing the generator variable, guards and body
  if (symtable_enter_scope(table) != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  t_variable_symbol symbol = {
      .name = for_statement->var_name,
      .type = TYPE_INT,
      .is_mutable = false,
      .is_parameter = false,
  };

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result =
      symtable_insert_variable(table, &symbol, &id);

  if (table_result != SYMTABLE_OK) {
    symtable_leave_scope(table);

    if (table_result == SYMTABLE_ERR_DUPLICATE)
      return SEMANTICS_ERR_REDEFINITION;

    return SEMANTICS_ERR_INTERNAL;
  }

  for_statement->variable_id = id;

  // Guards may use the generator variable
  for (size_t i = 0; i < for_statement->guard_count; i++) {
    AST_EXPRESSION *guard = for_statement->guards[i];

    if (!guard) {
      result = SEMANTICS_ERR_INTERNAL;
      break;
    }

    result = analyze_expression(guard, function, context);

    if (result != SEMANTICS_OK)
      break;

    if (guard->resolved_type != TYPE_BOOLEAN) {
      result = SEMANTICS_ERR_TYPE;
      break;
    }
  }

  if (result == SEMANTICS_OK)
    result = analyze_block(for_statement->body, function, context, false);

  if (symtable_leave_scope(table) != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  return result;
}

static t_semantics_result analyze_if(AST_IF *if_statement,
                                     AST_FUNCTION_DEF *function,
                                     t_semantics_context *context) {
  if (!if_statement || !if_statement->condition || !if_statement->then_block ||
      !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result =
      analyze_expression(if_statement->condition, function, context);

  if (result != SEMANTICS_OK)
    return result;

  if (if_statement->condition->resolved_type != TYPE_BOOLEAN)
    return SEMANTICS_ERR_TYPE;

  result = analyze_block(if_statement->then_block, function, context, true);

  if (result != SEMANTICS_OK)
    return result;

  if (if_statement->else_block) {
    result = analyze_block(if_statement->else_block, function, context, true);

    if (result != SEMANTICS_OK)
      return result;
  }

  if_statement->resolved_type = TYPE_UNIT;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_statement(AST_STATEMENT *statement,
                                            AST_FUNCTION_DEF *function,
                                            t_semantics_context *context) {
  if (!statement || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  switch (statement->type) {
  case STMT_VAR_DECL:
    return analyze_var_decl(&statement->data.var_decl, function, context);

  case STMT_ASSIGNMENT:
    return analyze_assignment(&statement->data.assignment, function, context);

  case STMT_RETURN:
    return analyze_return(&statement->data.return_stmt, function, context);

  case STMT_IF:
    return analyze_if(&statement->data.if_stmt, function, context);

  case STMT_WHILE:
    return analyze_while(&statement->data.while_stmt, function, context);

  case STMT_FOR:
    return analyze_for(&statement->data.for_stmt, function, context);

  case STMT_EXPR:
    return analyze_expr_statement(&statement->data.expr_stmt, function,
                                  context);

  default:
    return SEMANTICS_ERR_INTERNAL;
  }
}

static t_semantics_result analyze_var_decl(AST_VAR_DECL *declaration,
                                           AST_FUNCTION_DEF *function,
                                           t_semantics_context *context) {

  if (!declaration || !declaration->name || !declaration->initializer ||
      !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  // Analyze initializer before adding the new variable into scope
  t_semantics_result result =
      analyze_expression(declaration->initializer, function, context);

  if (result != SEMANTICS_OK)
    return result;

  ENUM_DATA_TYPE variable_type;

  // No explicit type -> infer it from initializer
  if (declaration->declared_type == TYPE_UNSPECIFIED) {
    variable_type = declaration->initializer->resolved_type;
  } else {
    variable_type = declaration->declared_type;

    result = check_expression_type(declaration->initializer, variable_type);
    if (result != SEMANTICS_OK)
      return result;
  }

  t_variable_symbol symbol = {
      .name = declaration->name,
      .type = variable_type,
      .is_mutable = declaration->is_mutable,
      .is_parameter = false,
  };

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result =
      symtable_insert_variable(table, &symbol, &id);

  if (table_result == SYMTABLE_ERR_DUPLICATE)
    return SEMANTICS_ERR_REDEFINITION;

  if (table_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  declaration->resolved_type = variable_type;
  declaration->variable_id = id;

  return SEMANTICS_OK;
}

static t_semantics_result check_expression_type(AST_EXPRESSION *expression,
                                                ENUM_DATA_TYPE expected_type) {
  if (!expression)
    return SEMANTICS_ERR_INTERNAL;

  if (expression->resolved_type == expected_type)
    return SEMANTICS_OK;

  if (expression->resolved_type == TYPE_INT && expected_type == TYPE_DOUBLE) {
    expression->conversion = CONVERSION_INT_TO_DOUBLE;
    return SEMANTICS_OK;
  }

  return SEMANTICS_ERR_TYPE;
}

static t_semantics_result analyze_expression(AST_EXPRESSION *expression,
                                             AST_FUNCTION_DEF *function,
                                             t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  expression->conversion = CONVERSION_NONE;

  switch (expression->type) {
  case EXPR_VAR_REF:
    return analyze_var_ref(expression, table);

  case EXPR_INT_LITERAL:
    expression->resolved_type = TYPE_INT;
    return SEMANTICS_OK;

  case EXPR_DOUBLE_LITERAL:
    expression->resolved_type = TYPE_DOUBLE;
    return SEMANTICS_OK;

  case EXPR_STRING_LITERAL:
    expression->resolved_type = TYPE_STRING;
    return SEMANTICS_OK;

  case EXPR_UNIT_LITERAL:
    expression->resolved_type = TYPE_UNIT;
    return SEMANTICS_OK;

  case EXPR_BOOL_LITERAL:
    expression->resolved_type = TYPE_BOOLEAN;
    return SEMANTICS_OK;

  case EXPR_UNARY:
    return analyze_unary(expression, function, context);

  case EXPR_BINARY:
    return analyze_binary(expression, function, context);

  case EXPR_CALL:
    return analyze_call(expression, function, context);

  case EXPR_MEMBER:
    return analyze_member(expression, function, context);

  case EXPR_INTERPOLATED:
    return analyze_interpolated(expression, function, context);

  case EXPR_IF:
    return analyze_if_expression(expression, function, context);

  default:
    return SEMANTICS_ERR_INTERNAL;
  }
}

static t_semantics_result analyze_var_ref(AST_EXPRESSION *expression,
                                          t_symtable *table) {
  if (!expression || !table)
    return SEMANTICS_ERR_INTERNAL;

  AST_VAR_REF *var_ref = &expression->data.var_ref;

  if (!var_ref->name)
    return SEMANTICS_ERR_INTERNAL;

  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result result = symtable_find_variable(table, var_ref->name, &id);

  if (result == SYMTABLE_ERR_NOT_FOUND)
    return SEMANTICS_ERR_UNDEFINED;

  if (result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  t_variable_symbol *symbol = symtable_get_variable_by_id(table, id);

  if (!symbol)
    return SEMANTICS_ERR_INTERNAL;

  var_ref->variable_id = id;
  expression->resolved_type = symbol->type;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_unary(AST_EXPRESSION *expression,
                                        AST_FUNCTION_DEF *function,
                                        t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  AST_UNARY_EXPRESSION *unary = &expression->data.unary;

  if (!unary->operand)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result =
      analyze_expression(unary->operand, function, context);

  if (result != SEMANTICS_OK)
    return result;

  switch (unary->op) {
  case UNARY_PLUS:
  case UNARY_MINUS:
    if (unary->operand->resolved_type != TYPE_INT &&
        unary->operand->resolved_type != TYPE_DOUBLE)
      return SEMANTICS_ERR_TYPE;

    expression->resolved_type = unary->operand->resolved_type;
    return SEMANTICS_OK;

  case UNARY_NOT:
    if (unary->operand->resolved_type != TYPE_BOOLEAN)
      return SEMANTICS_ERR_TYPE;

    expression->resolved_type = TYPE_BOOLEAN;
    return SEMANTICS_OK;

  default:
    return SEMANTICS_ERR_INTERNAL;
  }
}

static t_semantics_result analyze_binary(AST_EXPRESSION *expression,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  AST_BINARY_EXPRESSION *binary = &expression->data.binary;

  if (!binary->left || !binary->right)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result =
      analyze_expression(binary->left, function, context);
  if (result != SEMANTICS_OK)
    return result;

  result = analyze_expression(binary->right, function, context);
  if (result != SEMANTICS_OK)
    return result;

  ENUM_DATA_TYPE left_type = binary->left->resolved_type;
  ENUM_DATA_TYPE right_type = binary->right->resolved_type;

  switch (binary->op) {

  case BINARY_ADD:
  case BINARY_SUB:
  case BINARY_MUL:
  case BINARY_DIV:
    // String + String
    if (binary->op == BINARY_ADD && left_type == TYPE_STRING &&
        right_type == TYPE_STRING) {
      expression->resolved_type = TYPE_STRING;
      return SEMANTICS_OK;
    }

    // Both must be numeric
    if ((left_type != TYPE_INT && left_type != TYPE_DOUBLE) ||
        (right_type != TYPE_INT && right_type != TYPE_DOUBLE))
      return SEMANTICS_ERR_TYPE;

    // Int + Int -> Int
    if (left_type == TYPE_INT && right_type == TYPE_INT) {
      expression->resolved_type = TYPE_INT;
      return SEMANTICS_OK;
    }

    // Otherwise result is Double and Int operand is converted
    if (left_type == TYPE_INT)
      binary->left->conversion = CONVERSION_INT_TO_DOUBLE;

    if (right_type == TYPE_INT)
      binary->right->conversion = CONVERSION_INT_TO_DOUBLE;

    expression->resolved_type = TYPE_DOUBLE;
    return SEMANTICS_OK;

  // <, >, <=, >=
  case BINARY_LT:
  case BINARY_GT:
  case BINARY_LE:
  case BINARY_GE:
    // String comparison
    if (left_type == TYPE_STRING && right_type == TYPE_STRING) {
      expression->resolved_type = TYPE_BOOLEAN;
      return SEMANTICS_OK;
    }

    // Numeric comparison
    if ((left_type == TYPE_INT || left_type == TYPE_DOUBLE) &&
        (right_type == TYPE_INT || right_type == TYPE_DOUBLE)) {

      if (left_type == TYPE_INT && right_type == TYPE_DOUBLE)
        binary->left->conversion = CONVERSION_INT_TO_DOUBLE;

      if (left_type == TYPE_DOUBLE && right_type == TYPE_INT)
        binary->right->conversion = CONVERSION_INT_TO_DOUBLE;

      expression->resolved_type = TYPE_BOOLEAN;
      return SEMANTICS_OK;
    }

    return SEMANTICS_ERR_TYPE;

  // ==, !=
  case BINARY_EQ:
  case BINARY_NE:
    if (left_type == right_type) {
      expression->resolved_type = TYPE_BOOLEAN;
      return SEMANTICS_OK;
    }

    // Int and Double may be compared
    if (left_type == TYPE_INT && right_type == TYPE_DOUBLE) {
      binary->left->conversion = CONVERSION_INT_TO_DOUBLE;
      expression->resolved_type = TYPE_BOOLEAN;
      return SEMANTICS_OK;
    }

    if (left_type == TYPE_DOUBLE && right_type == TYPE_INT) {
      binary->right->conversion = CONVERSION_INT_TO_DOUBLE;
      expression->resolved_type = TYPE_BOOLEAN;
      return SEMANTICS_OK;
    }

    return SEMANTICS_ERR_TYPE;

  // &&, ||
  case BINARY_AND:
  case BINARY_OR:
    if (left_type != TYPE_BOOLEAN || right_type != TYPE_BOOLEAN)
      return SEMANTICS_ERR_TYPE;

    expression->resolved_type = TYPE_BOOLEAN;
    return SEMANTICS_OK;

  default:
    return SEMANTICS_ERR_INTERNAL;
  }
}

static t_semantics_result analyze_call(AST_EXPRESSION *expression,
                                       AST_FUNCTION_DEF *function,
                                       t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;

  AST_CALL *call = &expression->data.call;

  if (!call->name)
    return SEMANTICS_ERR_INTERNAL;

  bool has_receiver = call->receiver != NULL;
  ENUM_DATA_TYPE receiver_type = TYPE_UNSPECIFIED;

  if (has_receiver) {
    t_semantics_result result =
        analyze_expression(call->receiver, function, context);

    if (result != SEMANTICS_OK)
      return result;

    receiver_type = call->receiver->resolved_type;
  }

  // Find called function
  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result = symtable_find_function(
      table, call->name, has_receiver, receiver_type, &id);

  if (table_result == SYMTABLE_ERR_NOT_FOUND)
    return SEMANTICS_ERR_UNDEFINED;

  if (table_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  t_function_symbol *symbol = symtable_get_function_by_id(table, id);

  if (!symbol)
    return SEMANTICS_ERR_INTERNAL;

  // Check number of argument lists
  if (call->argument_list_count != symbol->parameter_list_count)
    return SEMANTICS_ERR_FUNCTION_ARGS;

  // Check arguments
  for (size_t i = 0; i < call->argument_list_count; i++) {
    AST_ARG_LIST *arguments = &call->argument_lists[i];
    t_function_parameter_list *parameters = &symbol->parameter_lists[i];

    if (arguments->argument_count != parameters->count)
      return SEMANTICS_ERR_FUNCTION_ARGS;

    for (size_t j = 0; j < arguments->argument_count; j++) {
      AST_EXPRESSION *argument = arguments->arguments[j];

      if (!argument)
        return SEMANTICS_ERR_INTERNAL;

      t_semantics_result result =
          analyze_expression(argument, function, context);

      if (result != SEMANTICS_OK)
        return result;

      ENUM_DATA_TYPE expected_type = parameters->types[j];

      // TYPE_ANY accepts any argument type
      if (expected_type != TYPE_ANY) {
        result = check_expression_type(argument, expected_type);

        if (result != SEMANTICS_OK)
          return SEMANTICS_ERR_FUNCTION_ARGS;
      }
    }
  }

  // If the return type is not known yet, analyze the called function first
  if (symbol->return_type == TYPE_UNSPECIFIED) {
    if (id >= context->function_count)
      return SEMANTICS_ERR_INTERNAL;

    t_function_analysis *analysis = &context->functions[id];

    if (!analysis->function)
      return SEMANTICS_ERR_INTERNAL;

    // We need the type of a function that is already waiting for its type.
    // This means recursive return-type inference, which is not allowed.
    if (analysis->state == FUNCTION_IN_PROGRESS)
      return SEMANTICS_ERR_TYPE;

    if (analysis->state == FUNCTION_UNVISITED) {
      t_scope *saved_scope = symtable_detach_scope(table);

      t_semantics_result result = analyze_function(analysis->function, context);

      t_symtable_result restore_result =
          symtable_restore_scope(table, saved_scope);

      if (restore_result != SYMTABLE_OK)
        return SEMANTICS_ERR_INTERNAL;

      if (result != SEMANTICS_OK)
        return result;
    }

    // Nested analysis must have determined the return type
    if (analysis->function->resolved_return_type == TYPE_UNSPECIFIED)
      return SEMANTICS_ERR_INTERNAL;

    // Keep the function symbol in sync with the AST
    symbol->return_type = analysis->function->resolved_return_type;
  }

  // Annotate AST for code generation
  call->function_id = id;

  switch (symbol->kind) {
  case FUNCTION_USER:
    call->callee_kind = CALLEE_USER_FUNCTION;
    break;

  case FUNCTION_BUILTIN:
    call->callee_kind = CALLEE_BUILTIN;
    break;

  case FUNCTION_EXTENSION:
    call->callee_kind = CALLEE_EXTENSION;
    break;

  default:
    return SEMANTICS_ERR_INTERNAL;
  }

  expression->resolved_type = symbol->return_type;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_member(AST_EXPRESSION *expression,
                                         AST_FUNCTION_DEF *function,
                                         t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  t_symtable *table = context->table;
  AST_MEMBER *member = &expression->data.member;

  if (!member->receiver || !member->member_name)
    return SEMANTICS_ERR_INTERNAL;

  t_semantics_result result =
      analyze_expression(member->receiver, function, context);

  if (result != SEMANTICS_OK)
    return result;

  ENUM_DATA_TYPE receiver_type = member->receiver->resolved_type;

  member->member_kind = MEMBER_UNKNOWN;
  member->function_id = SYMBOL_ID_INVALID;

  // value.toString -> String
  if (strcmp(member->member_name, "toString") == 0) {
    member->member_kind = MEMBER_TO_STRING;
    expression->resolved_type = TYPE_STRING;
    return SEMANTICS_OK;
  }

  // Double.toInt -> Int
  if (strcmp(member->member_name, "toInt") == 0) {
    if (receiver_type != TYPE_DOUBLE)
      return SEMANTICS_ERR_TYPE;

    member->member_kind = MEMBER_TO_INT;
    expression->resolved_type = TYPE_INT;
    return SEMANTICS_OK;
  }

  // Double.floor -> Double
  if (strcmp(member->member_name, "floor") == 0) {
    if (receiver_type != TYPE_DOUBLE)
      return SEMANTICS_ERR_TYPE;

    member->member_kind = MEMBER_FLOOR;
    expression->resolved_type = TYPE_DOUBLE;
    return SEMANTICS_OK;
  }

  // String.length -> Int
  if (strcmp(member->member_name, "length") == 0) {
    if (receiver_type != TYPE_STRING)
      return SEMANTICS_ERR_TYPE;

    member->member_kind = MEMBER_LENGTH;
    expression->resolved_type = TYPE_INT;
    return SEMANTICS_OK;
  }

  // Otherwise try a parameterless extension method
  t_symbol_id id = SYMBOL_ID_INVALID;

  t_symtable_result table_result = symtable_find_function(
      table, member->member_name, true, receiver_type, &id);

  if (table_result == SYMTABLE_ERR_NOT_FOUND)
    return SEMANTICS_ERR_UNDEFINED;

  if (table_result != SYMTABLE_OK)
    return SEMANTICS_ERR_INTERNAL;

  t_function_symbol *symbol = symtable_get_function_by_id(table, id);

  if (!symbol)
    return SEMANTICS_ERR_INTERNAL;

  if (symbol->kind != FUNCTION_EXTENSION)
    return SEMANTICS_ERR_INTERNAL;

  // x.foo is valid only for a method declared without a parameter list.
  // foo() would have one empty parameter list instead.
  if (symbol->parameter_list_count != 0)
    return SEMANTICS_ERR_FUNCTION_ARGS;

  // Resolve unknown extension return type just like in analyze_call()
  if (symbol->return_type == TYPE_UNSPECIFIED) {
    if (id >= context->function_count)
      return SEMANTICS_ERR_INTERNAL;

    t_function_analysis *analysis = &context->functions[id];

    if (!analysis->function)
      return SEMANTICS_ERR_INTERNAL;

    if (analysis->state == FUNCTION_IN_PROGRESS)
      return SEMANTICS_ERR_TYPE;

    if (analysis->state == FUNCTION_UNVISITED) {
      t_scope *saved_scope = symtable_detach_scope(table);

      result = analyze_function(analysis->function, context);

      t_symtable_result restore_result =
          symtable_restore_scope(table, saved_scope);

      if (restore_result != SYMTABLE_OK)
        return SEMANTICS_ERR_INTERNAL;

      if (result != SEMANTICS_OK)
        return result;
    }

    if (analysis->function->resolved_return_type == TYPE_UNSPECIFIED)
      return SEMANTICS_ERR_INTERNAL;

    symbol->return_type = analysis->function->resolved_return_type;
  }

  member->member_kind = MEMBER_EXT_PROPERTY;
  member->function_id = id;
  expression->resolved_type = symbol->return_type;

  return SEMANTICS_OK;
}

static t_semantics_result analyze_interpolated(AST_EXPRESSION *expression,
                                               AST_FUNCTION_DEF *function,
                                               t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  AST_INTERPOLATED *interpolated = &expression->data.interpolated;

  if (interpolated->part_count > 0 && !interpolated->parts)
    return SEMANTICS_ERR_INTERNAL;

  for (size_t i = 0; i < interpolated->part_count; i++) {
    AST_INTERP_PART *part = &interpolated->parts[i];

    if (part->is_literal) {
      if (!part->text)
        return SEMANTICS_ERR_INTERNAL;

      continue;
    }

    if (!part->expression)
      return SEMANTICS_ERR_INTERNAL;

    t_semantics_result result =
        analyze_expression(part->expression, function, context);

    if (result != SEMANTICS_OK)
      return result;
  }

  expression->resolved_type = TYPE_STRING;

  return SEMANTICS_OK;
}

static AST_EXPRESSION *get_block_result_expression(AST_BLOCK *block) {
  if (!block || block->statement_count == 0)
    return NULL;

  AST_STATEMENT *last = block->statements[block->statement_count - 1];

  if (!last || last->type != STMT_EXPR)
    return NULL;

  return last->data.expr_stmt.expression;
}

static t_semantics_result analyze_if_expression(AST_EXPRESSION *expression,
                                                AST_FUNCTION_DEF *function,
                                                t_semantics_context *context) {
  if (!expression || !function || !context || !context->table)
    return SEMANTICS_ERR_INTERNAL;

  AST_IF *if_expression = &expression->data.if_expr;

  if (!if_expression->condition || !if_expression->then_block ||
      !if_expression->else_block)
    return SEMANTICS_ERR_INTERNAL;

  // Analyze condition
  t_semantics_result result =
      analyze_expression(if_expression->condition, function, context);

  if (result != SEMANTICS_OK)
    return result;

  if (if_expression->condition->resolved_type != TYPE_BOOLEAN)
    return SEMANTICS_ERR_TYPE;

  // Each branch has its own scope
  result = analyze_block(if_expression->then_block, function, context, true);

  if (result != SEMANTICS_OK)
    return result;

  result = analyze_block(if_expression->else_block, function, context, true);

  if (result != SEMANTICS_OK)
    return result;

  ENUM_DATA_TYPE then_type = if_expression->then_block->resolved_type;
  ENUM_DATA_TYPE else_type = if_expression->else_block->resolved_type;

  if (then_type == TYPE_UNSPECIFIED || else_type == TYPE_UNSPECIFIED)
    return SEMANTICS_ERR_INTERNAL;

  ENUM_DATA_TYPE result_type;

  // Same type in both branches
  if (then_type == else_type) {
    result_type = then_type;
  }
  // Int and Double are compatible -> result is Double
  else if (then_type == TYPE_INT && else_type == TYPE_DOUBLE) {
    AST_EXPRESSION *result_expression =
        get_block_result_expression(if_expression->then_block);

    if (!result_expression)
      return SEMANTICS_ERR_INTERNAL;

    result_expression->conversion = CONVERSION_INT_TO_DOUBLE;
    result_type = TYPE_DOUBLE;
  } else if (then_type == TYPE_DOUBLE && else_type == TYPE_INT) {
    AST_EXPRESSION *result_expression =
        get_block_result_expression(if_expression->else_block);

    if (!result_expression)
      return SEMANTICS_ERR_INTERNAL;

    result_expression->conversion = CONVERSION_INT_TO_DOUBLE;
    result_type = TYPE_DOUBLE;
  } else {
    return SEMANTICS_ERR_TYPE;
  }
  if_expression->resolved_type = result_type;
  expression->resolved_type = result_type;

  return SEMANTICS_OK;
}

static ENUM_DATA_TYPE get_statement_result_type(AST_STATEMENT *statement) {
  if (!statement)
    return TYPE_UNSPECIFIED;

  switch (statement->type) {
  case STMT_EXPR:
    if (!statement->data.expr_stmt.expression)
      return TYPE_UNSPECIFIED;

    return statement->data.expr_stmt.expression->resolved_type;

  case STMT_IF:
    return statement->data.if_stmt.resolved_type;

  case STMT_VAR_DECL:
  case STMT_ASSIGNMENT:
  case STMT_WHILE:
  case STMT_FOR:
    return TYPE_UNIT;

  case STMT_RETURN:
    return TYPE_UNIT;

  default:
    return TYPE_UNSPECIFIED;
  }
}

// ========================================== //
// ============ PUBLIC FUNCTIONS ============ //
// ========================================== //

t_semantics_result semantics_analyze(AST_PROGRAM *program) {
  if (!program)
    return SEMANTICS_ERR_INTERNAL;

  //*out_pools = (t_symbol_pools){0};

  t_symtable *table = symtable_create();
  if (!table)
    return SEMANTICS_ERR_INTERNAL;

  // Register built-in functions
  t_semantics_result result = register_builtins(table);
  if (result != SEMANTICS_OK) {
    symtable_destroy(table);
    return result;
  }

  // First pass: collect function definitions
  result = semantics_first_pass(program, table);
  if (result != SEMANTICS_OK) {
    symtable_destroy(table);
    return result;
  }

  // Second pass: analyze function bodies
  result = semantics_second_pass(program, table);
  if (result != SEMANTICS_OK) {
    symtable_destroy(table);
    return result;
  }

  // Transfer symbol pools to caller
  // *out_pools = symtable_take_pools(table);
  symtable_destroy(table);

  return SEMANTICS_OK;
}
