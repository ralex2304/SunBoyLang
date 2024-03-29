#ifndef ASM_TRAVERSAL_H_
#define ASM_TRAVERSAL_H_

#include "utils/statuses.h"
#include "objects.h"
#include "config.h"
#include TREE_INCLUDE
#include "file/file.h"
#include "error_printer/error_printer.h"

#include "../backend_objects.h"
#include "../Stack/stack.h"
#include "output/output.h"

Status::Statuses asm_command_traversal(BackData* data, FILE* file, TreeNode* node,
                                       bool is_val_needed);

#endif //< #ifndef ASM_TRAVERSAL_H_
