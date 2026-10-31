//============================================================
// Fichier : memory.c
//============================================================
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- Initialisation ----

static MemoryPage* page_new(void) {
    MemoryPage* page = malloc(sizeof(MemoryPage));
    if (!page) { fprintf(stderr, "heap: echec malloc(page)\n"); exit(1); }
    page->start = malloc(PAGE_SIZE);
    if (!page->start) { fprintf(stderr, "heap: echec malloc(16Mo)\n"); exit(1); }
    page->end      = page->start + PAGE_SIZE;
    page->free_ptr = page->start;
    return page;
}

static void zone_init(MemoryZone* zone) {
    zone->page_count = 0;
    for (int i = 0; i < MAX_PAGES_PER_ZONE; i++) zone->pages[i] = NULL;
}

static MemoryPage* zone_new_page(MemoryZone* zone) {
    if (zone->page_count >= MAX_PAGES_PER_ZONE) {
        fprintf(stderr, "heap: limite de %d pages atteinte\n", MAX_PAGES_PER_ZONE);
        exit(1);
    }
    MemoryPage* page = page_new();
    zone->pages[zone->page_count++] = page;
    return page;
}

Heap* heap_init(void) {
    Heap* heap = malloc(sizeof(Heap));
    if (!heap) { fprintf(stderr, "heap: echec malloc(Heap)\n"); exit(1); }
    zone_init(&heap->object_zone);
    zone_init(&heap->constant_zone);
    zone_init(&heap->code_zone);
    return heap;
}

// ---- Allocation ----

void* heap_alloc(Heap* heap, MemoryZone* zone, size_t size, ObjectType type) {
    (void)heap; // reserve pour un futur suivi statistique
    size = (size + 7) & ~7;

    const size_t total = sizeof(ChunkHeader) + size;
    if (total > PAGE_SIZE) {
        fprintf(stderr, "heap: allocation de %zu octets > page (%d)\n",
                size, (int)PAGE_SIZE);
        return NULL;
    }

    for (uint32_t i = 0; i < zone->page_count; i++) {
        MemoryPage* page = zone->pages[i];
        uintptr_t aligned = ((uintptr_t)page->free_ptr + 7) & ~(uintptr_t)7;
        if (aligned + total <= (uintptr_t)page->end) {
            ChunkHeader* h = (ChunkHeader*)aligned;
            h->size   = (uint32_t)total;
            h->marked = 0;
            h->type   = type;
            page->free_ptr = (uint8_t*)aligned + total;
            return (void*)(h + 1);
        }
    }

    MemoryPage* page = zone_new_page(zone);
    ChunkHeader* h = (ChunkHeader*)page->start;
    h->size   = (uint32_t)total;
    h->marked = 0;
    h->type   = type;
    page->free_ptr = page->start + total;
    return (void*)(h + 1);
}

void heap_free(Heap* heap) {
    MemoryZone* zones[3] = { &heap->object_zone, &heap->constant_zone, &heap->code_zone };
    for (int z = 0; z < 3; z++) {
        for (uint32_t i = 0; i < zones[z]->page_count; i++) {
            free(zones[z]->pages[i]->start);
            free(zones[z]->pages[i]);
            zones[z]->pages[i] = NULL;
        }
    }
    free(heap);
}

// ---- heap_is_ptr : garde de debug/tests (le GC ne l'utilise plus) ----

static bool zone_contains(const MemoryZone* zone, const void* ptr) {
    for (uint32_t i = 0; i < zone->page_count; i++) {
        const MemoryPage* page = zone->pages[i];
        if ((const uint8_t*)ptr < page->start || (const uint8_t*)ptr >= page->free_ptr)
            continue;
        const ChunkHeader* h = (const ChunkHeader*)page->start;
        while ((const uint8_t*)h < page->free_ptr) {
            const uint8_t* data     = (const uint8_t*)(h + 1);
            const uint8_t* data_end = data + (h->size - sizeof(ChunkHeader));
            if ((const uint8_t*)ptr >= data && (const uint8_t*)ptr < data_end)
                return true;
            h = (const ChunkHeader*)((const uint8_t*)h + h->size);
        }
    }
    return false;
}

bool heap_is_ptr(const Heap* heap, const void* ptr) {
    if (ptr == NULL) return false;
    return zone_contains(&heap->object_zone, ptr)
        || zone_contains(&heap->constant_zone, ptr)
        || zone_contains(&heap->code_zone, ptr);
}

// ---- GC mark-and-sweep ----
// v0.3 : gc_mark prend un Value NaN-boxe. Le test de pointeur est
// O(1) (test de tag) : plus besoin du garde-fou heap_is_ptr dans le
// parcours, et plus de risque de dereferencer un entier brut.

void gc_mark(Heap* heap, Value v) {
    if (!VALUE_IS_PTR(v)) return;          // <-- O(1), remplace D5

    void* ptr = value_as_ptr(v);
    ChunkHeader* h = (ChunkHeader*)ptr - 1;
    if (h->marked) return;
    h->marked = 1;

    if (h->type == OBJ_ARRAY) {
        ArrayObject* arr = (ArrayObject*)ptr;
        for (int64_t i = 0; i < arr->length; i++)
            gc_mark(heap, arr->elements[i]);   // seuls les TAG_PTR descendent
    }
}

void gc_mark_roots(Heap* heap, const Value* roots, size_t root_count) {
    for (size_t i = 0; i < root_count; i++)
        gc_mark(heap, roots[i]);
}

void gc_sweep(Heap* heap) {
    MemoryZone* zones[3] = { &heap->object_zone, &heap->constant_zone, &heap->code_zone };
    for (int z = 0; z < 3; z++) {
        MemoryZone* zone = zones[z];
        for (uint32_t i = 0; i < zone->page_count; i++) {
            MemoryPage* page = zone->pages[i];
            ChunkHeader* h = (ChunkHeader*)page->start;
            while ((uint8_t*)h < page->free_ptr) {
                if (h->marked) h->marked = 0;
                h = (ChunkHeader*)((uint8_t*)h + h->size);
            }
        }
    }
}

void gc_collect(Heap* heap, const Value* roots, size_t root_count) {
    gc_mark_roots(heap, roots, root_count);
    gc_sweep(heap);
}
