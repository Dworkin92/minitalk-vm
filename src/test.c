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
    CHECK(sizeof(Instruction) == 16, "sizeof(Instruction) != 16");
    CHECK(sizeof(ArrayObject) == 16, "sizeof(ArrayObject) != 16");
}

// 2. En-tete de chunk correct apres allocation
static void test_chunk_header(void) {
    Heap* heap = heap_init();
    char* str = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    ChunkHeader* h = (ChunkHeader*)str - 1;

    size_t expected = sizeof(ChunkHeader) + ((10 + 7) & ~7); // 8 + 16 = 24
    CHECK(h->size == expected, "taille totale du chunk incorrecte");
    CHECK(h->type == OBJ_STRING, "type du chunk incorrect");
    CHECK(h->marked == 0, "chunk ne doit pas etre marque a l'allocation");
    heap_free(heap);
}

// 3. Deux allocations distinctes, donnees intactes
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

// 4. Allocation plus grande qu'une page -> doit echouer proprement
static void test_oversized_allocation(void) {
    Heap* heap = heap_init();
    void* too_big = heap_alloc(heap, &heap->object_zone,
                               (size_t)PAGE_SIZE, OBJ_OBJECT); // 16 Mo + header
    CHECK(too_big == NULL, "une allocation > page doit retourner NULL");
    void* ok = heap_alloc(heap, &heap->object_zone, 1024, OBJ_OBJECT);
    CHECK(ok != NULL, "une allocation normale doit reussir");
    heap_free(heap);
}

// 5. Deuxieme page allouee quand la premiere est PLEINE
static void test_page_growth(void) {
    Heap* heap = heap_init();
    CHECK(heap->object_zone.page_count == 0, "zone doit demarrer sans page");
    heap_alloc(heap, &heap->object_zone, 100, OBJ_OBJECT);
    CHECK(heap->object_zone.page_count == 1, "1 page attendue");

    // Remplit exactement le reste de la page 1
    MemoryPage* p1 = heap->object_zone.pages[0];
    size_t rest = (size_t)(p1->end - p1->free_ptr);   // espace restant
    void* big = heap_alloc(heap, &heap->object_zone,
                           rest - sizeof(ChunkHeader), OBJ_OBJECT);
    CHECK(big != NULL, "remplissage de page echoue");
    CHECK(heap->object_zone.page_count == 1, "toujours 1 page apres remplissage");
    CHECK(p1->free_ptr == p1->end, "la page 1 doit etre pleine");

    heap_alloc(heap, &heap->object_zone, 8, OBJ_OBJECT);
    CHECK(heap->object_zone.page_count == 2, "2e page attendue");
    heap_free(heap);
}

// 6. GC : un entier brut dans un array ne doit PAS faire planter le mark
//    (c'etait la cause du 0xC0000005)
static void test_gc_with_raw_ints(void) {
    Heap* heap = heap_init();
    ArrayObject* arr = heap_alloc(heap, &heap->object_zone,
                                  sizeof(ArrayObject), OBJ_ARRAY);
    arr->length = 2;
    arr->elements = heap_alloc(heap, &heap->object_zone,
                               2 * sizeof(Value), OBJ_INT);
    arr->elements[0] = 100;   // entiers bruts, PAS des pointeurs
    arr->elements[1] = 200;

    void* roots[] = { arr };
    gc_collect(heap, roots, 1);   // avant : crash ici

    ChunkHeader* arr_h = (ChunkHeader*)arr - 1;
    ChunkHeader* el_h  = (ChunkHeader*)arr->elements - 1;
    CHECK(arr_h->marked == 0 && el_h->marked == 0,
          "les marques doivent etre reinitialisees apres le sweep");
    heap_free(heap);
}

// 7. GC : les objets racine sont bien marques pendant la phase de mark
static void test_gc_marks_reachable(void) {
    Heap* heap = heap_init();
    char* root = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    char* garbage = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    (void)garbage;

    void* roots[] = { root };
    gc_mark_roots(heap, roots, 1);

    ChunkHeader* rh = (ChunkHeader*)root - 1;
    CHECK(rh->marked == 1, "la racine doit etre marquee");
    gc_sweep(heap);
    CHECK(rh->marked == 0, "sweep doit remettre la marque a zero");
    heap_free(heap);
}

// 8. heap_is_ptr distingue pointeur du heap et entier
static void test_heap_is_ptr(void) {
    Heap* heap = heap_init();
    char* p = heap_alloc(heap, &heap->object_zone, 10, OBJ_STRING);
    CHECK(heap_is_ptr(heap, p), "un pointeur du heap doit etre reconnu");
    CHECK(!heap_is_ptr(heap, (void*)100), "un entier brut doit etre rejete");
    CHECK(!heap_is_ptr(heap, NULL), "NULL doit etre rejete");
    heap_free(heap);
}

// 9. Bytecode charge comme objet dans code_zone, executable
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
    CHECK(vm->registers[3] == 30, "R3 doit valoir 30 apres execution");
    vm_free(vm);
}

// 10. Boucle : somme 0+1+...+5 = 15 via JUMP / JUMP_IF
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
    CHECK(vm->registers[3] == 15, "la somme 0..5 doit valoir 15");
    vm_free(vm);
}

void run_all_tests(void) {
    printf("--- Debut des tests ---\n");
    printf("sizeof(ChunkHeader) = %zu, sizeof(Instruction) = %zu\n",
           sizeof(ChunkHeader), sizeof(Instruction));
    RUN_TEST(test_struct_sizes);
    RUN_TEST(test_chunk_header);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_oversized_allocation);
    RUN_TEST(test_page_growth);
    RUN_TEST(test_gc_with_raw_ints);
    RUN_TEST(test_gc_marks_reachable);
    RUN_TEST(test_heap_is_ptr);
    RUN_TEST(test_bytecode_as_object);
    RUN_TEST(test_loop_bytecode);
    printf("--- %d tests OK ---\n", tests_run);
}
