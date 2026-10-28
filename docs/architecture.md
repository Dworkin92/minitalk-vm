# myVM — Cahier d'architecture

> VM bytecode à registres pour un langage dynamique, en C pur (C99, aucune dépendance).
> Ce document décrit l'état **réellement implémenté et testé** (v0.1, 10/10/2026).

## 1. Objectifs

| Caractéristique | Statut v0.1 |
|---|---|
| Instructions **fixes**, taille déterministe | ✅ 16 octets |
| Pages mémoire de **16 Mo** | ✅ |
| **32 registres universels** | ✅ |
| Value 64 bits avec **NaN-boxing** | ⬜ à faire (`int64_t` brut pour l'instant) |
| Objets alloués dans le **heap** | ✅ (chunks dans des pages) |
| **Constant pool** | ⬜ zone réservée (`constant_zone`), pas d'opcode |
| VM à registres inspirée de **Lua** | ✅ noyau de base |
| **GC mark-and-sweep** | ✅ phase mark + sweep (libération réelle ⬜) |

## 2. Vue d'ensemble

```
┌──────────────────────────── VM ──────────────────────────────────┐
│  registers[32]   ip   code_object ──► CodeObject (code_zone)     │
│                                    └─► Instruction[] (code_zone) │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                    ┌──────▼───────┐
                    │    Heap      │
                    ├──────────────┤
                    │ object_zone  │  objets dynamiques (strings, arrays…)
                    │ constant_zone│ constantes (réservée)
                    │ code_zone    │  CodeObject + bytecode
                    └──────┬───────┘
              chaque zone : pages[1024] → MemoryPage (16 Mo)
              chaque page : chunks contigus [ChunkHeader | données]
```

## 3. Fichiers

| Fichier | Rôle |
|---|---|
| `types.h` | `Value`, `Instruction` (16 o), opcodes (`#define`) |
| `memory.h/c` | Heap, zones, pages, chunks, `heap_alloc`, `heap_is_ptr`, GC |
| `vm.h/c` | Structure `VM`, boucle d'exécution |
| `loader.h/c` | Chargement du bytecode en `CodeObject` (dans le heap) |
| `utils.h` | Macros `INST_*`, `CHECK` |
| `test.c` | 10 tests (`run_all_tests()`, appelé par `main`) |
| `main.c` | Seul point d'entrée |

Compilation : `gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c`

## 4. Formats binaires

### 4.1 ChunkHeader — 8 octets (testé par `test_struct_sizes`)

```c
typedef struct {
    uint32_t size;   // taille TOTALE du chunk (header + données)
    uint8_t  marked; // drapeau GC
    uint8_t  type;   // ObjectType
    uint16_t pad;    // alignement → sizeof == 8 garanti
} ChunkHeader;
```

**Invariants** :
- `size` est **totale** : le chunk suivant est à `(uint8_t*)h + h->size` (toujours aligné 8).
- Les tailles demandées sont arrondies à 8 octets par `heap_alloc`.
- Un pointeur de données est toujours `header + 1`.

### 4.2 Instruction — 16 octets (testé)

```c
typedef struct {
    int64_t immediate; // 8 o, aligné en tête → pas de padding surprise
    uint8_t opcode;
    uint8_t reg1, reg2, reg3;
    uint8_t flags;     // réservé (futur encodage 64 bits)
    uint8_t pad[3];    // → sizeof == 16 garanti
} Instruction;
```

**Décisions** :
- Les opcodes sont des **`#define`** (0–7), pas un `enum` : un `enum` vaut `int` (4 o) chez la plupart des compilateurs et gonflait la structure de façon imprévisible.
- `immediate` en tête : une struct contenant un `int64_t` est arrondie au multiple de 8 supérieur ; les 4 octets restants sont explicités (`flags` + `pad`) pour être **déterministe**.
- Les programmes sont écrits avec des **initializers désignés C99** (`{ .opcode = OP_ADD, .reg1 = 3, ... }`) : l'ordre des champs n'importe plus.

### 4.3 Opcodes (v0.1)

| Opcode | Valeur | Sémantique |
|---|---|---|
| `OP_HALT` | 0 | fin d'exécution |
| `OP_LOAD` | 1 | `R[reg1] = immediate` |
| `OP_ADD` | 2 | `R[reg1] = R[reg2] + R[reg3]` |
| `OP_SUB` | 3 | `R[reg1] = R[reg2] - R[reg3]` |
| `OP_MUL` | 4 | `R[reg1] = R[reg2] * R[reg3]` |
| `OP_JUMP` | 5 | `ip = immediate` |
| `OP_JUMP_IF` | 6 | `si R[reg1] == 0 alors ip = immediate` |
| `OP_PRINT` | 7 | affiche `R[reg1]` |

### 4.4 Types d'objets (`ObjectType`, 1 octet)

`OBJ_INT=0, OBJ_FLOAT, OBJ_STRING, OBJ_ARRAY, OBJ_OBJECT, OBJ_CODE`

Objets définis :

```c
typedef struct { int64_t length; Value* elements; } ArrayObject;
typedef struct { size_t length; Instruction* code; } CodeObject;
```

## 5. Modèle mémoire

### 5.1 Zones

Trois zones indépendantes dans le `Heap` : `object_zone`, `constant_zone` (réservée), `code_zone`.
Chaque zone est un **tableau de 1024 pointeurs de pages** (`pages[1024]`), allouées **à la demande** — pas de liste chaînée : accès O(1) à la page *i*, parcours GC simple et prévisible. Capacité max : 1024 × 16 Mo = **16 Go par zone**.

### 5.2 Allocation (`heap_alloc`) — bump-pointer

1. Taille arrondie à 8 octets ;
2. Si `sizeof(ChunkHeader) + size > PAGE_SIZE` → **retourne `NULL`** (et diagnostic sur stderr) ;
3. Parcours des pages existantes, `free_ptr` aligné sur 8, première page avec assez d'espace ;
4. Sinon nouvelle page (limite 1024 → erreur fatale).

### 5.3 GC mark-and-sweep

- **Racines** : passées explicitement (`gc_collect(heap, roots, n)`). À terme : registres VM + pile.
- **`gc_mark`** : ne suit un pointeur **que si `heap_is_ptr()` confirme** qu'il pointe dans les données d'un chunk du heap. C'est le garde-fou qui distingue entiers bruts et pointeurs **en attendant le NaN-boxing** (il supprimera les faux positifs du genre `data[i] = 100` traité comme un pointeur — cause du `0xC0000005` historique).
- **`gc_sweep`** : parcourt les chunks (`h += h->size`), remet `marked` à 0. ⚠️ Le sweep **ne libère pas encore** réellement : la mémoire n'est pas réutilisée (free-list à venir).
- Coût actuel : `heap_is_ptr` est O(chunks de la page) — acceptable pour valider, à remplacer par le NaN-boxing.

## 6. Bytecode comme objet

Le bytecode vit **entièrement dans le heap** (`code_zone`) :
- un chunk `CodeObject` (métadonnées) ;
- un chunk pour le tableau d'instructions.

Aucun `malloc` hors heap pour le code : libération unique via `heap_free()`. La VM référence le `CodeObject` ; `vm_run` exécute `code_object->code`.

## 7. Décisions d'architecture consignées

| # | Décision | Motif |
|---|---|---|
| D1 | Opcodes en `#define`, `opcode: uint8_t` | taille déterministe de `Instruction` |
| D2 | `Instruction` = 16 o explicites (`flags` + `pad`) | struct avec `int64_t` ⇒ multiple de 8 inévitable ; autant l'expliciter |
| D3 | `ChunkHeader.size` = taille **totale** | parcours GC trivial, pas de recalcul |
| D4 | Tableau de pages plutôt que liste chaînée | O(1), borné (1024), adapté au sweep |
| D5 | `heap_is_ptr` garde-fou du GC | protège contre entiers bruts tant que pas de NaN-boxing |
| D6 | Rejet `NULL` des allocations > page | un objet plus grand qu'une page est une erreur (pas de multi-pages en v0.x) |
| D7 | Bytecode = objets dans `code_zone` | cohérence "tout est objet", métadonnées futures, libération unique |

## 8. Limitations connues (v0.1)

- Sweep non destructif : pas de réutilisation mémoire (free-list à implémenter).
- `heap_is_ptr` coûteux (disparaîtra avec le NaN-boxing).
- Pas de `CALL`/`RETURN`, pas de pile de frames.
- `ArrayObject.elements` sont des `Value` brutes (int ou pointeur, indiscernables).
- Pas de flottants, pas de constant pool actif, pas de dispatch dynamique.

## 9. Feuille de route (ordre logique)

1. **NaN-boxing** (`value.h`) : remplace D5 par un test de tag, prépare le dispatch typé.
2. **Free-list** : sweep qui rend réellement la mémoire.
3. **Constant pool + `LOAD_CONST`** (`constant_zone` déjà là).
4. **`CALL`/`RETURN`** + pile de frames.
5. **`NEW_ARRAY`, `GET_FIELD`, `SET_FIELD`** et objets du langage.
