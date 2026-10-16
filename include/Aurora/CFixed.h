// CFixed: um número em ponto fixo 20.12 (o fx32 do NitroSDK) com as conversões. O nome é
// nosso; as funções ficam em outro arquivo do jogo (perto de 0x020c0a0c), ainda em assembly.
#ifndef AURORA_CFIXED_H
#define AURORA_CFIXED_H

#include "nitro.h"

class CFixed {
public:
    CFixed() {}
    CFixed(int value) : m_value(value) {}
    CFixed(const CFixed& other) : m_value(other.m_value) {}
    CFixed(float value);   // 0x020c0a0c: value * 4096

    fx32 m_value;
};

#endif
