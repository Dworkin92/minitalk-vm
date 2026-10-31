// myVM v0.2 - encodage compact 64 bits (revision de D2)
// Chantier a comportement constant : les programmes et les tests v0.1 sont inchanges.
// 10 fichiers : types.h memory.h memory.c vm.h vm.c loader.h loader.c utils.h test.c main.c
// Compilation (MinGW/Code::Blocks) :
//   gcc -std=c99 -Wall -Wextra -O0 -o vm src/*.c
// ATTENTION : il n'y a QU'UN main() (dans main.c). test.c n'a pas de main.
//
// CHANGEMENTS v0.2 :
//   - Instruction = uint64_t (8 octets) au lieu d'une struct de 16 octets
//   - Disposition : opcode 8b | reg1 4b | reg2 4b | reg3 4b | flags 4b | immediate 40b
//   - Registres adressables 0..15 (transition vers D8 : fenetre de 16 slots)
//   - Immediate signe 40 bits : -2^39 .. 2^39-1 (les constantes plus grandes
//     passeront par le constant pool, a venir)
//   - Macros INST_* = assembleuses de mots 64 bits ; decodage dans vm_run
//   - Nouveau test 2 : round-trip encodage/decodage (11 tests au total)

//============================================================
// Fichier : types.h
//============================================================
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Valeur 64 bits (future base du NaN-boxing)
typedef int64_t Value;

#define NUM_REGISTERS 32
typedef Value Register[NUM_REGISTERS];

// Opcodes : #define explicites (1 octet dans l'instruction compacte)
#define OP_HALT     0
#define OP_LOAD     1
#define OP_ADD      2
#define OP_SUB      3
#define OP_MUL      4
#define OP_JUMP     5
#define OP_JUMP_IF  6
#define OP_PRINT    7

// ---- Instruction compacte : 64 bits (D2 revisee, v0.2) ----
//
//  bits 63..24 : immediate (40 bits, signe, complement a deux)
//  bits 23..20 : flags (4 bits, reserve)
//  bits 19..16 : reg3 (4 bits)
//  bits 15..12 : reg2 (4 bits)
//  bits 11..8  : reg1 (4 bits)
//  bits  7..0  : opcode (8 bits)
//
// Registres adressables : 0..15 (preparation de D8, fenetre de 16 slots).
// Immediate : -2^39 .. 2^39-1, soit +/- 549 755 813 888 environ.

typedef uint64_t Instruction;

#define INS_MAX_REG   15
#define INS_IMM_MIN   (-(int64_t)549755813888LL)  // -2^39
#define INS_IMM_MAX   ( (int64_t)549755813887LL)  //  2^39 - 1

// ---- Decodage (macros pures, sans fonction : utilisables partout) ----
// NB : extension de signe 40 bits portable via (v ^ S) - S,
//      sans dependre du >> arithmetique (implementation-defini en C).

#define INS_OPCODE(i) ((uint8_t)((i) & 0xFFu))
#define INS_REG1(i)   ((uint8_t)(((i) >> 8)  & 0xFu))
#define INS_REG2(i)   ((uint8_t)(((i) >> 12) & 0xFu))
#define INS_REG3(i)   ((uint8_t)(((i) >> 16) & 0xFu))
#define INS_FLAGS(i)  ((uint8_t)(((i) >> 20) & 0xFu))
#define INS_IMM(i)    ((((int64_t)((i) >> 24) ^ 0x8000000000LL) - 0x8000000000LL))

#endif
