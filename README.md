# minitalk-vm

A register-based bytecode virtual machine for the miniTalk dynamic language, written in pure C99 with zero external dependencies.

This VM is the runtime target of the [miniTalk](https://github.com/Dworkin92/minitalk-v3) project — a Smalltalk-like language implemented in Java. It is being built incrementally, from a validated minimal core toward a full dynamic-language VM.

## Features (v0.1)

- **Register-based bytecode**: fixed 16-byte instructions (`int64_t` immediate + opcode + 3 register operands + flags)
- **Heap with 3 zones** (object / constant / code), bump-pointer allocation, 16 MB pages allocated on demand
- **Self-describing chunks**: 8-byte `ChunkHeader` carries the total chunk size, so the GC walks the heap linearly (`h += h->size`)
- **Mark-and-sweep garbage collector** with a safe pointer check (`heap_is_ptr`) that only follows real heap pointers
- **Bytecode as heap objects**: `CodeObject` lives in the code zone, no `malloc` outside the heap
- **Test suite**: 10 built-in tests covering arithmetic, jumps/loops, heap allocation and GC (`run_all_tests()`)

## Build

Requires a C99 compiler (GCC, MinGW, Clang, MSVC works too).

```sh
gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c
```

Or open the Code::Blocks project file. There is a single `main()` in `src/main.c`.

## Run

```sh
./vm          # runs the test suite
```

## Project layout

```
src/
  types.h    Value and instruction formats, opcodes, constants
  memory.h/c Heap, zones, pages, chunk headers, allocator, GC
  vm.h/c     VM state and the interpreter loop
  loader.h/c Builds bytecode programs in the code zone
  utils.h    Shared helpers
  test.c     Test suite (run_all_tests)
  main.c     Entry point
docs/        Architecture notebook and development journal (French)
```

## Roadmap

1. **NaN-boxing** — tagged 64-bit `Value` (int / float / pointer / nil / bool), replacing the `heap_is_ptr` check with an O(1) tag test in `gc_mark`
2. Real sweep with free-list
3. Constant pool + `LOAD_CONST`
4. `CALL` / `RETURN` + call frames (GC roots: registers + stack) — possibly via a Lua-style register window
5. `NEW_ARRAY`, `GET_FIELD`, `SET_FIELD`, generic recursive marking
6. Later: compact 64-bit instruction encoding, dynamic dispatch, generational GC, frontend/parser

## Design decisions

The key architectural decisions (opcode encoding, 16-byte instruction layout, chunk header format, page tables, GC safety check) are documented in `docs/` — see the architecture notebook.

## License

Copyright (C) 2026 Jean-Marc de Lauwereyns.

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License v3** as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [LICENSE](LICENSE) file (or <https://www.gnu.org/licenses/>) for details.

---

---

# minitalk-vm (Français)

Machine virtuelle à bytecode à registres pour le langage dynamique miniTalk, écrite en C99 pur, sans aucune dépendance externe.

Cette VM est la cible d'exécution du projet [miniTalk](https://github.com/Dworkin92/minitalk-v3) — un langage de type Smalltalk implémenté en Java. Elle est construite de façon incrémentale : un noyau minimal validé d'abord, puis les extensions.

## Fonctionnalités (v0.1)

- **Bytecode à registres** : instructions fixes de 16 octets (immediate 64 bits + opcode + 3 registres + flags)
- **Heap à 3 zones** (objets / constantes / code), allocation bump-pointer, pages de 16 Mo allouées à la demande
- **Chunks auto-décrits** : `ChunkHeader` de 8 octets portant la taille totale, le GC parcourt donc le heap linéairement (`h += h->size`)
- **GC mark-and-sweep** avec garde-fou (`heap_is_ptr`) qui ne suit que les vrais pointeurs du heap
- **Bytecode = objet heap** : `CodeObject` dans la zone code, zéro `malloc` hors heap
- **Suite de tests** : 10 tests intégrés (arithmétique, sauts/boucles, allocation heap, GC) via `run_all_tests()`

## Compilation

Nécessite un compilateur C99 (GCC, MinGW, Clang).

```sh
gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c
```

Ou ouvrir le projet Code::Blocks. Un seul `main()`, dans `src/main.c`.

## Exécution

```sh
./vm          # lance la suite de tests
```

## Licence

Copyright (C) 2026 Jean-Marc de Lauwereyns.

Ce programme est un logiciel libre ; vous pouvez le redistribuer et/ou le modifier selon les termes de la **Licence Publique Générale GNU v3** publiée par la Free Software Foundation (version 3 ou, à votre choix, toute version ultérieure). Voir le fichier [LICENSE](LICENSE).