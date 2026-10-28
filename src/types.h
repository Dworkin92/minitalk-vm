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

// Opcodes : #define explicites (1 octet garanti dans Instruction)
#define OP_HALT     0
#define OP_LOAD     1
#define OP_ADD      2
#define OP_SUB      3
#define OP_MUL      4
#define OP_JUMP     5
#define OP_JUMP_IF  6
#define OP_PRINT    7

// Instruction de 16 octets, deterministe :
//   8 (immediate, aligne) + 1 opcode + 3 regs + 1 flags + 3 padding = 16
// NB : une structure contenant un int64_t fait toujours un multiple de 8,
// donc 12 octets est impossible sans __attribute__((packed)).
typedef struct {
    int64_t immediate;  // 8 octets
    uint8_t opcode;     // 1 octet
    uint8_t reg1;       // 1 octet
    uint8_t reg2;       // 1 octet
    uint8_t reg3;       // 1 octet
    uint8_t flags;      // 1 octet (reserve pour plus tard)
    uint8_t pad[3];     // 3 octets -> total 16, sans surprise de compilateur
} Instruction;

#endif
