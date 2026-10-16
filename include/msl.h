// As funções da biblioteca C (MSL, da Metrowerks) que o código do jogo chama.
// Só as declarações: as funções já estão no jogo (veja docs/BUILD.md, o MSL ligado).
#ifndef MSL_H
#define MSL_H

#ifdef __MWERKS__
typedef unsigned long size_t;

// stdarg do MSL no ARM: o va_list aponta para a pilha, logo depois do último
// parâmetro fixo (alinhado em 4)
typedef char* va_list;
#define __alinha4(n) ((((unsigned long)(n)) + 3U) & ~3U)
#define va_start(ap, parm) \
    ((ap) = (va_list)((char*)((unsigned long)(&parm) & ~3U) + __alinha4(sizeof(parm))))
#define va_end(ap) ((void)0)

extern "C" {
size_t strlen(const char* s);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
int strcmp(const char* a, const char* b);
char* strstr(const char* s, const char* sub);
void* memcpy(void* dst, const void* src, size_t n);
int sprintf(char* s, const char* format, ...);
int sscanf(const char* s, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list args);
int atoi(const char* s);
double atof(const char* s);

// MSL_Extras (extras_string.h): não são do C padrão
char* strlwr(char* s);
char* strupr(char* s);
int stricmp(const char* a, const char* b);
}

// memcpy passa pelo intrínseco __memcpy do compilador. Com tamanho constante e
// pequeno, ele vira cópias de byte no lugar (o jogo copia assim os 4 bytes de cada
// campo do índice HERF); com tamanho variável, vira a chamada a memcpy de sempre. O
// jogo tem as duas formas, então o memcpy do header que ele usava era este.
#define memcpy(dst, src, n) __memcpy(dst, src, n)

#else
// No PC (o teste com dados reais, decomp/tests/test_arquivos.cpp): a biblioteca C do
// sistema, mais as funções do MSL_Extras que ela não tem (em decomp/tests/nitro_pc.cpp).
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
char* strlwr(char* s);
char* strupr(char* s);
int stricmp(const char* a, const char* b);
}
#endif

#endif
