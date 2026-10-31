//============================================================
// Fichier : vm.h
//============================================================
#ifndef VM_H
#define VM_H

#include "types.h"
#include "memory.h"

typedef struct VM {
    Register     registers;    // Value NaN-boxes depuis la v0.3
    CodeObject*  code_object;
    size_t       ip;
    Heap*        heap;
} VM;

VM*  vm_new(void);
void vm_free(VM* vm);
void vm_run(VM* vm);

#endif
