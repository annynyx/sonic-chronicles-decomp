// O que todo arquivo do jogo inclui (o nome do header original não é conhecido).
//
// Os 149 __sinit do jogo começam calculando FX_Div(2.0, pi) e guardando o resultado
// numa variável do próprio arquivo, na .data: é esta constante. Em C++, uma constante
// fora de função é local ao arquivo, então cada .cpp que inclui o header ganha a sua
// cópia e o seu __sinit (como acontece com as strings). Daí ela servir de marcador de
// arquivo na .data e na .init (docs/DIARIO.md, seções 23 e 24).
#ifndef AURORA_COMUM_H
#define AURORA_COMUM_H

#include "nitro.h"

// 2/pi em ponto fixo 20.12 (0x3243 é pi). O nome é nosso.
const fx32 FX_TWO_OVER_PI = FX_Div(2 << 12, 0x3243);

#endif
