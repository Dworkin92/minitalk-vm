//============================================================
// Fichier : utils.h
//============================================================
#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <stdio.h>
#include <stdlib.h>

// Initializers designes (C99) : ordre des champs sans importance.
#define INST_LOAD(dst, imm)        { .opcode = OP_LOAD,   .reg1 = (dst), .immediate = (imm) }
#define INST_ADD(dst, a, b)        { .opcode = OP_ADD,    .reg1 = (dst), .reg2 = (a), .reg3 = (b) }
#define INST_SUB(dst, a, b)        { .opcode = OP_SUB,    .reg1 = (dst), .reg2 = (a), .reg3 = (b) }
#define INST_MUL(dst, a, b)        { .opcode = OP_MUL,    .reg1 = (dst), .reg2 = (a), .reg3 = (b) }
#define INST_JUMP(addr)            { .opcode = OP_JUMP,   .immediate = (addr) }
#define INST_JUMP_IF(reg, addr)    { .opcode = OP_JUMP_IF, .reg1 = (reg), .immediate = (addr) }
#define INST_PRINT(reg)            { .opcode = OP_PRINT,  .reg1 = (reg) }
#define INST_HALT                  { .opcode = OP_HALT }

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "[ECHEC] %s -- %s:%d\n", msg, __FILE__, __LINE__); \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

#endif
