//============================================================
// Fichier : loader.h
//============================================================
#ifndef LOADER_H
#define LOADER_H

#include "types.h"
#include "vm.h"

void vm_load_bytecode(VM* vm, const Instruction* bytecode, size_t size);

#endif
