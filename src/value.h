//============================================================
// Fichier : value.h  (NOUVEAU v0.3)
//============================================================
#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- NaN-boxing : Value = uint64_t ----
//
//   double : bits natifs IEEE 754, ranges tels quels.
//            Un mot est un double si : exposant != 0x7FF,
//            OU mantisse nulle (+-infini), OU NaN canonique VALUE_QNAN.
//   taggue : espace quiet NaN (exposant 0x7FF, mantisse != 0) :
//            bits 50..48 = tag (3 bits), bits 47..0 = payload (48 bits).
//
//            tag 1 = INT   : entier signe 48 bits (+-2^47, complement a deux)
//            tag 2 = PTR   : pointeur de donnee heap (48 bits, < 2^48)
//            tag 3 = NIL, tag 4 = TRUE, tag 5 = FALSE (payload nul)
//
//   NB : int(0) = QNAN|(1<<48) != QNAN -> pas de collision avec le NaN
//        canonique (d'ou les tags a partir de 1, jamais 0).

typedef uint64_t Value;

#define VALUE_QNAN   0x7FF8000000000000ULL
#define VALUE_EXP    0x7FF0000000000000ULL
#define VALUE_MANT   0x000FFFFFFFFFFFFFULL
#define VALUE_PAY48  0x0000FFFFFFFFFFFFULL

#define VALUE_TAG_INT    1
#define VALUE_TAG_PTR    2
#define VALUE_TAG_NIL    3
#define VALUE_TAG_TRUE   4
#define VALUE_TAG_FALSE  5

#define VALUE_INT_MIN  (-(int64_t)140737488355328LL)  // -2^47
#define VALUE_INT_MAX  ( (int64_t)140737488355327LL)  //  2^47 - 1

#define VALUE_NIL    (VALUE_QNAN | ((uint64_t)VALUE_TAG_NIL   << 48))
#define VALUE_TRUE   (VALUE_QNAN | ((uint64_t)VALUE_TAG_TRUE  << 48))
#define VALUE_FALSE  (VALUE_QNAN | ((uint64_t)VALUE_TAG_FALSE << 48))

// ---- Tests de type : O(1), sans parcours du heap (remplacent heap_is_ptr) ----
// NB : ce sont des fonctions (pas des macros multi-lignes) -> insensibles
//      aux copies/collers qui coupent les lignes.

#define VALUE_TAG(v)      ((int)(((v) >> 48) & 0x7))
#define VALUE_PAYLOAD(v)  ((v) & VALUE_PAY48)

static inline bool value_is_double(Value v) {
    return ((v & VALUE_EXP) != VALUE_EXP)
        || ((v & VALUE_MANT) == 0)
        || v == VALUE_QNAN;
}

static inline bool value_is_int(Value v) {
    return !value_is_double(v) && VALUE_TAG(v) == VALUE_TAG_INT;
}

static inline bool value_is_ptr(Value v) {
    return !value_is_double(v) && VALUE_TAG(v) == VALUE_TAG_PTR;
}

#define VALUE_IS_DOUBLE(v)  value_is_double(v)
#define VALUE_IS_INT(v)     value_is_int(v)
#define VALUE_IS_PTR(v)     value_is_ptr(v)
#define VALUE_IS_NIL(v)     ((v) == VALUE_NIL)
#define VALUE_IS_BOOL(v)    ((v) == VALUE_TRUE || (v) == VALUE_FALSE)

// ---- Encodeurs ----

static inline Value value_int(int64_t i) {
    if (i < VALUE_INT_MIN || i > VALUE_INT_MAX) {
        fprintf(stderr, "value_int: %lld hors bornes 48 bits "
                        "( passer par un objet heap )\n", (long long)i);
        exit(1);
    }
    return VALUE_QNAN | ((uint64_t)VALUE_TAG_INT << 48)
         | ((uint64_t)i & VALUE_PAY48);
}

static inline Value value_ptr(void* p) {
    uint64_t a = (uint64_t)(uintptr_t)p;
    if (a > VALUE_PAY48) {
        fprintf(stderr, "value_ptr: adresse %p hors de la plage 48 bits\n", p);
        exit(1);
    }
    return VALUE_QNAN | ((uint64_t)VALUE_TAG_PTR << 48) | a;
}

static inline Value value_double(double d) {
    Value v;
    memcpy(&v, &d, sizeof v);
    // Tout NaN devient le NaN canonique (sinon ses bits ressembleraient a un tag)
    if (((v & VALUE_EXP) == VALUE_EXP) && (v & VALUE_MANT) != 0)
        return VALUE_QNAN;
    return v;
}

// ---- Decodeurs ----
// Extension de signe 48 bits portable : (p ^ S) - S, sans dependre
// du >> arithmetique (defini par l'implementation en C).

static inline int64_t value_as_int(Value v) {
    uint64_t p = VALUE_PAYLOAD(v);
    return (int64_t)((p ^ 0x800000000000ULL) - 0x800000000000ULL);
}

static inline void* value_as_ptr(Value v) {
    return (void*)(uintptr_t)VALUE_PAYLOAD(v);
}

static inline double value_as_double(Value v) {
    double d;
    memcpy(&d, &v, sizeof d);
    return d;
}

// ---- Semantique "faux" : nil, false, entier 0 ----
static inline bool value_is_falsy(Value v) {
    return VALUE_IS_NIL(v) || v == VALUE_FALSE
        || (VALUE_IS_INT(v) && value_as_int(v) == 0);
}

// ---- Affichage ASCII selon le type ----
static inline void value_print(Value v) {
    if (VALUE_IS_NIL(v))        printf("nil");
    else if (v == VALUE_TRUE)   printf("true");
    else if (v == VALUE_FALSE)  printf("false");
    else if (VALUE_IS_INT(v))   printf("%lld", (long long)value_as_int(v));
    else if (VALUE_IS_DOUBLE(v))printf("%g", value_as_double(v));
    else if (VALUE_IS_PTR(v))   printf("ptr:%p", value_as_ptr(v));
    else                        printf("<inconnu>");
}

#endif
