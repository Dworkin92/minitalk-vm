//============================================================
// Fichier : memory.h
//============================================================
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

#define PAGE_SIZE        (16 * 1024 * 1024)  // 16 Mo
#define MAX_PAGES_PER_ZONE 1024

// Types d'objets : #define pour garantir 1 octet
#define OBJ_INT      0
#define OBJ_FLOAT    1
#define OBJ_STRING   2
#define OBJ_ARRAY    3
#define OBJ_OBJECT   4
#define OBJ_CODE     5

typedef uint8_t ObjectType;

// Objet Array : elements = valeurs 64 bits (int OU pointeur NaN-boxe)
typedef struct {
    int64_t  length;
    Value*   elements;
} ArrayObject;

// Objet Code : un bloc de bytecode
typedef struct {
    size_t        length;   // nombre d'instructions
    Instruction*  code;     // tableau d'instructions
} CodeObject;

// En-tete de chunk : toujours 8 octets
typedef struct {
    uint32_t size;      // taille TOTALE du chunk (header + donnees)
    uint8_t  marked;    // drapeau GC
    uint8_t  type;      // ObjectType
    uint16_t pad;       // alignement -> sizeof == 8
} ChunkHeader;

typedef struct MemoryPage {
    uint8_t* start;
    uint8_t* end;
    uint8_t* free_ptr;
} MemoryPage;

typedef struct MemoryZone {
    MemoryPage* pages[MAX_PAGES_PER_ZONE];
    uint32_t    page_count;
} MemoryZone;

typedef struct Heap {
    MemoryZone object_zone;
    MemoryZone constant_zone;
    MemoryZone code_zone;
} Heap;

Heap*  heap_init(void);
void   heap_free(Heap* heap);

// Alloue un chunk. Retourne NULL si size > PAGE_SIZE - sizeof(ChunkHeader).
void*  heap_alloc(Heap* heap, MemoryZone* zone, size_t size, ObjectType type);

// Verifie qu'un pointeur est bien une donnee allouee dans le heap.
// (Indispensable tant qu'on n'a pas le NaN-boxing pour distinguer
//  entiers et pointeurs : evite au GC de dereferencer des entiers bruts.)
bool   heap_is_ptr(const Heap* heap, const void* ptr);

// GC mark-and-sweep
void   gc_mark(Heap* heap, void* ptr);
void   gc_mark_roots(Heap* heap, void** roots, size_t root_count);
void   gc_sweep(Heap* heap);
void   gc_collect(Heap* heap, void** roots, size_t root_count);

#endif
