# O compilador do jogo

Resultado da **Fase 1** do [plano](PLANO-DECOMPILACAO.md). Sem o compilador e as
flags certas, nenhum C++ sai igual ao original; com eles, uma função reescrita pode
ser provada certa byte a byte.

## Resposta curta

| | Valor | Como sabemos |
|---|---|---|
| Compilador | **Metrowerks CodeWarrior para DS, mwccarm 2.0** | 3 funções com 100% de match; 1.2 e "DSi" (4.0) erram |
| Service pack | **2.0 sp2** (as bibliotecas com certeza; o compilador, quase) | o MSL e o Runtime ligados no jogo são os da sp2: [abaixo](#o-service-pack-pelas-bibliotecas) |
| Otimização | `-O4,p` | `-O4,s` (tamanho) erra 2 das 3; `-O3,p` também acerta |
| Modo | `-thumb -interworking` | o código do jogo é Thumb (e as bibliotecas da Nintendo também, veja abaixo) |
| C++ | `-lang=c++ -Cpp_exceptions off -RTTI on` | abaixo |
| IPA | `-ipa file` | a ordem dos typeinfo e das strings na `.data`: [abaixo](#-ipa-file-a-ordem-dos-dados) |
| Outras | `-proc arm946e -enum int -char signed` | padrão dos jogos de DS; ainda não testadas uma a uma |

As flags estão num lugar só: [`decomp/tools/compilar.sh`](../decomp/tools/compilar.sh).

## As três funções

| Função | Endereço | Tamanho | Arquivo |
|---|---|---|---|
| `CExoString::CStr` | `0x020052f0` | 0x10 | [`src/Aurora/CExoString.cpp`](../src/Aurora/CExoString.cpp) |
| `HashResourceName` | `0x02009b78` | 0x44 | [`src/Aurora/CDSFileSystem.cpp`](../src/Aurora/CDSFileSystem.cpp) |
| lista: remover item | `0x0202d428` | 0x2c | [`decomp/compilador/lista.cpp`](../decomp/compilador/lista.cpp) |

As duas primeiras já são arquivos do jogo de verdade: o build da
[Fase 0](BUILD.md) usa o `.o` compilado delas no lugar do código original, e a ROM
continua com o mesmo SHA-1.

A matriz abaixo é a saída de `decomp/tools/testar_compilador.sh`: quantas das 3
funções saem idênticas em cada versão e otimização.

```
versão     -O4,p   -O4,s   -O3,p   -O2
1.2/base    1/3     1/3     1/3     1/3      (1.2/sp2, sp2p3, sp3, sp4: igual)
2.0/base    3/3     1/3     3/3     1/3      (2.0/sp1 ... sp2p4: igual)
dsi/1.1     2/3     1/3     2/3     1/3      (todas as dsi/: igual)
```

Com a `CExoString` inteira decompilada (Fase 3.1), a mesma matriz tem 35 funções e
continua apontando para a 2.0:

```
versão     -O4,p   -O4,s   -O3,p   -O2
1.2/base    0/35    0/35    0/35    0/35     (a 1.2 não conhece a opção -ipa)
2.0/base    35/35   16/35   35/35   31/35    (2.0/sp1 ... sp2p4: igual)
dsi/1.1     21/35   15/35   21/35   19/35    (dsi/1.6sp1 e sp2: 19 em -O4,s)
```

- **1.2** gera `mul` para `hash * 33` e salva os registradores com outra convenção
  (`push {r4, r5, lr}` + `sub sp, #4` em vez de `push {r3, r4, r5, lr}`).
- **DSi (4.0)** acerta CStr e o hash, mas na remoção de item aloca os registradores
  de outro jeito (16 instruções diferentes).
- **`-O4,s`** monta o laço do hash de outro jeito; só a função trivial sobrevive.

## Por que exceções desligadas e RTTI ligado

- **RTTI ligado:** o jogo tem typeinfo para 290 classes do próprio jogo
  (`13CGameCreature`...). É assim que os nomes das classes foram recuperados.
- **Exceções desligadas:** com `-Cpp_exceptions on`, o compilador cria uma entrada
  de 12 bytes na seção `.exceptix` para cada função que chama outra (como o hash, que
  chama `CStr`). As 206 entradas do `.exceptix` do jogo são todas de endereços acima
  de `0x020e09d1`, a região da biblioteca C++ (MSL). Ou seja: a Metrowerks compilou a
  biblioteca com exceções, a BioWare compilou o jogo sem.

## O que aprendemos igualando funções

Igualar uma função é ajustar o C++ até o compilador escolher as mesmas instruções.
Algumas lições destas três, que devem valer para o resto do jogo:

- **`||` dentro de `?:` vira um valor guardado.** O padrão estranho
  `r7 = 1; se 0 <= c < 0x80 então r7 = 0; se r7 == 0 então c = tabela[c]` é um
  `tolower` inline escrito como `(c < 0 || c >= 0x80) ? c : tabela[c]`.
- **Variáveis temporárias mudam a ordem dos registradores.** Com
  `hash = (hash << 5) + hash + tolower(c)` a última soma sai `add r4, r4, r5`; com o
  `tolower` numa variável própria sai `add r4, r5, r4`, como no jogo. O valor é o
  mesmo, a instrução não.
- **O compilador relê da memória o que pode ter mudado.** Na remoção de item, `data`
  e `count` são relidos a cada volta: gravar em `data[i]` poderia, para o compilador,
  mudar o próprio objeto. Reproduzir isso pede ler os campos pelo objeto, e não
  copiá-los para variáveis locais.
- **`ldrsb r5, [r3, r6]` com `r6 = 0`** é só um `*p`: em Thumb, o `ldrsb` (byte com
  sinal) não tem a forma com deslocamento fixo, então o compilador usa um registrador
  zerado.

Mais lições, da `CExoString` (33 funções, [`src/Aurora/CExoString.cpp`](../src/Aurora/CExoString.cpp)):

- **Uma chamada dentro dos argumentos muda a ordem das leituras.** Em
  `stricmp(string.CStr(), m_sString)`, o compilador lê `m_sString` antes de chamar
  `CStr` e o guarda num registrador; o jogo lê depois. Com a chamada numa linha própria
  (`const char* other = string.CStr();`), a ordem fica a do jogo. O mesmo no `Find`.
- **Um valor guardado numa variável é lido uma vez só.** O `operator[]` do jogo lê
  `m_sString`, chama `strlen` e usa o mesmo registrador depois; escrito com
  `m_sString` nas duas vezes, o compilador relê o campo depois da chamada.
- **Um `BOOL` calculado fora do laço** dá o padrão "r2 = 1 se o ponteiro é NULL", testado a
  cada volta (o construtor de texto de 16 bits).
- **O `ldr r0, =0` no lugar de `movs r0, #0`:** o `movs` mexe nas flags, e o compilador
  ainda precisa delas para o `beq` seguinte. Não é preciso fazer nada no fonte.
- **O tamanho de um array local** só aparece arredondado na pilha (`sub sp, #0x24`):
  de 33 a 36 bytes, o código é o mesmo.
- **Construtores em dobro.** Para cada construtor, o compilador gera o `C1` (objeto
  inteiro) e o `C2` (a parte "base" de uma classe derivada), com os mesmos bytes; o
  linker descarta o que ninguém chama. Com `-ipa file`, o `C2` vem antes do `C1`, e é
  assim no jogo (`0x02004e2c`, chamado 1 vez, e `0x02004e68`, chamado 293 vezes): mais
  uma confirmação da opção. Os destrutores saem `D1`, `D0` (o que também faz
  `delete this`) e `D2`.

E do alocador ([`src/Aurora/Memory.cpp`](../src/Aurora/Memory.cpp), 15 funções, com a
`.bss`, a `.data` e o `__sinit`):

- **Uma constante `const` com valor calculado vai para a `.data`.** `const fx32 X =
  FX_Div(...)` fica na `.data` e é calculada no `__sinit`; sem o `const`, a variável vai
  para a `.bss`. No jogo ela está na `.data`.
- **A ordem das atribuições no construtor aparece no `__sinit`.** O construtor inline dos
  pools é copiado dentro do `__sinit`, e o jogo grava a quantidade antes do mínimo e do
  tamanho: o construtor atribui nessa ordem.
- **Onde fica o `return NULL`.** Com `if (size == 0) return NULL;` no começo, o compilador
  põe esse retorno logo depois do teste; no jogo ele está no fim, o que corresponde a
  `if (size != 0) { ... } return NULL;`.
- **O compilador junta constantes, mas não através de um ponteiro.**
  `(u32)top - 0xa000 + 31` vira `top - 0x9fe1`; `(u32)((char*)top - 0xa000) + 31`
  mantém as duas contas, como no jogo.
- **Um array de objetos globais com construtor** (`static MemoryPool s_pools[5] = {
  MemoryPool(...), ... }`) gera, no `__sinit`, a construção e o registro de cada
  elemento, cada um com a sua variável escondida; as variáveis saem na `.bss` numa
  ordem própria do compilador, e é a mesma do jogo.

E do sistema de arquivos ([`src/Aurora/CDSFileSystem.cpp`](../src/Aurora/CDSFileSystem.cpp),
45 funções no arquivo e 24 cópias de template que o jogo guardou em outros lugares):

- **Num campo de bits, `x.campo--` repete a expressão.** `m_requests[i].timeout--`
  chama `operator[]` duas vezes (uma para ler, outra para gravar), e a conta sai como
  "+ 0xffff" em 16 bits. Escrito `x = x - 1` com duas chamadas, a conta sai `subs #1`.
  O mesmo vale para `count++`.
- **Um `(int)` muda o tamanho da conta.** `estado == 1 || estado == 2`, num campo de bits
  de 16 bits, vira "`estado - 1 <= 1`" feito em 16 bits; o jogo faz em 32, o que sai com
  `(int)estado == 1 || (int)estado == 2`.
- **`if (tipo == 0) ... else switch`.** Um `switch` de 4 casos vira uma árvore de
  comparações; o jogo testa o 0 primeiro (com as flags do `ands`) e os outros três em
  fila, que é o `if` seguido de um `switch` de 3. Os corpos saem na ordem do fonte (LZ,
  RL, Huffman), as comparações na ordem dos valores.
- **Ler um campo uma vez só antes de uma chamada** (`int cache = m_files[i].cache;`) faz
  o compilador reusar o valor depois da chamada; lido duas vezes, `operator[]` é chamado
  de novo.
- **`b ? 1 : 0` num campo de 1 bit** sai com um corte para 16 bits a mais; atribuir a
  comparação direto (`campo = x != 0`) não.
- **Uma `struct` dentro de outra é copiada em bloco.** O jogo copia o `FSFile` palavra
  por palavra nos primeiros 0x20 bytes e com `ldm`/`stm` em dois blocos depois: `prop` e
  `arg` são `union`s, como no NitroSDK. Campos soltos, ou um array de 2 palavras, saem
  palavra por palavra.
- **O `memcpy` é o intrínseco `__memcpy`.** Com tamanho constante e pequeno ele vira
  cópias de byte; com tamanho variável, `bl memcpy`. `include/msl.h` faz a troca.
- **`(high + low) / 2`, não `(low + high) / 2`:** a ordem da soma aparece na instrução.
- **A base `Singleton<T>` tem destrutor virtual vazio** (DIARIO 26). Ele não gera código
  que fique no jogo, mas muda três coisas que aparecem: quais endereços de campo saem
  como constante pronta no construtor copiado em `Instance()`, a ordem das cargas no
  construtor de `CErfMan` e a ordem dos typeinfos e vtables na `.data`.
- **Cada função numa seção própria.** O `.o` do mwccarm tem uma seção `.text` por
  função; os dados pequenos (typeinfo, strings) também. A seção de dados de alinhamento 1
  fica num múltiplo de 4 no jogo: o linker alinha.

E da tabela de textos ([`src/Aurora/CTlkTable.cpp`](../src/Aurora/CTlkTable.cpp), 12 de
13 funções, DIARIO 28):

- **As cópias de template saem na ordem da declaração dos métodos na classe**, não na
  ordem em que o arquivo os usa. Os que só outro método do template pede (o `Allocate`,
  que o `Add` chama) vêm numa segunda passada, a não ser que o arquivo também os peça.
  No arquivo da `CTlkTable` o `Allocate` vem antes do `Add`, e foi isso que fixou a
  ordem em `include/Aurora/CExoArrayList.h`.
- **O tipo do elemento aparece na cópia.** Para elementos de 4 bytes, o laço do
  `Allocate` que copia ponteiros prepara primeiro a origem (`r3`) e depois o destino
  (`r5`); com uma `struct` de um campo, a ordem e os registradores se invertem. O jogo tem
  as duas formas, e isso diz o tipo.

E das tabelas GDA e da leitura de GFF4 ([`src/Aurora/G2DA.cpp`](../src/Aurora/G2DA.cpp), 52
funções, DIARIO 29):

- **Os argumentos são calculados da direita para a esquerda.** Um objeto montado com
  `GFF4File(m_data, m_size)` lê o tamanho antes dos dados; a cópia do mesmo objeto
  (`m_file`) lê os dados primeiro. Quando o jogo lê os campos "de trás para a frente", é um
  construtor.
- **Onde fica um corte de tipo.** `(u16)x` passado a um parâmetro `u16` corta na chamada;
  um parâmetro `u32` guardado com `m_campo((u16)x)` corta na hora de gravar, depois das
  outras leituras. O jogo tem os dois, e a posição do `lsls`/`lsrs` diz qual.
- **Templates de função saem logo depois de quem os usa.** Os métodos de uma classe
  template saem no fim do arquivo, na ordem da declaração; uma função template (ou um
  método template, como `Get<T>`) sai logo depois da primeira função do arquivo que a
  instancia, seguida das que ela mesma instancia. É o padrão "função, auxiliar, auxiliar"
  no meio de um arquivo.
- **Um literal dentro de uma função inline ganha nome.** O `""` de uma função inline vira
  um dado com nome (`_ZZ...Es`), multidef, que vai para o bloco das variáveis da `.data`,
  ordenado por tamanho; numa função comum, ele é local e vai para o fim, junto das strings.
  Um literal compartilhado entre arquivos do jogo é desse tipo.
- **Uma classe com construtor de cópia vai por ponteiro.** Passada por valor, uma struct de
  4 bytes vai num registrador; com construtor de cópia escrito à mão, o compilador passa o
  endereço de uma cópia.
- **Funções inline mudam a pilha e os registradores.** O valor devolvido por uma função
  inline vira um temporário, e os temporários dela entram na pilha depois dos da função de
  fora. Na mesma função, escrever `s.GetTemplate().FindField(x)->type` ou chamar uma
  inline que faz isso dá alocações diferentes; os dois aparecem no jogo.
- **O padrão `flag_found` do MSL.** Uma variável que vale 1 antes de um `switch` e 0 no
  `default` fica pronta em `r0` antes da árvore de comparações, e cada caso ganha o seu
  fim de função. Com `return TRUE` em cada caso, o 1 sai repetido em cada um.
- **Procurar a sequência no jogo.** Quando uma forma não aparece, vale procurar no ARM9 a
  mesma sequência de instruções: se ela estiver numa função de biblioteca (MSL, NitroSDK),
  o fonte dela mostra a construção.
- **O compilador fatora as contas, e a forma fatorada não é a do fonte.** A `C2DA` aloca
  `linhas * colunas + colunas + linhas` células; o código do jogo calcula
  `(linhas + 1) * colunas + linhas`, mas escrever assim troca a ordem dos operandos do
  `muls`. Vale tentar a forma "ingênua" quando a fatorada não bate.
- **As virtuais inline saem no fim do `.o`, na ordem inversa da declaração.** Com
  `GetNumColumns` declarada antes de `GetNumRows`, o `.o` traz `GetNumRows` primeiro; o
  jogo tem o contrário. A ordem na vtable vem da classe base que declara as virtuais, então
  a classe derivada pode declará-las em qualquer ordem sem mudar a vtable.
- **`sizeof` no lugar do número.** `linhas * sizeof(char*)` dá o mesmo código que
  `linhas * 4` no DS e continua certo no teste de 64 bits do PC, onde um ponteiro tem 8
  bytes.

## Registradores e pilha: quem fica onde

Quando uma função tem mais valores vivos do que registradores, o compilador escolhe quais
vão para a pilha, e em que posição. Isso explica a maioria das diferenças que sobram
quando as instruções já são as mesmas. Em Thumb, só `r4`-`r7` sobrevivem a uma chamada.
O que funções de teste pequenas mostraram (só chamadas e variáveis, nada do jogo; DIARIO
28):

- **Vão para a pilha os valores com menos usos; no empate, os mais novos.** Com sete
  valores vivos durante uma chamada, os quatro primeiros ficam em registradores e os três
  últimos vão para `[sp]`, `[sp+4]`, `[sp+8]`. Um valor com mais usos fica no registrador
  e empurra outro para a pilha.
- **As posições na pilha seguem uma numeração interna, não a ordem do fonte.** Nos testes
  pequenos, inverter a ordem das declarações não mudou nada. Na `C2DA` (DIARIO 30) houve
  dois casos em que mudou: as variáveis cujo endereço é passado (`&line`) ficam na pilha
  na ordem inversa da declaração, e na `SkipNewLines`, das 24 ordens possíveis das quatro
  variáveis, só uma deu os registradores do jogo.
- **O que entra na numeração muda tudo.** Na `CopyFrom` da `C2DA`, as instruções já eram as
  do jogo e só a pilha mudava. Bateu quando os contadores dos laços passaram a ser os
  mesmos em todos os laços (`int i, j;` no topo, no estilo C) e quando a expressão
  `labelText + position`, usada três vezes, ficou **sem variável**: o compilador a guarda
  como temporário próprio, numerado depois das variáveis. Um contador declarado no topo
  e só incrementado no laço (`column++`) continua no código; declarado no laço, o
  compilador o apaga.
- **Numa função inline copiada**, a numeração põe primeiro as variáveis da inline, depois
  os parâmetros dela (do último para o primeiro), depois as variáveis da função de fora
  (da última declarada para a primeira). A `CTlkTable::GetString` do jogo segue isso.
- **Ainda sem regra:** uma variável definida dentro de um laço da inline vai para o fim da
  numeração se recebe o resultado de uma chamada, e para o começo se recebe uma leitura de
  memória. É o que falta para a `GetString` bater (no jogo, as duas leituras do laço estão
  no fim).

## `-ipa file`: a ordem dos dados

As três funções batem com ou sem `-ipa file` (a "análise interprocedural por arquivo":
o compilador lê o arquivo inteiro antes de gerar o código). Quem mostrou a opção foi a
**ordem dos dados** na `.data`, que importa assim que um arquivo com variáveis e
classes for decompilado inteiro.

Cada classe com RTTI tem um typeinfo (`_ZTI`) e o nome dela (`_ZTS`, a string `"4Time"`).
No padrão do compilador (`-ipa function`), os nomes vão para o fim do arquivo, depois
das strings do código. Com `-ipa file`, eles ficam junto das variáveis e dos typeinfo, e
as strings do código vêm por último. `decomp/tools/conferir_ipa.py` compila um exemplo
com as duas opções e confere o jogo:

```
1. a previsão (o exemplo compilado):
   padrão (-ipa function): string typeinfo nome
   com -ipa file:          nome typeinfo nome string
2a. no jogo: 51 de 270 nomes vêm antes do typeinfo (no padrão seriam 0)
2b. 266 de 270 typeinfo sem nenhuma string no meio (o que -ipa file prevê)
```

As 4 exceções de 2b são a mesma string, `"ScrollBG01.tga"`, lida por uma função do
`GameModeInventory`. É a string de uma função inline: o compilador lhe dá um nome
(`_ZZ...Es`) e, com `-ipa file`, ela vai para o bloco das variáveis. Dos 51 nomes que
vêm antes do typeinfo, 49 são de classes derivadas, como a ordem do `-ipa file` prevê
(nome da derivada, typeinfo da base, typeinfo da derivada, nome da base).

Cuidado: `-ipa off` não é o padrão. Ele até inverte a ordem das funções no `.o`.

O que a opção diz sobre a organização de cada arquivo (e por isso sobre o mapa de
arquivos da Fase 2.4), conferido em `.o` de teste:

- **`.text`:** as funções na ordem do fonte, e depois as "multidef" (métodos inline e de
  template, de que o linker guarda uma cópia só).
- **`.data`:** primeiro as variáveis, as vtables, os typeinfo, os nomes deles e as
  strings citadas só por dados (`static const char* nomes[] = {"a", ...}`); por
  último, as strings citadas pelo código, cada uma antes da primeira função que a usa.
- **`.rodata`:** as tabelas `const`. Os `float` e `double` ficam no `.text`, junto da
  função (o "pool" de constantes).

## O service pack pelas bibliotecas

O código do próprio jogo não separa as versões 2.0 entre si. Mas o jogo também traz,
no fim do ARM9 (a partir de `0x020e09d0`), a biblioteca C/C++ da Metrowerks (MSL) e
o Runtime (divisão, ponto flutuante, `new`/`delete`...). Essas bibliotecas vêm
**prontas** com cada versão do CodeWarrior, e mudam um pouco de uma versão para outra.

`decomp/tools/achar_funcoes.py` procura cada função de uma biblioteca no jogo
(mesmo tamanho, mesmos bytes fora das relocações). `decomp/tools/versao_msl.sh` faz
isso com as bibliotecas de cada versão (pacote do [metroskrew](https://github.com/mid-kid/metroskrew)):

```
versão  endereços achados (de 283 que alguma versão acha)
base     274
p2       274
p4       276
sp1      276
sp1p2    280
sp2      283      <- a única que acha todos
sp2p3    275
```

Das 15 funções que mudam entre versões, a sp2 acerta as 15; cada outra versão erra
pelo menos 3. Então o jogo foi ligado com o CodeWarrior 2.0 **sp2**. O compilador quase
certamente é o da mesma instalação: `2.0/sp2`, ou um patch dela que não tenha trazido
bibliotecas novas (o pacote não tem bibliotecas próprias da `sp2p2`; isto é dedução).
É o que `compilar.sh` usa.

Para comparação: no decomp do Pokémon Platinum, o NitroSDK 4.2.30001 (a mesma versão
deste jogo) foi compilado pela Nintendo com a `2.0/sp1p2`, e a NitroSystem com a `2.0/sp2`.

**Bônus:** as funções achadas sem ambiguidade ganharam o nome verdadeiro em
`symbols.txt` (171 nomes: `memcpy`, `fwrite`, `__flush_buffer`, `abort`...). Funções que
são idênticas entre si (`abs` e `labs`, os destrutores vazios) ficaram com o nome
antigo, porque não dá para saber qual é qual. O comando:

```
L=work/ferramentas/metroskrew/lib/metroskrew/sdk/ds/2.0/sp2
python3 decomp/tools/achar_funcoes.py --de 0x020e09d0 --aplicar \
    $L/msl/MSL_C/MSL_ARM/Lib/MSL_C_NITRO_{T,Ai}_LE.a \
    $L/msl/MSL_C++/MSL_ARM/Lib/MSL_CPP_NITRO_{T,Ai}_LE.a \
    $L/Runtime/Runtime_ARM/Runtime_NITRO/Lib/NITRO_Runtime_{T,Ai}_LE.a
```

## O NitroSDK: compilado pela Nintendo, em Thumb

O jogo usa o NitroSDK **4.2.30001** (o número `0x04027531` no ARM9). Essa exata versão
foi decompilada pela comunidade ([ntrtwl/NitroSDK](https://github.com/ntrtwl/NitroSDK),
a mesma base do decomp do Pokémon Platinum). `decomp/tools/nitrosdk.sh` baixa esse
fonte, compila os 169 arquivos do ARM9 e procura cada função no jogo.

- **Thumb, não ARM.** A primeira tentativa, em ARM (como o Platinum), achou 0 de 4
  funções do `os_ownerInfo.c`. O `OS_GetOwnerInfo` do jogo começa com `push {r4, lr}`
  em Thumb: a BioWare ligou a versão Thumb das bibliotecas do SDK
  (`-thumb -DSDK_CODE_THUMB`). Com isso, 3 de 4 bateram de cara.
- **Versão do compilador do SDK.** Funções achadas na região do SDK
  (`0x020d4000`-`0x020e09d0`) com cada versão: `2.0/base` 627, `sp1`/`sp1p2`/`sp1p5`
  652, `sp2` 649. O SDK foi compilado pela Nintendo com uma 2.0 sp1 (o Platinum usa a
  `sp1p2`, a que adotamos aqui), não com a sp2 do jogo. Faz sentido: a Nintendo
  entrega o SDK já compilado.
- **562 funções ganharam o nome verdadeiro** (`OS_IrqHandler`, `MTX_Concat43`,
  `FS_OpenFileFast`...), contando ITCM e o `crt0` (`_start`, `do_autoload`). Só entram as
  sem ambiguidade.
- **Prova independente:** `decomp/tools/conferir_chamadas.py` segue todas as chamadas
  e ponteiros das funções nomeadas: das 1.140 referências, as 1.140 apontam para a
  função com o nome que o fonte diz. Um nome errado por coincidência de bytes quebraria
  essa conta.

Com o MSL (171) e o SDK (562), as funções com nome automático (`func_...`) caíram de
5.685 para 4.973 no ARM9 (e para 4.611 com a NitroSystem, abaixo).

## A NitroSystem: 100% da região reconhecida

A NitroSystem é a outra biblioteca da Nintendo: 3D (`NNS_G3d*`, que desenha os
modelos), 2D (`NNS_G2d*`), som (`NNS_Snd*`), memória (`NNS_Fnd*`) e VRAM (`NNS_Gfd*`).
Ela também tem decompilação pública ([ntrtwl/NitroSystem](https://github.com/ntrtwl/NitroSystem),
versão 071126). `decomp/tools/nitrosystem.sh` compila os 96 arquivos dela e procura cada
função no jogo, como `nitrosdk.sh` faz com o SDK.

- **Onde está:** de `0x020c8278` a `0x020d4394`, logo antes do NitroSDK (o SDK começa
  em `MTX_Identity22_`, não em `0x020d4000` como eu achava).
- **Thumb de novo:** em Thumb, 519 funções da biblioteca batem. Em ARM, só batem as dos
  arquivos que o próprio fonte já força para ARM (`#include <nitro/code32.h>`, como os
  cálculos de animação e de textura do 3D), que saem iguais nos dois modos.
- **Compilador:** `2.0/sp2` e `sp2p2` acham 519, `sp1p2` 517, `sp2p3` 513, `2.0/base` 494.
  As duas funções que só a sp2 acerta são `NNSi_G3dFuncSbc_MAT_InternalDefault` e
  `NNSi_G3dFuncSbc_NODEDESC`. Igual ao Platinum: a NitroSystem foi compilada com a sp2,
  e o SDK com a sp1.
- **A versão é a mesma:** das 464 funções da região, **todas** batem com alguma função da
  NitroSystem 071126. Nenhuma sobra: o jogo usa exatamente esta versão.
- **360 nomes novos** (`NNS_G3dDraw`, `NNS_SndArcInit`, `NNS_FndAllocFromExpHeapEx`...).
  As outras ~100 funções são ambíguas: funções `static` com o mesmo nome em vários
  arquivos (`texmtxCalc_flagTRS_` existe nos arquivos do Maya, do 3ds Max e do XSI) ou
  funções idênticas entre si.
- **Conferido pelas chamadas:** 708 de 708 referências certas (1.848 de 1.848 com o SDK).

Com o MSL, o SDK e a NitroSystem, as funções com nome automático no ARM9 caíram de
5.685 para 4.611. Na região do SDK, 628 das 631 funções batem com o fonte; as 3 que
sobram (`0x020d6930`, `0x020e067c`, `0x020e096c`) ainda não sabemos de onde vêm.

## O que não sabemos (ainda)

- **O service pack do compilador com prova direta.** As bibliotecas são da sp2, mas
  em cerca de 30 construções de C++ as versões `2.0/sp1` a `2.0/sp2p4` geraram código
  idêntico; a única diferença achada foi `while (n--)` entre a `2.0/base` e as outras.
  Se uma função futura só bater com outra versão, troca-se em `compilar.sh`.
- **As outras bibliotecas** (o vídeo VX da Actimagine, a de backup): ainda não
  procuradas, e sem decompilação pública conhecida.
- `-enum int`, `-char signed`, `-inline`, `-str`: ainda sem uma função que dependa
  delas. (`-ipa file` foi achada pelos dados, acima.)
- **O `DelIndex` das listas com elementos de tamanho que não é potência de 2.** O jogo
  calcula o endereço do elemento seguinte somando o tamanho ao deslocamento atual
  (redução de força); o mesmo template, aqui, refaz a multiplicação. Com elementos de 4
  ou 16 bytes (deslocamento de bits) os dois batem. Nenhuma das 960 formas do laço
  testadas, nem outra versão do compilador, reproduz os dois casos (DIARIO 26).
- **A `CTlkTable::GetString` (`0x020989fc`).** As instruções são as do jogo, mas duas
  variáveis do laço (`data` e `position`) vão para posições da pilha diferentes, e com
  elas os registradores do laço. Cerca de 300 formas testadas (DIARIO 28); falta entender
  a numeração acima.

## Como repetir

```
decomp/tools/ferramentas.sh
decomp/tools/montar_rom.sh sua_copia.nds      # extrai o ARM9 em work/extract
decomp/tools/testar_compilador.sh             # a matriz acima (~50 s)
```

Para tentar uma função nova: escreva o C++, compile com `compilar.sh` e compare com
`comparar.py` (exemplos em [BUILD.md](BUILD.md#quando-o-build-falha)). Quando bater,
acrescente a linha em `decomp/compilador/casos.txt` e regrave `esperado.txt` com
`conferir_sem_rom.py --gerar` (veja [BUILD.md](BUILD.md#o-que-falta-da-fase-0)): ela passa a
fazer parte do teste e do CI.
