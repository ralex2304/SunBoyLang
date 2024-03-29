#include "text_parser.h"

#include "text_parser_dsl.h"

static Status::Statuses CH_Def_var_(ParseData* data, size_t* const pos, TreeNode** dest,
                                    size_t* const size, size_t var_num, DebugInfo* var_debug_info,
                                    DebugInfo* assign_debug_info);

static Status::Statuses CH_Def_array_(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size, size_t var_num, TreeNode* array_size_expr,
                                      DebugInfo* var_debug_info, DebugInfo* assign_debug_info);

static Status::Statuses CH_Def_func_(ParseData* data, size_t* const pos, TreeNode** dest,
                                     size_t* const size, size_t var_num, DebugInfo* var_debug_info);


Status::Statuses parse_text(ParseData* data) {
    assert(data);

    size_t pos = 0;
    size_t size = 0;
    TreeNode* result = nullptr;

    STATUS_CHECK(TextParser::Main(data, &pos, &result, &size), tree_dtor_untied_subtree(&result));
    assert(result);

    data->tree.root = result;
    data->tree.size = size;

    return Status::NORMAL_WORK;
}

using namespace TextParser;

Status::Statuses TextParser::Main(ParseData* data, size_t* const pos, TreeNode** dest,
                                  size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    while ((ssize_t)(*pos) < data->tokens.size() && !IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::TERMINATOR)) {

        if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
            (*pos)++;

        TreeNode* new_node = nullptr;
        STATUS_CHECK(CH_DefFunc(data, pos, &new_node, size),
                                        tree_dtor_untied_subtree(&new_node));
        if (new_node == nullptr) {
            STATUS_CHECK(CH_DefVar(data, pos, &new_node, size),
                                        tree_dtor_untied_subtree(&new_node));

            if (new_node == nullptr)
                break;
        }

        STATUS_CHECK(new_oper_node(dest, OperNum::CMD_SEPARATOR, DEBUG_INFO(CUR_TOKEN),
                                   new_node, nullptr),
                                        tree_dtor_untied_subtree(&new_node));
        (*size)++;

        if (parent)
            (*dest)->parent = parent;

        parent = *dest;
        dest = R(*dest);
    }


    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::TERMINATOR))
        return Status::NORMAL_WORK;

    STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Unexpected expression"));

    return Status::SYNTAX_ERROR;
}

Status::Statuses TextParser::CH_DefFunc(ParseData* data, size_t* const pos, TreeNode** dest,
                                        size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR))
        return Status::NORMAL_WORK;

    size_t var_num = CUR_TOKEN.data.var;
    DebugInfo var_debug_info = DEBUG_INFO(CUR_TOKEN);

    TreeNode* parent = nullptr;

    if ((IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::VAR) ||
         IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::CONST)) && ND_TOKEN_EXISTS(2) &&
        IS_TOKEN_TERM_EQ(ND_TOKEN(2), TerminalNum::OPEN_BRACE))
        *pos += 2;
    else
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_Def_func_(data, pos, dest, size, var_num, &var_debug_info));

    if (parent != nullptr)
        (*dest)->parent = parent;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_DefVar(ParseData* data, size_t* const pos, TreeNode** dest,
                                       size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR)) {
        return Status::NORMAL_WORK;
    }

    size_t var_num = CUR_TOKEN.data.var;
    DebugInfo var_debug_info = DEBUG_INFO(CUR_TOKEN);

    TreeNode* parent = nullptr;

    if (IS_TOKEN_TERM_EQ(NEXT_TOKEN,  TerminalNum::VAR) && ND_TOKEN_EXISTS(2) &&
        IS_TOKEN_TERM_EQ(ND_TOKEN(2), TerminalNum::CONST)) {

        STATUS_CHECK(new_oper_node(dest, OperNum::CONST_VAR_DEF, DEBUG_INFO(NEXT_TOKEN),
                                   nullptr, nullptr));
        (*size)++;
        *pos += 3;

        parent = *dest;
        dest = R(*dest);
    } else if (IS_TOKEN_TERM_EQ(NEXT_TOKEN,  TerminalNum::VAR)) {
        *pos += 2;
    }else
        return Status::NORMAL_WORK;

    TreeNode* array_size_expr = nullptr;
    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_INDEX_BRACE)) {
        (*pos)++;

        STATUS_CHECK(Expr(data, pos, &array_size_expr, size));
        assert(array_size_expr);

        if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_INDEX_BRACE)) {
            STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected index brace closing"),
                                                        tree_dtor_untied_subtree(&array_size_expr));
            tree_dtor_untied_subtree(&array_size_expr);
            return Status::SYNTAX_ERROR;
        }
        (*pos)++;
    }

    if (array_size_expr != nullptr)
        STATUS_CHECK(CH_Def_array_(data, pos, dest, size, var_num, array_size_expr,
                                    &var_debug_info,
                                    &DEBUG_INFO(ND_TOKEN(-1))));
    else
        STATUS_CHECK(CH_Def_var_(data, pos, dest, size, var_num, &var_debug_info,
                                    &DEBUG_INFO(ND_TOKEN(-1))));

    if (parent != nullptr)
        (*dest)->parent = parent;

    return Status::NORMAL_WORK;
}

static Status::Statuses CH_Def_var_(ParseData* data, size_t* const pos, TreeNode** dest,
                                    size_t* const size, size_t var_num, DebugInfo* var_debug_info,
                                    DebugInfo* assign_debug_info) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);
    assert(var_debug_info);
    assert(assign_debug_info);

    TreeNode* var_definition = nullptr;
    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::ASSIGNMENT)) {
        (*pos)++;

        STATUS_CHECK(Expr(data, pos, &var_definition, size));
        assert(var_definition);
    }

    TreeNode* new_var = nullptr;
    STATUS_CHECK(new_var_node(&new_var, var_num, *var_debug_info),
                                                    tree_dtor_untied_subtree(&var_definition));
    (*size)++;

    STATUS_CHECK(new_oper_node(dest, OperNum::VAR_DEFINITION, *assign_debug_info,
                               new_var, var_definition),
                                                    tree_dtor_untied_subtree(&var_definition);
                                                    tree_dtor_untied_subtree(&new_var));
    (*size)++;

    return Status::NORMAL_WORK;
}

static Status::Statuses CH_Def_array_(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size, size_t var_num, TreeNode* array_size_expr,
                                      DebugInfo* var_debug_info, DebugInfo* assign_debug_info) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);
    assert(array_size_expr);
    assert(var_debug_info);
    assert(assign_debug_info);

    TreeNode* definitions = nullptr;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::ASSIGNMENT)) {
        (*pos)++;

        STATUS_CHECK(new_oper_node(&definitions, OperNum::VAR_SEPARATOR, DEBUG_INFO(CUR_TOKEN),
                                   nullptr, nullptr));
        (*size)++;

        TreeNode* cur_separator = definitions;

        do {
            TreeNode* elem_defenition = nullptr;
            STATUS_CHECK(Expr(data, pos, &elem_defenition, size),
                                                        tree_dtor_untied_subtree(&definitions));
            assert(elem_defenition);

            *L(cur_separator) = elem_defenition;
            elem_defenition->parent = cur_separator;

            STATUS_CHECK(new_oper_node(R(cur_separator), OperNum::VAR_SEPARATOR, DEBUG_INFO(CUR_TOKEN),
                        nullptr, nullptr),                tree_dtor_untied_subtree(&definitions));
            (*size)++;
            (*R(cur_separator))->parent = cur_separator;

            cur_separator = *R(cur_separator);

        } while (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::VAR_SEPARATOR) && ++(*pos));
    }

    TreeNode* new_var = nullptr;
    STATUS_CHECK(new_var_node(&new_var, var_num, *var_debug_info),
                                                        tree_dtor_untied_subtree(&definitions));
    (*size)++;

    TreeNode* name_and_size = nullptr;
    STATUS_CHECK(new_oper_node(&name_and_size, OperNum::VAR_SEPARATOR, *var_debug_info,
                               new_var, array_size_expr),
                                                        tree_dtor_untied_subtree(&definitions);
                                                        tree_dtor_untied_subtree(&new_var));
    (*size)++;

    STATUS_CHECK(new_oper_node(dest, OperNum::ARRAY_DEFINITION, *assign_debug_info,
                               name_and_size, definitions),
                                                        tree_dtor_untied_subtree(&definitions);
                                                        tree_dtor_untied_subtree(&new_var);
                                                        tree_dtor_untied_subtree(&name_and_size));
    (*size)++;

    return Status::NORMAL_WORK;
}

static Status::Statuses CH_Def_func_(ParseData* data, size_t* const pos, TreeNode** dest,
                                     size_t* const size, size_t var_num, DebugInfo* var_debug_info) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);
    assert(var_debug_info);

    (*pos)++;

    TreeNode* args = nullptr;
    STATUS_CHECK(FuncArgsDef(data, pos, &args, size),
                                                    tree_dtor_untied_subtree(&args));

    TreeNode* func_name = nullptr;
    STATUS_CHECK(new_var_node(&func_name, var_num, *var_debug_info),
                                                    tree_dtor_untied_subtree(&args));
    (*size)++;

    TreeNode* left_subtree = nullptr;
    STATUS_CHECK(new_oper_node(&left_subtree, OperNum::VAR_SEPARATOR, DEBUG_INFO(CUR_TOKEN),
                               func_name, args),    tree_dtor_untied_subtree(&func_name);
                                                    tree_dtor_untied_subtree(&args));
    (*size)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
        tree_dtor_untied_subtree(&left_subtree);
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected separator"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
        (*pos)++;

    TreeNode* commands = nullptr;
    STATUS_CHECK(CH_Commands(data, pos, &commands, size),
                                                        tree_dtor_untied_subtree(&left_subtree);
                                                        tree_dtor_untied_subtree(&commands));

    STATUS_CHECK(new_oper_node(dest, OperNum::FUNC_DEFINITION, DEBUG_INFO(CUR_TOKEN),
                                left_subtree, commands), tree_dtor_untied_subtree(&left_subtree);
                                                        tree_dtor_untied_subtree(&commands));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::FuncArgsDef(ParseData* data, size_t* const pos, TreeNode** dest,
                                         size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR))
        return Status::NORMAL_WORK;

    TreeNode* parent = nullptr;

    do {
        if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR)) {
            STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected var or function name"));
            return Status::SYNTAX_ERROR;
        }
        DebugInfo def_debug_info = DEBUG_INFO(CUR_TOKEN);
        size_t var_num = CUR_TOKEN.data.var;
        (*pos)++;

        if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::VAR)) {
            STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected var or function declaration"));
            return Status::SYNTAX_ERROR;
        }

        TreeNode* var_node = nullptr;
        STATUS_CHECK(new_var_node(&var_node, var_num, DEBUG_INFO(CUR_TOKEN)));
        (*size)++;
        (*pos)++;

        TreeNode* var_def = nullptr;
        STATUS_CHECK(new_oper_node(&var_def, OperNum::VAR_DEFINITION, def_debug_info, nullptr, var_node),
                                                            tree_dtor_untied_subtree(&var_node));
        (*size)++;

        if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CONST)) {
            TreeNode* var_def_tmp = var_def;
            var_def = nullptr;
            STATUS_CHECK(new_oper_node(&var_def, OperNum::CONST_VAR_DEF, def_debug_info,
                                        nullptr, var_def_tmp),
                                                            tree_dtor_untied_subtree(&var_def));
            (*size)++;
            (*pos)++;
        }

        STATUS_CHECK(new_oper_node(dest, OperNum::VAR_SEPARATOR, def_debug_info, var_def, nullptr),
                                                            tree_dtor_untied_subtree(&var_node));
        (*size)++;

        if (parent != nullptr)
            (*dest)->parent = parent;

        parent = *dest;

        dest = R(*dest);

    } while (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::VAR_SEPARATOR) && ++(*pos));

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_Commands(ParseData* data, size_t* const pos, TreeNode** dest,
                                         size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_SCOPE))
        return Status::NORMAL_WORK;
    (*pos)++;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
        (*pos)++;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_SCOPE)) {
        (*pos)++;
        return Status::NORMAL_WORK;
    }

    TreeNode* parent = nullptr;

    do {
        DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);

        TreeNode* command = nullptr;
        STATUS_CHECK(Command(data, pos, &command, size),    tree_dtor_untied_subtree(&command));
        assert(command);

        STATUS_CHECK(new_oper_node(dest, OperNum::CMD_SEPARATOR, debug_info, command, nullptr),
                                                            tree_dtor_untied_subtree(&command));
        (*size)++;

        if (parent != nullptr)
            (*dest)->parent = parent;

        parent = *dest;

        dest = R(*dest);

    } while (IS_TOKEN_TERM_EQ(CUR_TOKEN,  TerminalNum::CMD_SEPARATOR) &&
            !IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::CLOSE_SCOPE)   && ++(*pos));

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
        (*pos)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_SCOPE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected scope closing"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_FuncCall(ParseData* data, size_t* const pos, TreeNode** dest,
                                         size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR))
        return Status::NORMAL_WORK;
    size_t var_num = CUR_TOKEN.data.var;
    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);

    if (!IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::OPEN_BRACE))
        return Status::NORMAL_WORK;
    *pos += 2;

    TreeNode* func_name = nullptr;
    STATUS_CHECK(new_var_node(&func_name, var_num, DEBUG_INFO(CUR_TOKEN)));
    (*size)++;

    TreeNode* func_args = nullptr;
    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {

        STATUS_CHECK(FuncArgs(data, pos, &func_args, size), tree_dtor_untied_subtree(&func_name);
                                                            tree_dtor_untied_subtree(&func_args));
    }

    STATUS_CHECK(new_oper_node(dest, OperNum::FUNC_CALL, debug_info, func_name, func_args),
                                                            tree_dtor_untied_subtree(&func_name);
                                                            tree_dtor_untied_subtree(&func_args));
    (*size)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected closing brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::FuncArgs(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    do {
        DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);

        TreeNode* expr = nullptr;
        STATUS_CHECK(Expr(data, pos, &expr, size));
        assert(expr);

        STATUS_CHECK(new_oper_node(dest, OperNum::VAR_SEPARATOR, debug_info, expr, nullptr));
        (*size)++;

        if (parent != nullptr)
            (*dest)->parent = parent;

        parent = *dest;

        dest = R(*dest);

    } while (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::VAR_SEPARATOR) && ++(*pos));

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Command(ParseData* data, size_t* const pos, TreeNode** dest,
                                     size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    STATUS_CHECK(CH_Commands(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_DefVar(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_CommandWithArg(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_ComplexCommand(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_CommandWithConstArg(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(SimpleCommand(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected command or commands"));
    return Status::SYNTAX_ERROR;
}

Status::Statuses TextParser::SimpleCommand(ParseData* data, size_t* const pos, TreeNode** dest,
                                           size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::BREAK)) {
        STATUS_CHECK(new_oper_node(dest, OperNum::BREAK, DEBUG_INFO(CUR_TOKEN), nullptr, nullptr));

    } else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CONTINUE)) {
        STATUS_CHECK(new_oper_node(dest, OperNum::CONTINUE, DEBUG_INFO(CUR_TOKEN), nullptr, nullptr));

    } else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::SHOW)) {
        STATUS_CHECK(new_oper_node(dest, OperNum::SHOW, DEBUG_INFO(CUR_TOKEN), nullptr, nullptr));

    } else {
        STATUS_CHECK(Expr(data, pos, dest, size));
        assert(*dest);

        return Status::NORMAL_WORK;
    }
    (*size)++;
    (*pos)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_ComplexCommand(ParseData* data, size_t* const pos, TreeNode** dest,
                                               size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    STATUS_CHECK(CH_SimpleClause(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_PostClause(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Clause(ParseData* data, size_t* const pos, TreeNode** dest,
                                    size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_BRACE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected opening brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    STATUS_CHECK(Expr(data, pos, dest, size));
    assert(*dest);

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected closing brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::ClauseAction(ParseData* data, size_t* const pos, TreeNode** dest,
                                          size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
        (*pos)++;

    STATUS_CHECK(CH_Commands(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(Command(data, pos, dest, size));
    if (*dest != nullptr)
        return Status::NORMAL_WORK;

    STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected command or commands"));
    return Status::SYNTAX_ERROR;
}

Status::Statuses TextParser::CH_SimpleClause(ParseData* data, size_t* const pos, TreeNode** dest,
                                             size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    OperNum oper = OperNum::NONE;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::WHILE))
        oper = OperNum::WHILE;
    else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::IF))
        oper = OperNum::IF;
    else
        return Status::NORMAL_WORK;

    DebugInfo cmd_debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    TreeNode* clause = nullptr;
    STATUS_CHECK(Clause(data, pos, &clause, size),              tree_dtor_untied_subtree(&clause));

    TreeNode* clause_action = nullptr;
    STATUS_CHECK(ClauseAction(data, pos, &clause_action, size), tree_dtor_untied_subtree(&clause);
                                                                tree_dtor_untied_subtree(&clause_action));

    size_t tmp_pos = *pos;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CMD_SEPARATOR))
        (*pos)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::ELSE)) {
        *pos = tmp_pos;
    } else {
        DebugInfo else_debug_info = DEBUG_INFO(CUR_TOKEN);
        (*pos)++;

        TreeNode* else_action = nullptr;
        STATUS_CHECK(ClauseAction(data, pos, &else_action, size),
                                                                tree_dtor_untied_subtree(&clause);
                                                                tree_dtor_untied_subtree(&clause_action);
                                                                tree_dtor_untied_subtree(&else_action));

        TreeNode* else_node = nullptr;
        STATUS_CHECK(new_oper_node(&else_node, OperNum::ELSE, else_debug_info,
                                   clause_action, else_action), tree_dtor_untied_subtree(&clause);
                                                                tree_dtor_untied_subtree(&clause_action);
                                                                tree_dtor_untied_subtree(&else_action));
        (*size)++;

        clause_action = else_node;
    }

    STATUS_CHECK(new_oper_node(dest, oper, cmd_debug_info, clause, clause_action),
                                                                tree_dtor_untied_subtree(&clause);
                                                                tree_dtor_untied_subtree(&clause_action));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_PostClause(ParseData* data, size_t* const pos, TreeNode** dest,
                                           size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    OperNum oper = OperNum::NONE;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::DO_WHILE_DO))
        oper = OperNum::DO_WHILE;
    else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::DO_IF_DO))
        oper = OperNum::DO_IF;
    else
        return Status::NORMAL_WORK;

    DebugInfo cmd_debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    TreeNode* clause_action = nullptr;
    STATUS_CHECK(ClauseAction(data, pos, &clause_action, size), tree_dtor_untied_subtree(&clause_action));

    if (!((oper == OperNum::DO_WHILE && IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::DO_WHILE_WHILE)) ||
          (oper == OperNum::DO_IF    && IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::DO_IF_IF)))) {

        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected end of prefix-clause expression"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    TreeNode* clause = nullptr;
    STATUS_CHECK(Clause(data, pos, &clause, size),              tree_dtor_untied_subtree(&clause_action);
                                                                tree_dtor_untied_subtree(&clause));

    STATUS_CHECK(new_oper_node(dest, oper, cmd_debug_info, clause, clause_action),
                                                                tree_dtor_untied_subtree(&clause_action);
                                                                tree_dtor_untied_subtree(&clause));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_CommandWithArg(ParseData* data, size_t* const pos, TreeNode** dest,
                                               size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    OperNum oper = OperNum::NONE;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::RETURN))
        oper = OperNum::RETURN;
    else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OUT))
        oper = OperNum::OUT;
    else
        return Status::NORMAL_WORK;

    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    TreeNode* expr = nullptr;
    STATUS_CHECK(Expr(data, pos, &expr, size),                  tree_dtor_untied_subtree(&expr));
    assert(expr);

    STATUS_CHECK(new_oper_node(dest, oper, debug_info, nullptr, expr),
                                                                tree_dtor_untied_subtree(&expr));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_CommandWithConstArg(ParseData* data, size_t* const pos, TreeNode** dest,
                                                    size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::SET_FPS))
        return Status::NORMAL_WORK;

    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    TreeNode* num = nullptr;
    STATUS_CHECK(Double(data, pos, &num, size),                 tree_dtor_untied_subtree(&num));
    assert(num);

    STATUS_CHECK(new_oper_node(dest, OperNum::SET_FPS, debug_info, nullptr, num),
                                                                tree_dtor_untied_subtree(&num));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Expr(ParseData* data, size_t* const pos, TreeNode** dest,
                                  size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    while (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR)) { //< while is used here for break. Mustn't iterate
        size_t var_num = CUR_TOKEN.data.var;
        DebugInfo var_debug_info = DEBUG_INFO(CUR_TOKEN);
        const size_t begin_pos = (*pos)++;
        const size_t begin_size = *size;

        TreeNode* arr_index_expr = nullptr;
        if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_INDEX_BRACE)) {
            (*pos)++;

            STATUS_CHECK(Expr(data, pos, &arr_index_expr, size));

            if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_INDEX_BRACE)) {
                STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected index brace closing"),
                                                            tree_dtor_untied_subtree(&arr_index_expr));
                tree_dtor_untied_subtree(&arr_index_expr);
                *size = begin_size;
                return Status::SYNTAX_ERROR;
            }
            (*pos)++;
        }

        OperNum oper = OperNum::NONE;
        DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);

        if (IS_TOKEN_TERM_EQ(CUR_TOKEN,  TerminalNum::ASSIGNMENT) &&
           !IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {

            oper = OperNum::ASSIGNMENT;
            (*pos)++;
        } else if ((ssize_t)(*pos) + 1 < data->tokens.size() &&
                   IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
            if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM))
                switch (CUR_TOKEN.data.term) {
                    case TerminalNum::MATH_ADD:
                        oper = OperNum::ASSIGNMENT_ADD;
                        break;
                    case TerminalNum::MATH_SUB:
                        oper = OperNum::ASSIGNMENT_SUB;
                        break;
                    case TerminalNum::MATH_MUL:
                        oper = OperNum::ASSIGNMENT_MUL;
                        break;
                    case TerminalNum::MATH_DIV:
                        oper = OperNum::ASSIGNMENT_DIV;
                        break;
                    default:
                        oper = OperNum::NONE;
                        break;
                }
#pragma GCC diagnostic pop

            if (oper == OperNum::NONE) {
                *pos = begin_pos;
                *size = begin_size;
                tree_dtor_untied_subtree(&arr_index_expr);
                break;
            }

            (*pos) += 2;
        } else {
            *pos = begin_pos;
            *size = begin_size;
            tree_dtor_untied_subtree(&arr_index_expr);
            break;
        }

        TreeNode* expr = nullptr;
        STATUS_CHECK(Expr(data, pos, &expr, size),      tree_dtor_untied_subtree(&expr);
                                                        tree_dtor_untied_subtree(&arr_index_expr));

        TreeNode* var_node = nullptr;
        STATUS_CHECK(new_var_node(&var_node, var_num, var_debug_info),
                                                        tree_dtor_untied_subtree(&expr);
                                                        tree_dtor_untied_subtree(&var_node);
                                                        tree_dtor_untied_subtree(&arr_index_expr));
        (*size)++;

        if (arr_index_expr != nullptr) {
            TreeNode* var_node_tmp = var_node;
            var_node = nullptr;

            STATUS_CHECK(new_oper_node(&var_node, OperNum::ARRAY_ELEM, var_debug_info,
                                       var_node_tmp, arr_index_expr),
                                                        tree_dtor_untied_subtree(&expr);
                                                        tree_dtor_untied_subtree(&var_node);
                                                        tree_dtor_untied_subtree(&arr_index_expr));
            (*size)++;
        }

        STATUS_CHECK(new_oper_node(dest, oper, debug_info, var_node, expr),
                                                        tree_dtor_untied_subtree(&expr);
                                                        tree_dtor_untied_subtree(&var_node));
        (*size)++;
        return Status::NORMAL_WORK;

        assert(0 && "While mustn't iterate");
    }

    STATUS_CHECK(MathLvl1(data, pos, dest, size));

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::MathLvl1(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    do {
        STATUS_CHECK(MathLvl2(data, pos, dest, size));
        assert(*dest);

        if (parent)
            (*dest)->parent = parent;

        DebugInfo oper_debug_info = DEBUG_INFO(CUR_TOKEN);

        OperNum oper = OperNum::NONE;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM))
            switch (CUR_TOKEN.data.term) {
                case TerminalNum::ASSIGNMENT:
                    if (IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {
                        oper = OperNum::LOGIC_EQUAL;
                        *pos += 2;
                    }
                    break;
                case TerminalNum::LOGIC_NOT:
                    if (IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {
                        oper = OperNum::LOGIC_NOT_EQUAL;
                        *pos += 2;
                    }
                    break;
                case TerminalNum::LOGIC_LOWER:
                    if (IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {
                        oper = OperNum::LOGIC_LOWER_EQ;
                        *pos += 2;
                    } else {
                        oper = OperNum::LOGIC_LOWER;
                        (*pos)++;
                    }
                    break;
                case TerminalNum::LOGIC_GREATER:
                    if (IS_TOKEN_TERM_EQ(NEXT_TOKEN, TerminalNum::ASSIGNMENT)) {
                        oper = OperNum::LOGIC_GREAT_EQ;
                        *pos += 2;
                    } else {
                        oper = OperNum::LOGIC_GREAT;
                        (*pos)++;
                    }
                    break;

                default:
                    oper = OperNum::NONE;
                    break;
                }
#pragma GCC diagnostic pop

        if (oper == OperNum::NONE)
            break;


        TreeNode* oper_node = nullptr;
        STATUS_CHECK(new_oper_node(&oper_node, oper, oper_debug_info, *dest, nullptr),
                                                                tree_dtor_untied_subtree(&oper_node));
        (*size)++;

        if (parent)
            oper_node->parent = parent;

        *dest = oper_node;

        parent = *dest;
        dest = R(*dest);

    } while (1);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::MathLvl2(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    do {
        STATUS_CHECK(MathLvl3(data, pos, dest, size));
        assert(*dest);

        if (parent)
            (*dest)->parent = parent;

        DebugInfo oper_debug_info = DEBUG_INFO(CUR_TOKEN);

        OperNum oper = OperNum::NONE;

        if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
            switch (CUR_TOKEN.data.term) {
                case TerminalNum::MATH_ADD:
                    oper = OperNum::MATH_ADD;
                    (*pos)++;
                    break;
                case TerminalNum::MATH_SUB:
                    oper = OperNum::MATH_SUB;
                    (*pos)++;
                    break;

                default:
                    oper = OperNum::NONE;
                    break;
            }
#pragma GCC diagnostic pop

        if (oper == OperNum::NONE)
            break;

        TreeNode* oper_node = nullptr;
        STATUS_CHECK(new_oper_node(&oper_node, oper, oper_debug_info, *dest, nullptr),
                                                                tree_dtor_untied_subtree(&oper_node));
        (*size)++;

        if (parent)
            oper_node->parent = parent;

        *dest = oper_node;

        parent = *dest;
        dest = R(*dest);

    } while (1);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::MathLvl3(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    do {
        STATUS_CHECK(MathLvl4(data, pos, dest, size));
        assert(*dest);

        if (parent)
            (*dest)->parent = parent;

        DebugInfo oper_debug_info = DEBUG_INFO(CUR_TOKEN);

        OperNum oper = OperNum::NONE;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM))
            switch (CUR_TOKEN.data.term) {
                case TerminalNum::MATH_MUL:
                    oper = OperNum::MATH_MUL;
                    (*pos)++;
                    break;
                case TerminalNum::MATH_DIV:
                    oper = OperNum::MATH_DIV;
                    (*pos)++;
                    break;

                default:
                    oper = OperNum::NONE;
                    break;
            }
#pragma GCC diagnostic pop

        if (oper == OperNum::NONE)
            break;

        TreeNode* oper_node = nullptr;
        STATUS_CHECK(new_oper_node(&oper_node, oper, oper_debug_info, *dest, nullptr),
                                                                tree_dtor_untied_subtree(&oper_node));
        (*size)++;

        if (parent)
            oper_node->parent = parent;

        *dest = oper_node;

        parent = *dest;
        dest = R(*dest);

    } while (1);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::MathLvl4(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    do {
        STATUS_CHECK(MathLvl5(data, pos, dest, size));
        assert(*dest);

        if (parent)
            (*dest)->parent = parent;

        DebugInfo oper_debug_info = DEBUG_INFO(CUR_TOKEN);

        if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM) &&
            CUR_TOKEN.data.term != TerminalNum::MATH_POW)
            break;

        (*pos)++;

        TreeNode* oper_node = nullptr;
        STATUS_CHECK(new_oper_node(&oper_node, OperNum::MATH_POW, oper_debug_info, *dest, nullptr),
                                                                tree_dtor_untied_subtree(&oper_node));
        (*size)++;

        if (parent)
            oper_node->parent = parent;

        *dest = oper_node;

        parent = *dest;
        dest = R(*dest);

    } while (1);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::MathLvl5(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    TreeNode* parent = nullptr;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN,  TerminalNum::MATH_SUB)) {

        STATUS_CHECK(new_oper_node(dest, OperNum::MATH_NEGATIVE, DEBUG_INFO(CUR_TOKEN), nullptr, nullptr));
        (*size)++;

        parent = *dest;
        dest = R(*dest);

        (*pos)++;

        STATUS_CHECK(MathLvl5(data, pos, dest, size));
        (*dest)->parent = parent;

        return Status::NORMAL_WORK;
    }

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_BRACE)) {
        (*pos)++;

        STATUS_CHECK(Expr(data, pos, dest, size));

        if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
            STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected closing brace"));
            return Status::SYNTAX_ERROR;
        }
        (*pos)++;

        if (parent)
            (*dest)->parent = parent;

        return Status::NORMAL_WORK;
    }

    STATUS_CHECK(CH_Binary(data, pos, dest, size));
    if (*dest) {
        if (parent)
            (*dest)->parent = parent;
        return Status::NORMAL_WORK;
    }

    STATUS_CHECK(CH_Unary(data, pos, dest, size));
    if (*dest) {
        if (parent)
            (*dest)->parent = parent;
        return Status::NORMAL_WORK;
    }

    STATUS_CHECK(Primary(data, pos, dest, size));
    assert(*dest);

    if (parent)
        (*dest)->parent = parent;
    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_Binary(ParseData* data, size_t* const pos, TreeNode** dest,
                                       size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::MATH_DIFF))
        return Status::NORMAL_WORK;
    (*pos)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_BRACE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected opening brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    TreeNode* arg1 = nullptr;
    STATUS_CHECK(Expr(data, pos, &arg1, size),      tree_dtor_untied_subtree(&arg1));

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::VAR_SEPARATOR)) {
        tree_dtor_untied_subtree(&arg1);

        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected next argument"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR)) {
        tree_dtor_untied_subtree(&arg1);

        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected var name"));
        return Status::SYNTAX_ERROR;
    }

    TreeNode* var_node = nullptr;
    STATUS_CHECK(new_var_node(&var_node, CUR_TOKEN.data.var, DEBUG_INFO(CUR_TOKEN)),
                                                    tree_dtor_untied_subtree(&arg1));
    (*pos)++;
    (*size)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
        tree_dtor_untied_subtree(&arg1);
        tree_dtor_untied_subtree(&var_node);

        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected closing brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    STATUS_CHECK(new_oper_node(dest, OperNum::MATH_DIFF, debug_info, arg1, var_node),
                                                    tree_dtor_untied_subtree(&arg1);
                                                    tree_dtor_untied_subtree(&var_node));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::CH_Unary(ParseData* data, size_t* const pos, TreeNode** dest,
                                      size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    OperNum oper = OperNum::NONE;

    if (IS_TOKEN_TYPE(CUR_TOKEN, TokenType::TERM))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        switch (CUR_TOKEN.data.term) {
            case TerminalNum::MATH_SQRT:
                oper = OperNum::MATH_SQRT;
                break;
            case TerminalNum::MATH_SIN:
                oper = OperNum::MATH_SIN;
                break;
            case TerminalNum::MATH_COS:
                oper = OperNum::MATH_COS;
                break;
            case TerminalNum::MATH_LN:
                oper = OperNum::MATH_LN;
                break;

            default:
                oper = OperNum::NONE;
                break;
        }
#pragma GCC diagnostic pop

    if (oper == OperNum::NONE)
        return Status::NORMAL_WORK;

    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_BRACE)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected opening brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    TreeNode* expr = nullptr;
    STATUS_CHECK(Expr(data, pos, &expr, size),      tree_dtor_untied_subtree(&expr));

    if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_BRACE)) {
        tree_dtor_untied_subtree(&expr);
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected closing brace"));
        return Status::SYNTAX_ERROR;
    }
    (*pos)++;

    STATUS_CHECK(new_oper_node(dest, oper, debug_info, nullptr, expr),
                                                    tree_dtor_untied_subtree(&expr));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Primary(ParseData* data, size_t* const pos, TreeNode** dest,
                                     size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::IN)) {
        STATUS_CHECK(new_oper_node(dest, OperNum::IN, DEBUG_INFO(CUR_TOKEN), nullptr, nullptr));
        (*size)++;
        (*pos)++;
        return Status::NORMAL_WORK;
    }

    STATUS_CHECK(Double(data, pos, dest, size));
    if (*dest)
        return Status::NORMAL_WORK;

    STATUS_CHECK(CH_FuncCall(data, pos, dest, size));
    if (*dest)
        return Status::NORMAL_WORK;

    STATUS_CHECK(Prefix(data, pos, dest, size));
    assert(*dest);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Prefix(ParseData* data, size_t* const pos, TreeNode** dest,
                                    size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);
    OperNum oper = OperNum::NONE;

    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::MATH_DOUBLE_ADD))
        oper = OperNum::PREFIX_ADD;
    else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::MATH_DOUBLE_SUB))
        oper = OperNum::PREFIX_SUB;

    if (oper == OperNum::NONE) {
        STATUS_CHECK(Postfix(data, pos, dest, size));
        return Status::NORMAL_WORK;
    }
    (*pos)++;

    TreeNode* prefix_node = nullptr;
    STATUS_CHECK(Prefix(data, pos, &prefix_node, size),     tree_dtor_untied_subtree(&prefix_node));
    assert(prefix_node);

    TreeNode** arr_elem_subtree = nullptr;
    TreeNode* var_node_src = nullptr;
    if (NODE_TYPE(prefix_node) == TreeElemType::VAR) {
        var_node_src = prefix_node;
    } else {
        assert(NODE_TYPE(prefix_node) == TreeElemType::OPER);

        if (NODE_DATA(prefix_node).oper == OperNum::ARRAY_ELEM) {
            var_node_src = *L(prefix_node);
            arr_elem_subtree = &prefix_node;
        } else {
            assert(*L(prefix_node));

            if (NODE_TYPE(*L(prefix_node)) == TreeElemType::OPER) {
                assert(NODE_DATA(*L(prefix_node)).oper == OperNum::ARRAY_ELEM);
                assert(*L(*L(prefix_node)));
                assert(NODE_TYPE(*L(*L(prefix_node))) == TreeElemType::VAR);

                var_node_src = *L(*L(prefix_node));
                arr_elem_subtree = L(prefix_node);
            } else {
                assert(NODE_TYPE(*L(prefix_node)) == TreeElemType::VAR);

                var_node_src = *L(prefix_node);
            }
        }
    }

    TreeNode* var_node_new = nullptr;
    STATUS_CHECK(new_var_node(&var_node_new, NODE_DATA(var_node_src).var,
                                                  ELEM(var_node_src)->debug_info));
    (*size)++;

    if (arr_elem_subtree != nullptr) {
        // swap
        TreeNode* arr_node = *arr_elem_subtree;
        *arr_elem_subtree = var_node_new;
        (*arr_elem_subtree)->parent = prefix_node;
        var_node_new = arr_node;
    }

    STATUS_CHECK(new_oper_node(dest, oper, debug_info, var_node_new, prefix_node),
                                                            tree_dtor_untied_subtree(&prefix_node));
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Postfix(ParseData* data, size_t* const pos, TreeNode** dest,
                                     size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::VAR)) {
        STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected var name"));
        return Status::SYNTAX_ERROR;
    }
    size_t var_num = CUR_TOKEN.data.var;
    DebugInfo var_debug_info = DEBUG_INFO(CUR_TOKEN);
    (*pos)++;

    TreeNode* arr_index_expr = nullptr;
    if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::OPEN_INDEX_BRACE)) {
        (*pos)++;

        STATUS_CHECK(Expr(data, pos, &arr_index_expr, size));
        assert(arr_index_expr);

        if (!IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::CLOSE_INDEX_BRACE)) {
            STATUS_CHECK(syntax_error(DEBUG_INFO(CUR_TOKEN), "Expected index brace closing"),
                                                        tree_dtor_untied_subtree(&arr_index_expr));
            tree_dtor_untied_subtree(&arr_index_expr);
            return Status::SYNTAX_ERROR;
        }
        (*pos)++;
    }

    TreeNode* parent = nullptr;

    do {
        DebugInfo debug_info = DEBUG_INFO(CUR_TOKEN);
        OperNum oper = OperNum::NONE;

        if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::MATH_DOUBLE_ADD))
            oper = OperNum::POSTFIX_ADD;
        else if (IS_TOKEN_TERM_EQ(CUR_TOKEN, TerminalNum::MATH_DOUBLE_SUB))
            oper = OperNum::POSTFIX_SUB;

        TreeNode* var_node = nullptr;
        STATUS_CHECK(new_var_node(&var_node, var_num, var_debug_info));
        (*size)++;

        if (arr_index_expr) { //< This is array element, not var
            TreeNode* var_node_tmp = var_node;
            var_node = nullptr;
            STATUS_CHECK(new_oper_node(&var_node, OperNum::ARRAY_ELEM, var_debug_info,
                                       var_node_tmp, arr_index_expr));
            (*size)++;

            arr_index_expr = nullptr; //< Expression must be tied once
        }

        if (oper == OperNum::NONE) {
            *dest = var_node;
            if (parent)
                (*dest)->parent = parent;
            break;
        }
        (*pos)++;

        STATUS_CHECK(new_oper_node(dest, oper, debug_info, var_node, nullptr),
                                                        tree_dtor_untied_subtree(&var_node));
        (*size)++;

        if (parent)
            (*dest)->parent = parent;

        parent = *dest;
        dest = R(*dest);
    } while (1);

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::Double(ParseData* data, size_t* const pos, TreeNode** dest,
                                    size_t* const size) {
    assert(data);
    assert(dest);
    assert(*dest == nullptr);
    assert(pos);

    if (!IS_TOKEN_TYPE(CUR_TOKEN, TokenType::NUM))
        return Status::NORMAL_WORK;

    STATUS_CHECK(new_num_node(dest, CUR_TOKEN.data.num, DEBUG_INFO(CUR_TOKEN)));
    (*pos)++;
    (*size)++;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::new_oper_node(TreeNode** dest, OperNum oper, DebugInfo debug_info,
                                           TreeNode* l_child, TreeNode* r_child) {
    assert(dest);
    assert(*dest == nullptr);

    TreeElem new_elem = OPER_ELEM(oper, debug_info);
    if (tree_node_ctor(dest, &new_elem, sizeof(TreeElem), nullptr) != Tree::OK)
        return Status::TREE_ERROR;

    if (l_child != nullptr)
        l_child->parent = *dest;

    if (r_child != nullptr)
        r_child->parent = *dest;

    *L(*dest) = l_child;
    *R(*dest) = r_child;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::new_num_node(TreeNode** dest, double val, DebugInfo debug_info) {
    assert(dest);
    assert(*dest == nullptr);
    assert(isfinite(val));

    TreeElem new_elem = NUM_ELEM(val, debug_info);
    if (tree_node_ctor(dest, &new_elem, sizeof(TreeElem), nullptr) != Tree::OK)
        return Status::TREE_ERROR;

    return Status::NORMAL_WORK;
}

Status::Statuses TextParser::new_var_node(TreeNode** dest, size_t var_num, DebugInfo debug_info) {
    assert(dest);
    assert(*dest == nullptr);

    TreeElem new_elem = VAR_ELEM(var_num, debug_info);
    if (tree_node_ctor(dest, &new_elem, sizeof(TreeElem), nullptr) != Tree::OK)
        return Status::TREE_ERROR;

    return Status::NORMAL_WORK;
}
