//============================================================
// Fichier : vm.c
//============================================================
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

VM* vm_new(void) {
    VM* vm = malloc(sizeof(VM));
    if (!vm) { fprintf(stderr, "vm: echec malloc(VM)\n"); exit(1); }
    for (int i = 0; i < NUM_REGISTERS; i++) vm->registers[i] = 0;
    vm->code_object = NULL;
    vm->ip = 0;
    vm->heap = heap_init();
    return vm;
}

void vm_free(VM* vm) {
    // Tout le contenu vit dans le heap : une seule liberation.
    heap_free(vm->heap);
    free(vm);
}

void vm_run(VM* vm) {
    if (!vm->code_object) {
        fprintf(stderr, "vm: aucun bytecode charge\n");
        return;
    }
    const Instruction* code = vm->code_object->code;

    while (vm->ip < vm->code_object->length) {
        Instruction inst = code[vm->ip++];
        switch (inst.opcode) {
            case OP_HALT: return;
            case OP_LOAD:
                vm->registers[inst.reg1] = inst.immediate;
                break;
            case OP_ADD:
                vm->registers[inst.reg1] =
                    vm->registers[inst.reg2] + vm->registers[inst.reg3];
                break;
            case OP_SUB:
                vm->registers[inst.reg1] =
                    vm->registers[inst.reg2] - vm->registers[inst.reg3];
                break;
            case OP_MUL:
                vm->registers[inst.reg1] =
                    vm->registers[inst.reg2] * vm->registers[inst.reg3];
                break;
            case OP_JUMP:
                vm->ip = (size_t)inst.immediate;
                break;
            case OP_JUMP_IF:   // saute si R[reg1] == 0
                if (vm->registers[inst.reg1] == 0)
                    vm->ip = (size_t)inst.immediate;
                break;
            case OP_PRINT:
                printf("R%u = %lld\n", inst.reg1,
                       (long long)vm->registers[inst.reg1]);
                break;
            default:
                fprintf(stderr, "vm: opcode inconnu %u\n", inst.opcode);
                exit(1);
        }
    }
}
