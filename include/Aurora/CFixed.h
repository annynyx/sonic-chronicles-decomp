// CFixed: um número em ponto fixo 20.12 (o fx32 do NitroSDK) com as conversões. O nome é
// nosso; as funções ficam em outro arquivo do jogo (perto de 0x020c0a0c), ainda em assembly.
// O construtor de int recebe o número inteiro (3 vira 3.0, 0x3000).
#ifndef AURORA_CFIXED_H
#define AURORA_CFIXED_H

#include "nitro.h"

class CFixed {
public:
    CFixed() {}
    CFixed(int value) : m_value(value << 12) {}
    CFixed(const CFixed& other) : m_value(other.m_value) {}
    CFixed(float value);   // 0x020c0a0c: value * 4096

    // 0x020c0b68: um valor sorteado entre min e max
    static CFixed Random(const CFixed& min, const CFixed& max);

    CFixed& operator+=(const CFixed& other)
    {
        m_value += other.m_value;
        return *this;
    }
    // A comparação com um inteiro é feita em 64 bits, com o inteiro já como ponto fixo.
    // Devolver BOOL (e não bool) faz o compilador guardar o resultado num registrador antes
    // do teste, como o jogo faz em todo lugar que compara assim.
    BOOL operator<(int value) const { return (s64)m_value < ((s64)value << 12); }
    BOOL operator>(int value) const { return (s64)m_value > ((s64)value << 12); }

    fx32 m_value;
};

#endif
