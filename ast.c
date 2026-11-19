/**
 * @file AST.c
 * @brief Implementation of the abstract syntax tree (AST).
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

#include "ast.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Copies a string.
 *
 * @param[in] source Source string.
 *
 * @return New string, NULL if source is NULL or allocation failed.
 */
static char *ast_str_duplicate(const char *source);

/**
 * @brief Ensures that a dynamic array has room for one more element.
 *
 * @param[in] array Current array (NULL if empty).
 * @param[in,out] capacity Current capacity.
 * @param[in] count Current number of used elements.
 * @param[in] element_size Size of one element in bytes.
 *
 * @return Array with free room, NULL if allocation failed.
 */
static void *ast_array_grow(void *array, size_t *capacity, size_t count, size_t element_size);

/**
 * @brief Allocates an expression node.
 *
 * @param[in] type Expression type.
 * @param[in] location Position in source.
 *
 * @return New node, NULL if allocation failed.
 */
static AST_EXPRESSION *ast_expression_alloc(ENUM_EXPRESSION_TYPE type, SOURCE_LOCATION location);

/**
 * @brief Allocates a statement node.
 *
 * @param[in] type Statement type.
 * @param[in] location Position in source.
 *
 * @return New node, NULL if allocation failed.
 */
static AST_STATEMENT *ast_statement_alloc(ENUM_STATEMENT_TYPE type, SOURCE_LOCATION location);

/**
 * @brief Frees the content of an IF node (not the node itself, it is stored by value).
 *
 * @param[in] if_node IF node.
 */
static void ast_if_destroy_members(AST_IF *if_node);

/**
 * @brief Frees the content of a parameter list (not the list itself).
 *
 * @param[in] list Parameter list.
 */
static void ast_param_list_destroy_members(AST_PARAM_LIST *list);

/**
 * @brief Frees the content of an argument list (not the list itself).
 *
 * @param[in] list Argument list.
 */
static void ast_arg_list_destroy_members(AST_ARG_LIST *list);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static char *ast_str_duplicate(const char *source) {
  if (source == NULL) {
    return NULL;
  }

  size_t length = strlen(source);
  char *copy = malloc(length + 1);

  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, source, length + 1);
  return copy;
}

 /*
  * AI use
  * Prompt: Napiš funkci static void *ast_array_grow(void *array, size_t *capacity, size_t count, size_t element_size),
  * vstup: dynamické pole (NULL pokud prázdné), ukazatel na kapacitu, počet použitých prvků, velikost jednoho prvku,
  * vrací: pole s místem pro další prvek; stejné pole, pokud se vejde; NULL při selhání (původní pole zůstane platné),
  * ošetři: nulová kapacita (začni na AST_INITIAL_CAPACITY, jinak zdvojnásob), přetečení max kapacity, 
  * selhání realloc bez ztráty původního pole.
  */

static void *ast_array_grow(void *array, size_t *capacity, size_t count, size_t element_size) {
  if (count < *capacity) {
    return array;
  }

  size_t new_capacity;

  if (*capacity == 0) {
    new_capacity = AST_INITIAL_CAPACITY;
  } else if (*capacity > SIZE_MAX / 2) {
    return NULL;
  } else {
    new_capacity = *capacity * 2;
  }

  if (new_capacity > SIZE_MAX / element_size) {
    return NULL;
  }

  void *tmp = realloc(array, new_capacity * element_size);

  if (tmp == NULL) {
    return NULL;
  }

  *capacity = new_capacity;
  return tmp;
}
/*
 * END AI
 */

static AST_EXPRESSION *ast_expression_alloc(ENUM_EXPRESSION_TYPE type, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = calloc(1, sizeof(AST_EXPRESSION));

  if (expression == NULL) {
    return NULL;
  }

  expression->type = type;
  expression->resolved_type = TYPE_UNSPECIFIED;
  expression->conversion = CONVERSION_NONE;
  expression->location = location;
  return expression;
}

static AST_STATEMENT *ast_statement_alloc(ENUM_STATEMENT_TYPE type, SOURCE_LOCATION location) {
  AST_STATEMENT *statement = calloc(1, sizeof(AST_STATEMENT));

  if (statement == NULL) {
    return NULL;
  }

  statement->type = type;
  statement->location = location;
  return statement;
}

static void ast_if_destroy_members(AST_IF *if_node) {
  ast_expression_destroy(if_node->condition);
  ast_block_destroy(if_node->then_block);
  ast_block_destroy(if_node->else_block);
}

static void ast_param_list_destroy_members(AST_PARAM_LIST *list) {
  for (size_t i = 0; i < list->parameter_count; i++) {
    free(list->parameters[i].name);
  }

  free(list->parameters);
}

static void ast_arg_list_destroy_members(AST_ARG_LIST *list) {
  for (size_t i = 0; i < list->argument_count; i++) {
    ast_expression_destroy(list->arguments[i]);
  }

  free(list->arguments);
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

// Tree

AST_PROGRAM *ast_program_create(void) {
  return calloc(1, sizeof(AST_PROGRAM));
}

t_ast_result ast_program_add_function(AST_PROGRAM *program, AST_FUNCTION_DEF *function) {
  if (program == NULL || function == NULL) {
    ast_function_def_destroy(function);
    return AST_INVALID_PARAM;
  }
  
  AST_FUNCTION_DEF **tmp = ast_array_grow(program->functions, &program->function_capacity,
                                          program->function_count, sizeof *tmp);
  
  if (tmp == NULL) {
    ast_function_def_destroy(function);
    return AST_INTERNAL_ERROR;
  }

  program->functions = tmp;
  program->functions[program->function_count++] = function;
  return AST_OK;
}

void ast_program_destroy(AST_PROGRAM *program) {
  if (program == NULL) {
    return;
  }

  for (size_t i = 0; i < program->function_count; i++) {
    ast_function_def_destroy(program->functions[i]);
  }

  free(program->functions);
  free(program);
}

// Functions

AST_FUNCTION_DEF *ast_function_def_create(const char *name, SOURCE_LOCATION location) {
  char *name_copy = ast_str_duplicate(name);

  if (name_copy == NULL) {
    return NULL;
  }

  AST_FUNCTION_DEF *function = calloc(1, sizeof(AST_FUNCTION_DEF));

  if (function == NULL) {
    free(name_copy);
    return NULL;
  }
  
  function->name = name_copy;
  function->location = location;
  function->declared_return_type = TYPE_UNSPECIFIED;
  function->resolved_return_type = TYPE_UNSPECIFIED;
  return function;
}

/*
 * AI use
 * Promt: Napiš t_ast_result ast_function_def_set_receiver(AST_FUNCTION_DEF *function, const char *name, ENUM_DATA_TYPE type, SOURCE_LOCATION location),
 * vstup: funkce, název příjemce, jeho typ, pozice, vrací: AST_OK, AST_INVALID_PARAM, AST_INTERNAL_ERROR,
 * ošetři: název se kopíruje (ast_str_duplicate), AST_PARAMETER přes calloc, při selhání uvolni vše, co vzniklo, starý receiver uvolni až po úspěšné alokaci nového, fce se nepřebírá
 */

t_ast_result ast_function_def_set_receiver(AST_FUNCTION_DEF *function, const char *name,
                                           ENUM_DATA_TYPE type, SOURCE_LOCATION location) {
  if (function == NULL || name == NULL) {
    return AST_INVALID_PARAM;
  }

  char *name_copy = ast_str_duplicate(name);
  AST_PARAMETER *receiver = calloc(1, sizeof(AST_PARAMETER));

  if (name_copy == NULL || receiver == NULL) {
    free(name_copy);
    free(receiver);
    return AST_INTERNAL_ERROR;
  }

  receiver->name = name_copy;
  receiver->type = type;
  receiver->location = location;

  if (function->receiver != NULL) {
    free(function->receiver->name);
    free(function->receiver);
  }

  function->receiver = receiver;
  return AST_OK;
}
/*
 * END AI
 */

t_ast_result ast_function_def_add_param_list(AST_FUNCTION_DEF *function) {
  if (function == NULL) {
    return AST_INVALID_PARAM;
  }

  AST_PARAM_LIST *tmp = ast_array_grow(function->param_lists, &function->param_list_capacity,
                                       function->param_list_count, sizeof *tmp);

  if (tmp == NULL) {
    return AST_INTERNAL_ERROR;
  }

  function->param_lists = tmp;
  memset(&function->param_lists[function->param_list_count], 0, sizeof *tmp);
  function->param_list_count++;
  return AST_OK;
}

/*
 * AI use
 * Prompt: napiš t_ast_result ast_function_def_add_parameter(AST_FUNCTION_DEF *function, const char *name, ENUM_DATA_TYPE type, SOURCE_LOCATION location),
 * přidá parametr do POSLEDNÍHO seznamu parametrů, vrací: AST_OK, AST_INVALID_PARAM (function nebo name NULL), AST_INTERNAL_ERROR,
 * ošetři: kopie názvu, růst pole přes ast_array_grow, při selhání růstu uvolni kopii názvu, count zvys až po úspěšném růstu, variable_id nastav na 0
 */

t_ast_result ast_function_def_add_parameter(AST_FUNCTION_DEF *function, const char *name,
                                            ENUM_DATA_TYPE type, SOURCE_LOCATION location) {
  if (function == NULL || name == NULL || function->param_list_count == 0) {
    return AST_INVALID_PARAM;
  }

  AST_PARAM_LIST *list = &function->param_lists[function->param_list_count - 1];
  char *name_copy = ast_str_duplicate(name);

  if (name_copy == NULL) {
    return AST_INTERNAL_ERROR;
  }

  AST_PARAMETER *tmp = ast_array_grow(list->parameters, &list->parameter_capacity,
                                      list->parameter_count, sizeof *tmp);

  if (tmp == NULL) {
    free(name_copy);
    return AST_INTERNAL_ERROR;
  }

  list->parameters = tmp;
  AST_PARAMETER *parameter = &list->parameters[list->parameter_count++];
  parameter->variable_id = 0;
  parameter->name = name_copy;
  parameter->type = type;
  parameter->location = location;
  return AST_OK;
}
/*
 * END AI
 */

void ast_function_def_destroy(AST_FUNCTION_DEF *function) {
  if (function == NULL) {
    return;
  }

  free(function->name);

  if (function->receiver != NULL) {
    free(function->receiver->name);
    free(function->receiver);
  }

  for (size_t i = 0; i < function->param_list_count; i++) {
    ast_param_list_destroy_members(&function->param_lists[i]);
  }

  free(function->param_lists);
  ast_block_destroy(function->body);
  free(function);
}

// Block

AST_BLOCK *ast_block_create(void) {
  AST_BLOCK *block = calloc(1, sizeof(AST_BLOCK));

  if (block != NULL) {
    block->resolved_type = TYPE_UNSPECIFIED;
  }

  return block;
}

t_ast_result ast_block_add_statement(AST_BLOCK *block, AST_STATEMENT *statement) {
  if (block == NULL || statement == NULL) {
    ast_statement_destroy(statement);
    return AST_INVALID_PARAM;
  }

  AST_STATEMENT **tmp = ast_array_grow(block->statements, &block->statement_capacity,
                                       block->statement_count, sizeof *tmp);

  if (tmp == NULL) {
    ast_statement_destroy(statement);
    return AST_INTERNAL_ERROR;
  }

  block->statements = tmp;
  block->statements[block->statement_count++] = statement;
  return AST_OK;
}

void ast_block_destroy(AST_BLOCK *block) {
  if (block == NULL) {
    return;
  }

  for (size_t i = 0; i < block->statement_count; i++) {
    ast_statement_destroy(block->statements[i]);
  }

  free(block->statements);
  free(block);
}

// Statements

AST_STATEMENT *ast_statement_create_var_decl(const char *name, bool is_mutable,
                                             ENUM_DATA_TYPE declared_type,
                                             AST_EXPRESSION *initializer, SOURCE_LOCATION location) {
  char *name_copy = NULL;
  AST_STATEMENT *statement = NULL;

  if (name != NULL && initializer != NULL) {
    name_copy = ast_str_duplicate(name);
    statement = ast_statement_alloc(STMT_VAR_DECL, location);
  }

  if (name_copy == NULL || statement == NULL) {
    free(name_copy);
    free(statement);
    ast_expression_destroy(initializer);
    return NULL;
  }

  AST_VAR_DECL *node = &statement->data.var_decl;
  node->name = name_copy;
  node->is_mutable = is_mutable;
  node->declared_type = declared_type;
  node->resolved_type = TYPE_UNSPECIFIED;
  node->initializer = initializer;
  return statement;
}

AST_STATEMENT *ast_statement_create_assignment(const char *name, AST_EXPRESSION *value,
                                               SOURCE_LOCATION location) {
  char *name_copy = NULL;
  AST_STATEMENT *statement = NULL;

  if (name != NULL && value != NULL) {
    name_copy = ast_str_duplicate(name);
    statement = ast_statement_alloc(STMT_ASSIGNMENT, location);
  }

  if (name_copy == NULL || statement == NULL) {
    free(name_copy);
    free(statement);
    ast_expression_destroy(value);
    return NULL;
  }
  
  statement->data.assignment.name = name_copy;
  statement->data.assignment.value = value;
  return statement;
}

AST_STATEMENT *ast_statement_create_return(AST_EXPRESSION *value, SOURCE_LOCATION location) {
  AST_STATEMENT *statement = ast_statement_alloc(STMT_RETURN, location);

  if (statement == NULL) {
    ast_expression_destroy(value);
    return NULL;
  }

  statement->data.return_stmt.value = value;
  return statement;
}

AST_STATEMENT *ast_statement_create_if(AST_EXPRESSION *condition, AST_BLOCK *then_block,
                                       AST_BLOCK *else_block, SOURCE_LOCATION location) {
  AST_STATEMENT *statement = NULL;

  if (condition != NULL && then_block != NULL) {
    statement = ast_statement_alloc(STMT_IF, location);
  }

  if (statement == NULL) {
    ast_expression_destroy(condition);
    ast_block_destroy(then_block);
    ast_block_destroy(else_block);
    return NULL;
  }

  AST_IF *node = &statement->data.if_stmt;
  node->condition = condition;
  node->then_block = then_block;
  node->else_block = else_block;
  node->resolved_type = TYPE_UNSPECIFIED;
  return statement;
}

AST_STATEMENT *ast_statement_create_while(AST_EXPRESSION *condition, AST_BLOCK *body,
                                          SOURCE_LOCATION location) {
  AST_STATEMENT *statement = NULL;

  if (condition != NULL && body != NULL) {
    statement = ast_statement_alloc(STMT_WHILE, location);
  }

  if (statement == NULL) {
    ast_expression_destroy(condition);
    ast_block_destroy(body);
    return NULL;
  }

  statement->data.while_stmt.condition = condition;
  statement->data.while_stmt.body = body;
  return statement;
}

AST_STATEMENT *ast_statement_create_for(const char *var_name, bool is_exclusive,
                                        AST_EXPRESSION *from, AST_EXPRESSION *to,
                                        AST_EXPRESSION *step, AST_BLOCK *body,
                                        SOURCE_LOCATION location) {
  char *name_copy = NULL;
  AST_STATEMENT *statement = NULL;

  if (var_name != NULL && from != NULL && to != NULL && body != NULL) {
    name_copy = ast_str_duplicate(var_name);
    statement = ast_statement_alloc(STMT_FOR, location);
  }

  if (name_copy == NULL || statement == NULL) {
    free(name_copy);
    free(statement);
    ast_expression_destroy(from);
    ast_expression_destroy(to);
    ast_expression_destroy(step);
    ast_block_destroy(body);
    return NULL;
  }

  AST_FOR *node = &statement->data.for_stmt;
  node->var_name = name_copy;
  node->is_exclusive = is_exclusive;
  node->from = from;
  node->to = to;
  node->step = step;
  node->body = body;
  return statement;
}

t_ast_result ast_statement_for_add_guard(AST_STATEMENT *statement, AST_EXPRESSION *guard) {
  if (statement == NULL || statement->type != STMT_FOR || guard == NULL) {
    ast_expression_destroy(guard);
    return AST_INVALID_PARAM;
  }

  AST_FOR *node = &statement->data.for_stmt;
  AST_EXPRESSION **tmp = ast_array_grow(node->guards, &node->guard_capacity,
                                        node->guard_count, sizeof *tmp);

  if (tmp == NULL) {
    ast_expression_destroy(guard);
    return AST_INTERNAL_ERROR;
  }

  node->guards = tmp;
  node->guards[node->guard_count++] = guard;
  return AST_OK;
}

AST_STATEMENT *ast_statement_create_expr(AST_EXPRESSION *expression, SOURCE_LOCATION location) {
  AST_STATEMENT *statement = NULL;

  if (expression != NULL) {
    statement = ast_statement_alloc(STMT_EXPR, location);
  }

  if (statement == NULL) {
    ast_expression_destroy(expression);
    return NULL;
  }

  statement->data.expr_stmt.expression = expression;
  return statement;
}

/*
 * AI use
 * Prompt: Napiš rekurzivní void ast_statement_destroy(AST_STATEMENT *statement),
 * uvolní příkaz včetně podstromu podle statement type (switch přes všech 7 druhů, bez default) name u var_decl a assignment,
 * výrazy, bloky, u STMT_FOR var_name, from, to, step, všechny stráže i pole guards, body - STMT_IF přes ast_if_destroy_members,
 * nakonec free(statement), ošetři: NULL, volitelné části (step, else_block, value u return) mohou být NULL, indexy typu size_t.
 */

void ast_statement_destroy(AST_STATEMENT *statement) {
  if (statement == NULL) {
    return;
  }

  switch (statement->type) {
    case STMT_VAR_DECL:
      free(statement->data.var_decl.name);
      ast_expression_destroy(statement->data.var_decl.initializer);
      break;
    case STMT_ASSIGNMENT:
      free(statement->data.assignment.name);
      ast_expression_destroy(statement->data.assignment.value);
      break;
    case STMT_RETURN:
      ast_expression_destroy(statement->data.return_stmt.value);
      break;
    case STMT_IF:
      ast_if_destroy_members(&statement->data.if_stmt);
      break;
    case STMT_WHILE:
      ast_expression_destroy(statement->data.while_stmt.condition);
      ast_block_destroy(statement->data.while_stmt.body);
      break;
    case STMT_FOR: {
      AST_FOR *for_node = &statement->data.for_stmt;
      free(for_node->var_name);
      ast_expression_destroy(for_node->from);
      ast_expression_destroy(for_node->to);
      ast_expression_destroy(for_node->step);

      for (size_t i = 0; i < for_node->guard_count; i++) {
        ast_expression_destroy(for_node->guards[i]);
      }

      free(for_node->guards);
      ast_block_destroy(for_node->body);
      break;
    }
    case STMT_EXPR:
      ast_expression_destroy(statement->data.expr_stmt.expression);
      break;
  }

  free(statement);
}
/*
 * END AI
 */

// Expressions

AST_EXPRESSION *ast_expression_create_int(int64_t value, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = ast_expression_alloc(EXPR_INT_LITERAL, location);

  if (expression != NULL) {
    expression->data.int_literal = value;
  }

  return expression;
}

AST_EXPRESSION *ast_expression_create_double(double value, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = ast_expression_alloc(EXPR_DOUBLE_LITERAL, location);

  if (expression != NULL) {
    expression->data.double_literal = value;
  }

  return expression;
}

AST_EXPRESSION *ast_expression_create_string(const char *value, size_t length,
                                             SOURCE_LOCATION location) {
  char *string_copy = ast_str_duplicate(value);

  if (string_copy == NULL) {
    return NULL;
  }

  AST_EXPRESSION *expression = ast_expression_alloc(EXPR_STRING_LITERAL, location);

  if (expression == NULL) {
    free(string_copy);
    return NULL;
  }

  expression->data.string_literal.value = string_copy;
  expression->data.string_literal.length = length;
  return expression;
}

AST_EXPRESSION *ast_expression_create_bool(bool value, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = ast_expression_alloc(EXPR_BOOL_LITERAL, location);

  if (expression != NULL) {
    expression->data.bool_literal = value;
  }

  return expression;
}

AST_EXPRESSION *ast_expression_create_unit(SOURCE_LOCATION location) {
  return ast_expression_alloc(EXPR_UNIT_LITERAL, location);
}

AST_EXPRESSION *ast_expression_create_var_ref(const char *name, SOURCE_LOCATION location) {
  char *name_copy = ast_str_duplicate(name);

  if (name_copy == NULL) {
    return NULL;
  }

  AST_EXPRESSION *expression = ast_expression_alloc(EXPR_VAR_REF, location);

  if (expression == NULL) {
    free(name_copy);
    return NULL;
  }

  expression->data.var_ref.name = name_copy;
  return expression;
}

AST_EXPRESSION *ast_expression_create_unary(ENUM_UNARY_OPERATOR op, AST_EXPRESSION *operand,
                                            SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = NULL;

  if (operand != NULL) {
    expression = ast_expression_alloc(EXPR_UNARY, location);
  }

  if (expression == NULL) {
    ast_expression_destroy(operand);
    return NULL;
  }

  expression->data.unary.op = op;
  expression->data.unary.operand = operand;
  return expression;
}

AST_EXPRESSION *ast_expression_create_binary(ENUM_BINARY_OPERATOR op, AST_EXPRESSION *left,
                                             AST_EXPRESSION *right, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = NULL;

  if (left != NULL && right != NULL) {
    expression = ast_expression_alloc(EXPR_BINARY, location);
  }

  if (expression == NULL) {
    ast_expression_destroy(left);
    ast_expression_destroy(right);
    return NULL;
  }

  expression->data.binary.op = op;
  expression->data.binary.left = left;
  expression->data.binary.right = right;
  return expression;
}

AST_EXPRESSION *ast_expression_create_call(const char *name, AST_EXPRESSION *receiver,
                                           SOURCE_LOCATION location) {
  char *name_copy = NULL;
  AST_EXPRESSION *expression = NULL;

  if (name != NULL) {
    name_copy = ast_str_duplicate(name);
    expression = ast_expression_alloc(EXPR_CALL, location);
  }

  if (name_copy == NULL || expression == NULL) {
    free(name_copy);
    free(expression);
    ast_expression_destroy(receiver);
    return NULL;
  }

  expression->data.call.callee_kind = CALLEE_UNKNOWN;
  expression->data.call.name = name_copy;
  expression->data.call.receiver = receiver;
  return expression;
}

t_ast_result ast_expression_call_add_argument_list(AST_EXPRESSION *call) {
  if (call == NULL || call->type != EXPR_CALL) {
    return AST_INVALID_PARAM;
  }

  AST_CALL *node = &call->data.call;
  AST_ARG_LIST *tmp = ast_array_grow(node->argument_lists, &node->argument_list_capacity,
                                     node->argument_list_count, sizeof *tmp);

  if (tmp == NULL) {
    return AST_INTERNAL_ERROR;
  }

  node->argument_lists = tmp;
  memset(&node->argument_lists[node->argument_list_count++], 0, sizeof *tmp);
  return AST_OK;
}

t_ast_result ast_expression_call_add_argument(AST_EXPRESSION *call, AST_EXPRESSION *argument) {
  if (call == NULL || call->type != EXPR_CALL || argument == NULL ||
      call->data.call.argument_list_count == 0) {
    ast_expression_destroy(argument);
    return AST_INVALID_PARAM;
  }

  AST_CALL *node = &call->data.call;
  AST_ARG_LIST *list = &node->argument_lists[node->argument_list_count - 1];
  AST_EXPRESSION **tmp = ast_array_grow(list->arguments, &list->argument_capacity,
                                        list->argument_count, sizeof *tmp);

  if (tmp == NULL) {
    ast_expression_destroy(argument);
    return AST_INTERNAL_ERROR;
  }

  list->arguments = tmp;
  list->arguments[list->argument_count++] = argument;
  return AST_OK;
}

/*
 * AI use
 * Prompt: Napiš t_ast_result ast_expression_var_ref_to_call(AST_EXPRESSION *expression), která
 * přepíše uzel EXPR_VAR_REF na EXPR_CALL bez seznamů argumentů, vrací: AST_OK, AST_INVALID_PARAM (NULL nebo jiný typ než EXPR_VAR_REF),
 * Ošetři: var_ref a call sdílejí union data - název ulož do lokální proměnné před přepsáním, data vynuluj, nastav type = EXPR_CALL, callee_kind = CALLEE_UNKNOWN
 * a název vrať do call.name; název neuvolňuj ani nekopíruj
 */

t_ast_result ast_expression_var_ref_to_call(AST_EXPRESSION *expression) {

  if (expression == NULL || expression->type != EXPR_VAR_REF) {
    return AST_INVALID_PARAM;
  }

  char *name = expression->data.var_ref.name;
  memset(&expression->data, 0, sizeof expression->data);
  expression->type = EXPR_CALL;
  expression->data.call.callee_kind = CALLEE_UNKNOWN;
  expression->data.call.name = name;
  return AST_OK;
}
/*
 * END AI
 */

AST_EXPRESSION *ast_expression_create_member(AST_EXPRESSION *receiver, const char *member_name,
                                             SOURCE_LOCATION location) {
  char *name_copy = NULL;
  AST_EXPRESSION *expression = NULL;

  if (receiver != NULL && member_name != NULL) {
    name_copy = ast_str_duplicate(member_name);
    expression = ast_expression_alloc(EXPR_MEMBER, location);
  }

  if (name_copy == NULL || expression == NULL) {
    free(name_copy);
    free(expression);
    ast_expression_destroy(receiver);
    return NULL;
  }

  expression->data.member.receiver = receiver;
  expression->data.member.member_name = name_copy;
  expression->data.member.member_kind = MEMBER_UNKNOWN;
  return expression;
}

AST_EXPRESSION *ast_expression_create_interpolated(SOURCE_LOCATION location) {
  return ast_expression_alloc(EXPR_INTERPOLATED, location);
}

t_ast_result ast_expression_interpolated_add_literal(AST_EXPRESSION *expression, const char *text) {
  if (expression == NULL || expression->type != EXPR_INTERPOLATED || text == NULL) {
    return AST_INVALID_PARAM;
  }

  char *text_copy = ast_str_duplicate(text);

  if (text_copy == NULL) {
    return AST_INTERNAL_ERROR;
  }

  AST_INTERPOLATED *node = &expression->data.interpolated;
  AST_INTERP_PART *tmp = ast_array_grow(node->parts, &node->part_capacity,
                                        node->part_count, sizeof *tmp);

  if (tmp == NULL) {
    free(text_copy);
    return AST_INTERNAL_ERROR;
  }

  node->parts = tmp;
  AST_INTERP_PART *part = &node->parts[node->part_count++];
  part->is_literal = true;
  part->text = text_copy;
  part->expression = NULL;
  return AST_OK;
}

t_ast_result ast_expression_interpolated_add_expression(AST_EXPRESSION *expression,
                                                        AST_EXPRESSION *part_expression) {
  if (expression == NULL || expression->type != EXPR_INTERPOLATED || part_expression == NULL) {
    ast_expression_destroy(part_expression);
    return AST_INVALID_PARAM;
  }

  AST_INTERPOLATED *node = &expression->data.interpolated;
  AST_INTERP_PART *tmp = ast_array_grow(node->parts, &node->part_capacity,
                                        node->part_count, sizeof *tmp);

  if (tmp == NULL) {
    ast_expression_destroy(part_expression);
    return AST_INTERNAL_ERROR;
  }

  node->parts = tmp;
  AST_INTERP_PART *part = &node->parts[node->part_count++];
  part->is_literal = false;
  part->text = NULL;
  part->expression = part_expression;
  return AST_OK;
}

AST_EXPRESSION *ast_expression_create_if(AST_EXPRESSION *condition, AST_BLOCK *then_block,
                                         AST_BLOCK *else_block, SOURCE_LOCATION location) {
  AST_EXPRESSION *expression = NULL;

  if (condition != NULL && then_block != NULL && else_block != NULL) {
    expression = ast_expression_alloc(EXPR_IF, location);
  }

  if (expression == NULL) {
    ast_expression_destroy(condition);
    ast_block_destroy(then_block);
    ast_block_destroy(else_block);
    return NULL;
  }

  AST_IF *node = &expression->data.if_expr;
  node->condition = condition;
  node->then_block = then_block;
  node->else_block = else_block;
  node->resolved_type = TYPE_UNSPECIFIED;
  return expression;
}

/*
 * AI use
 * Prompt: Napiš rekurzivní void ast_expression_destroy(AST_EXPRESSION *expression),
 * uvolní výraz včetně podstromu podle expression->type (switch přes všechny druhy, literaly vypiš, bez default): var_ref.name, string_literal.value,
 * operand / left / right; u CALL name, receiver, všechny seznamy argumentů (ast_arg_list_destroy_members) i pole argument_lists,
 * u MEMBER receiver a member_name, u INTERPOLATED text i expression každé části a pole parts, EXPR_IF přes ast_if_destroy_members,
 * nakonec free(expression), ošetři: NULL, receiver == NULL, části interpolace mají text nebo expression NULL, indexy typu size_t
 */

void ast_expression_destroy(AST_EXPRESSION *expression) {
  if (expression == NULL) {
    return;
  }

  switch (expression->type) {
    case EXPR_VAR_REF:
      free(expression->data.var_ref.name);
      break;
    case EXPR_STRING_LITERAL:
      free(expression->data.string_literal.value);
      break;
    case EXPR_UNARY:
      ast_expression_destroy(expression->data.unary.operand);
      break;
    case EXPR_BINARY:
      ast_expression_destroy(expression->data.binary.left);
      ast_expression_destroy(expression->data.binary.right);
      break;
    case EXPR_CALL: {
      AST_CALL *node = &expression->data.call;
      free(node->name);
      ast_expression_destroy(node->receiver);

      for (size_t i = 0; i < node->argument_list_count; i++) {
        ast_arg_list_destroy_members(&node->argument_lists[i]);
      }

      free(node->argument_lists);
      break;
    }
    case EXPR_MEMBER:
      ast_expression_destroy(expression->data.member.receiver);
      free(expression->data.member.member_name);
      break;
    case EXPR_INTERPOLATED: {
      AST_INTERPOLATED *node = &expression->data.interpolated;

      for (size_t i = 0; i < node->part_count; i++) {
        free(node->parts[i].text);
        ast_expression_destroy(node->parts[i].expression);
      }

      free(node->parts);
      break;
    }
    case EXPR_IF:
      ast_if_destroy_members(&expression->data.if_expr);
      break;
    case EXPR_INT_LITERAL:
    case EXPR_DOUBLE_LITERAL:
    case EXPR_UNIT_LITERAL:
    case EXPR_BOOL_LITERAL:
      break;
  }
  free(expression);
}
/*
 * END AI 
 */