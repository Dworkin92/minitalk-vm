//============================================================
// Fichier : vm.c
//============================================================
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

VM* vm_new(void) {
    VM* vm = malloc(sizeof(VM));
    if (!vm) { fprintf(stderr, "vm: echec malloc(VM)\n"); exit(1); }
    for (int i = 0; i < NUM_REGISTERS; i++) vm->registers[i] = VALUE_NIL;
    vm->code_object = NULL;
    vm->ip = 0;
    vm->heap = heap_init();
    return vm;
}

void vm_free(VM* vm) {
    heap_free(vm->heap);
    free(vm);
}

// v0.3 : registres = Value NaN-boxes. Les operations arithmetiques
// extraient les entiers ; JUMP_IF saute sur nil / false / int 0 ;
// PRINT affiche selon le type. Programmes v0.2 inchanges.
void vm_run(VM* vm) {
    if (!vm->code_object) {
        fprintf(stderr, "vm: aucun bytecode charge\n");
        return;
    }
    const Instruction* code = vm->code_object->code;

    while (vm->ip < vm->code_object->length) {
        Instruction word = code[vm->ip++];
        uint8_t reg1 = INS_REG1(word);
        uint8_t reg2 = INS_REG2(word);
        uint8_t reg3 = INS_REG3(word);

        switch (INS_OPCODE(word)) {
            case OP_HALT: return;
            case OP_LOAD:
                vm->registers[reg1] = value_int(INS_IMM(word));
                break;
            case OP_ADD:
                vm->registers[reg1] = value_int(
                    value_as_int(vm->registers[reg2]) +
                    value_as_int(vm->registers[reg3]));
                break;
            case OP_SUB:
                vm->registers[reg1] = value_int(
                    value_as_int(vm->registers[reg2]) -
                    value_as_int(vm->registers[reg3]));
                break;
            case OP_MUL:
                vm->registers[reg1] = value_int(
                    value_as_int(vm->registers[reg2]) *
                    value_as_int(vm->registers[reg3]));
                break;
            case OP_JUMP:
                vm->ip = (size_t)INS_IMM(word);
                break;
            case OP_JUMP_IF:   // saute si la valeur est "fausse"
                if (value_is_falsy(vm->registers[reg1]))
                    vm->ip = (size_t)INS_IMM(word);
                break;
            case OP_PRINT:
                printf("R%u = ", reg1);
                value_print(vm->registers[reg1]);
                printf("\n");
                break;
            default:
                fprintf(stderr, "vm: opcode inconnu %u\n", INS_OPCODE(word));
                exit(1);
        }
    }
}
