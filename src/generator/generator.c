/**
 * @file generator.c
 * @brief Code generator module implementation.
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

#include "generator.h"

#include "../ast/ast.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

/** @brief Structure for function name, files, checks and counters */
typedef struct {
  char *function_name;
  FILE *function_head;
  FILE *function_body;
  bool is_main;
  bool is_last;
  size_t var_mangling;
  size_t var_mangling_counter;
  size_t if_counter;
  size_t while_counter;
  size_t for_counter;
  bool was_concat;
  bool was_strlen;
} function_globals;

/** @brief List for keeping track of mangling for variables declared inside the body of a function.*/
typedef struct {
  char *name;
  size_t mangling;
  var_list_node *next;
} var_list_node;

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Generates code for function definitions.
 *
 * @param[in] function AST node of the function definition.
 *
 * @return Error check.
 * @retval 0 - success.
 * @retval 1 - error.
 */
static int generate_function(AST_FUNCTION_DEF *function);

/**
 * @brief Generates code for statements.
 *
 * @param[in] statement AST node of the function definition.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in] var_prefix Frame prefix for variables.
 * @param[in,out] var_list List of declared local variables.
 *
 * @return Error check.
 * @retval 0 - success.
 * @retval 1 - error.
 */
static int generate_statement(AST_STATEMENT *statement, function_globals *func_globals, char var_prefix, var_list_node *var_list);

/**
 * @brief Generates code for variable declarations.
 *
 * @param[in] statement AST node of the variable declaration.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in] var_prefix Frame prefix for variables.
 * @param[in,out] var_list List of declared local variables.
 *
 * @return Error check.
 * @retval 0 - success.
 * @retval 1 - error.
 */
static int generate_var_decl(AST_VAR_DECL *var_decl, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_assignment(AST_ASSIGNMENT *assignment, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_return(AST_RETURN *return_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_expression(AST_EXPRESSION *expression, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_unary_expr(AST_UNARY_EXPRESSION *unary_expr, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_binary_expr(AST_BINARY_EXPRESSION *binary_expr, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_call(AST_CALL *call, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_member(AST_MEMBER *member, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_interpolated(AST_INTERPOLATED *interpolated, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_if(AST_IF *if_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_while(AST_WHILE *while_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list);


static int generate_for(AST_FOR *for_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list);

//======================================

/**
 * @brief Finds mangling of a variable.
 *
 * @param[in] var_list List of declared local variables.
 * @param[in] name Name of the variable.
 *
 * @return Mangling of found variable.
 * @retval -1 - variable not found.
 */
size_t find_variable(var_list_node *var_list, char name);

/**
 * @brief Converts a string into IFJcode26 string format.
 *
 * @param[in] string String.
 * @param[in] length Length of the string.
 * 
 * @return Converted string.
 */
static char* convert_string(char *string, size_t length);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static int generate_function(AST_FUNCTION_DEF *function) {
  if (function == NULL) {
    return 1;
  }

  function_globals *func_globals = malloc(sizeof(function_globals));
  if (func_globals == NULL) {
    return 1;
  }
  func_globals->var_mangling_counter = 0;
  func_globals->var_mangling = 0;
  func_globals->if_counter = 0;
  func_globals->while_counter = 0;
  func_globals->for_counter = 0;
  func_globals->is_main = false;
  func_globals->is_last = false;
  func_globals->was_concat = false;
  func_globals->was_strlen = false;
  func_globals->function_head = tmpfile();
  func_globals->function_body = tmpfile();

  var_list_node *var_list = NULL;

  char var_prefix;
  if (function->is_main) {
    fprintf(func_globals->function_head, "LABEL $main\n");
    sprinf(func_globals->function_name, "$main");
    var_prefix = "GF@";
    func_globals->is_main = true;
  } else {
    if (function->receiver == NULL) {
      fprintf(func_globals->function_head, "LABEL %s\n", function->name);
      sprinf(func_globals->function_name, "%s", function->name);
    } else {
      fprintf(func_globals->function_head, "LABEL *%s\n", function->name);
      sprinf(func_globals->function_name, "*%s", function->name);
    }
    var_prefix = "LF@";

    fprintf(func_globals->function_head, "CREATEFRAME\n");
    fprintf(func_globals->function_head, "PUSHFRAME\n");

    if (function->receiver != NULL) {
      fprintf(func_globals->function_head, "DEFVAR %s%s\n", var_prefix, function->receiver->name);
      var_list_node *var_node_old = var_list;
      var_list_node *var_list = malloc(sizeof(var_list_node));
      if (var_list == NULL) {
        return 1;
      }
      var_list->mangling = 0;
      var_list->name = function->receiver->name;
      var_list->next = var_node_old;
    }

    for (size_t i = 0; i < function->param_list_count; i++) {
      AST_PARAM_LIST *param_list = &function->param_lists[i];
      for (size_t j = 0; j < param_list->parameter_count; j++) {
        AST_PARAMETER *param = &param_list->parameters[j];
        fprintf(func_globals->function_head, "DEFVAR %s%s$%i\n", var_prefix, param->name, func_globals->var_mangling);
        var_list_node *var_node_old = var_list;
        var_list_node *var_list = malloc(sizeof(var_list_node));
        if (var_list == NULL) {
          return 1;
        }
        var_list->mangling = 0;
        var_list->name = param->name;
        var_list->next = var_node_old;
      }
    }

    for (size_t i = function->param_list_count; i > 0; i--) {
      AST_PARAM_LIST *param_list = &function->param_lists[i - 1];
      for (size_t j = param_list->parameter_count; j > 0; j--) {
        AST_PARAMETER *param = &param_list->parameters[j - 1];
        fprintf(func_globals->function_head, "POPS %s%s$%i\n", var_prefix, param->name, func_globals->var_mangling);
      }
    }

    if (function->receiver != NULL) {
      fprintf(func_globals->function_head, "POPS %s%s\n", var_prefix, function->receiver->name);
    }
  }

  for (size_t i = 0; i < function->body->statement_count; i++) {
    AST_STATEMENT *statement = function->body->statements[i];
    if (statement == NULL) {
      return 1;
    }
    if (i == function->body->statement_count - 1) {
      func_globals->is_last = true;
    }
    if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    if (func_globals->is_last && statement->type != STMT_RETURN) {
      if (func_globals->is_main) {
        fprintf(func_globals->function_body, "EXIT int@0");
      } else {
        fprintf(func_globals->function_body, "POPFRAME");
        fprintf(func_globals->function_body, "RETURN");
      }
    }
  }

  while (var_list != NULL) {
    var_list_node *var_node_delete = var_list;
    var_list = var_list->next;
    free(var_node_delete);
  }

  char c;
  while ((c = fgetc(func_globals->function_head)) != EOF) {
    putchar(c);
  }
  while ((c = fgetc(func_globals->function_body)) != EOF) {
    putchar(c);
  }

  fclose(func_globals->function_head);
  fclose(func_globals->function_body);

  free(func_globals);
  return 0;
}

static int generate_statement(AST_STATEMENT *statement, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (statement == NULL) {
    return 1;
  }

  if (statement->type == STMT_VAR_DECL) {
    if (generate_var_decl(&statement->data.var_decl, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_ASSIGNMENT) {
    if (generate_assignment(&statement->data.assignment, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_RETURN) {
    if (generate_return(&statement->data.return_stmt, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_IF) {
    if (generate_if(&statement->data.if_stmt, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_WHILE) {
    if (generate_while(&statement->data.while_stmt, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_FOR) {
    if (generate_for(&statement->data.for_stmt, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_EXPR) {
    if (generate_expression(statement->data.expr_stmt.expression, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}

static int generate_var_decl(AST_VAR_DECL *var_decl, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (var_decl == NULL) {
    return 1;
  }

  var_list_node *var_node_old = var_list;
  var_list_node *var_node_new = malloc(sizeof(var_list_node));
  if (var_node_new == NULL) {
    return 1;
  }
  var_node_new->mangling = func_globals->var_mangling;
  var_node_new->name = var_decl->name;
  var_node_new->next = var_node_old;
  *var_list = *var_node_new;

  size_t local_mangling = func_globals->var_mangling;

  if (var_decl->initializer->type == EXPR_VAR_REF) {
    size_t mangling = find_variable(var_list, var_decl->initializer->data.var_ref.name);
    if (mangling < 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "MOVE %s%s$%i %s%s$%i\n", var_prefix, var_decl->name, local_mangling, var_prefix, var_decl->initializer->data.var_ref.name, mangling);

  } else if (var_decl->initializer->type == EXPR_BOOL_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i bool@%s\n", var_prefix, var_decl->name, local_mangling, var_decl->initializer->data.bool_literal ? "true" : "false");

  } else if (var_decl->initializer->type == EXPR_DOUBLE_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i float@%a\n", var_prefix, var_decl->name, local_mangling, var_decl->initializer->data.double_literal);

  } else if (var_decl->initializer->type == EXPR_INT_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i int@%i\n", var_prefix, var_decl->name, local_mangling, var_decl->initializer->data.int_literal);

  } else if (var_decl->initializer->type == EXPR_STRING_LITERAL) {
    char *string = convert_string(var_decl->initializer->data.string_literal.value, var_decl->initializer->data.string_literal.length);
    fprintf(func_globals->function_body, "MOVE %s%s$%i string@%s\n", var_prefix, var_decl->name, local_mangling, string);

  } else if (var_decl->initializer->type == EXPR_UNIT_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i nil@nil\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_UNARY) {
    if (generate_unary_expr(&var_decl->initializer->data.unary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_BINARY) {
    if (generate_binary_expr(&var_decl->initializer->data.binary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_CALL) {
    if (generate_call(&var_decl->initializer->data.call, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_MEMBER) {
    if (generate_member(&var_decl->initializer->data.member, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_INTERPOLATED) {
    if (generate_interpolated(&var_decl->initializer->data.interpolated, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else if (var_decl->initializer->type == EXPR_IF) {
    if (generate_if(&var_decl->initializer->data.if_expr, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  } else {
    return 1;
  }

  fprintf(func_globals->function_head, "DEFVAR %s%s$%i\n", var_prefix, var_decl->name, local_mangling);

  return 0;
}

static int generate_assignment(AST_ASSIGNMENT *assignment, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (assignment == NULL) {
    return 1;
  }

  size_t local_mangling = find_variable(var_list, assignment->name);
  if (local_mangling < 0) {
    return 1;
  }

  if (assignment->value->type == EXPR_VAR_REF) {
    size_t mangling = find_variable(var_list, assignment->value->data.var_ref.name);
    if (mangling < 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "MOVE %s%s$%i %s%s$%i\n", var_prefix, assignment->name, local_mangling, var_prefix, assignment->value->data.var_ref.name, mangling);

  } else if (assignment->value->type == EXPR_BOOL_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i bool@%s\n", var_prefix, assignment->name, func_globals->var_mangling, assignment->value->data.bool_literal ? "true" : "false");

  } else if (assignment->value->type == EXPR_DOUBLE_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i float@%a\n", var_prefix, assignment->name, func_globals->var_mangling, assignment->value->data.double_literal);

  } else if (assignment->value->type == EXPR_INT_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i int@%i\n", var_prefix, assignment->name, func_globals->var_mangling, assignment->value->data.int_literal);

  } else if (assignment->value->type == EXPR_STRING_LITERAL) {
    char *string = convert_string(assignment->value->data.string_literal.value, assignment->value->data.string_literal.length);
    fprintf(func_globals->function_body, "MOVE %s%s$%i string@%s\n", var_prefix, assignment->name, func_globals->var_mangling, string);

  } else if (assignment->value->type == EXPR_UNIT_LITERAL) {
    fprintf(func_globals->function_body, "MOVE %s%s$%i nil@nil\n", var_prefix, assignment->name, func_globals->var_mangling);

  } else if (assignment->value->type == EXPR_UNARY) {
    if (generate_unary_expr(&assignment->value->data.unary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else if (assignment->value->type == EXPR_BINARY) {
    if (generate_binary_expr(&assignment->value->data.binary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else if (assignment->value->type == EXPR_CALL) {
    if (generate_call(&assignment->value->data.call, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else if (assignment->value->type == EXPR_MEMBER) {
    if (generate_member(&assignment->value->data.member, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else if (assignment->value->type == EXPR_INTERPOLATED) {
    if (generate_interpolated(&assignment->value->data.interpolated, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else if (assignment->value->type == EXPR_IF) {
    if (generate_if(&assignment->value->data.if_expr, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, assignment->name, local_mangling);

  } else {
    return 1;
  }

  return 0;
}

static int generate_return(AST_RETURN *return_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (return_stmt == NULL) {
    return 1;
  }

  if (return_stmt->value != NULL) {
    if (generate_expression(return_stmt->value, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  }

  if (func_globals->is_main) {
      fprintf(func_globals->function_body, "EXIT int@0\n");
    } else {
      fprintf(func_globals->function_body, "POPFRAME\n");
      fprintf(func_globals->function_body, "RETURN\n");
  }

  return 0;
}

static int generate_expression(AST_EXPRESSION *expression, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (expression == NULL) {
    return 1;
  }

  if (expression->type == EXPR_VAR_REF) {
    size_t mangling = find_variable(var_list, expression->data.var_ref.name);
    if (mangling < 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "PUSHS %s%s$%i\n", var_prefix, expression->data.var_ref.name, mangling);

  } else if (expression->type == EXPR_BOOL_LITERAL) {
    fprintf(func_globals->function_body, "PUSHS bool@%s\n", expression->data.bool_literal ? "true" : "false");

  } else if (expression->type == EXPR_DOUBLE_LITERAL) {
    fprintf(func_globals->function_body, "PUSHS float@%a\n", expression->data.double_literal);

  } else if (expression->type == EXPR_INT_LITERAL) {
    fprintf(func_globals->function_body, "PUSHS int@%i\n", expression->data.int_literal);

  } else if (expression->type == EXPR_STRING_LITERAL) {
    char *string = convert_string(expression->data.string_literal.value, expression->data.string_literal.length);
    fprintf(func_globals->function_body, "PUSHS string@%s\n", string);

  } else if (expression->type == EXPR_UNIT_LITERAL) {
    fprintf(func_globals->function_body, "PUSHS nil@nil\n");

  } else if (expression->type == EXPR_UNARY) {
    if (generate_unary_expr(&expression->data.unary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_BINARY) {
    if (generate_binary_expr(&expression->data.binary, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_CALL) {
    if (generate_call(&expression->data.call, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_MEMBER) {
    if (generate_member(&expression->data.member, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_INTERPOLATED) {
    if (generate_interpolated(&expression->data.interpolated, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_IF) {
    if (generate_if(&expression->data.if_expr, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }

  } else {
    return 1;
  }

  return 0;
}

static int generate_unary_expr(AST_UNARY_EXPRESSION *unary_expr, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (unary_expr == NULL) {
    return 1;
  }

  if (generate_expression(unary_expr->operand, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }

  if (unary_expr->op == UNARY_PLUS) {
    return 0;

  } else if (unary_expr->op == UNARY_MINUS) {
    if (unary_expr->operand->resolved_type == TYPE_INT) {
      fprintf(func_globals->function_body, "PUSHS int@-1\n");
      fprintf(func_globals->function_body, "MULS\n");
    } else if (unary_expr->operand->resolved_type == TYPE_DOUBLE) {
      fprintf(func_globals->function_body, "PUSHS float@%a\n", -1);
      fprintf(func_globals->function_body, "MULS\n");
    } else {
      return 1;
    }

  } else if (unary_expr->op == UNARY_NOT) {
    fprintf(func_globals->function_body, "NOTS\n");
    
  } else {
    return 1;
  }
  
  return 0;
}

static int generate_binary_expr(AST_BINARY_EXPRESSION *binary_expr, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (binary_expr == NULL) {
    return 1;
  }

  if (binary_expr->op == BINARY_ADD) { // +
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "ADDS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "ADDS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "ADDS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "ADDS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "POPS %s$str1\n", var_prefix);
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "POPS %s$str2\n", var_prefix);
      fprintf(func_globals->function_body, "CONCAT %s$concatstr %s$str1 %s$str2\n", var_prefix, var_prefix, var_prefix);
      fprintf(func_globals->function_body, "PUSHS %s$concatstr\n", var_prefix);
      if (func_globals->was_concat == false) {
        func_globals->was_concat = true;
        fprintf(func_globals->function_head, "DEFVAR %s$str1\n", var_prefix);
        fprintf(func_globals->function_head, "DEFVAR %s$str2\n", var_prefix);
        fprintf(func_globals->function_head, "DEFVAR %s$concatstr\n", var_prefix);
      }
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_SUB) { // -
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "SUBS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "SUBS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "SUBS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "SUBS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_MUL) { // *
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "MULS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "MULS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "MULS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "MULS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_DIV) { // /
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "IDIVS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "DIVS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "DIVS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "DIVS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_LT) { // <
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_GT) { // >
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_LE) { // <=
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "GTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "GTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_GE) { // >=
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "LTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "LTS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_EQ) { // ==
    if (binary_expr->left->resolved_type == binary_expr->right->resolved_type) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "EQS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "EQS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "EQS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_NE) { // !=
    if (binary_expr->left->resolved_type == binary_expr->right->resolved_type) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "EQS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_DOUBLE) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "EQS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_DOUBLE && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "INT2FLOATS\n");
      fprintf(func_globals->function_body, "EQS\n");
      fprintf(func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_AND) { // &&
    if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "ANDS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_OR) { // ||
    if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "ORS\n");
    } else {
      return 1;
    }

  } else {
    return 1;
  }

  return 0;
}

static int generate_call(AST_CALL *call, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (call == NULL) {
    return 1;
  }

  if (call->callee_kind == CALLEE_EXTENSION) {
    if (generate_expression(call->receiver, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
    }
  }

  for (size_t i = 0; i < call->argument_list_count; i++) {
    AST_ARG_LIST *arg_list = &call->argument_lists[i];
    for (size_t j = 0; i < arg_list->argument_count; j++) {
      AST_EXPRESSION *arg = arg_list->arguments[j];
      if (generate_expression(arg, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
    }
  }
  
  if (call->callee_kind == CALLEE_USER_FUNCTION) {
    fprintf(func_globals->function_body, "CALL %s\n", call->name);
  } else if (call->callee_kind == CALLEE_EXTENSION) {
    fprintf(func_globals->function_body, "CALL *%s\n", call->name);
  } else if (call->callee_kind == CALLEE_BUILTIN) {
    fprintf(func_globals->function_body, "CALL !%s\n", call->name);
  }

  return 0;
}

static int generate_member(AST_MEMBER *member, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (member == NULL) {
    return 1;
  }
  
  if (generate_expression(member->receiver, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }

  if (member->member_kind == MEMBER_EXT_PROPERTY) {
    fprintf(func_globals->function_body, "CALL *%s\n", member->member_name);
  } else if (member->member_kind == MEMBER_TO_STRING) {
    fprintf(func_globals->function_body, "CALL !toStr\n");
    /*
    if (member->receiver->resolved_type == TYPE_INT) {
      fprintf(func_globals->function_body, "INT2STRS\n");
    } else if (member->receiver->resolved_type == TYPE_DOUBLE) {
      fprintf(func_globals->function_body, "FLOAT2STRS\n");
    } else if (member->receiver->resolved_type == TYPE_STRING) {
      return 0;
    } else if (member->receiver->resolved_type == TYPE_UNIT) {
      fprintf(func_globals->function_body, "POPS %s$temp\n", var_prefix);
      fprintf(func_globals->function_body, "PUSHS string@()\n");
    } else if (member->receiver->resolved_type == TYPE_BOOLEAN) {
      return 1; //TODO
      //fprintf(func_globals->function_body, "CALL !BoolToStr\n");
    } else {
      return 1;
    }
    */
  } else if (member->member_kind == MEMBER_TO_INT) {
    fprintf(func_globals->function_body, "FLOAT2INTS\n");
  } else if (member->member_kind == MEMBER_FLOOR) {
    fprintf(func_globals->function_body, "FLOAT2INTS\n");
    fprintf(func_globals->function_body, "INT2FLOATS\n");
  } else if (member->member_kind == MEMBER_LENGTH) {
    fprintf(func_globals->function_body, "POPS %s$temp\n", var_prefix);
    fprintf(func_globals->function_body, "STRLEN %s$len %s$temp\n" , var_prefix);
    fprintf(func_globals->function_body, "PUSHS %s$len\n", var_prefix);
    if (func_globals->was_strlen == false) {
      func_globals->was_strlen = true;
      fprintf(func_globals->function_head, "DEFVAR %s$temp\n", var_prefix);
      fprintf(func_globals->function_head, "DEFVAR %s$len\n", var_prefix);
      }
  } else {
    return 1;
  }

  return 0;
}

static int generate_interpolated(AST_INTERPOLATED *interpolated, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (interpolated == NULL) {
    return 1;
  }

  bool is_first = true;

  for (size_t i = 0; i < interpolated->part_count; i++) {
    if (interpolated->parts[i].is_literal == true) {
      char *string = convert_string(interpolated->parts[i].text, strlen(interpolated->parts[i].text));
      if (interpolated->part_count > 1) {
        fprintf(func_globals->function_body, "MOV %s$str%i string@%s\n", var_prefix, is_first ? "1" : "2", string);
      } else {
        fprintf(func_globals->function_body, "PUSHS string@%s\n", string);
        break;
      }
    } else {
      if (generate_expression(interpolated->parts[i].expression, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
      fprintf(func_globals->function_body, "CALL !toStr\n");
      if (interpolated->part_count > 1) {
        fprintf(func_globals->function_body, "POPS %s$str%i\n", var_prefix, is_first ? "1" : "2");
      } else {
        break;
      }
    }
    if (is_first) {
      is_first = false;
      continue;
    }
    fprintf(func_globals->function_body, "CONCAT %s$concatstr %s$str1 %s$str2\n", var_prefix, var_prefix, var_prefix);    
    if (i < interpolated->part_count - 1) {
      fprintf(func_globals->function_body, "MOV %s$str1 %s$concatstr\n", var_prefix, var_prefix);
    }
  }

  if (interpolated->part_count > 1) {
    fprintf(func_globals->function_body, "PUSHS %s$concatstr\n", var_prefix);
  }

  if (func_globals->was_concat == false) {
    func_globals->was_concat = true;
    fprintf(func_globals->function_head, "DEFVAR %s$str1\n", var_prefix);
    fprintf(func_globals->function_head, "DEFVAR %s$str2\n", var_prefix);
    fprintf(func_globals->function_head, "DEFVAR %s$concatstr\n", var_prefix);
  }

  return 0;
}

static int generate_if(AST_IF *if_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (if_stmt == NULL) {
    return 1;
  }

  size_t prev_mangling = func_globals->var_mangling;
  func_globals->if_counter++;

  if (generate_expression(if_stmt->condition, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }

  fprintf(func_globals->function_body, "PUSHS bool@true\n");
  if (if_stmt->else_block == NULL) {
    fprintf(func_globals->function_body, "JUMPIFNEQS %s$if$%i$end\n", func_globals->function_name, func_globals->if_counter);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->then_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->then_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
    }
  } else {
    fprintf(func_globals->function_body, "JUMPIFEQS %s$if$%i\n", func_globals->function_name, func_globals->if_counter);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->else_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->else_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
    }
    fprintf(func_globals->function_body, "JUMP %s$if$%i$end\n", func_globals->function_name, func_globals->if_counter);
    fprintf(func_globals->function_body, "LABEL %s$if$%i\n", func_globals->function_name, func_globals->if_counter);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->then_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->then_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
        return 1;
      }
    }
  }
  fprintf(func_globals->function_body, "LABEL %s$if$%i$end\n", func_globals->function_name, func_globals->if_counter);

  func_globals->var_mangling = prev_mangling;

  while (var_list != NULL && var_list->mangling > prev_mangling) {
    var_list_node *var_node_delete = var_list;
    var_list = var_list->next;
    free(var_node_delete);
  }

  return 0;
}

static int generate_while(AST_WHILE *while_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (while_stmt == NULL) {
    return 1;
  }
  
  size_t prev_mangling = func_globals->var_mangling;
  func_globals->while_counter++;

  fprintf(func_globals->function_body, "LABEL %s$while$%i$loop\n", func_globals->function_name, func_globals->while_counter);
  if (generate_expression(while_stmt->condition, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }

  fprintf(func_globals->function_body, "PUSHS bool@true\n");
  fprintf(func_globals->function_body, "JUMPIFNEQS %s$while$%i$end\n", func_globals->function_name, func_globals->while_counter);
  func_globals->var_mangling_counter++;
  func_globals->var_mangling = func_globals->var_mangling_counter;
  for (size_t i = 0; i < while_stmt->body->statement_count; i++) {
    AST_STATEMENT *statement = while_stmt->body->statements[i];
    if (statement == NULL) {
      return 1;
    }
    if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  }
  fprintf(func_globals->function_body, "JUMP %s$while$%i$loop\n", func_globals->function_name, func_globals->while_counter);
  fprintf(func_globals->function_body, "LABEL %s$while$%i$end\n", func_globals->function_name, func_globals->while_counter);

  func_globals->var_mangling = prev_mangling;

  while (var_list != NULL && var_list->mangling > prev_mangling) {
    var_list_node *var_node_delete = var_list;
    var_list = var_list->next;
    free(var_node_delete);
  }

  return 0;
}

static int generate_for(AST_FOR *for_stmt, function_globals *func_globals, char var_prefix, var_list_node *var_list) {
  if (for_stmt == NULL) {
    return 1;
  }

  if (func_globals->for_counter > 0) {
    fprintf(func_globals->function_head, "DEFVAR %s$for$to\n", var_prefix);
    fprintf(func_globals->function_head, "DEFVAR %s$for$step\n", var_prefix);
    fprintf(func_globals->function_head, "DEFVAR %s$for$check\n", var_prefix);
  }
  
  size_t prev_mangling = func_globals->var_mangling;
  func_globals->for_counter++;
  func_globals->var_mangling_counter++;
  func_globals->var_mangling = func_globals->var_mangling_counter;

  var_list_node *var_node_old = var_list;
  var_list_node *var_node_new = malloc(sizeof(var_list_node));
  if (var_node_new == NULL) {
    return 1;
  }
  var_node_new->mangling = func_globals->var_mangling;
  var_node_new->name = for_stmt->var_name;
  var_node_new->next = var_node_old;
  *var_list = *var_node_new;

  if (generate_expression(for_stmt->from, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }
  fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, for_stmt->var_name, func_globals->var_mangling);

  if (generate_expression(for_stmt->to, &func_globals, var_prefix, &var_list) != 0) {
    return 1;
  }
  fprintf(func_globals->function_body, "POPS %s$for$to\n", var_prefix);

  if (for_stmt->step != NULL) {
    if (generate_expression(for_stmt->step, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "POPS %s$for$step\n", var_prefix);
    fprintf(func_globals->function_body, "PUSHS %s$for$step\n", var_prefix);
    fprintf(func_globals->function_body, "PUSHS int@0\n");
    fprintf(func_globals->function_body, "EQS\n");
    fprintf(func_globals->function_body, "PUSHS bool@true\n");
    fprintf(func_globals->function_body, "JUMPIFEQS %s$for$%i$error\n", func_globals->function_name, func_globals->for_counter);
  } else {
    fprintf(func_globals->function_body, "MOVE %s$for$step int@1\n", var_prefix);
  }

  fprintf(func_globals->function_body, "LABEL %s$for$%i$loop\n", func_globals->function_name, func_globals->for_counter);
  if (for_stmt->is_exclusive) {
    fprintf(func_globals->function_body, "LT %s$for$check %s%s$%i %s$for$to\n", var_prefix, var_prefix, for_stmt->var_name, func_globals->var_mangling, var_prefix);
    fprintf(func_globals->function_body, "JUMPIFEQ %s$for$%i$end %s$for$check bool@false\n", func_globals->function_name, func_globals->for_counter, var_prefix);
  } else {
    fprintf(func_globals->function_body, "GT %s$for$check %s%s$%i %s$for$to\n", var_prefix, var_prefix, for_stmt->var_name, func_globals->var_mangling, var_prefix);
    fprintf(func_globals->function_body, "JUMPIFEQ %s$for$%i$end %s$for$check bool@true\n", func_globals->function_name, func_globals->for_counter, var_prefix);
  }

  for (size_t i = 0; i < for_stmt->guard_count; i++) {
    AST_EXPRESSION *guard = for_stmt->guards[i];
    if (guard == NULL) {
      return 1;
    }
    if (generate_statement(guard, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
    fprintf(func_globals->function_body, "PUSHS bool@false\n");
    fprintf(func_globals->function_body, "JUMPIFEQS %s$for$%i$end\n", func_globals->function_name, func_globals->for_counter);
  }

  for (size_t i = 0; i < for_stmt->body->statement_count; i++) {
    AST_STATEMENT *statement = for_stmt->body->statements[i];
    if (statement == NULL) {
      return 1;
    }
    if (generate_statement(statement, &func_globals, var_prefix, &var_list) != 0) {
      return 1;
    }
  }

  fprintf(func_globals->function_body, "PUSHS %s%s$%i\n", var_prefix, for_stmt->var_name, func_globals->var_mangling);
  fprintf(func_globals->function_body, "PUSHS %s$for$step\n", var_prefix);
  fprintf(func_globals->function_body, "ADDS\n");
  fprintf(func_globals->function_body, "POPS %s%s$%i\n", var_prefix, for_stmt->var_name, func_globals->var_mangling);
  fprintf(func_globals->function_body, "JUMP %s$for$%i$loop\n", func_globals->function_name, func_globals->for_counter);

  if (for_stmt->step != NULL) {
    fprintf(func_globals->function_body, "LABEL %s$for$%i$error\n", func_globals->function_name, func_globals->for_counter);
    fprintf(func_globals->function_body, "EXIT int@40");
  }
  fprintf(func_globals->function_body, "LABEL %s$for$%i$end\n", func_globals->function_name, func_globals->for_counter);

  func_globals->var_mangling = prev_mangling;

  while (var_list != NULL && var_list->mangling > prev_mangling) {
    var_list_node *var_node_delete = var_list;
    var_list = var_list->next;
    free(var_node_delete);
  }

  return 0;
}

size_t find_variable(var_list_node *var_list, char name) {
  var_list_node *var_node = var_list;
  while (var_node != NULL) {
    if (strcmp(name, var_node->name) == 0) {
      return var_node->mangling;
    }
    var_node = var_node->next;
  }
  return -1;
}

static char* convert_string(char *string, size_t length) {
  char *converted;
  if (length == 0) {
    converted = "\\000";
  }

  for (size_t i = 0; i < length; i++) {
    if (string[i] <= 32 || string[i] == 35 || string[i] == 92) {
      char ascii_value[3];
      sprintf(ascii_value, "%d", string[i]);
      char escape[4];
      escape[0] = '\\';
      escape[1] = '0';
      if (string[i] < 10) {
        escape[2] = '0';
        escape[3] = ascii_value[0];
      }
      escape[2] = ascii_value[0];
      escape[3] = ascii_value[1];
      strcat(converted, escape);

    } else {
      strcat(converted, string[i]);
    }
  }
  return converted;
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

int generate_code(AST_PROGRAM *program) {
  if (program == NULL) {
    return 1;
  }

  printf(".IFJcode26\n\n");
  printf("JUMP main\n\n");

  for (size_t i = 0; i < program->function_count; i++) {
    AST_FUNCTION_DEF *function = program->functions[i];
    if (function == NULL) {
      return 1;
    }
    if (generate_function(function) != 0) {
      return 1;
    }
  }

  return 0;
}
