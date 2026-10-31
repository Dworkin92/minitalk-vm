# myVM — Cahier d'architecture

> VM bytecode pour le langage dynamique miniTalk, en C pur (C99, aucune dépendance).
> Ce document décrit l'état **réellement implémenté et testé** (v0.1, 10/10/2026), **et** la direction validée : depuis le 28/10/2026, la VM évolue d'une VM à registres universels (v0.1) vers un modèle **hybride pile/registres à fenêtres façon Lua** (décision **D8**, spécifiée §10, non encore implémentée). Les sections décrivant l'existant restent celles de v0.1 tant que D8 n'est pas codée et testée.

## 1. Objectifs


| Caractéristique                             | Statut v0.1                                     | Cible (D8)                                                                                           |
| ------------------------------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Instructions **fixes**, taille déterministe | ✅ 16 octets                                     | idem d'abord, puis **encodage compact 64 bits** (révision D2)                                        |
| Pages mémoire de **16 Mo**                  | ✅                                               | inchangé                                                                                             |
| **Registres**                               | ✅ 32 registres universels                       | ⬜ remplacés par **fenêtre de 16 slots** par appel (opérande 4 bits) + `LOAD_EXT`/`STORE_EXT` au-delà |
| Value 64 bits avec **NaN-boxing**           | ⬜ à faire (`int64_t` brut pour l'instant)       | inchangé                                                                                             |
| Objets alloués dans le **heap**             | ✅ (chunks dans des pages)                       | inchangé                                                                                             |
| **Constant pool**                           | ⬜ zone réservée (`constant_zone`), pas d'opcode | inchangé                                                                                             |
| Modèle d'exécution                          | ✅ registres simples (noyau de base)             | ⬜ **hybride pile/registres inspiré de Lua 5** (fenêtre de registres, §10)                            |
| **GC mark-and-sweep**                       | ✅ phase mark + sweep (libération réelle ⬜)      | racines = pile d'exécution (au lieu des registres)                                                   |


## 2. Vue d'ensemble

**État v0.1 (implémenté)** :

```
┌──────────────────────────── VM ──────────────────────────────────┐
│  registers[32]   ip   code_object ──► CodeObject (code_zone)     │
│                                    └─► Instruction[] (code_zone) |
└──────────────────────────┬───────────────────────────────────────┘
                           │
                    ┌──────▼────-──┐
                    │    Heap      │
                    ├─────────-────┤
                    │ object_zone  │  objets dynamiques (strings, arrays…)
                    │ constant_zone│ constantes (réservée)
                    │ code_zone    │  CodeObject + bytecode
                    └──────┬────-──┘
              chaque zone : pages[1024] → MemoryPage (16 Mo)
              chaque page : chunks contigus [ChunkHeader | données]
```

**Cible D8 (à implémenter, détaillée §10)** — les `registers[32]` disparaissent au profit d'une pile d'exécution unique, découpée en fenêtres d'appel ; `ip` devient `pc` dans la structure VM ; le heap et ses zones ne changent pas.

## 3. Fichiers


| Fichier      | Rôle                                                        |
| ------------ | ----------------------------------------------------------- |
| `types.h`    | `Value`, `Instruction` (16 o), opcodes (`#define`)          |
| `memory.h/c` | Heap, zones, pages, chunks, `heap_alloc`, `heap_is_ptr`, GC |
| `vm.h/c`     | Structure `VM`, boucle d'exécution                          |
| `loader.h/c` | Chargement du bytecode en `CodeObject` (dans le heap)       |
| `utils.h`    | Macros `INST_*`, `CHECK`                                    |
| `test.c`     | 10 tests (`run_all_tests()`, appelé par `main`)             |
| `main.c`     | Seul point d'entrée                                         |


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

### 4.2 Instruction — 8 octets compactés (v0.2, validée le 31/10/2026 : 11/11 tests, MinGW)

```c
typedef uint64_t Instruction;   // un mot de 64 bits

// disposition : opcode 8 | reg1 4 | reg2 4 | reg3 4 | flags 4 | immediate 40
#define INS_OPCODE(i) ((uint8_t)((i) & 0xFFu))
#define INS_REG1(i)   ((uint8_t)(((i) >> 8)  & 0xFu))
#define INS_REG2(i)   ((uint8_t)(((i) >> 12) & 0xFu))
#define INS_REG3(i)   ((uint8_t)(((i) >> 16) & 0xFu))
#define INS_FLAGS(i)  ((uint8_t)(((i) >> 20) & 0xFu))
#define INS_IMM(i)    ((((int64_t)((i) >> 24) ^ 0x8000000000LL) - 0x8000000000LL)
```

**Décisions** (révision de D2, voir §9 item 1) :

- L'immédiat 40 bits est signé en complément à deux ; l'extension de signe au décodage est **portable** (`(v ^ S) - S` avec `S = 2^39`), sans dépendre du `>>` arithmétique sur signé (défini par l'implémentation en C). *Bug attrapé par la simulation JS avant l'écriture du C : sans cette précaution, `-4` était décodé `2^40-4`.*
- Registres adressables **0..15** (transition vers la fenêtre D8 — les 32 registres de la VM restent présents mais seuls les 16 premiers sont encodables).
- Bornes vérifiées à l'encodage par `ins_encode()` (opcode, regs, flags, immédiat) : hors bornes → diagnostic + `exit(1)`.
- Les macros `INST_*` deviennent des **assembleuses** (`ins_encode(...)`) : les programmes v0.1 sont inchangés (chantier à comportement constant).
- Historique v0.1 : instruction de 16 octets explicites (`int64_t immediate` + `opcode/reg1..3/flags` + `pad[3]`), remplacée par ce format.

**Décisions** :

- Les opcodes sont des **`#define`** (0–7), pas un `enum` : un `enum` vaut `int` (4 o) chez la plupart des compilateurs et gonflait la structure de façon imprévisible.
- `immediate` en tête : une struct contenant un `int64_t` est arrondie au multiple de 8 supérieur ; les 4 octets restants sont explicités (`flags` + `pad`) pour être **déterministe**.
- Les programmes sont écrits avec des **initializers désignés C99** (`{ .opcode = OP_ADD, .reg1 = 3, ... }`) : l'ordre des champs n'importe plus.

### 4.3 Opcodes (v0.1)


| Opcode       | Valeur | Sémantique                             |
| ------------ | ------ | -------------------------------------- |
| `OP_HALT`    | 0      | fin d'exécution                        |
| `OP_LOAD`    | 1      | `R[reg1] = immediate`                  |
| `OP_ADD`     | 2      | `R[reg1] = R[reg2] + R[reg3]`          |
| `OP_SUB`     | 3      | `R[reg1] = R[reg2] - R[reg3]`          |
| `OP_MUL`     | 4      | `R[reg1] = R[reg2] * R[reg3]`          |
| `OP_JUMP`    | 5      | `ip = immediate`                       |
| `OP_JUMP_IF` | 6      | `si R[reg1] == 0 alors ip = immediate` |
| `OP_PRINT`   | 7      | affiche `R[reg1]`                      |


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


| #   | Décision                                                                                        | Motif                                                                                                                                                                                                                                                                                                  |
| --- | ----------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| D1  | Opcodes en `#define`, `opcode: uint8_t`                                                         | taille déterministe de `Instruction`                                                                                                                                                                                                                                                                   |
| D2  | `Instruction` = 16 o explicites (`flags` + `pad`)                                               | struct avec `int64_t` ⇒ multiple de 8 inévitable ; autant l'expliciter                                                                                                                                                                                                                                 |
| D3  | `ChunkHeader.size` = taille **totale**                                                          | parcours GC trivial, pas de recalcul                                                                                                                                                                                                                                                                   |
| D4  | Tableau de pages plutôt que liste chaînée                                                       | O(1), borné, adapté au sweep. **Croissance par doublement** (31/10/2026) : le tableau devient `pages*` + `max_pages` (1024 au départ) ; à saturation, realloc ×2 et copie des pointeurs (coût : 8 Ko→16 Ko de pointeurs, microsecondes, les pages elles-mêmes ne bougent pas → aucun `Value` invalidé) |
| D5  | `heap_is_ptr` garde-fou du GC                                                                   | protège contre entiers bruts tant que pas de NaN-boxing                                                                                                                                                                                                                                                |
| D6  | Gros objets = **chunks multi-pages** (révisée le 31/10/2026)                                    | une allocation &gt; page occupe *n* slots consécutifs de la table de pages ; l'accès aux données passe par un adressage via la table (`big_chunk_addr`, O(1)) — aucune contiguïté d'adresse requise. Voir §11                                                                                          |
| D7  | Bytecode = objets dans `code_zone`                                                              | cohérence "tout est objet", métadonnées futures, libération unique                                                                                                                                                                                                                                     |
| D8  | **(décidée le 28/10/2026, non encore implémentée)** Fenêtre de registres à la Lua + CALL/RETURN | chaque fonction reçoit sa propre fenêtre de slots dans une pile d'exécution unique ; allocation de registres triviale (pile LIFO de slots, pas de coloration) ; fusionne le chantier CALL/RETURN + frames. Voir §10.                                                                                   |


**Notes sur D8** :

- Les `reg1..reg3` de l'instruction 16 octets actuelle gardent leur format ; leur **sémantique** devient « offset relatif à `base` » (offsets 0..15 adressent la fenêtre). L'encodage compact 64 bits (révision de D2) est désormais réalisé **avant** D8 : la fenêtre sera donc implémentée directement sur le format compact.
- Réduction convenue : **16 slots** par fenêtre (registre = 4 bits, un demi-octet) pour le futur encodage compact 64 bits, avec **échappatoire** `LOAD_EXT`/`STORE_EXT` (offset 16 bits) au-delà de 16 slots.
- Les 32 registres universels actuels (D2 sémantique) disparaîtront au profit de la pile ; `pc`, `base`, `top`, `stack_limit` vivent dans la structure `VM`, jamais dans les slots.

## 8. Limitations connues (v0.1)

- Sweep non destructif : pas de réutilisation mémoire (free-list à implémenter).
- `heap_is_ptr` coûteux (disparaîtra avec le NaN-boxing).
- Pas de `CALL`/`RETURN`, pas de pile de frames *(→ chantier D8, §10)*.
- `ArrayObject.elements` sont des `Value` brutes (int ou pointeur, indiscernables).
- Pas de flottants, pas de constant pool actif, pas de dispatch dynamique.

## 9. Feuille de route (ordre logique, révisé le 31/10/2026)

1. **Encodage compact 64 bits** (révision de D2) : `Instruction` = `uint64_t` (`opcode 8 | reg1..3 4 bits | flags 4 | immediate 40`), décodage dans `vm_step`, macros assembleuses. **Chantier à comportement constant : les 10 tests doivent repasser sans modification.** Fait en premier : chaque opcode futur n'est écrit qu'une seule fois, dans son format définitif.
2. **NaN-boxing** (`value.h`) : remplace D5 par un test de tag, prépare le dispatch typé.
3. **Free-list** : sweep qui rend réellement la mémoire.
4. **Constant pool + `LOAD_CONST`** (`constant_zone` déjà là) — devient aussi la porte de sortie des constantes trop grandes pour l'immédiat 40 bits.
5. **Fenêtre de registres (D8) + `CALL`/`RETURN`** : pile d'exécution, frames, `base`/`top`/`stack_limit` dans la VM, racines GC = pile.
6. **`NEW_ARRAY`, `GET_FIELD`, `SET_FIELD`** et objets du langage.
7. **Chunks multi-pages** (§11) : allocations &gt; 16 Mo via slots de table consécutifs + `big_chunk_addr` ; croissance de la table par doublement.

## 10. D8 — Fenêtre de registres (spécification à implémenter)

Modèle hybride pile/registres inspiré de Lua 5 : une **pile physique unique** de `Value`, découpée en fenêtres par appel de fonction.

**État VM** (jamais dans les slots) :

```c
typedef struct VM {
    Instruction* pc;       // compteur de programme (pointe dans code_zone)
    Value* stack;          // pile physique (grand tableau de Value)
    Value* base;           // debut de la fenetre de la frame courante
    Value* top;            // sommet des donnees utiles
    Value* stack_limit;    // garde de debordement
    Heap* heap;
} VM;
```

**Convention d'appel** :

- `CALL` : les *k* premiers slots de la fenêtre du callee = arguments ; vérification `base + max_stacksize(callee) < stack_limit` (sinon croissance de la pile).
- `RETURN` : valeur de retour déplacée, base restaurée, fenêtre recyclée.
- Slots = **données uniquement** (jamais de plomberie : la pile est déplaçable si elle grandit, et le GC ne doit jamais parser d'état interne).
- La frontière args / locaux / temporaires est une **convention du compilateur**, invisible pour la VM.

**Adressage** :

- Instructions normales : opérandes sur **4 bits** → slots 0–15 de la fenêtre (le cas courant).
- Au-delà de 15 : instructions étendues `LOAD_EXT`/`STORE_EXT` (offset 16 bits), émises par le compilateur si `max_stacksize > 16`.
- Le compilateur alloue les slots en pile LIFO (`next_slot++` / `--`) ; `max_stacksize` enregistré par prototype. Aucune analyse de durée de vie.

**Encodage compact 64 bits** (révision de D2, **désormais planifiée AVANT D8** — feuille de route §9) :

```
| immediate : 40 bits | flags : 4 | reg3 : 4 | reg2 : 4 | reg1 : 4 | opcode : 8 |
```

Constantes trop grandes pour 40 bits signés (±549 milliards) : via le constant pool (à venir).

## 11. Chunks multi-pages (spécification à implémenter)

Une allocation plus grande qu'une page n'est plus une erreur : elle occupe **n slots consécutifs de la table de pages** (contigus dans la table, *pas* en adresse — tout accès aux données passe par la table).

```
pages[k]   : ┌ ChunkHeader (size TOTALE, flags = n_pages)
             └ données... (jusqu'à la fin de la page)
pages[k+1] : 100% données
pages[k+2] : 100% données
```

**Allocation** (`heap_alloc`, révision de D6) :

- `size + 8 ≤ PAGE_SIZE` : bump-pointer dans une page, inchangé (chunks normaux) ;
- sinon : `n = ceil((size + 8) / PAGE_SIZE)` slots réservés (liste de slots libres issue du sweep), header sur la première page, `flags` du header porte `n_pages` (4 bits → max 15 pages ≈ 240 Mo par objet ; à étendre si besoin).

**Adressage — l'« algo interne »** (`big_chunk_addr`, O(1)) :

```c
uint8_t* big_chunk_addr(Zone* z, uint32_t first, size_t off) {
    size_t first_data = PAGE_SIZE - sizeof(ChunkHeader);
    if (off < first_data) return (uint8_t*)z->pages[first] + sizeof(ChunkHeader) + off;
    size_t rest = off - first_data;
    return (uint8_t*)z->pages[first + 1 + rest / PAGE_SIZE] + rest % PAGE_SIZE;
}
```

**Invariants préservés** :

- D3 : `size` du header reste **totale** ; le sweep des chunks normaux (`h += h->size`) est inchangé. Le sweep d'un gros chunk libère les *n* slots.
- GC mark : itération des éléments **via `big_chunk_addr`** — jamais d'arithmétique de pointeur nue au-delà de la première page (leçon du `0xC0000005`).
- À traiter au codage : `heap_is_ptr` doit reconnaître les pointeurs tombant dans une page dédiée à un gros chunk (recherche binaire sur les bases de pages triées + identification des pages dédiées).

**Croissance de la table** (D4) : `pages*` + `max_pages` (1024 au départ), doublement à saturation — realloc et copie de pointeurs uniquement, les pages ne bougent pas, aucun `Value` invalidé.

