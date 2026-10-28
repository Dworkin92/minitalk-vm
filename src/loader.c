//============================================================
// Fichier : loader.c
//============================================================
#include "loader.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Le bytecode vit entierement dans code_zone (zone dediee du heap) :
//  - un chunk CodeObject (metadonnees)
//  - un chunk pour le tableau d'instructions
// Aucun malloc() hors heap : liberation unique via heap_free().

void vm_load_bytecode(VM* vm, const Instruction* bytecode, size_t size) {
    CodeObject* co = (CodeObject*)heap_alloc(
        vm->heap, &vm->heap->code_zone, sizeof(CodeObject), OBJ_CODE);
    if (!co) { fprintf(stderr, "loader: echec allocation CodeObject\n"); exit(1); }

    Instruction* code = (Instruction*)heap_alloc(
        vm->heap, &vm->heap->code_zone, size * sizeof(Instruction), OBJ_CODE);
    if (!code) { fprintf(stderr, "loader: echec allocation bytecode\n"); exit(1); }

    memcpy(code, bytecode, size * sizeof(Instruction));

    co->length = size;
    co->code   = code;

    vm->code_object = co;
    vm->ip = 0;
}
