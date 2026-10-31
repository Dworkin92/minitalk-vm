// myVM v0.2 - encodage compact 64 bits (revision de D2)
// Chantier a comportement constant : les programmes et les tests v0.1 sont inchanges.
// 10 fichiers : types.h memory.h memory.c vm.h vm.c loader.h loader.c utils.h test.c main.c
// Compilation (MinGW/Code::Blocks) :
//   gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c
// ATTENTION : il n'y a QU'UN main() (dans main.c). test.c n'a pas de main.
//
// CHANGEMENTS v0.2 :
//   - Instruction = uint64_t (8 octets) au lieu d'une struct de 16 octets
//   - Disposition : opcode 8b | reg1 4b | reg2 4b | reg3 4b | flags 4b | immediate 40b
//   - Registres adressables 0..15 (transition vers D8 : fenetre de 16 slots)
//   - Immediate signe 40 bits : -2^39 .. 2^39-1 (les constantes plus grandes
//     passeront par le constant pool, a venir)
//   - Macros INST_* = assembleuses de mots 64 bits ; decodage dans vm_run
//   - Nouveau test 2 : round-trip encodage/decodage (11 tests au total)
//-----------------------------------------------------------------------------------------------
// myVM v0.3 - NaN-boxing des valeurs (chantier 2 de la feuille de route)
// Value = uint64_t : les doubles vivent dans leurs bits natifs IEEE 754 ;
// int 48 bits, pointeur heap, nil, true, false vivent dans l'espace quiet NaN.
// gc_mark passe de heap_is_ptr (O(chunks)) a un test de tag O(1).
// 11 fichiers : value.h types.h memory.h memory.c vm.h vm.c loader.h loader.c utils.h test.c main.c
// Compilation (MinGW/Code::Blocks) :
//   gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c
// ATTENTION : il n'y a QU'UN main() (dans main.c). test.c n'a pas de main.
//
// CHANGEMENTS v0.3 :
//   - NOUVEAU value.h : encodeurs/decodeurs + tests de tag O(1)
//   - Value = uint64_t (et non plus int64_t brut)
//   - gc_mark prend un Value et ne suit que les TAG_PTR (D5 remplacé)
//   - gc_mark_roots prend un tableau de Value (value_ptr(...) pour les racines)
//   - registres VM = Value taggues ; ADD/SUB/MUL extraient les int ;
//     JUMP_IF saute sur nil / false / int 0 ; PRINT affiche selon le type
//   - programmes de tests/main inchanges (comportement constant)
//   - nouveau test 12 : round-trip NaN-boxing + GC sur array mixte -> 12 tests


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
    printf("== myVM v0.3 (NaN-boxing) ==\n");
    printf("sizeof(ChunkHeader)=%zu  sizeof(Value)=%zu  sizeof(Instruction)=%zu\n\n",
           sizeof(ChunkHeader), sizeof(Value), sizeof(Instruction));

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

    // 2. Heap + objets + GC avec valeurs NaN-boxes
    vm = vm_new();
    char* str = heap_alloc(vm->heap, &vm->heap->object_zone, 10, OBJ_STRING);
    strcpy(str, "Bonjour");
    printf("Chaine allouee : %s\n", str);

    int64_t* num = heap_alloc(vm->heap, &vm->heap->object_zone,
                              sizeof(int64_t), OBJ_INT);
    *num = 42;
    printf("Entier alloue : %lld\n", (long long)*num);

    // Array mixte : deux entiers NaN-boxes + un pointeur vers une string
    ArrayObject* arr = heap_alloc(vm->heap, &vm->heap->object_zone,
                                  sizeof(ArrayObject), OBJ_ARRAY);
    arr->length = 3;
    arr->elements = heap_alloc(vm->heap, &vm->heap->object_zone,
                               3 * sizeof(Value), OBJ_INT);
    arr->elements[0] = value_int(100);
    arr->elements[1] = value_ptr(str);   // le GC devra suivre celle-ci
    arr->elements[2] = value_int(200);
    printf("Array alloue : [%lld, ptr, %lld]\n",
           (long long)value_as_int(arr->elements[0]),
           (long long)value_as_int(arr->elements[2]));

    printf("Affichage type : ");
    value_print(value_int(42));      printf(" ");
    value_print(value_double(3.5));  printf(" ");
    value_print(VALUE_NIL);          printf(" ");
    value_print(VALUE_TRUE);         printf("\n");

    Value roots[] = { value_ptr(str), value_ptr(num), value_ptr(arr) };
    gc_collect(vm->heap, roots, 3);
    printf("GC passe sans crash (tag O(1), plus de heap_is_ptr dans mark).\n");
    vm_free(vm);

    // 3. Tests
    printf("\n");
    run_all_tests();
    return 0;
}
