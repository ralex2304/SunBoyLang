#ifndef BACKEND_OBKECTS_H_
#define BACKEND_OBKECTS_H_

#include <assert.h>
#include <stdlib.h>

#include "objects.h"
#include "utils/vector.h"
#include "utils/statuses.h"
#include "config.h"
#include TREE_INCLUDE
#include "Stack/stack.h"

struct BackData {
    Tree tree = {};
    Vector vars = {};

    Stack scopes = {};

    FuncTable func_table = {};

    inline bool ctor() {
        if (TREE_CTOR(&tree, sizeof(TreeElem), &tree_elem_dtor, &tree_elem_verify,
                                               &tree_elem_str_val) != Tree::OK)
            return false;

        if (!vars.ctor(sizeof(String)))
            return false;

        if (STK_CTOR(&scopes) != Stack::OK)
            return false;

        if (!func_table.ctor())
            return false;

        return true;
    };

    inline bool dtor() {
        bool no_error = true;

        if (tree_dtor(&tree) != Tree::OK)
            no_error = false;

        vars.dtor();

        Elem_t tmp = {};
        while (scopes.size > 0) {
            no_error = no_error && (stk_pop(&scopes, &tmp) == Stack::OK);

            tmp.dtor();
        }

        if (stk_dtor(&scopes))
            no_error = false;

        func_table.dtor();

        return no_error;
    };
};

#endif //< #ifndef BACKEND_OBKECTS_H_
