//============================================================
// Fichier : types.h
//============================================================
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "value.h"   // Value (NaN-boxing) vit ici des la v0.3

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

typedef uint64_t Instruction;

#define INS_MAX_REG   15
#define INS_IMM_MIN   (-(int64_t)549755813888LL)  // -2^39
#define INS_IMM_MAX   ( (int64_t)549755813887LL)  //  2^39 - 1

// ---- Decodage (fonctions inline : pas de macro multi-ligne fragile) ----
#define INS_OPCODE(i) ((uint8_t)((i) & 0xFFu))
#define INS_REG1(i)   ((uint8_t)(((i) >> 8)  & 0xFu))
#define INS_REG2(i)   ((uint8_t)(((i) >> 12) & 0xFu))
#define INS_REG3(i)   ((uint8_t)(((i) >> 16) & 0xFu))
#define INS_FLAGS(i)  ((uint8_t)(((i) >> 20) & 0xFu))

// Extension de signe 40 bits portable : (v ^ S) - S, sans dependre
// du >> arithmetique (defini par l'implementation en C).
static inline int64_t ins_imm(Instruction i) {
    return (int64_t)((i >> 24) ^ 0x8000000000ULL) - 0x8000000000LL;
}
#define INS_IMM(i) ins_imm(i)

#endif
