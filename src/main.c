//============================================================
// Fichier : main.c
//============================================================
#include "vm.h"
#include "loader.h"
#include "utils.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

void run_all_tests(void);

int main(void) {
    printf("== myVM ==\n");
    printf("sizeof(ChunkHeader)=%zu  sizeof(Instruction)=%zu\n\n",
           sizeof(ChunkHeader), sizeof(Instruction));

    // 1. Execution de bytecode (addition)
    VM* vm = vm_new();
    Instruction program[] = {
        INST_LOAD(1, 10),
        INST_LOAD(2, 20),
        INST_ADD(3, 1, 2),
        INST_PRINT(3),
        INST_HALT
    };
    vm_load_bytecode(vm, program, sizeof(program) / sizeof(program[0]));
    vm_run(vm);
    vm_free(vm);

    // 2. Heap + objets + GC
    vm = vm_new();
    char* str = heap_alloc(vm->heap, &vm->heap->object_zone, 10, OBJ_STRING);
    strcpy(str, "Bonjour");
    printf("Chaine allouee : %s\n", str);

    int64_t* num = heap_alloc(vm->heap, &vm->heap->object_zone,
                              sizeof(int64_t), OBJ_INT);
    *num = 42;
    printf("Entier alloue : %lld\n", (long long)*num);

    ArrayObject* arr = heap_alloc(vm->heap, &vm->heap->object_zone,
                                  sizeof(ArrayObject), OBJ_ARRAY);
    arr->length = 2;
    arr->elements = heap_alloc(vm->heap, &vm->heap->object_zone,
                               2 * sizeof(Value), OBJ_INT);
    arr->elements[0] = 100;
    arr->elements[1] = 200;
    printf("Array alloue : [%lld, %lld]\n",
           (long long)arr->elements[0], (long long)arr->elements[1]);

    void* roots[] = { str, num, arr };
    gc_collect(vm->heap, roots, 3);
    printf("GC passe sans crash.\n");
    vm_free(vm);

    // 3. Tests
    printf("\n");
    run_all_tests();
    return 0;
}
