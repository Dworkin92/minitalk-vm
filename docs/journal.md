# myVM — Journal de développement

> Journal minimal, mis à jour à chaque séance.
> Convention : les entrées les plus récentes en **tête** de liste.

---

## Réalisé (état courant)

**v0.3 — NaN-boxing des valeurs, 12 tests OK** *(validé sur MinGW le 31/10/2026 ; `Value` = 64 bits taggués, GC à test de tag O(1))*

**v0.2 — encodage compact 64 bits, 11 tests OK** *(validé sur MinGW le 31/10/2026, compilation sans warning du premier coup ; code publié sur GitHub : `Dworkin92/minitalk-vm`)*

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

### Samedi 31 octobre 2026 — séance 3 (v0.3 : NaN-boxing)

- **Chantier 2 réalisé : NaN-boxing des valeurs** (`value.h`, nouveau fichier — le projet passe à 11 fichiers) :
  - `Value` = `uint64_t` : les **doubles** vivent dans leurs bits natifs IEEE 754 ; int 48 bits signés, pointeur heap, nil, true, false vivent dans l'espace **quiet NaN** (tag sur 3 bits en 50..48, payload 48 bits) ;
  - `gc_mark` prend un `Value` et ne suit que les `TAG_PTR` : **test O(1)** — le garde-fou `heap_is_ptr` (D5) quitte le chemin du GC (conservé pour tests/debug) ;
  - `gc_mark_roots`/`gc_collect` prennent des tableaux de `Value` (`value_ptr(...)` pour les racines) ;
  - registres VM = `Value` taggués ; `JUMP_IF` saute sur nil/false/int 0 (`value_is_falsy`) ; `PRINT` affiche selon le type ; programmes v0.2 inchangés ;
  - tests remaniés (racines en `Value`, array mixte int+ptr) + **nouveau test 3 `test_nan_boxing`** : round-trips int/ptr/double/nil/bool, bornes ±2^47, NaN canonisé, infini, falsy → **12 tests au total**.
- **Validation par simulation JS** — deux cas limites attrapés avant l'écriture du C :
  1. **les infinis** (`0x7FF0000000000000`, exposant 0x7FF + mantisse nulle) tombaient à tort dans l'espace taggué → clause « mantisse nulle = double » ajoutée au test de type ;
  2. vérification qu'aucune collision `int(0)`/NaN canonique n'existe (résolue par des tags commençant à 1, jamais 0).
  - Aussi validés : canonisation de tout NaN en `VALUE_QNAN`, rejet des pointeurs ≥ 2^48, extension de signe 48 bits (même formule portable que v0.2), GC qui marque uniquement le ptr d'un array mixte, boucle VM = 15 avec registres taggués.
- **Correction post-livraison (copier/coller)** : premiers essais de compilation côté Jean-Marc en échec — le canvas source est intègre (vérifié octet par octet), ce sont les **rétro-lignes des macros multi-lignes** (`CHECK`, `VALUE_IS_DOUBLE`) et une longue ligne de macro (`INS_IMM`) qui ont été abîmées à la copie dans l'éditeur. Durcissement définitif : toutes les macros multi-lignes remplacées par des **fonctions `static inline`** (`check_fail` + alias `CHECK` mono-ligne, `value_is_double/int/ptr` + alias courts, `ins_imm` + alias `INS_IMM`) → le code n'a plus **aucune** rétro-ligne fragile. Leçon consignée : zéro macro multi-lignes dans ce projet (la console/copie Windows de Jean-Marc n'est pas fiable pour elles).
- Code complet livré dans le canvas **« myVM v0.3 – NaN-boxing des valeurs »** (version durcie). Cahier : §4.5 ajouté (« écrit, en attente de validation »), tableaux fichiers/objectifs mis à jour.
- **À faire côté Jean-Marc** : ✅ **fait** — après recopie de la version durcie : compilation MinGW propre, **12/12 tests OK**, `sizeof(Value) = 8`, démo `Affichage type : 42 3.5 nil true`, array mixte `[100, ptr, 200]` GC-sans-crash. Commit v0.3 + push à effectuer.

### Samedi 31 octobre 2026 — séance 2 (v0.2 : encodage compact)

- **Chantier 1 réalisé : encodage compact 64 bits** (révision de D2, à comportement constant — programmes et tests v0.1 inchangés) :
  - `Instruction` = `uint64_t` : `opcode 8 | reg1 4 | reg2 4 | reg3 4 | flags 4 | immediate 40` ; registres adressables 0..15 (transition D8) ;
  - décodage par macros `INS_*` dans `types.h`, exécution dans `vm_run` (aucune struct reconstituée) ; extension de signe 40 bits **portable** (`(v ^ S) - S`) ;
  - `ins_encode()` valide toutes les bornes (opcode, regs ≤ 15, flags ≤ 15, immédiat 40 bits signé) ; les macros `INST_*` deviennent des assembleuses — **les programmes des tests 9/10 et de `main` n'ont pas une ligne de changée** ;
  - nouveau test 2 `test_instruction_encoding` : round-trip champs, immédiat négatif, bornes `INS_IMM_MIN/MAX`, cohérence des 8 macros → **11 tests au total**.
- **Validation par simulation JS** (pas de compilateur C ici) :
  - **bug réel attrapé avant l'écriture du C** : l'extension de signe de l'immédiat n'était pas faite au décodage (`-4` décodé `2^40-4`) → corrigé par la formule portable ;
  - round-trip complet OK, rejets hors bornes OK, programmes addition (R3=30) et boucle (somme=15) exécutés en encodé/décodé → identiques.
- Code complet des 10 fichiers livré dans le canvas **« myVM v0.2 – encodage compact 64 bits »**.
- **À faire côté Jean-Marc** : ✅ **fait** — compilation MinGW sans warning du premier coup, `sizeof(Instruction) = 8`, **11/11 tests OK** (le diagnostic `allocation > page` du test 5 étant le comportement attendu). Commit v0.2 + push à effectuer.
- Retravail du début du cahier (déjà acté plus haut dans la journée) : transition v0.1 → D8 explicite, tableau d'objectifs à 3 colonnes. **§4.2 mis à jour** : format compact documenté comme « écrit, en attente de validation ».

### Samedi 31 octobre 2026

- **Réordonnancement de la feuille de route** : l'**encodage compact 64 bits** (révision de D2) passe en **chantier 1**, avant le NaN-boxing. Motif (décision de Jean-Marc, validée ensemble) : chaque opcode ajouté par les chantiers futurs (LOAD\_CONST, CALL, NEW\_ARRAY…) ne sera écrit qu'**une seule fois, dans son format définitif** — sinon il faudrait tout réécrire au moment d'une bascule tardive. L'encodage est orthogonal aux autres chantiers (Value, mémoire, appels) donc aucun ne le bloque et il n'en bloque aucun.
- **Plan v0.2 — encodage compact** :
  - `Instruction` devient un `uint64_t` : `opcode 8 | reg1 4 | reg2 4 | reg3 4 | flags 4 | immediate 40` ;
  - décodage dans `vm_step` (pas de struct reconstituée) ; macros `INST_*` deviennent des assembleuses de mots 64 bits ;
  - **critère de réussite : les 10 tests existants repassent sans modification** (chantier à comportement constant) ;
  - ajout d'un test round-trip encode/decode de chaque champ (immédiat négatif inclus) ;
  - vigilance : immédiat limité à 40 bits signés (±549 755 813 888) — les constantes plus grandes passeront par le constant pool (à venir, devenu aussi la porte de sortie des gros immediats).
- Retravail du **début du cahier d'architecture** (incohérent avec D8) : en-tête explicite sur la transition registres universels → fenêtre Lua, tableau d'objectifs à 3 colonnes (Statut v0.1 / Cible D8), vue d'ensemble étiquetée v0.1 + paragraphe cible D8.

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

1. **Encodage compact 64 bits** (révision de D2, chantier v0.2) : `Instruction` = `uint64_t` (`opcode 8 | reg 3×4 | flags 4 | immediate 40`), décodage dans `vm_step`, macros assembleuses, test round-trip. Critère : les 10 tests repassent sans modification. *(Fait en premier : chaque opcode futur n'est écrit qu'une fois, dans son format définitif.)*
2. **NaN-boxing** (`value.h`) : `Value` = `uint64_t` taggué (entier / flottant / pointeur / nil / bool). Remplace `heap_is_ptr` par un test de tag O(1) dans `gc_mark`. Prépare le dispatch typé des opcodes.
3. **Sweep réel / free-list** : réutiliser les chunks non marqués (le sweep actuel ne rend pas la mémoire).

### Ensuite

4. **Constant pool** : activer `constant_zone` + opcode `LOAD_CONST` (index dans le pool) — porte de sortie des constantes trop grandes pour l'immédiat 40 bits.
5. **Fenêtre de registres (D8) + `CALL`/`RETURN`** : pile d'exécution, frames, `base`/`top`/`stack_limit` dans la VM, 16 slots par fenêtre + `LOAD_EXT`/`STORE_EXT`, racines GC = pile. Chantier fusionné — voir cahier §10.

### Plus tard

6. **Objets du langage** : `NEW_ARRAY`, `GET_FIELD`, `SET_FIELD` ; marquage récursif générique des champs par objet.
7. Dispatch dynamique + tables de méthodes (nature dynamique du langage).
8. Optimisations GC : générationnel, incrémental.
9. Frontend : parseur + compilateur AST → bytecode, format de fichier binaire (magic, version, constant pool, code).

---

*Mise à jour suivante : validation v0.3 (NaN-boxing), puis chantier 3 : sweep réel / free-list.*
