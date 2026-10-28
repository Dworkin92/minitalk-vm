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
// Contrat :
//  - tailles arrondies a 8 octets (alignement)
//  - header->size = taille TOTALE (header + donnees)
//  - le chunk suivant demarre a (header + header->size) : toujours aligne
//  - retourne NULL si la demande ne tient pas dans une page

void* heap_alloc(Heap* heap, MemoryZone* zone, size_t size, ObjectType type) {
    (void)heap; // reserve pour un futur suivi statistique
    size = (size + 7) & ~7;

    const size_t total = sizeof(ChunkHeader) + size;
    if (total > PAGE_SIZE) {
        fprintf(stderr, "heap: allocation de %zu octets > page (%d)\n",
                size, (int)PAGE_SIZE);
        return NULL;   // CORRECTIF : avant, on debordait de la page !
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

// ---- heap_is_ptr : le garde-fou du GC ----
// Parcourt les chunks de toutes les pages et verifie que ptr pointe
// dans la zone de DONNEES d'un chunk (pas dans un header, pas ailleurs).
// C'est ce qui evite au GC de dereferencer un entier brut (ex: 100).

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

void gc_mark(Heap* heap, void* ptr) {
    if (ptr == NULL) return;

    // CORRECTIF CRITIQUE : si ce n'est pas un pointeur du heap
    // (ex: un entier stocke dans un array), on ne le suit PAS.
    // Avant : gc_mark((void*)100) -> access violation 0xC0000005.
    if (!heap_is_ptr(heap, ptr)) return;

    ChunkHeader* h = (ChunkHeader*)ptr - 1;
    if (h->marked) return;
    h->marked = 1;

    // Enfants : seuls les arrays contiennent des references pour l'instant.
    // Chaque element est une Value ; on ne suit que les vrais pointeurs.
    if (h->type == OBJ_ARRAY) {
        ArrayObject* arr = (ArrayObject*)ptr;
        for (int64_t i = 0; i < arr->length; i++) {
            if (heap_is_ptr(heap, arr->elements))   // elements est un pointeur
                gc_mark(heap, arr->elements);        // le chunk elements lui-meme
            // Remarque : quand le NaN-boxing sera en place, on marquera
            // element par element selon le tag de chaque valeur.
        }
    }
}

void gc_mark_roots(Heap* heap, void** roots, size_t root_count) {
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
                if (h->marked) h->marked = 0;  // remet a zero pour le prochain GC
                // (le sweep "vrai" liberera ici via une free-list plus tard)
                h = (ChunkHeader*)((uint8_t*)h + h->size);
            }
        }
    }
}

void gc_collect(Heap* heap, void** roots, size_t root_count) {
    gc_mark_roots(heap, roots, root_count);
    gc_sweep(heap);
}
