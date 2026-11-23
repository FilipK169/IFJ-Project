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

#include "ast.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

/** @brief Buffer structure for the generated code. */
typedef struct {
  char *data;
  size_t length;
  size_t capacity;
  bool failed;
} code_buffer;

/** @brief Structure for function name, files, checks and counters.*/
typedef struct {
  char *function_name; // Name of current function.
  code_buffer function_head; // Generated head of current function, used for variable declarations.
  code_buffer function_body; // Generated body of current function.
  char *var_prefix; // Variable prefix for current function.
  bool is_main; // Is function the main function?
  size_t var_mangling; // Mangling of variables in the current scope.
  size_t var_mangling_counter; // Mangling counter for whole function.
  size_t if_counter; // Counter of if statements in a function.
  size_t while_counter; // Counter of while cycles in a function.
  size_t for_counter; // Counter of for cycles in a function.
  bool was_concat; // Check for generating helper variables for concatenation.
  bool returns_unit; // Check if function returns unit type.
  size_t and_counter; // Counter of and expressions in a function.
  size_t or_counter; // Counter of or expressions in a function.
} function_globals;

/** @brief Checklist of used built-in functions.*/
typedef struct {
  bool print;
  bool readLine;
  bool readInt;
  bool readDouble;
  bool readBoolean;
  bool toStr;
  bool strLength;
  bool subStr;
  bool ord;
} builtins_checklist;

/** @brief List for keeping track of mangling for variables declared inside the body of a function.*/
typedef struct var_list_node var_list_node;

struct var_list_node {
  char *name;
  size_t mangling;
  var_list_node *next;
};

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Generates code for function definitions.
 *
 * @param[in] function AST node of the function definition.
 * @param[in,out] builtins Checklist of used built-in functions.
 *
 * @return 0 on success, 1 on error.
 */
static int generate_function(AST_FUNCTION_DEF *function, builtins_checklist *builtins);

/**
 * @brief Generates code for statements.
 *
 * @param[in] statement AST node of the statement.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_statement(AST_STATEMENT *statement, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for variable declarations.
 *
 * @param[in] var_decl AST node of the variable declaration.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_var_decl(AST_VAR_DECL *var_decl, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for variable assignments.
 *
 * @param[in] assignment AST node of the variable assignment.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_assignment(AST_ASSIGNMENT *assignment, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for returns and exit.
 *
 * @param[in] return_stmt AST node of the return.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_return(AST_RETURN *return_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for expressions.
 *
 * @param[in] expression AST node of the expression.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_expression(AST_EXPRESSION *expression, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for unary expressions.
 *
 * @param[in] unary_expr AST node of the unary expression.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_unary_expr(AST_UNARY_EXPRESSION *unary_expr, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for binary expressions.
 *
 * @param[in] binary_expr AST node of the binary expression.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_binary_expr(AST_BINARY_EXPRESSION *binary_expr, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for function calls.
 *
 * @param[in] call AST node of the function call.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_call(AST_CALL *call, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for member function calls.
 *
 * @param[in] member AST node of the member function call.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_member(AST_MEMBER *member, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for interpolated strings.
 *
 * @param[in] interpolated AST node of the interpolated strings.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_interpolated(AST_INTERPOLATED *interpolated, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for if statements.
 *
 * @param[in] if_stmt AST node of the if statement.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_if(AST_IF *if_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for while cycles.
 *
 * @param[in] while_stmt AST node of the while cycle.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_while(AST_WHILE *while_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for for cycles.
 *
 * @param[in] for_stmt AST node of the for cycle.
 * @param[in,out] func_globals Structure for function name, files, checks and counters.
 * @param[in,out] var_list List of currently declared variables.
 * @param[in,out] builtins Checklist of used built-in functions.
 * 
 * @return 0 on success, 1 on error.
 */
static int generate_for(AST_FOR *for_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins);

/**
 * @brief Generates code for built-in print function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_print();

/**
 * @brief Generates code for built-in readLine function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_readLine();

/**
 * @brief Generates code for built-in readInt function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_readInt();

/**
 * @brief Generates code for built-in readDouble function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_readDouble();

/**
 * @brief Generates code for built-in readBoolean function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_readBoolean();

/**
 * @brief Generates code for built-in toStr function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_toStr();

/**
 * @brief Generates code for built-in strLength function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_strLength();

/**
 * @brief Generates code for built-in subStr function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_subStr();

/**
 * @brief Generates code for built-in ord function.
 *
 * @return 0 on success, 1 on error.
 */
static int builtin_ord();

/**
 * @brief Finds mangling of a variable.
 *
 * @param[in] var_list List of declared local variables.
 * @param[in] name Name of the variable.
 *
 * @return Mangling of found variable.
 * @retval SIZE_MAX - variable not found.
 */
static size_t find_variable(var_list_node *var_list, const char *name);

/**
 * @brief Converts a string into IFJcode26 string format.
 *
 * @param[in] string String.
 * @param[in] length Length of the string.
 * 
 * @return Converted string.
 */
static char* convert_string(const char *string, size_t length);

/*
 * AI use
 * Prompt: How do I replace my code printing if I am not allowed to use tmpfile?
 */
static void buffer_printf(code_buffer *buffer, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

static void buffer_printf(code_buffer *buffer, const char *format, ...) {
  if (buffer->failed) {
    return;
  }

  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  int formatted_length = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  if (formatted_length < 0 ||
      buffer->length > SIZE_MAX - (size_t)formatted_length - 1) {
    buffer->failed = true;
    va_end(args);
    return;
  }

  size_t required_capacity = buffer->length + (size_t)formatted_length + 1;
  if (required_capacity > buffer->capacity) {
    size_t new_capacity = buffer->capacity == 0 ? 128 : buffer->capacity;
    while (new_capacity < required_capacity) {
      if (new_capacity > SIZE_MAX / 2) {
        new_capacity = required_capacity;
        break;
      }
      new_capacity *= 2;
    }

    char *new_data = realloc(buffer->data, new_capacity);
    if (new_data == NULL) {
      buffer->failed = true;
      va_end(args);
      return;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
  }

  int written = vsnprintf(buffer->data + buffer->length,
                          buffer->capacity - buffer->length, format, args);
  va_end(args);
  if (written != formatted_length) {
    buffer->failed = true;
    return;
  }
  buffer->length += (size_t)written;
}

static void buffer_free(code_buffer *buffer) {
  free(buffer->data);
  buffer->data = NULL;
  buffer->length = 0;
  buffer->capacity = 0;
}
/*
 * END AI
 */

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static int generate_function(AST_FUNCTION_DEF *function, builtins_checklist *builtins) {
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
  func_globals->was_concat = false;
  func_globals->function_head = (code_buffer){0};
  func_globals->function_body = (code_buffer){0};
  func_globals->function_name = NULL;
  if (function->is_main || function->declared_return_type == TYPE_UNIT) {
    func_globals->returns_unit = true;
  } else {
    func_globals->returns_unit = false;
  }

  const char *base_function_name = function->is_main ? "$main" : function->name;
  bool has_receiver = !function->is_main && function->receiver != NULL;
  if (base_function_name == NULL) {
    buffer_free(&func_globals->function_head);
    buffer_free(&func_globals->function_body);
    free(func_globals->function_name);
    free(func_globals);
    return 1;
  }

  size_t name_length = strlen(base_function_name);
  size_t ext_prefix_length = has_receiver ? 1 : 0;
  if (name_length > SIZE_MAX - ext_prefix_length - 1) {
    buffer_free(&func_globals->function_head);
    buffer_free(&func_globals->function_body);
    free(func_globals->function_name);
    free(func_globals);
    return 1;
  }
  func_globals->function_name = malloc(name_length + ext_prefix_length + 1);
  if (func_globals->function_name == NULL) {
    buffer_free(&func_globals->function_head);
    buffer_free(&func_globals->function_body);
    free(func_globals->function_name);
    free(func_globals);
    return 1;
  }
  if (has_receiver) {
    func_globals->function_name[0] = '*';
  }
  memcpy(func_globals->function_name + ext_prefix_length, base_function_name, name_length + 1);

  bool is_last = false;
  var_list_node *var_list = NULL;

  if (function->is_main) {
    buffer_printf(&func_globals->function_head, "LABEL $main\n");
    func_globals->var_prefix = "GF@";
    func_globals->is_main = true;
  } else {
    if (function->receiver == NULL) {
      buffer_printf(&func_globals->function_head, "LABEL %s\n", function->name);
    } else {
      buffer_printf(&func_globals->function_head, "LABEL *%s\n", function->name);
    }
    func_globals->var_prefix = "LF@";

    buffer_printf(&func_globals->function_head, "CREATEFRAME\n");
    buffer_printf(&func_globals->function_head, "PUSHFRAME\n");

    if (function->receiver != NULL) {
      buffer_printf(&func_globals->function_head, "DEFVAR %s%s$0\n", func_globals->var_prefix, function->receiver->name);
      var_list_node *var_node_new = malloc(sizeof(var_list_node));
      if (var_node_new == NULL) {
        while (var_list != NULL) {
          var_list_node *var_node_delete = var_list;
          var_list = var_list->next;
          free(var_node_delete);
        }
        buffer_free(&func_globals->function_head);
        buffer_free(&func_globals->function_body);
        free(func_globals->function_name);
        free(func_globals);
        return 1;
      }
      var_node_new->mangling = 0;
      var_node_new->name = function->receiver->name;
      var_node_new->next = var_list;
      var_list = var_node_new;
    }

    for (size_t i = 0; i < function->param_list_count; i++) {
      AST_PARAM_LIST *param_list = &function->param_lists[i];
      for (size_t j = 0; j < param_list->parameter_count; j++) {
        AST_PARAMETER *param = &param_list->parameters[j];
        buffer_printf(&func_globals->function_head, "DEFVAR %s%s$%zu\n", func_globals->var_prefix, param->name, func_globals->var_mangling);
        var_list_node *var_node_new = malloc(sizeof(var_list_node));
        if (var_node_new == NULL) {
          while (var_list != NULL) {
            var_list_node *var_node_delete = var_list;
            var_list = var_list->next;
            free(var_node_delete);
          }
          buffer_free(&func_globals->function_head);
          buffer_free(&func_globals->function_body);
          free(func_globals->function_name);
          free(func_globals);
          return 1;
        }
        var_node_new->mangling = 0;
        var_node_new->name = param->name;
        var_node_new->next = var_list;
        var_list = var_node_new;
      }
    }

    for (size_t i = function->param_list_count; i > 0; i--) {
      AST_PARAM_LIST *param_list = &function->param_lists[i - 1];
      for (size_t j = param_list->parameter_count; j > 0; j--) {
        AST_PARAMETER *param = &param_list->parameters[j - 1];
        buffer_printf(&func_globals->function_head, "POPS %s%s$%zu\n", func_globals->var_prefix, param->name, func_globals->var_mangling);
      }
    }

    if (function->receiver != NULL) {
      buffer_printf(&func_globals->function_head, "POPS %s%s\n", func_globals->var_prefix, function->receiver->name);
    }
  }

  for (size_t i = 0; i < function->body->statement_count; i++) {
    AST_STATEMENT *statement = function->body->statements[i];
    if (statement == NULL) {
      while (var_list != NULL) {
        var_list_node *var_node_delete = var_list;
        var_list = var_list->next;
        free(var_node_delete);
      }
      buffer_free(&func_globals->function_head);
      buffer_free(&func_globals->function_body);
      free(func_globals->function_name);
      free(func_globals);
      return 1;
    }
    if (i == function->body->statement_count - 1) {
      is_last = true;
    }
    if (generate_statement(statement, func_globals, &var_list, builtins) != 0) {
      while (var_list != NULL) {
        var_list_node *var_node_delete = var_list;
        var_list = var_list->next;
        free(var_node_delete);
      }
      buffer_free(&func_globals->function_head);
      buffer_free(&func_globals->function_body);
      free(func_globals->function_name);
      free(func_globals);
      return 1;
    }
    if (is_last && func_globals->returns_unit && statement->type != STMT_RETURN) {
      buffer_printf(&func_globals->function_body, "PUSHS nil@nil\n");
    }
    if (is_last && statement->type != STMT_RETURN) {
      if (func_globals->is_main) {
        buffer_printf(&func_globals->function_body, "EXIT int@0\n");
      } else {
        buffer_printf(&func_globals->function_body, "POPFRAME\n");
        buffer_printf(&func_globals->function_body, "RETURN\n");
      }
    }
  }

  while (var_list != NULL) {
    var_list_node *var_node_delete = var_list;
    var_list = var_list->next;
    free(var_node_delete);
  }

  if (func_globals->function_head.failed || func_globals->function_body.failed) {
    buffer_free(&func_globals->function_head);
    buffer_free(&func_globals->function_body);
    free(func_globals->function_name);
    free(func_globals);
    return 1;
  }

  bool output_failed = false;
  if (func_globals->function_head.length > 0 &&
      fwrite(func_globals->function_head.data, 1, func_globals->function_head.length,
             stdout) != func_globals->function_head.length) {
    output_failed = true;
  }
  if (!output_failed && func_globals->function_body.length > 0 &&
      fwrite(func_globals->function_body.data, 1, func_globals->function_body.length,
             stdout) != func_globals->function_body.length) {
    output_failed = true;
  }

  buffer_free(&func_globals->function_head);
  buffer_free(&func_globals->function_body);
  free(func_globals->function_name);
  free(func_globals);
  return output_failed ? 1 : 0;
}

static int generate_statement(AST_STATEMENT *statement, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (statement == NULL) {
    return 1;
  }

  if (statement->type == STMT_VAR_DECL) {
    if (generate_var_decl(&statement->data.var_decl, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_ASSIGNMENT) {
    if (generate_assignment(&statement->data.assignment, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_RETURN) {
    if (generate_return(&statement->data.return_stmt, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_IF) {
    if (generate_if(&statement->data.if_stmt, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_WHILE) {
    if (generate_while(&statement->data.while_stmt, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_FOR) {
    if (generate_for(&statement->data.for_stmt, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else if (statement->type == STMT_EXPR) {
    if (generate_expression(statement->data.expr_stmt.expression, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}

static int generate_var_decl(AST_VAR_DECL *var_decl, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (var_decl == NULL) {
    return 1;
  }

  size_t local_mangling = func_globals->var_mangling;
  if (generate_expression(var_decl->initializer, func_globals, var_list, builtins) != 0) {
    return 1;
  }
  buffer_printf(&func_globals->function_body, "POPS %s%s$%zu\n", func_globals->var_prefix, var_decl->name, local_mangling);

  buffer_printf(&func_globals->function_head, "DEFVAR %s%s$%zu\n", func_globals->var_prefix, var_decl->name, local_mangling);
  var_list_node *var_node_new = malloc(sizeof(var_list_node));
  if (var_node_new == NULL) {
    return 1;
  }
  var_node_new->mangling = local_mangling;
  var_node_new->name = var_decl->name;
  var_node_new->next = *var_list;
  *var_list = var_node_new;

  return 0;
}

static int generate_assignment(AST_ASSIGNMENT *assignment, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (assignment == NULL) {
    return 1;
  }

  size_t local_mangling = find_variable(*var_list, assignment->name);
  if (local_mangling == SIZE_MAX) {
    return 1;
  }

  if (generate_expression(assignment->value, func_globals, var_list, builtins) != 0) {
    return 1;
  }
  buffer_printf(&func_globals->function_body, "POPS %s%s$%zu\n", func_globals->var_prefix, assignment->name, local_mangling);

  return 0;
}

static int generate_return(AST_RETURN *return_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (return_stmt == NULL) {
    return 1;
  }

  if (return_stmt->value != NULL) {
    if (generate_expression(return_stmt->value, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  }

  if (func_globals->returns_unit) {
    buffer_printf(&func_globals->function_body, "PUSHS nil@nil\n");
  }

  if (func_globals->is_main) {
      buffer_printf(&func_globals->function_body, "EXIT int@0\n");
    } else {
      buffer_printf(&func_globals->function_body, "POPFRAME\n");
      buffer_printf(&func_globals->function_body, "RETURN\n");
  }

  return 0;
}

static int generate_expression(AST_EXPRESSION *expression, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (expression == NULL) {
    return 1;
  }

  if (expression->type == EXPR_VAR_REF) {
    size_t mangling = find_variable(*var_list, expression->data.var_ref.name);
    if (mangling == SIZE_MAX) {
      return 1;
    }
    buffer_printf(&func_globals->function_body, "PUSHS %s%s$%zu\n", func_globals->var_prefix, expression->data.var_ref.name, mangling);

  } else if (expression->type == EXPR_BOOL_LITERAL) {
    buffer_printf(&func_globals->function_body, "PUSHS bool@%s\n", expression->data.bool_literal ? "true" : "false");

  } else if (expression->type == EXPR_DOUBLE_LITERAL) {
    buffer_printf(&func_globals->function_body, "PUSHS float@%a\n", expression->data.double_literal);

  } else if (expression->type == EXPR_INT_LITERAL) {
    buffer_printf(&func_globals->function_body, "PUSHS int@%lld\n", (long long)expression->data.int_literal);

  } else if (expression->type == EXPR_STRING_LITERAL) {
    char *string = convert_string(expression->data.string_literal.value, expression->data.string_literal.length);
    if (string == NULL) {
      return 1;
    }
    buffer_printf(&func_globals->function_body, "PUSHS string@%s\n", string);
    free(string);

  } else if (expression->type == EXPR_UNIT_LITERAL) {
    buffer_printf(&func_globals->function_body, "PUSHS nil@nil\n");

  } else if (expression->type == EXPR_UNARY) {
    if (generate_unary_expr(&expression->data.unary, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_BINARY) {
    if (generate_binary_expr(&expression->data.binary, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_CALL) {
    if (generate_call(&expression->data.call, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_MEMBER) {
    if (generate_member(&expression->data.member, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_INTERPOLATED) {
    if (generate_interpolated(&expression->data.interpolated, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else if (expression->type == EXPR_IF) {
    if (generate_if(&expression->data.if_expr, func_globals, var_list, builtins) != 0) {
      return 1;
    }

  } else {
    return 1;
  }

  if (expression->conversion == CONVERSION_INT_TO_DOUBLE) {
    buffer_printf(&func_globals->function_body, "INT2FLOATS\n");
  }

  return 0;
}

static int generate_unary_expr(AST_UNARY_EXPRESSION *unary_expr, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (unary_expr == NULL) {
    return 1;
  }

  if (generate_expression(unary_expr->operand, func_globals, var_list, builtins) != 0) {
    return 1;
  }

  if (unary_expr->op == UNARY_PLUS) {
    return 0;

  } else if (unary_expr->op == UNARY_MINUS) {
    if (unary_expr->operand->resolved_type == TYPE_INT) {
      buffer_printf(&func_globals->function_body, "PUSHS int@-1\n");
      buffer_printf(&func_globals->function_body, "MULS\n");
    } else if (unary_expr->operand->resolved_type == TYPE_DOUBLE) {
      buffer_printf(&func_globals->function_body, "PUSHS float@%a\n", -1.0);
      buffer_printf(&func_globals->function_body, "MULS\n");
    } else {
      return 1;
    }

  } else if (unary_expr->op == UNARY_NOT) {
    buffer_printf(&func_globals->function_body, "NOTS\n");
    
  } else {
    return 1;
  }
  
  return 0;
}

static int generate_binary_expr(AST_BINARY_EXPRESSION *binary_expr, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (binary_expr == NULL) {
    return 1;
  }

  if (binary_expr->op == BINARY_ADD) { // +
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "ADDS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "POPS %s$str2\n", func_globals->var_prefix);
      buffer_printf(&func_globals->function_body, "POPS %s$str1\n", func_globals->var_prefix);
      buffer_printf(&func_globals->function_body, "CONCAT %s$concatstr %s$str1 %s$str2\n", func_globals->var_prefix, func_globals->var_prefix, func_globals->var_prefix);
      buffer_printf(&func_globals->function_body, "PUSHS %s$concatstr\n", func_globals->var_prefix);
      if (func_globals->was_concat == false) {
        func_globals->was_concat = true;
        buffer_printf(&func_globals->function_head, "DEFVAR %s$str1\n", func_globals->var_prefix);
        buffer_printf(&func_globals->function_head, "DEFVAR %s$str2\n", func_globals->var_prefix);
        buffer_printf(&func_globals->function_head, "DEFVAR %s$concatstr\n", func_globals->var_prefix);
      }
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_SUB) { // -
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "SUBS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_MUL) { // *
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "MULS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_DIV) { // /
    if (binary_expr->left->resolved_type == TYPE_INT && binary_expr->right->resolved_type == TYPE_INT) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "IDIVS\n");
    } else if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "DIVS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_LT) { // <
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_GT) { // >
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_LE) { // <=
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "GTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_GE) { // >=
    if ((binary_expr->left->resolved_type == TYPE_INT || binary_expr->left->resolved_type == TYPE_DOUBLE) && (binary_expr->right->resolved_type == TYPE_INT || binary_expr->right->resolved_type == TYPE_DOUBLE)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else if (binary_expr->left->resolved_type == TYPE_STRING && binary_expr->right->resolved_type == TYPE_STRING) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "LTS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_EQ) { // ==
    if ((binary_expr->left->resolved_type == binary_expr->right->resolved_type) || (binary_expr->left->resolved_type == TYPE_UNIT || binary_expr->right->resolved_type == TYPE_UNIT)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "EQS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_NE) { // !=
    if ((binary_expr->left->resolved_type == binary_expr->right->resolved_type) || (binary_expr->left->resolved_type == TYPE_UNIT || binary_expr->right->resolved_type == TYPE_UNIT)) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "EQS\n");
      buffer_printf(&func_globals->function_body, "NOTS\n");
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_AND) { // &&
    if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "PUSHS bool@false\n");
      buffer_printf(&func_globals->function_body, "JUMPIFEQS %s$and$%zu$false\n", func_globals->function_name, func_globals->and_counter);
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "JUMP %s$and$%zu$end\n", func_globals->function_name, func_globals->and_counter);
      buffer_printf(&func_globals->function_body, "LABEL %s$and$%zu$false\n", func_globals->function_name, func_globals->and_counter);
      buffer_printf(&func_globals->function_body, "PUSHS bool@false\n");
      buffer_printf(&func_globals->function_body, "LABEL %s$and$%zu$end\n", func_globals->function_name, func_globals->and_counter);
      func_globals->and_counter++;
    } else {
      return 1;
    }

  } else if (binary_expr->op == BINARY_OR) { // ||
    if (binary_expr->left->resolved_type == TYPE_BOOLEAN && binary_expr->right->resolved_type == TYPE_BOOLEAN) {
      if (generate_expression(binary_expr->left, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "PUSHS bool@true\n");
      buffer_printf(&func_globals->function_body, "JUMPIFEQS %s$or$%zu$true\n", func_globals->function_name, func_globals->or_counter);
      if (generate_expression(binary_expr->right, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "JUMP %s$or$%zu$end\n", func_globals->function_name, func_globals->or_counter);
      buffer_printf(&func_globals->function_body, "LABEL %s$or$%zu$true\n", func_globals->function_name, func_globals->or_counter);
      buffer_printf(&func_globals->function_body, "PUSHS bool@true\n");
      buffer_printf(&func_globals->function_body, "LABEL %s$or$%zu$end\n", func_globals->function_name, func_globals->or_counter);
      func_globals->or_counter++;
    } else {
      return 1;
    }

  } else {
    return 1;
  }

  return 0;
}

static int generate_call(AST_CALL *call, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (call == NULL) {
    return 1;
  }

  if (call->callee_kind == CALLEE_EXTENSION) {
    if (generate_expression(call->receiver, func_globals, var_list, builtins) != 0) {
        return 1;
    }
  }

  for (size_t i = 0; i < call->argument_list_count; i++) {
    AST_ARG_LIST *arg_list = &call->argument_lists[i];
    for (size_t j = 0; j < arg_list->argument_count; j++) {
      AST_EXPRESSION *arg = arg_list->arguments[j];
      if (generate_expression(arg, func_globals, var_list, builtins) != 0) {
        return 1;
      }
    }
  }
  
  if (call->callee_kind == CALLEE_USER_FUNCTION) {
    buffer_printf(&func_globals->function_body, "CALL %s\n", call->name);

  } else if (call->callee_kind == CALLEE_EXTENSION) {
    buffer_printf(&func_globals->function_body, "CALL *%s\n", call->name);

  } else if (call->callee_kind == CALLEE_BUILTIN) {
    if (strcmp(call->name, "print") == 0) {
      builtins->print = true;
    } else if (strcmp(call->name, "readLine") == 0) {
      builtins->readLine = true;
    } else if (strcmp(call->name, "readInt") == 0) {
      builtins->readInt = true;
    } else if (strcmp(call->name, "readDouble") == 0) {
      builtins->readDouble = true;
    } else if (strcmp(call->name, "readBoolean") == 0) {
      builtins->readBoolean = true;
    } else if (strcmp(call->name, "toStr") == 0) {
      builtins->toStr = true;
    } else if (strcmp(call->name, "toInt") == 0) {
      buffer_printf(&func_globals->function_body, "FLOAT2INTS\n");
      return 0;
    } else if (strcmp(call->name, "strLength") == 0) {
      builtins->strLength = true;
    } else if (strcmp(call->name, "subStr") == 0) {
      builtins->subStr = true;
    } else if (strcmp(call->name, "ord") == 0) {
      builtins->ord = true;
    } else if (strcmp(call->name, "chr") == 0) {
      buffer_printf(&func_globals->function_body, "INT2CHARS\n");
      return 0;
    } else {
      return 1;
    }
    buffer_printf(&func_globals->function_body, "CALL !%s\n", call->name);
  }

  return 0;
}

static int generate_member(AST_MEMBER *member, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (member == NULL) {
    return 1;
  }
  
  if (generate_expression(member->receiver, func_globals, var_list, builtins) != 0) {
    return 1;
  }

  if (member->member_kind == MEMBER_EXT_PROPERTY) {
    buffer_printf(&func_globals->function_body, "CALL *%s\n", member->member_name);
  } else if (member->member_kind == MEMBER_TO_STRING) {
    buffer_printf(&func_globals->function_body, "CALL !toStr\n");
    builtins->toStr = true;
  } else if (member->member_kind == MEMBER_TO_INT) {
    buffer_printf(&func_globals->function_body, "FLOAT2INTS\n");
  } else if (member->member_kind == MEMBER_FLOOR) {
    buffer_printf(&func_globals->function_body, "FLOAT2INTS\n");
    buffer_printf(&func_globals->function_body, "INT2FLOATS\n");
  } else if (member->member_kind == MEMBER_LENGTH) {
    buffer_printf(&func_globals->function_body, "CALL !strLength\n");
    builtins->strLength = true;
  } else {
    return 1;
  }

  return 0;
}

static int generate_interpolated(AST_INTERPOLATED *interpolated, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (interpolated == NULL) {
    return 1;
  }

  bool is_first = true;

  if (interpolated->part_count == 0) {
    buffer_printf(&func_globals->function_body, "PUSHS string@\n");
  }

  for (size_t i = 0; i < interpolated->part_count; i++) {
    if (interpolated->parts[i].is_literal == true) {
      if (interpolated->parts[i].text == NULL) {
        return 1;
      }
      char *string = convert_string(interpolated->parts[i].text, strlen(interpolated->parts[i].text));
      if (string == NULL) {
        return 1;
      }
      if (interpolated->part_count > 1) {
        buffer_printf(&func_globals->function_body, "MOVE %s$str%s string@%s\n", func_globals->var_prefix, is_first ? "1" : "2", string);
        free(string);
      } else {
        buffer_printf(&func_globals->function_body, "PUSHS string@%s\n", string);
        free(string);
        break;
      }
    } else {
      if (generate_expression(interpolated->parts[i].expression, func_globals, var_list, builtins) != 0) {
        return 1;
      }
      buffer_printf(&func_globals->function_body, "CALL !toStr\n");
      builtins->toStr = true;
      if (interpolated->part_count > 1) {
        buffer_printf(&func_globals->function_body, "POPS %s$str%s\n", func_globals->var_prefix, is_first ? "1" : "2");
      } else {
        break;
      }
    }
    if (is_first) {
      is_first = false;
      continue;
    }
    buffer_printf(&func_globals->function_body, "CONCAT %s$concatstr %s$str1 %s$str2\n", func_globals->var_prefix, func_globals->var_prefix, func_globals->var_prefix);
    if (i < interpolated->part_count - 1) {
      buffer_printf(&func_globals->function_body, "MOVE %s$str1 %s$concatstr\n", func_globals->var_prefix, func_globals->var_prefix);
    }
  }

  if (interpolated->part_count > 1) {
    buffer_printf(&func_globals->function_body, "PUSHS %s$concatstr\n", func_globals->var_prefix);
  }

  if (func_globals->was_concat == false) {
    func_globals->was_concat = true;
    buffer_printf(&func_globals->function_head, "DEFVAR %s$str1\n", func_globals->var_prefix);
    buffer_printf(&func_globals->function_head, "DEFVAR %s$str2\n", func_globals->var_prefix);
    buffer_printf(&func_globals->function_head, "DEFVAR %s$concatstr\n", func_globals->var_prefix);
  }

  return 0;
}

static int generate_if(AST_IF *if_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (if_stmt == NULL) {
    return 1;
  }

  size_t prev_mangling = func_globals->var_mangling;
  func_globals->if_counter++;
  size_t local_if = func_globals->if_counter;

  if (generate_expression(if_stmt->condition, func_globals, var_list, builtins) != 0) {
    return 1;
  }

  buffer_printf(&func_globals->function_body, "PUSHS bool@true\n");
  if (if_stmt->else_block == NULL) {
    buffer_printf(&func_globals->function_body, "JUMPIFNEQS %s$if$%zu$end\n", func_globals->function_name, local_if);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->then_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->then_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, func_globals, var_list, builtins) != 0) {
        return 1;
      }
    }
  } else {
    buffer_printf(&func_globals->function_body, "JUMPIFEQS %s$if$%zu\n", func_globals->function_name, local_if);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->else_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->else_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, func_globals, var_list, builtins) != 0) {
        return 1;
      }
    }
    buffer_printf(&func_globals->function_body, "JUMP %s$if$%zu$end\n", func_globals->function_name, local_if);
    buffer_printf(&func_globals->function_body, "LABEL %s$if$%zu\n", func_globals->function_name, local_if);
    func_globals->var_mangling_counter++;
    func_globals->var_mangling = func_globals->var_mangling_counter;
    for (size_t i = 0; i < if_stmt->then_block->statement_count; i++) {
      AST_STATEMENT *statement = if_stmt->then_block->statements[i];
      if (statement == NULL) {
        return 1;
      }
      if (generate_statement(statement, func_globals, var_list, builtins) != 0) {
        return 1;
      }
    }
  }
  buffer_printf(&func_globals->function_body, "LABEL %s$if$%zu$end\n", func_globals->function_name, local_if);

  func_globals->var_mangling = prev_mangling;

  while (*var_list != NULL && (*var_list)->mangling > prev_mangling) {
    var_list_node *var_node_delete = *var_list;
    *var_list = (*var_list)->next;
    free(var_node_delete);
  }

  return 0;
}

static int generate_while(AST_WHILE *while_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (while_stmt == NULL) {
    return 1;
  }
  
  size_t prev_mangling = func_globals->var_mangling;
  func_globals->while_counter++;
  size_t local_while = func_globals->while_counter;

  buffer_printf(&func_globals->function_body, "LABEL %s$while$%zu$loop\n", func_globals->function_name, local_while);
  if (generate_expression(while_stmt->condition, func_globals, var_list, builtins) != 0) {
    return 1;
  }

  buffer_printf(&func_globals->function_body, "PUSHS bool@true\n");
  buffer_printf(&func_globals->function_body, "JUMPIFNEQS %s$while$%zu$end\n", func_globals->function_name, local_while);
  func_globals->var_mangling_counter++;
  func_globals->var_mangling = func_globals->var_mangling_counter;
  for (size_t i = 0; i < while_stmt->body->statement_count; i++) {
    AST_STATEMENT *statement = while_stmt->body->statements[i];
    if (statement == NULL) {
      return 1;
    }
    if (generate_statement(statement, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  }
  buffer_printf(&func_globals->function_body, "JUMP %s$while$%zu$loop\n", func_globals->function_name, local_while);
  buffer_printf(&func_globals->function_body, "LABEL %s$while$%zu$end\n", func_globals->function_name, local_while);

  func_globals->var_mangling = prev_mangling;

  while (*var_list != NULL && (*var_list)->mangling > prev_mangling) {
    var_list_node *var_node_delete = *var_list;
    *var_list = (*var_list)->next;
    free(var_node_delete);
  }

  return 0;
}

static int generate_for(AST_FOR *for_stmt, function_globals *func_globals, var_list_node **var_list, builtins_checklist *builtins) {
  if (for_stmt == NULL) {
    return 1;
  }

  func_globals->for_counter++;
  size_t local_for = func_globals->for_counter;

  buffer_printf(&func_globals->function_head, "DEFVAR %s$for$%zu$to\n", func_globals->var_prefix, local_for);
  buffer_printf(&func_globals->function_head, "DEFVAR %s$for$%zu$step\n", func_globals->var_prefix, local_for);
  buffer_printf(&func_globals->function_head, "DEFVAR %s$for$%zu$check\n", func_globals->var_prefix, local_for);
  
  size_t prev_mangling = func_globals->var_mangling;
  func_globals->var_mangling_counter++;
  func_globals->var_mangling = func_globals->var_mangling_counter;

  if (generate_expression(for_stmt->from, func_globals, var_list, builtins) != 0) {
    return 1;
  }
  buffer_printf(&func_globals->function_body, "POPS %s%s$%zu\n", func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling);

  if (generate_expression(for_stmt->to, func_globals, var_list, builtins) != 0) {
    return 1;
  }
  buffer_printf(&func_globals->function_body, "POPS %s$for$%zu$to\n", func_globals->var_prefix, local_for);

  if (for_stmt->step != NULL) {
    if (generate_expression(for_stmt->step, func_globals, var_list, builtins) != 0) {
      return 1;
    }
    buffer_printf(&func_globals->function_body, "POPS %s$for$%zu$step\n", func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "PUSHS %s$for$%zu$step\n", func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "PUSHS int@0\n");
    buffer_printf(&func_globals->function_body, "JUMPIFEQS %s$for$%zu$error\n", func_globals->function_name, local_for);
  } else {
    buffer_printf(&func_globals->function_body, "MOVE %s$for$%zu$step int@1\n", func_globals->var_prefix, local_for);
  }

  buffer_printf(&func_globals->function_head, "DEFVAR %s%s$%zu\n", func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling);

  var_list_node *var_node_new = malloc(sizeof(var_list_node));
  if (var_node_new == NULL) {
    return 1;
  }
  var_node_new->mangling = func_globals->var_mangling;
  var_node_new->name = for_stmt->var_name;
  var_node_new->next = *var_list;
  *var_list = var_node_new;

  buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$loop\n", func_globals->function_name, local_for);
  buffer_printf(&func_globals->function_body, "LT %s$for$%zu$check %s$for$%zu$step int@0\n", func_globals->var_prefix, local_for, func_globals->var_prefix, local_for);
  buffer_printf(&func_globals->function_body, "JUMPIFEQ %s$for$%zu$negative %s$for$%zu$check bool@true\n", func_globals->function_name, local_for, func_globals->var_prefix, local_for);

  if (for_stmt->is_exclusive) {
    buffer_printf(&func_globals->function_body, "LT %s$for$%zu$check %s%s$%zu %s$for$%zu$to\n", func_globals->var_prefix, local_for, func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "JUMPIFEQ %s$for$%zu$end %s$for$%zu$check bool@false\n", func_globals->function_name, local_for, func_globals->var_prefix, local_for);
  } else {
    buffer_printf(&func_globals->function_body, "GT %s$for$%zu$check %s%s$%zu %s$for$%zu$to\n", func_globals->var_prefix, local_for, func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "JUMPIFEQ %s$for$%zu$end %s$for$%zu$check bool@true\n", func_globals->function_name, local_for, func_globals->var_prefix, local_for);
  }
  buffer_printf(&func_globals->function_body, "JUMP %s$for$%zu$body\n", func_globals->function_name, local_for);

  buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$negative\n", func_globals->function_name, local_for);
  if (for_stmt->is_exclusive) {
    buffer_printf(&func_globals->function_body, "GT %s$for$%zu$check %s%s$%zu %s$for$%zu$to\n", func_globals->var_prefix, local_for, func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "JUMPIFEQ %s$for$%zu$end %s$for$%zu$check bool@false\n", func_globals->function_name, local_for, func_globals->var_prefix, local_for);
  } else {
    buffer_printf(&func_globals->function_body, "LT %s$for$%zu$check %s%s$%zu %s$for$%zu$to\n", func_globals->var_prefix, local_for, func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, local_for);
    buffer_printf(&func_globals->function_body, "JUMPIFEQ %s$for$%zu$end %s$for$%zu$check bool@true\n", func_globals->function_name, local_for, func_globals->var_prefix, local_for);
  }

  buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$body\n", func_globals->function_name, local_for);
  for (size_t i = 0; i < for_stmt->guard_count; i++) {
    AST_EXPRESSION *guard = for_stmt->guards[i];
    if (guard == NULL) {
      return 1;
    }
    if (generate_expression(guard, func_globals, var_list, builtins) != 0) {
      return 1;
    }
    buffer_printf(&func_globals->function_body, "PUSHS bool@false\n");
    buffer_printf(&func_globals->function_body, "JUMPIFEQS %s$for$%zu$skip\n", func_globals->function_name, local_for);
  }

  size_t pre_body_mangling = func_globals->var_mangling;
  func_globals->var_mangling_counter++;
  func_globals->var_mangling = func_globals->var_mangling_counter;
  for (size_t i = 0; i < for_stmt->body->statement_count; i++) {
    AST_STATEMENT *statement = for_stmt->body->statements[i];
    if (statement == NULL) {
      return 1;
    }
    if (generate_statement(statement, func_globals, var_list, builtins) != 0) {
      return 1;
    }
  }

  buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$skip\n", func_globals->function_name, local_for);

  func_globals->var_mangling = pre_body_mangling;
  buffer_printf(&func_globals->function_body, "ADD %s%s$%zu %s%s$%zu %s$for$%zu$step\n", func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, for_stmt->var_name, func_globals->var_mangling, func_globals->var_prefix, local_for);
  buffer_printf(&func_globals->function_body, "JUMP %s$for$%zu$loop\n", func_globals->function_name, local_for);

  if (for_stmt->step != NULL) {
    buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$error\n", func_globals->function_name, local_for);
    buffer_printf(&func_globals->function_body, "EXIT int@40\n");
  }
  buffer_printf(&func_globals->function_body, "LABEL %s$for$%zu$end\n", func_globals->function_name, local_for);

  func_globals->var_mangling = prev_mangling;

  while (*var_list != NULL && (*var_list)->mangling > prev_mangling) {
    var_list_node *var_node_delete = *var_list;
    *var_list = (*var_list)->next;
    free(var_node_delete);
  }

  return 0;
}

static size_t find_variable(var_list_node *var_list, const char *name) {
  var_list_node *var_node = var_list;
  while (var_node != NULL) {
    if (strcmp(name, var_node->name) == 0) {
      return var_node->mangling;
    }
    var_node = var_node->next;
  }
  return SIZE_MAX;
}

static char* convert_string(const char *string, size_t length) {
  if (string == NULL || length > (SIZE_MAX - 1) / 4) {
    return NULL;
  }
  size_t capacity = length == 0 ? 5 : length * 4 + 1;
  char *converted = malloc(capacity);
  if (converted == NULL) {
    return NULL;
  }
  size_t conv_len = 0;
  if (length == 0) {
    if (string[0] == '\0') {
      memcpy(converted, "", 0);
      conv_len = 0;
    }
  }

  for (size_t i = 0; i < length; i++) {
    unsigned char character = (unsigned char)string[i];
    if (character <= 32 || character == 35 || character == 92) {
      converted[conv_len++] = '\\';
      converted[conv_len++] = '0' + character / 100;
      converted[conv_len++] = '0' + (character / 10) % 10;
      converted[conv_len++] = '0' + character % 10;
    } else {
      converted[conv_len++] = (char)character;
    }
  }
  converted[conv_len] = '\0';
  return converted;
}

static int builtin_print() {
  printf("LABEL !print\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@output\n");
  printf("POPS LF@output\n");

  printf("JUMPIFNEQ !print$not$unit LF@output nil@nil\n");
  printf("MOVE LF@output string@()\n");

  printf("LABEL !print$not$unit\n");
  printf("WRITE LF@output\n");

  printf("PUSH nil@nil\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_readLine() {
  printf("LABEL !readLine\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@input\n");
  printf("READ LF@input string\n");
  printf("PUSHS LF@input\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_readInt() {
  printf("LABEL !readInt\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@input\n");
  printf("READ LF@input int\n");
  printf("PUSHS LF@input\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_readDouble() {
  printf("LABEL !readDouble\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@input\n");
  printf("READ LF@input float\n");
  printf("PUSHS LF@input\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_readBoolean() {
  printf("LABEL !readBoolean\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@input\n");
  printf("READ LF@input bool\n");
  printf("PUSHS LF@input\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_toStr() {
  printf("LABEL !toStr\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@str\n");
  printf("DEFVAR LF@val\n");
  printf("POPS LF@val\n");
  printf("DEFVAR LF@type\n");

  printf("TYPE LF@type LF@val\n");
  printf("JUMPIFEQ !toStr$string LF@type string@string\n");
  printf("JUMPIFEQ !toStr$int LF@type string@int\n");
  printf("JUMPIFEQ !toStr$float LF@type string@float\n");
  printf("JUMPIFEQ !toStr$bool LF@type string@bool\n");
  printf("JUMPIFEQ !toStr$nil LF@type string@nil\n");

  printf("LABEL !toStr$string\n");
  printf("MOVE LF@str LF@val\n");
  printf("JUMP !toStr$end\n");

  printf("LABEL !toStr$int\n");
  printf("INT2STR LF@str LF@val\n");
  printf("JUMP !toStr$end\n");

  printf("LABEL !toStr$float\n");
  printf("FLOAT2STR LF@str LF@val\n");
  printf("JUMP !toStr$end\n");

  printf("LABEL !toStr$bool\n");
  printf("JUMPIFEQ !toStr$bool$true LF@val bool@true\n");
  printf("MOVE LF@str string@false\n");
  printf("JUMP !toStr$end\n");
  printf("LABEL !toStr$bool$true\n");
  printf("MOVE LF@str string@true\n");
  printf("JUMP !toStr$end\n");

  printf("LABEL !toStr$nil\n");
  printf("MOVE LF@str string@()\n");
  printf("JUMP !toStr$end\n");

  printf("LABEL !toStr$end\n");
  printf("PUSHS LF@str\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_strLength() {
  printf("LABEL !strLength\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@str\n");
  printf("POPS LF@str\n");
  printf("DEFVAR LF@len\n");

  printf("STRLEN LF@len LF@str\n");
  printf("PUSHS LF@len\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_subStr() {
  printf("LABEL !subStr\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@end\n");
  printf("POPS LF@end\n");
  printf("DEFVAR LF@start\n");
  printf("POPS LF@start\n");
  printf("DEFVAR LF@val\n");
  printf("POPS LF@val\n");
  printf("DEFVAR LF@check\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@str\n");
  printf("MOVE LF@str string@\n");
  printf("DEFVAR LF@char\n");
  printf("DEFVAR LF@index\n");
  printf("MOVE LF@index int@0\n");

  printf("LT LF@check LF@start int@0\n");
  printf("JUMPIFEQ !subStr$error LF@check bool@true\n");
  printf("LT LF@check LF@end int@0\n");
  printf("JUMPIFEQ !subStr$error LF@check bool@true\n");
  printf("GT LF@check LF@start LF@end\n");
  printf("JUMPIFEQ !subStr$error LF@check bool@true\n");
  printf("STRLEN LF@len LF@val\n");
  printf("LT LF@check LF@start LF@len\n");
  printf("PUSHS LF@check\n");
  printf("NOTS\n");
  printf("POPS LF@check\n");
  printf("JUMPIFEQ !subStr$error LF@check bool@true\n");
  printf("GT LF@check LF@end LF@len\n");
  printf("JUMPIFEQ !subStr$error LF@check bool@true\n");

  printf("LABEL !subStr$loop\n");
  printf("LT LF@check LF@start LF@end\n");
  printf("JUMPIFEQ !subStr$end LF@check bool@false\n");
  printf("GETCHAR LF@char LF@val LF@start\n");
  printf("CONCAT LF@str LF@str LF@char\n");
  printf("ADD LF@start LF@start int@1\n");
  printf("ADD LF@index LF@index int@1\n");
  printf("JUMP !subStr$loop\n");

  printf("LABEL !subStr$error\n");
  printf("EXIT int@40\n");
  printf("LABEL !subStr$end\n");
  printf("PUSHS LF@str\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

static int builtin_ord() {
  printf("LABEL !ord\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");

  printf("DEFVAR LF@index\n");
  printf("POPS LF@index\n");
  printf("DEFVAR LF@str\n");
  printf("POPS LF@str\n");
  printf("DEFVAR LF@code\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@check\n");

  printf("STRLEN LF@len LF@str\n");
  printf("JUMPIFEQ !ord$error LF@len int@0\n");
  
  printf("LT LF@check LF@index LF@len\n");
  printf("JUMPIFEQ !ord$error LF@check bool@false\n");

  printf("LT LF@check LF@index int@0\n");
  printf("JUMPIFEQ !ord$error LF@check bool@true\n");

  printf("STRI2INT LF@code LF@str LF@index\n");
  printf("JUMP !ord$end\n");

  printf("LABEL !ord$error\n");
  printf("MOVE LF@code int@-1\n");

  printf("LABEL !ord$end\n");
  printf("PUSHS LF@code\n");

  printf("POPFRAME\n");
  printf("RETURN\n");
  printf("\n");
  return 0;
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

int generate_code(AST_PROGRAM *program) {
  if (program == NULL) {
    return 1;
  }

  printf(".IFJcode26\n\n");
  printf("JUMP $main\n\n");

  builtins_checklist *builtins = malloc(sizeof(builtins_checklist));
  if (builtins == NULL) {
    return 1;
  }
  builtins->print = false;
  builtins->readLine = false;
  builtins->readInt = false;
  builtins->readDouble = false;
  builtins->readBoolean = false;
  builtins->toStr = false;
  builtins->strLength= false;
  builtins->subStr = false;
  builtins->ord = false;

  for (size_t i = 0; i < program->function_count; i++) {
    AST_FUNCTION_DEF *function = program->functions[i];
    if (function == NULL) {
      free(builtins);
      return 1;
    }
    if (generate_function(function, builtins) != 0) {
      free(builtins);
      return 1;
    }
    printf("\n");
  }

  printf("\n");

  if (builtins->print) {
    if (builtin_print() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->readLine) {
    if (builtin_readLine() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->readInt) {
    if (builtin_readInt() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->readDouble) {
    if (builtin_readDouble() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->readBoolean) {
    if (builtin_readBoolean() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->toStr) {
    if (builtin_toStr() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->strLength) {
    if (builtin_strLength() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->subStr) {
    if (builtin_subStr() != 0) {
      free(builtins);
      return 1;
    }
  }
  if (builtins->ord) {
    if (builtin_ord() != 0) {
      free(builtins);
      return 1;
    }
  }
  free(builtins);
  return 0;
}
