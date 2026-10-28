
# myVM — Journal de développement

> Journal minimal, mis à jour à chaque séance.
> Convention : les entrées les plus récentes en **tête** de liste.

---

## Réalisé (état courant)

**v0.1 — VM minimale fonctionnelle, 10 tests OK** *(code publié sur GitHub : `Dworkin92/minitalk-vm`)*

- VM à registres (32), bytecode en instructions fixes de 16 octets.
- Opcodes : `HALT, LOAD, ADD, SUB, MUL, JUMP, JUMP_IF, PRINT`.
- Heap en 3 zones (objets / constantes / code), tableau de 1024 pages de 16 Mo par zone, allocation à la demande.
- Chunks contigus avec `ChunkHeader` de 8 octets (`size` totale), alignement 8 octets.
- Bytecode chargé comme objet (`CodeObject` + instructions dans `code_zone`), exécution validée (`R3 = 30`, boucle somme 0→5 = 15).
- GC mark-and-sweep avec garde-fou `heap_is_ptr` (racines explicites).
- 10 tests unitaires, sortie détaillée, tous verts.

---

## Historique

### Vendredi 10 octobre 2026

- **Cadrage** : VM à registres inspirée de Lua, instructions 64 bits prévues, pages 16 Mo, 32 registres, NaN-boxing, constant pool, GC mark-and-sweep. Choix : partir sur une version minimale (entiers + opérations de base), C pur sans GLib.
- **VM minimale** : `types.h`, `vm.h/c`, `loader.h/c`, `utils.h`, programme d'exemple `R3 = 10 + 20` → OK.
- **Ajout `MUL`**, sauts `JUMP`/`JUMP_IF`, premiers tests (`test.c`).
- **Modèle mémoire revu** : abandon de la liste chaînée de pages au profit d'un tableau de 1024 pointeurs de pages par zone (accès O(1), parcours GC simple), chunks contigus pour le GC. Ajout des zones `constant_zone` puis `code_zone`.
- **Bytecode = objet** : `CodeObject` dans `code_zone`, le bytecode devient un objet à part entière (préparation des métadonnées futures : noms de fonctions, infos de debug).
- **Debugging (séance chargée)** — trois bugs réels trouvés et corrigés :
  1. *Sortie mojibake* (`Cha├«ne`) : encodage console Windows, réglé côté terminal.
  2. *`0xC0000005` (access violation)* : `gc_mark` déréférençait des **entiers bruts** (`array->data[i] = 100`) comme des pointeurs → ajout de `heap_is_ptr()`, le GC ne suit que les vrais pointeurs du heap.
  3. *Débordement silencieux* : une allocation > 16 Mo débordait de la page → `heap_alloc` retourne `NULL` + diagnostic.
- **Leçons de structuration** :
  - `enum` opcode = 4 octets chez MinGW → opcodes en `#define`, champ `opcode` en `uint8_t`.
  - `sizeof(Instruction)` = 16, pas 12 : une struct avec `int64_t` est arrondie au multiple de 8 → champ `flags` + `pad[3]` explicites, taille déterministe testée.
  - `ChunkHeader.size` = **taille totale** (header + données) → test d'en-tête corrigé en conséquence.
  - Macros `INST_*` passées en **initializers désignés C99** (ordre des champs immaterial).
  - Un seul `main()` (dans `main.c`) ; `test.c` expose `run_all_tests()`.
- **Tests restructurés** : 10 tests, message par test (`[n] nom ... OK`), `CHECK` avec fichier/ligne. Un bug dans le programme de test de boucle lui-même (`R1 += R2` au lieu de `R1 += 1`) a été attrapé par simulation avant livraison.
- **Résultat final** : compilation MinGW propre, 10/10 tests OK, GC sans crash, boucle bytecode validée.
- **Documentation** : création du cahier d'architecture et de ce journal.


### Mercredi 28 octobre 2026

- **Publication du code** : dépôt GitHub **`minitalk-vm`** créé (nom : cohérence `minitalk-*`, composant après projet). Contenu : `src/` (10 fichiers v0.1), README bilingue (EN principal / FR), licence **GPLv3**, `.gitignore` (`bin/`, `obj/`, `*.depend`, `*.layout`). Premier commit `v0.1`, push en **SSH** (remote initialement en HTTPS, corrigé par `git remote set-url`).
- **Débat d'architecture : registres vs pile** — doute légitime sur la complexité du compilateur (allocation de registres par coloration). Étude du modèle **Lua 5 : fenêtre de registres** = pile physique unique découpée en tranches par appel. Bénéfice clé : bytecode dense d'une VM à registres, mais allocation de registres **triviale** (pile LIFO de slots, aucune analyse de durée de vie) ; `CALL`/`RETURN` = création/destruction de la fenêtre.
- **Décision D8 arbitrée** (consignée au cahier, §10) : fenêtre de registres + `CALL`/`RETURN` fusionnés en un seul chantier. Points clés :
  - `pc`, `base`, `top`, `stack_limit` dans la structure VM, **jamais** dans les slots ;
  - slots = données uniquement (arguments en tête, puis locaux, puis temporaires — convention du compilateur, invisible pour la VM) ;
  - **16 slots** par fenêtre (opérande 4 bits, préparation de l'encodage 64 bits compact) + échappatoire `LOAD_EXT`/`STORE_EXT` (offset 16 bits) au-delà de 15 ;
  - garde de débordement à chaque `CALL` (`base + max_stacksize(callee) < stack_limit`, croissance de la pile sinon) ;
  - les 32 registres universels actuels disparaîtront au profit de la pile ; l'instruction 16 octets est conservée telle quelle dans un premier temps (seule la sémantique des opérandes change : offsets relatifs à `base`).
- **Plan réordonné** : valider la fenêtre avec l'instruction actuelle **avant** l'encodage compact 64 bits (un chantier à la fois, sinon les bugs se masquent mutuellement).

### Vendredi 10 octobre 2026

- **Cadrage** : VM à registres inspirée de Lua, instructions 64 bits prévues, pages 16 Mo, 32 registres, NaN-boxing, constant pool, GC mark-and-sweep. Choix : partir sur une version minimale (entiers + opérations de base), C pur sans GLib.
- **VM minimale** : `types.h`, `vm.h/c`, `loader.h/c`, `utils.h`, programme d'exemple `R3 = 10 + 20` → OK.
- **Ajout `MUL`**, sauts `JUMP`/`JUMP_IF`, premiers tests (`test.c`).
- **Modèle mémoire revu** : abandon de la liste chaînée de pages au profit d'un tableau de 1024 pointeurs de pages par zone (accès O(1), parcours GC simple), chunks contigus pour le GC. Ajout des zones `constant_zone` puis `code_zone`.
- **Bytecode = objet** : `CodeObject` dans `code_zone`, le bytecode devient un objet à part entière (préparation des métadonnées futures : noms de fonctions, infos de debug).
- **Debugging (séance chargée)** — trois bugs réels trouvés et corrigés :
  1. *Sortie mojibake* (`Cha├«ne`) : encodage console Windows, réglé côté terminal.
  2. *`0xC0000005` (access violation)* : `gc_mark` déréférençait des **entiers bruts** (`array->data[i] = 100`) comme des pointeurs → ajout de `heap_is_ptr()`, le GC ne suit que les vrais pointeurs du heap.
  3. *Débordement silencieux* : une allocation &gt; 16 Mo débordait de la page → `heap_alloc` retourne `NULL` + diagnostic.
- **Leçons de structuration** :
  - `enum` opcode = 4 octets chez MinGW → opcodes en `#define`, champ `opcode` en `uint8_t`.
  - `sizeof(Instruction)` = 16, pas 12 : une struct avec `int64_t` est arrondie au multiple de 8 → champ `flags` + `pad[3]` explicites, taille déterministe testée.
  - `ChunkHeader.size` = **taille totale** (header + données) → test d'en-tête corrigé en conséquence.
  - Macros `INST_*` passées en **initializers désignés C99** (ordre des champs immaterial).
  - Un seul `main()` (dans `main.c`) ; `test.c` expose `run_all_tests()`.
- **Tests restructurés** : 10 tests, message par test (`[n] nom ... OK`), `CHECK` avec fichier/ligne. Un bug dans le programme de test de boucle lui-même (`R1 += R2` au lieu de `R1 += 1`) a été attrapé par simulation avant livraison.
- **Résultat final** : compilation MinGW propre, 10/10 tests OK, GC sans crash, boucle bytecode validée.
- **Documentation** : création du cahier d'architecture et de ce journal.

---

## Reste à faire

### Prioritaire

1. **NaN-boxing** (`value.h`) : `Value` = `uint64_t` taggué (entier / flottant / pointeur / nil / bool). Remplace `heap_is_ptr` par un test de tag O(1) dans `gc_mark`. Prépare le dispatch typé des opcodes.
2. **Sweep réel / free-list** : réutiliser les chunks non marqués (le sweep actuel ne rend pas la mémoire).

### Ensuite

3. **Constant pool** : activer `constant_zone` + opcode `LOAD_CONST` (index dans le pool).
4. **Fenêtre de registres (D8) + `CALL`/`RETURN`** : pile d'exécution, frames, `base`/`top`/`stack_limit` dans la VM, 16 slots par fenêtre + `LOAD_EXT`/`STORE_EXT`, racines GC = pile. Chantier fusionné — voir cahier §10.

### Plus tard

5. **Objets du langage** : `NEW_ARRAY`, `GET_FIELD`, `SET_FIELD` ; marquage récursif générique des champs par objet.
6. Instructions 64 bits compactes (encodage binaire `opcode 8 | reg 3×4 | flags 4 | immediate 40` — révision de D2, après validation de la fenêtre).
7. Dispatch dynamique + tables de méthodes (nature dynamique du langage).
8. Optimisations GC : générationnel, incrémental.
9. Frontend : parseur + compilateur AST → bytecode, format de fichier binaire (magic, version, constant pool, code).

---

*Mise à jour suivante : session NaN-boxing.*
---

