//============================================================
// Fichier : test.c
//============================================================
#include "vm.h"
#include "loader.h"
#include "utils.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;

#define RUN_TEST(fn)                            \
    do {                                        \
        tests_run++;                            \
        printf("  [%d] %-28s ... ", tests_run, #fn); \
        fn();                                   \
        printf("OK\n");                          \
    } while (0)

// 1. Tailles deterministes des structures
static void test_struct_sizes(void) {
    CHECK(sizeof(ChunkHeader) == 8,  "sizeof(ChunkHeader) != 8");
    CHECK(sizeof(Value) == 8,        "sizeof(Value) != 8");
    CHECK(sizeof(Instruction) == 8,  "sizeof(Instruction) != 8");
    CHECK(sizeof(ArrayObject) == 16, "sizeof(ArrayObject) != 16");
}

// 2. Round-trip encodage/decodage de l'instruction compacte
static void test_instruction_encoding(void) {
    Instruction w = ins_encode(OP_LOAD, 3, 15, 14, 13, 42);
    CHECK(INS_OPCODE(w) == OP_LOAD, "opcode mal encode");
    CHECK(INS_REG1(w) == 3,   "reg1 mal encode");
    CHECK(INS_REG2(w) == 15,  "reg2 mal encode");
    CHECK(INS_REG3(w) == 14,  "reg3 mal encode");
    CHECK(INS_FLAGS(w) == 13, "flags mal encodes");
    CHECK(INS_IMM(w) == 42,   "immediate mal encode");

    w = INST_LOAD(1, -12345);
    CHECK(INS_IMM(w) == -12345, "immediate negatif mal decode");

    w = INST_LOAD(1, INS_IMM_MIN);
    CHECK(INS_IMM(w) == INS_IMM_MIN, "borne basse mal decodee");
    w = INST_LOAD(1, INS_IMM_MAX);
    CHECK(INS_IMM(w) == INS_IMM_MAX, "borne haute mal decodee");

    Instruction prog[] = {
        INST_LOAD(1, 10), INST_ADD(3, 1, 2), INST_SUB(3, 3, 1),
        INST_MUL(3, 3, 2), INST_JUMP(0), INST_JUMP_IF(4, 9),
        INST_PRINT(3), INST_HALT
    };
    CHECK(INS_OPCODE(prog[0]) == OP_LOAD && INS_REG1(prog[0]) == 1
          && INS_IMM(prog[0]) == 10, "macro LOAD incorrecte");
    CHECK(INS_OPCODE(prog[1]) == OP_ADD && INS_REG2(prog[1]) == 1
          && INS_REG3(prog[1]) == 2, "macro ADD incorrecte");
    CHECK(INS_OPCODE(prog[5]) == OP_JUMP_IF && INS_REG1(prog[5]) == 4
          && INS_IMM(prog[5]) == 9, "macro JUMP_IF incorrecte");
    CHECK(INS_OPCODE(prog[7]) == OP_HALT, "macro HALT incorrecte");
}

// 3. Round-trip NaN-boxing (NOUVEAU v0.3)
static void test_nan_boxing(void) {
    // Entiers : zero, positifs, negatifs, bornes 48 bits
    int64_t ints[] = { 0, 42, -123, 100, 200,
                       VALUE_INT_MAX, VALUE_INT_MIN, 549755813887 };
    for (int i = 0; i < 8; i++) {
        Value v = value_int(ints[i]);
        CHECK(VALUE_IS_INT(v), "tag INT non reconnu");
        CHECK(!VALUE_IS_DOUBLE(v) && !VALUE_IS_PTR(v),
              "un int ne doit pas etre confondu avec un autre type");
        CHECK(value_as_int(v) == ints[i], "int round-trip KO");
    }
    // int(0) ne doit pas entrer en collision avec le NaN canonique
    CHECK(value_int(0) != VALUE_QNAN, "collision int(0) / NaN canonique");
    CHECK(!VALUE_IS_DOUBLE(value_int(0)), "int(0) decode comme double");

    // Pointeurs heap : alloue, taggue, retrouve
    Heap* heap = heap_init();
    char* str = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    Value pv = value_ptr(str);
    CHECK(VALUE_IS_PTR(pv), "tag PTR non reconnu");
    CHECK(value_as_ptr(pv) == (void*)str, "ptr round-trip KO");
    CHECK(!VALUE_IS_INT(pv) && !VALUE_IS_DOUBLE(pv), "ptr confondu");

    // nil / booleans
    CHECK(VALUE_IS_NIL(VALUE_NIL) && !VALUE_IS_NIL(VALUE_TRUE), "nil KO");
    CHECK(VALUE_IS_BOOL(VALUE_TRUE) && VALUE_IS_BOOL(VALUE_FALSE)
          && !VALUE_IS_BOOL(VALUE_NIL), "bool KO");

    // Doubles : natifs, negatif, infini ; NaN canonise
    Value d1 = value_double(3.14);
    CHECK(VALUE_IS_DOUBLE(d1) && !VALUE_IS_INT(d1) && !VALUE_IS_PTR(d1),
          "double 3.14 mal taggue");
    CHECK(value_as_double(d1) == 3.14, "double round-trip KO");
    Value dn = value_double(-0.5);
    CHECK(value_as_double(dn) == -0.5, "double negatif round-trip KO");
    Value dinf = value_double(1.0 / 0.0);
    CHECK(VALUE_IS_DOUBLE(dinf), "infini doit rester un double");
    Value dnan = value_double(0.0 / 0.0);
    CHECK(dnan == VALUE_QNAN, "tout NaN doit etre canonise en VALUE_QNAN");
    CHECK(VALUE_IS_DOUBLE(dnan), "le NaN canonise reste un double");
    heap_free(heap);

    // falsy : nil, false, int 0 ; ni true ni les autres entiers
    CHECK(value_is_falsy(VALUE_NIL), "nil doit etre faux");
    CHECK(value_is_falsy(VALUE_FALSE), "false doit etre faux");
    CHECK(value_is_falsy(value_int(0)), "int 0 doit etre faux");
    CHECK(!value_is_falsy(VALUE_TRUE), "true doit etre vrai");
    CHECK(!value_is_falsy(value_int(7)), "int 7 doit etre vrai");
}

// 4. En-tete de chunk correct apres allocation
static void test_chunk_header(void) {
    Heap* heap = heap_init();
    char* str = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    ChunkHeader* h = (ChunkHeader*)str - 1;

    size_t expected = sizeof(ChunkHeader) + ((10 + 7) & ~7);
    CHECK(h->size == expected, "taille totale du chunk incorrecte");
    CHECK(h->type == OBJ_STRING, "type du chunk incorrect");
    CHECK(h->marked == 0, "chunk ne doit pas etre marque a l'allocation");
    heap_free(heap);
}

// 5. Deux allocations distinctes, donnees intactes
static void test_multiple_allocations(void) {
    Heap* heap = heap_init();
    char* a = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    char* b = heap_alloc(heap, &heap->object_zone, 20, OBJ_STRING);
    CHECK(a && b, "allocations echouees");
    strcpy(a, "Hello");
    strcpy(b, "World");
    CHECK(strcmp(a, "Hello") == 0 && strcmp(b, "World") == 0, "donnees corrompues");
    CHECK(a + 24 <= b, "les chunks ne doivent pas se chevaucher");
    heap_free(heap);
}

// 6. Allocation plus grande qu'une page -> doit echouer proprement
static void test_oversized_allocation(void) {
    Heap* heap = heap_init();
    void* too_big = heap_alloc(heap, &heap->object_zone,
                               (size_t)PAGE_SIZE, OBJ_OBJECT);
    CHECK(too_big == NULL, "une allocation > page doit retourner NULL");
    void* ok = heap_alloc(heap, &heap->object_zone, 1024, OBJ_OBJECT);
    CHECK(ok != NULL, "une allocation normale doit reussir");
    heap_free(heap);
}

// 7. Deuxieme page allouee quand la premiere est PLEINE
static void test_page_growth(void) {
    Heap* heap = heap_init();
    CHECK(heap->object_zone.page_count == 0, "zone doit demarrer sans page");
    heap_alloc(heap, &heap->object_zone, 100, OBJ_OBJECT);
    CHECK(heap->object_zone.page_count == 1, "1 page attendue");

    MemoryPage* p1 = heap->object_zone.pages[0];
    size_t rest = (size_t)(p1->end - p1->free_ptr);
    void* big = heap_alloc(heap, &heap->object_zone,
                           rest - sizeof(ChunkHeader), OBJ_OBJECT);
    CHECK(big != NULL, "remplissage de page echoue");
    CHECK(heap->object_zone.page_count == 1, "toujours 1 page apres remplissage");
    CHECK(p1->free_ptr == p1->end, "la page 1 doit etre pleine");

    heap_alloc(heap, &heap->object_zone, 8, OBJ_OBJECT);
    CHECK(heap->object_zone.page_count == 2, "2e page attendue");
    heap_free(heap);
}

// 8. GC : array mixte (ints + ptr) -> le mark suit UNIQUEMENT le ptr
static void test_gc_mixed_array(void) {
    Heap* heap = heap_init();
    ArrayObject* arr = heap_alloc(heap, &heap->object_zone,
                                  sizeof(ArrayObject), OBJ_ARRAY);
    arr->length = 3;
    arr->elements = heap_alloc(heap, &heap->object_zone,
                              3 * sizeof(Value), OBJ_INT);
    char* cible = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    strcpy(cible, "vive");
    arr->elements[0] = value_int(100);      // entiers : ne doivent PAS
    arr->elements[1] = value_ptr(cible);    // etre suivis ; le ptr oui
    arr->elements[2] = value_int(-45);

    Value roots[] = { value_ptr(arr) };
    gc_collect(heap, roots, 1);

    ChunkHeader* arr_h = (ChunkHeader*)arr - 1;
    ChunkHeader* el_h  = (ChunkHeader*)arr->elements - 1;
    ChunkHeader* ci_h  = (ChunkHeader*)cible - 1;
    CHECK(arr_h->marked == 0 && el_h->marked == 0 && ci_h->marked == 0,
          "les marques doivent etre reinitialisees apres le sweep");
    heap_free(heap);
}

// 9. GC : les objets racine sont bien marques pendant la phase de mark
static void test_gc_marks_reachable(void) {
    Heap* heap = heap_init();
    char* root = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    char* garbage = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    (void)garbage;

    Value roots[] = { value_ptr(root) };
    gc_mark_roots(heap, roots, 1);

    ChunkHeader* rh = (ChunkHeader*)root - 1;
    CHECK(rh->marked == 1, "la racine doit etre marquee");
    gc_sweep(heap);
    CHECK(rh->marked == 0, "sweep doit remettre la marque a zero");
    heap_free(heap);
}

// 10. heap_is_ptr distingue pointeur du heap et entier (garde de debug)
static void test_heap_is_ptr(void) {
    Heap* heap = heap_init();
    char* p = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    CHECK(heap_is_ptr(heap, p), "un pointeur du heap doit etre reconnu");
    CHECK(!heap_is_ptr(heap, (void*)100), "un entier brut doit etre rejete");
    CHECK(!heap_is_ptr(heap, NULL), "NULL doit etre rejete");
    // Depuis le NaN-boxing, le tag PTR est le discriminateur O(1)
    CHECK(VALUE_IS_PTR(value_ptr(p)) && !VALUE_IS_PTR(value_int(100)),
          "le tag doit distinguer ptr et int");
    heap_free(heap);
}

// 11. Bytecode charge comme objet dans code_zone, executable
static void test_bytecode_as_object(void) {
    VM* vm = vm_new();
    Instruction program[] = {
        INST_LOAD(1, 10),
        INST_LOAD(2, 20),
        INST_ADD(3, 1, 2),
        INST_HALT
    };
    vm_load_bytecode(vm, program, sizeof(program) / sizeof(Instruction));

    CHECK(vm->code_object != NULL, "CodeObject non alloue");
    CHECK(vm->code_object->length == 4, "longueur du bytecode incorrecte");
    CHECK(heap_is_ptr(vm->heap, vm->code_object),
          "le CodeObject doit vivre dans le heap");
    CHECK(heap_is_ptr(vm->heap, vm->code_object->code),
          "les instructions doivent vivre dans le heap");

    vm_run(vm);
    CHECK(vm->registers[3] == value_int(30), "R3 doit valoir int 30");
    CHECK(VALUE_IS_INT(vm->registers[3]), "R3 doit etre un entier NaN-boxe");
    vm_free(vm);
}

// 12. Boucle : somme 0+1+...+5 = 15 via JUMP / JUMP_IF
static void test_loop_bytecode(void) {
    VM* vm = vm_new();
    Instruction program[] = {
        /* 0 */ INST_LOAD(1, 0),    // R1 = i = 0
        /* 1 */ INST_LOAD(2, 6),    // R2 = limite = 6
        /* 2 */ INST_LOAD(5, 1),    // R5 = constante 1
        /* 3 */ INST_LOAD(3, 0),    // R3 = somme = 0
        /* 4 */ INST_ADD(3, 3, 1),  // R3 += R1        <- debut boucle
        /* 5 */ INST_ADD(1, 1, 5),  // R1 += R5
        /* 6 */ INST_SUB(4, 1, 2),  // R4 = R1 - R2
        /* 7 */ INST_JUMP_IF(4, 9), // si R4 == 0 -> fin
        /* 8 */ INST_JUMP(4),       // sinon -> boucle
        /* 9 */ INST_PRINT(3),
        /*10 */ INST_HALT
    };
    vm_load_bytecode(vm, program, sizeof(program) / sizeof(Instruction));
    vm_run(vm);
    CHECK(vm->registers[3] == value_int(15), "la somme 0..5 doit valoir 15");
    vm_free(vm);
}

void run_all_tests(void) {
    printf("--- Debut des tests ---\n");
    printf("sizeof(ChunkHeader)=%zu sizeof(Value)=%zu sizeof(Instruction)=%zu\n",
           sizeof(ChunkHeader), sizeof(Value), sizeof(Instruction));
    RUN_TEST(test_struct_sizes);
    RUN_TEST(test_instruction_encoding);
    RUN_TEST(test_nan_boxing);
    RUN_TEST(test_chunk_header);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_oversized_allocation);
    RUN_TEST(test_page_growth);
    RUN_TEST(test_gc_mixed_array);
    RUN_TEST(test_gc_marks_reachable);
    RUN_TEST(test_heap_is_ptr);
    RUN_TEST(test_bytecode_as_object);
    RUN_TEST(test_loop_bytecode);
    printf("--- %d tests OK ---\n", tests_run);
}

