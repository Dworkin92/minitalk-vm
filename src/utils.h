//============================================================
// Fichier : utils.h
//============================================================
#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <stdio.h>
#include <stdlib.h>

// ---- Assembleur : encodage d'une instruction compacte ----

static inline Instruction ins_encode(uint8_t op, uint8_t r1, uint8_t r2,
                                     uint8_t r3, uint8_t flags, int64_t imm) {
    if (op > 255) {
        fprintf(stderr, "ins_encode: opcode %u hors bornes\n", op);
        exit(1);
    }
    if (r1 > INS_MAX_REG || r2 > INS_MAX_REG || r3 > INS_MAX_REG) {
        fprintf(stderr, "ins_encode: registre > %d\n", INS_MAX_REG);
        exit(1);
    }
    if (flags > 15) {
        fprintf(stderr, "ins_encode: flags > 15\n");
        exit(1);
    }
    if (imm < INS_IMM_MIN || imm > INS_IMM_MAX) {
        fprintf(stderr, "ins_encode: immediate %lld hors bornes 40 bits "
                        "( passer par le constant pool )\n",
                (long long)imm);
        exit(1);
    }

    Instruction word = (Instruction)op
        | ((Instruction)r1 << 8)
        | ((Instruction)r2 << 12)
        | ((Instruction)r3 << 16)
        | ((Instruction)flags << 20)
        | (((Instruction)imm << 24) & 0xFFFFFFFFFF000000ULL);
    return word;
}

#define INST_LOAD(dst, imm)      ins_encode(OP_LOAD,   (dst), 0,    0,    0, (imm))
#define INST_ADD(dst, a, b)      ins_encode(OP_ADD,    (dst), (a),   (b),  0, 0)
#define INST_SUB(dst, a, b)      ins_encode(OP_SUB,    (dst), (a),   (b),  0, 0)
#define INST_MUL(dst, a, b)      ins_encode(OP_MUL,    (dst), (a),   (b),  0, 0)
#define INST_JUMP(addr)          ins_encode(OP_JUMP,   0,     0,    0,    0, (addr))
#define INST_JUMP_IF(reg, addr)   ins_encode(OP_JUMP_IF, (reg), 0,   0,    0, (addr))
#define INST_PRINT(reg)          ins_encode(OP_PRINT,  (reg), 0,    0,    0, 0)
#define INST_HALT                ins_encode(OP_HALT,  0,     0,    0,    0, 0)

// ---- Verification : FONCTION (pas de macro multi-ligne, insensible
//      aux copies/collers qui abiment les retro-lignes) ----

static inline void check_fail(const char* msg, const char* file, int line) {
    fprintf(stderr, "[ECHEC] %s -- %s:%d\n", msg, file, line);
    exit(1);
}

#define CHECK(cond, msg) ((cond) ? (void)0 : check_fail((msg), __FILE__, __LINE__))

#endif
