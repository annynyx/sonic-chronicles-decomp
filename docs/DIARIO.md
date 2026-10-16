# Diário das descobertas

A história do projeto, em ordem, com as hipóteses que deram certo e as que deram errado.
Cada entrada diz **como** a coisa foi descoberta e **como** foi confirmada.

---

## 1. Abrindo a ROM
- Código do jogo `YWSE`, ARM9 de 1,1 MB **sem compressão e sem overlays** (raro: facilita
  muito), NitroSDK 4.2 (`0x04027531`, achado no bloco de parâmetros do módulo, assinatura
  `0xDEC00621`).
- 303 arquivos na raiz do NitroFS. As extensões `2DA`, `GFF`, `TLK` denunciaram o motor:
  **Aurora, da BioWare**, o mesmo de NWN/KotOR/Dragon Age, adaptado para o DS.
- O xoreos (reimplementação livre dos motores BioWare) já tinha estudado o jogo, mas só
  como visualizador.

## 2. Mapeando o código
- O `dsd` (ds-decomp) achou as funções seguindo as chamadas. Travou em `0x020ecc3c`: uma
  função **escrita à mão em assembly** que salva todos os registradores (estilo
  `setjmp`), logo após outra que termina num incomum `bx r2`. Solução: permitir
  chamadas para funções desconhecidas. Resultado: **6.820 funções**.
- Tentativa que não deu em nada: uma tabela com **98 tags de profiling**
  (`"Creat:UpdtGamepl"`, `"BG:SetFocusPoint"`...). Procurei a função `Begin` do profiler
  estatisticamente e não achei: o profiler foi **desligado** na versão final (a função de
  relatório, `0x020660c4`, não tem chamador). Ainda assim, os nomes revelam os subsistemas.

## 3. O RTTI: os nomes reais das classes
- Strings como `13CGameCreature` são nomes C++ "mangled", guardados para
  `dynamic_cast` e exceções. Seguindo `nome → typeinfo → vtable → funções`:
  **290 classes com herança, 351 vtables**.
- **Erro nº 1:** marquei como construtor toda função que lia o endereço de uma vtable.
  O assembly mostrou funções que só *registravam um singleton* (`static` local com o
  construtor inlinado). Correção: construtor é quem **grava** a vtable no offset 0, e
  quem faz isso dentro de outra tarefa vira `constructs_<Classe>`.
- **Erro nº 2:** supus o layout da ABI Itanium (destrutores nos slots 0 e 1 da vtable).
  O pseudo-C do Ghidra mostrou que os slots 0 e 1 eram métodos comuns. No CodeWarrior,
  o destrutor fica na ordem em que foi declarado. Correção: destrutor é **o método
  virtual que grava a vtable da própria classe em `this`**. 151 encontrados.

## 4. Uma função do começo ao fim: o hash dos nomes
O pacote `test.herf` guarda só hashes dos nomes. Testei candidatos (DJB2, CRC32, FNV) e
o DJB2 do nome em minúsculas bateu. Depois achei **a função do jogo** que calcula isso,
pela constante 5381 (`0x1505`):

```asm
ldr   r4, =0x1505          ; hash = 5381          <- assinatura do DJB2
bl    CExoString__CStr     ; s = name.CStr()
ldrsb r5, [r0, r1]         ; c = (signed char)*s   <- COM sinal!
ldrb  r5, [r0, r5]         ; se 0<=c<0x80: c = tabela_minusculas[c]
lsl   r7, r4, #5 ; add r4, r7 ; add r4, r5, r4      ; hash = hash*33 + c
```
Reescrita em C (`decomp/src/resource_hash.c`) e em Rust. Detalhe que só o assembly
revela: o caractere é lido **com sinal**. O teste em C recalcula todos os nomes do jogo.

**Erro nº 3:** o teste em C pegou **4 nomes falsos** que o ataque de dicionário tinha
aceitado: colisões de hash com "palavras" tiradas de bytes aleatórios.

## 5. Os formatos de dados
- **`.small`**: primeiro achei só LZ10 (byte `0x10`). Sobravam 185 arquivos começando com
  `00 28 02 00`. O cabeçalho é `tipo | tamanho << 8`, e o **tipo 0 é sem compressão**.
- **GFF4**: o mesmo do Dragon Age, mas o TLK usa texto de **8 bits** (no PC é UTF-16),
  enquanto os diálogos usam UTF-16. Há dois tipos de campo só do Sonic: 18 (ponto fixo
  20.12 do DS) e 20 (ASCII inline).
- **GDA**: os nomes de coluna são `CRC32(nome.lower() em UTF-16)`. 534 de 845 são
  conhecidos (os mesmos 63% do xoreos).
- **Diálogos**: o campo "quem fala" vale sempre `PLAYER`. Quem fala de verdade, e com
  que **emoção**, está no nome do retrato: `tailssca` = Tails assustado.

## 6. Os cenários (formato que ninguém tinha decifrado)
1. `.pal` com 136.192 bytes = 1.064 × 128? Primeira hipótese: paleta de 64 cores por
   tile. Os índices chegavam a 215, então estava errada.
2. Os tiles são LZ10 de 4.096 bytes (64×64). Desenhados linha a linha, saíam listrados;
   em **blocos de 8×8**, apareceram folhas e pedras.
3. Ainda havia ruído: 136.192 = 266 × 512 = **266 paletas de 256 cores**, e
   38 × 28 tiles / 4 = 266. Ou seja, uma paleta por bloco de 2×2 tiles.
4. A posição dos tiles ainda estava errada. Em vez de chutar, **medi** a diferença de
   cor nas bordas entre vizinhos para cada hipótese de ordem. A melhor deu 35, ainda
   ruim. O `.2da` da área era um **mapa de paletas** (paletas numeradas coluna a coluna).
   Com ele: **4,8**. Green Hill apareceu inteira.
5. Profundidade (`.cdpth`): mesmo índice, mas valores de 16 bits em ordem **linear**
   (diferente da cor!), com `0x7FFF` = vazio. Ela mostra os pilares e as estátuas que
   ficam na frente dos personagens.

## 7. O dicionário oficial
Dentro do próprio pacote havia um `erf.dict` com **todos os nomes** dos recursos. Ele
levou a recuperação a 100% e revelou mais **12 nomes falsos** do ataque de dicionário
(ex.: `1_c.emit`, que na verdade é `BTN_PUZZ_ON.NCGR`). Lição: um método esperto que
acerta 91% ainda erra; a fonte oficial, quando existe, vale mais.

## 8. Paletas dos sprites
**Erro nº 4:** a regra "paleta mais comum do mesmo prefixo" pintou **555 retratos com a
paleta do Tails**. Correção: as tabelas dizem a paleta de cada personagem
(`creatures.gda`: `PRTL_TAILS` → `PRTL_Tal.nclr`), e as telas `.gui` dizem a de cada
elemento. 1.747 imagens passaram a ter a paleta confirmada pelo jogo.

## 9. Escrevendo de volta: o caminho do modding
Para modificar, é preciso **escrever** os formatos. O critério foi ler e reescrever sem
mudanças e obter o arquivo original byte a byte:
- GDA: 137 de 229 na primeira tentativa. O preenchimento entre textos é `0xFF`, não
  zero. Depois disso, 229 de 229.
- HERF: faltava alinhar também o fim do arquivo. Depois, 6 de 6 (incluindo o de 49 MB).
- **TLK: é uma tabela hash.** Das 18.845 entradas, 4.711 estavam vazias (id
  `0xFFFFFFFF`) e os ids não tinham ordem. Hashes comuns não explicavam as posições.
  Fui ao **código do jogo**: a função `0x020989fc` (a busca de textos) mostrou a fórmula,
  uma variante do hash de inteiros de Thomas Wang, com sondagem linear. Conferida:
  14.134 de 14.134 posições corretas. Sem isso, um texto novo ficaria numa posição
  onde o jogo nunca o procuraria.
- Compressor LZ10 **sem referências de distância 1**: a rotina da BIOS que escreve na
  VRAM grava 16 bits por vez e quebraria com elas.

## 10. No emulador
- O DeSmuME roda sem janela (`py-desmume` com `SDL_VIDEODRIVER=dummy`) a ~110 fps. Um
  roteiro de toques chega à tela de título e à citação de abertura.
- Teste 1: trocar o texto da citação (TLK). Funcionou.
- Teste 2, pelo `sonic-mod`: editar a tabela `Chapter0` para apontar para um **texto novo**
  (id 990001) que não existia. A tela mostrou o texto novo. Isso prova a edição de
  tabela, a inserção na tabela hash do TLK e a gravação de arquivo maior no fim do
  cartucho, tudo ao mesmo tempo.

## 11. O combate
O resultado está em [`COMBATE.md`](COMBATE.md). O caminho:

- **Das regras para o código.** `combatrules.gda` tem 74 números sem nome. A função que
  a carrega (0x0201f270) copia cada linha para uma variável global; cruzando cada global
  com as funções que a leem, cada regra ganhou um "dono" (ex.: as regras 7, 8 e 11 só
  aparecem no `Defend`). Foi assim que as fórmulas foram achadas: pelos números que elas
  usam, não pelos nomes.
- **Funções pequenas executadas de verdade.** O dano usa ponto flutuante por software
  (`0x020ea840`, `0x020ea8d0`...). Em vez de adivinhar pelo nome, cada uma foi executada
  no Unicorn com entradas conhecidas (`3 → 3.0`, `6.0 / 4.0 → 1.5`). O mesmo para o
  `RollDice` (0x02008550).
- **Erro nº 5:** o Ghidra mostrou `func_020d5cfc(x)` com **um** argumento e eu a tratei
  como uma função de uma entrada (raiz? inverso?). No Unicorn ela travava. O assembly
  mostrou que é o **divisor de hardware** do DS (registradores 0x4000280) e que o segundo
  argumento (`r1 = 0x64000`, ou 100,0 em ponto fixo) existe, só o Ghidra o perdeu. Lição:
  quando o pseudo-C parece estranho, o assembly decide.
- **Os nomes das colunas mentem às vezes.** A coluna `GUITypeAggressive` de `combo.gda`
  (nome vindo do dicionário do xoreos) guarda 0,3/0,5/0,8: a chance do status por nível.
- **O texto do próprio jogo como prova.** As habilidades especiais têm só um código
  numérico. As descrições dos Chao e dos acessórios ("Increases the team's chances of
  Ambushing...") deram o significado de cada código, e o código da emboscada confirmou
  os de número 21 e 22. O diário de regras do jogo (`journalrules.gda`) confirmou a
  ligação entre Armor/Grit, Power/dano e os pares de status e curas.
- **Uma descoberta que contradiz o texto.** O diário do jogo diz que o Clover Juice cura
  Cursed; o `.ITM` dele não tem a linha de remoção. Ficou registrado como está.
- O gerador `analise/tools/combate_tabelas.py` transforma as tabelas e os arquivos de
  efeito em Markdown legível, para conferir tudo isto na sua cópia.

## 14. A ROM reconstruída (Fase 0)
O guia está em [`BUILD.md`](BUILD.md). O caminho:

- **Ferramentas sem Windows.** Os compiladores da Metrowerks são `.exe` de 32 bits. O
  `wibo`, um carregador mínimo feito pela comunidade de decompilação, roda eles no
  Linux sem Wine. O pacote de compiladores é o mesmo que o decomp.me usa.
- **Desmontar e montar de novo.** `dsd delink` corta o ARM9 em `.o` com as
  relocações no lugar dos endereços; o `mwldarm` (o linker original) junta tudo. O
  ARM9, o ITCM e o DTCM saíram idênticos na primeira tentativa: as 60 mil relocações
  achadas pelo `dsd init` estão certas.
- **Os 20 bytes teimosos.** A ROM inteira ainda tinha 20 bytes diferentes, nenhum de
  código. 16 eram do ícone: a paleta tem duas cores iguais e o PNG intermediário do
  `dsd` não guarda qual índice era qual. Os outros eram o CRC da área segura, que
  depende da chave da BIOS do ARM7. Os dois foram resolvidos sem a BIOS, e o SHA-1
  bateu: `f4ff8291...`.
- **Erro nº 7:** primeiro li as diferenças do ícone como "índice 8 virou 6" (olhando
  os bytes em octal do `cmp`). Comparando nibble a nibble, todas eram 4 → 2: as duas
  entradas com a mesma cor. A causa certa só apareceu quando li o código do `ds-rom`
  que converte o PNG.
- **Uma falha da ferramenta.** Compilado do código mais novo, o `dsd` recusava
  qualquer divisão do ARM9 em arquivos ("nome duplicado" nos buracos sem fonte). O
  binário da release 0.12.1 não tem o problema; ficamos com ele.

## 15. O compilador (Fase 1)
O resultado está em [`COMPILADOR.md`](COMPILADOR.md): **mwccarm 2.0, `-O4,p`, Thumb**.

- **Comparar sem o linker.** `decomp/tools/comparar.py` põe lado a lado a função do
  jogo e a do `.o`, ignorando os bytes que só o linker preenche (o destino de um
  `bl`, os ponteiros do pool de constantes).
- **CExoString::CStr** bateu de primeira em todas as versões 2.0: é pequena demais para
  dizer qual. **HashResourceName** precisou de 3 rodadas: o `tolower` inline com `||`
  explicou o registrador-bandeira, e uma variável temporária acertou a ordem dos
  operandos da última soma.
- **Procurando uma função que separe as versões.** Compilei ~30 construções de C++
  com todas as versões: só três famílias geram código diferente (1.2, 2.0 e DSi).
  Uma diferença da DSi é alocar `r4-r7` onde a 2.0 usa `r3-r6`; procurando funções do
  jogo com esse padrão, a remoção de item de uma lista (`0x0202d428`) bateu com toda a
  2.0 e com nenhuma DSi.
- **Exceções desligadas.** Com exceções ligadas o hash ganhava uma entrada no
  `.exceptix`; as do jogo são todas da biblioteca MSL. O jogo foi compilado sem.
- **Erro nº 8:** o `comparar.py` media as relocações pela primeira seção `.text` do
  `.o`. Com uma função de template (que o compilador põe numa seção própria), as
  relocações erradas eram mascaradas e uma função idêntica aparecia como diferente. A
  seção certa é a que o campo `sh_info` da seção de relocações aponta.
- **O service pack pelas bibliotecas.** O código do jogo não separava as versões 2.0,
  mas o MSL que vem pronto com cada CodeWarrior sim. Procurando no jogo as funções das
  bibliotecas de 7 versões, só a 2.0 sp2 achou todas as 283 (as outras, de 274 a 280).
  De quebra, 171 funções do MSL ganharam o nome verdadeiro (`memcpy`, `fwrite`...).
- **Do teste para o jogo.** CStr e o hash viraram `src/Aurora/*.cpp`, marcados
  `complete` no `delinks.txt`. O build passou a usar o nosso `.o` no lugar do código
  original, e o SHA-1 continuou o mesmo. Para provar que a verificação funciona,
  desfiz a variável temporária do hash: o build falhou apontando o byte
  `0x02009ba6`, a soma com os operandos trocados.

## 16. O NitroSDK pelo fonte (começo da Fase 2)
- **A mesma versão, decompilada.** O SDK do jogo é o 4.2.30001, o mesmo que a
  comunidade decompilou para o Pokémon Platinum. Compilando aquele fonte, cada função
  do SDK deveria aparecer no jogo com os mesmos bytes.
- **Erro nº 9:** compilei em ARM, como o Platinum faz, e nada bateu. O assembly do
  `OS_GetOwnerInfo` no jogo é Thumb: a BioWare usou a versão Thumb das bibliotecas.
  Recompilado com `-thumb -DSDK_CODE_THUMB`, 562 funções ganharam nome.
- **Como saber que não é coincidência.** Duas funções pequenas podem ter os mesmos
  bytes. Por isso os nomes foram conferidos pelas chamadas: se `X` chama `Y` no fonte,
  o `bl` da função `X` no jogo tem que cair na função `Y`. As 1.140 referências
  bateram. A única que parecia errada era um ponteiro para *dentro* da própria função
  (o endereço de retorno de `OSi_DisplayExContext`), e o erro era do conferidor, que
  ignorava o deslocamento da relocação.
- **O SDK não foi compilado com o compilador do jogo.** As bibliotecas da Nintendo
  batem melhor com a 2.0 sp1 (652 funções) do que com a sp2 (649) que compilou o jogo:
  a Nintendo entrega o SDK já compilado.

## 17. A NitroSystem
- **Mesmo caminho do SDK.** Compilei a NitroSystem decompilada (versão 071126) em Thumb,
  já sabendo do Erro nº 9, e 360 funções ganharam nome: o motor de 3D (`NNS_G3d*`), o
  de som (`NNS_Snd*`), o 2D e os gerenciadores de memória e VRAM.
- **Uma região inteira explicada.** As 464 funções entre `0x020c8278` e `0x020d4394`
  batem, todas, com funções da NitroSystem. Não sobra nenhuma: é exatamente a versão
  que o jogo usa. Isso também corrigiu o começo do SDK, que eu tinha posto em
  `0x020d4000` só olhando por cima.
- **Quem compilou o quê:** a NitroSystem bate melhor com a 2.0 sp2 (519 funções) do
  que com a sp1p2 (517). O SDK é sp1, a NitroSystem e o MSL são sp2, como no Pokémon
  Platinum.
- **O conferidor de chamadas errou de novo, e para o lado seguro.** Acusou 3 erros em
  `AlarmCallback`. Não era o nome: existem duas funções `static` com esse nome
  (`stream.c` e `capture.c`), e o conferidor comparava as chamadas de uma com o endereço
  da outra. Agora ele só usa a função do `.o` com o mesmo tamanho da do jogo: 708 de 708.
- **Ligada do fonte.** O passo seguinte foi o build usar os `.o` compilados no lugar do
  assembly. A primeira tentativa linkou, mas mudou 19 mil bytes. As causas, uma a uma:
  - a `.rodata` de um arquivo terminava num endereço ímpar, e o linker alinha o próximo
    a 4: tudo depois andou 2 bytes. O enchimento é do arquivo;
  - o jogo aponta para campos no meio de estruturas (`NNS_G3dGlb + 0x80`), e o `.o` só
    tem o começo; as relocações viraram "símbolo + deslocamento";
  - funções de código idêntico (os `NNS_G3dFree*`) estavam com o nome trocado. Os bytes
    batiam, mas o linker põe as funções na ordem do `.o`, e a ordem saiu errada.
  Com isso, os 59 arquivos da NitroSystem entram compilados e a ROM sai idêntica.
- **Uma "relocação" que não era.** O `dsd init` marcou o número `0x021b0fdc`, numa
  tabela de constantes, como ponteiro para a variável `static` `sDriverInfo` do som. Uma
  variável `static` não pode ser usada de outro arquivo, então era só um número que
  parecia endereço. Saiu da lista de relocações.
- **O NitroSDK também.** 73 dos 87 arquivos do SDK passaram a vir do fonte, e a ROM
  continua idêntica. Com a NitroSystem, são 90 KB do ARM9 (9,3%) saindo de código C.
  O último obstáculo foi o linker jogar fora `OS_DisableProtectionUnit`, que ninguém
  chama mas o jogo tem. A opção `-force_active` resolvia com um nome e falhava com a lista
  inteira. Testando listas de tamanhos diferentes, o limite apareceu: uns 256 caracteres. A saída foi o
  mesmo pedido dentro do arquivo do linker (`FORCE_ACTIVE` no `.lcf`).
- **Símbolos que nenhum `.c` define.** Cinco arquivos (`os_arena.c`, `os_thread.c`...)
  usam `SDK_SYS_STACKSIZE`, `SDK_MAIN_ARENA_LO`... que o `.lcf` da Nintendo calculava.
  Os valores foram lidos das constantes que o jogo tem gravadas (ex.:
  `SDK_SECTION_ARENA_EX_START` = `0x023e0000`) e entraram em `simbolos_linker.lcf`.
  A prova de que estão certos é a ROM: um valor errado mudaria esses bytes. Agora são 78 arquivos do SDK, 92 KB (9,7% do ARM9) de C, e
  a ROM idêntica. A configuração gerada do zero sai igual à versionada.
- **ITCM e DTCM.** Seis arquivos do SDK têm partes fora do ARM9 principal. Primeiro um
  teste à mão com `os_irqHandler.c` (o `dsd` recusou o mesmo nome em dois módulos; com
  o apelido `os_irqHandler.itcm.c` aceitou, e o mapa do linker mostrou `OS_IrqHandler`
  vindo do `.o` compilado). Depois `ligar_bibliotecas.py` aprendeu a dividir o ITCM e a
  achar os dados do DTCM como faz com o resto. Dois arquivos (`os_cache.c`,
  `mi_dma_gxcommand.c`) nem precisavam disso: as funções deles no ITCM não estão no
  jogo, o linker as descartou. Foram para 84.
- **Erro de conta, e um empate mal resolvido.** O `os_china.c` aparecia como "não
  liga", mas nenhuma função dele está no jogo: a checagem olhava todos os `.o`
  compilados. E o `card_backup.c` "não batia" por engano: `PXI_Init` (8 bytes, um "pula
  para X") é idêntica a `CARD_WaitBackupAsync`, o empate foi para o arquivo errado, e
  quando o `card_backup` apareceu no lugar dele parecia "repetido". O desempate agora
  olha para onde a função pula, e o `pxi_init.c` apareceu. O total certo é 87 (sem o
  `os_china.c`, com o `pxi_init.c`), não os 87 de antes por coincidência.
- **`.version`.** A string `[SDK+NINTENDO:BACKUP]` do `card_backup.c` está em
  `0x02000bd4`, logo depois do crt0, como o `.lcf` da Nintendo manda. Entrou pelo mesmo
  apelido do ITCM (`card_backup.version.c`). 86 de 87.
- **O que falta: `gx_vramcnt.c`.** `GX_SetBankForSubBG` faz o mesmo que o fonte, mas o
  `switch` saiu com outra árvore de comparações. Testados: as dez versões 2.0 do
  compilador, as 24 ordens dos `case`, `case` a mais ou a menos, outras otimizações.
  Nenhum bateu; o arquivo que a Nintendo compilou deve ser diferente do público.

## 18. O MSL ligado
- **De onde vêm os `.o`.** O MSL não tem fonte público: vem pronto nos `.a` do
  CodeWarrior. `msl.sh` tira de dentro deles os 204 `.o` (o C, o C++, o Runtime e a
  matemática de ponto flutuante, que é assembly). 68 arquivos estão no jogo e 62 ligam.
- **O primeiro link nem fechava.** O `mwldarm` quer que todo nome citado exista, mesmo
  em código que ele vai descartar: `powf` chama `pow`, e nenhum dos dois está no jogo.
  Ligar os `.a` inteiros dava "Internal linker error", e definir os nomes como 0 no
  `.lcf` também. A saída foi um `.o` de funções vazias para esses nomes, gerado no build.
- **Depois fechou, e a ROM saiu diferente.** Nove causas, achadas comparando o mapa do
  linker com os endereços do jogo, da primeira diferença para a frente:
  - um rótulo automático (`.L_020ead44`) que o `.o` não define: o `mwldarm` o resolveu
    como 0, **sem erro**, e pôs um desvio de 8 bytes no meio do código. Agora o build
    para se o código do jogo citar um nome que ninguém define;
  - funções gêmeas com o nome trocado: `strtold` no lugar de `strtod`, os stubs de
    8 bytes `exp`/`pow` com nomes do `math.o`, os destrutores do RTTI. Cada troca
    mantinha viva uma função a mais;
  - a tabela de exceções guardava a entrada de toda função, e com ela a função que o
    jogo descartou;
  - a `.exception` de cada função começa num múltiplo de 4 (Erro nº 10, abaixo);
  - `__sinit__` gravado como rótulo de código: o linker criou um desvio para cada
    ponteiro da tabela `.ctor`;
  - o typeinfo de `std::exception` é "multidef": o jogo ficou com a cópia de um arquivo
    dele, e a do `.o` precisava ceder o lugar;
  - os buffers de `stdin`/`stdout` e a vtable de `__si_class_type_info` só aparecem por
    ponteiros dentro de dados, que a ferramenta não seguia: o linker os deixou de fora e
    pôs 0 nos ponteiros;
  - um `b .` de 2 bytes no fim de outra função parecia o `__rt_div0` inteiro.
  Com isso a ROM saiu idêntica: 132 KB (13,6% do código do ARM9) vêm de bibliotecas
  ligadas. A configuração gerada do zero sai igual à versionada, e o NitroSDK e a
  NitroSystem saem iguais aos de antes (menos o `.L_020ead44`).
- **Erro nº 10:** a ferramenta não arredondava o fim da `.exception` de um arquivo para
  múltiplo de 4, porque o `.o` diz alinhamento 1 e eu acreditei. O linker alinha a 4
  mesmo assim, e o pedaço do jogo seguinte começava 3 bytes antes do que devia.

## 19. Os arquivos do jogo (Fase 2.4)
- **O que a ROM não diz.** O linker põe o código de cada `.cpp` junto, mas não marca
  onde um acaba. Nomes de arquivo não há (nem em strings de `assert`), e todas as 4.942
  funções do jogo começam em múltiplo de 4, então o alinhamento também não ajuda.
- **O que o compilador mostrou.** Compilei arquivos de teste com a 2.0 e olhei o `.o`:
  cada string literal vai numa seção `.data` própria, logo depois da função que a usa
  primeiro, e uma string repetida no mesmo arquivo vira uma cópia só. No jogo há 105
  strings com várias cópias (`"BackButton"` tem 9): uma por arquivo. Daí a regra:
  quem lê a mesma cópia está no mesmo arquivo. E as funções inline vão para o fim do
  `.text` do arquivo.
- **A ordem do linker é uma só.** As 149 funções da tabela `.ctor` (os construtores de
  variáveis globais, um por arquivo que tem) estão em ordem crescente de endereço, e os
  dados que cada uma escreve também: a ordem dos arquivos é a mesma no código e nos
  dados.
- **O resultado:** 175 pedaços com certeza de um arquivo só, 38% dos bytes do código do
  jogo, em `delinks.txt` com a ROM idêntica. 338 pares de cópias da mesma string
  conferem a regra, sem nenhuma falha. O `dsd` ainda pegou um caso que eu não tinha
  previsto: dois pedaços do `GameModeInventory` com o código numa ordem e as strings na
  outra ("ciclo na ordem de link"). Era uma função inline no fim do arquivo, que lê
  uma string do começo dele: os dois pedaços são o mesmo arquivo, e a ferramenta agora
  junta pedaços assim.
- **Erro nº 11:** no meu teste, cada arquivo tinha um bloco só de variáveis, antes das
  strings, e eu ia cortar o `.data` em cada variável que viesse depois de uma string.
  No jogo, os typeinfo e as vtables ficam intercalados com os nomes das classes, e 18
  funções liam strings dos dois lados de um desses "cortes". Larguei esse corte; a
  regra das cópias de strings não depende dele.
- **Onde parou (pausa em 08/10/2026).** Os 175 pedaços estão em `delinks.txt` e a ROM
  sai idêntica. O próximo passo seria achar onde cada arquivo começa e acaba de
  verdade, juntando às pontas de cada pedaço as funções sem strings. As pistas para
  isso são as variáveis `.bss`/`.data` que cada função usa, quem chama quem e as
  sequências de métodos da mesma classe.

## 20. A métrica de progresso
- **Um número que alguém possa cobrar.** Os "13,6%" eram uma conta feita à mão. Agora
  `decomp/tools/progresso.py` lê `config/YWSE/` e diz quantos bytes de código o build
  tira de um `.o` compilado de fonte: **13,48%** (135.556 de 1.005.780 bytes), 87,71% das
  bibliotecas e 0,01% do jogo. A conta está em [BUILD.md](BUILD.md#a-métrica-de-progresso).
- **Por que o número caiu 0,1 ponto.** Refazendo a conta antiga, só a `.text` do ARM9 e do
  ITCM dava os mesmos 13,58%. Ficava de fora a `.init`: 7.760 bytes de `__sinit_*`, as
  funções que o compilador gera em cada `.cpp` que tem variáveis globais com construtor.
  São código do jogo e vão ter de sair do fonte junto com o arquivo delas, então entram.
- **Uma conferência que pode falhar.** O script lê o mapa do linker e confere que tudo o
  que caiu dentro de um arquivo `complete` veio do `.o` dele. O mapa só mostra o nome do
  `.o` sem a pasta, e o pedaço cortado de `jogo/X.cpp` também se chama `X.o`; então a
  lista de `.o` que o linker recebeu entra na conta. Testado ao contrário: um núcleo
  marcado `complete` sem compilar e um arquivo com o fim esticado sobre o vizinho foram
  os dois recusados.
- **Código ARM no meio do jogo.** Procurando o que separa biblioteca de jogo, achei dois
  blocos de funções ARM (o jogo é Thumb) dentro da região do jogo. Não são outra
  biblioteca: são métodos do próprio jogo, do `CMoviePlayer` (`0x0208ab44`, 24 funções) e
  do `Timer` (`0x020c70e4`, 46 funções). A BioWare compilou alguns arquivos em ARM,
  provavelmente os que precisam de velocidade. Para decompilá-los, `compilar.sh` vai
  precisar de um modo ARM. O começo do ITCM (`0x01ff8000`-`0x01ffe89c`, ARM, sem nome)
  continua sem dono e fica contado como jogo.

## 21. Procurando as bordas dos arquivos, e a opção `-ipa file`
O passo seguinte da Fase 2.4 era estender os 175 núcleos às funções das pontas, que não
usam strings. Testei cada pista contra o que já se sabe (os núcleos) antes de usá-la:

- **O enchimento entre funções.** Se o linker enchesse com outro valor entre arquivos, a
  borda apareceria. Não: os 2 bytes depois de uma função Thumb são sempre `0000`
  (1.091 nos buracos, 43 dentro dos núcleos, 5 no fim de núcleos).
- **Erro nº 12: a variável escondida do `__register_global_object`.** Num `.o` de teste,
  cada objeto global com destrutor ganha uma variável de 12 bytes, só do arquivo, que
  vai como 3º argumento para `__register_global_object`. Achei que isso amarraria cada
  função à `.bss` do seu arquivo. No jogo são 3.119 chamadas, 3.076 delas em funções
  comuns e não nos `__sinit`, e só 481 variáveis diferentes: `main` usa a mesma três
  vezes. São `static` dentro de funções inline (o padrão `Singleton<T>`), copiadas em
  cada lugar que as chama, e por isso compartilhadas entre arquivos. Não serve de prova.
- **"Uma classe, um arquivo".** Falha em 9 classes: métodos do `GuiTextInterface` e do
  `GuiAnimatedImage` estão dentro do núcleo do `GameModeCombat`, por exemplo (métodos
  inline emitidos no arquivo que os usa).
- **Quem chama e quem usa os dados.** Conferidas nas 604 funções dos núcleos, onde o
  arquivo é conhecido: "só é chamada por funções do núcleo X" erra 51 de 249 vezes, e
  "usa dados que só o núcleo X usa" erra 11 de 226. Pistas, não regras.
- **A `.rodata`.** Em C++, uma constante `const` fora de função é local ao arquivo, e
  achei que a `.rodata` funcionaria como as strings. Mas o começo dela são constantes
  globais (`extern const`): a palavra `0xffffffff` em `0x020ef844` é lida por 150
  funções do jogo todo, e `0x020ef814` é o ponteiro para a string vazia do `CStr`.
- **O que deu certo: `-ipa file`.** Para entender a `.data`, compilei `.o` de teste e
  comparei com o jogo. No jogo, o nome de uma classe (`"4Time"`) vem antes do typeinfo
  dela e antes das strings do arquivo; com as nossas flags, ia para o fim. Testando
  opções, só `-ipa file` reproduz a ordem. As três funções continuam idênticas com ela,
  e ela entrou em `compilar.sh`. A conferência (`conferir_ipa.py`, em
  [COMPILADOR.md](COMPILADOR.md#-ipa-file-a-ordem-dos-dados)) mede duas coisas que a
  opção padrão não produziria: 51 nomes antes do typeinfo (49 de classes derivadas,
  como a ordem prevê) e 266 de 270 typeinfo sem strings no meio. As 4 exceções são uma
  string de função inline, que com `-ipa file` vai para o bloco das variáveis.
- **Erro nº 13:** na primeira comparação usei `-ipa off` como se fosse o padrão. Não é:
  ele até inverte a ordem das funções no `.o`. O padrão se comporta como
  `-ipa function`.
- **O que o `-ipa file` diz sobre um arquivo.** O `.text` tem as funções na ordem do
  fonte e, no fim, as multidef (métodos inline e de template). A `.data` tem primeiro as
  variáveis, vtables, typeinfo e nomes, e por último as strings do código. Então uma
  passagem de "multidef" para "comum" no `.text`, ou de "string" para "variável" na
  `.data`, é o começo de outro arquivo. A primeira só tem 8 funções reconhecíveis (as de
  `Singleton<T>` e `ResourcePool<T>`, pelo nome). A segunda separa a `.data`, mas uma
  variável pode ser lida de qualquer arquivo, então não diz onde fica uma função.
- **A conclusão.** O binário não guarda o suficiente para provar em que arquivo cai cada
  função sem strings. A borda de um arquivo se prova quando ele é decompilado inteiro: o
  `.o` tem de reproduzir juntos o `.text`, as variáveis, as strings, a `.bss` e o
  `__sinit`. Uma função a mais ou a menos na ponta, se usar strings ou variáveis do seu
  arquivo, muda essa ordem e o build acusa. Se só usar dados globais, a borda errada não
  muda nenhum byte: o erro fica só na organização do fonte, e não na ROM.

## 22. As sobras das bibliotecas (tentativa)
- **Os dois arquivos com modelos do C++.** O `CPP_ios.c` e o `CPP_msl_thread.c` ficavam de
  fora porque definem funções de modelo (`shared_ptr<char>`, `move_ptr`) que o jogo tem
  em outro lugar. Elas são "multidef" no `.o`, e a ideia era deixar a cópia do `.o` ceder
  à do jogo, como já se faz com os dados. Com a trava desligada, a configuração
  regenerada do zero (desde o `af53d17`) saiu igual à versionada mais os dois arquivos,
  mas o build cresceu 160 bytes: o `CPP_ios.o` trouxe a sua cópia das funções, e o jogo
  tem outra no mesmo lugar, que não cede porque no assembly elas têm nome automático.
  Pior, as duas cópias são diferentes: no jogo, a sequência depois do `CPP_ios` tem os
  tamanhos 0x1a, 0x2, 0x4, 0x12..., e no `.o` 0x28, 0x14, 0xc, 0x2. As cópias que o
  linker da Nintendo guardou vieram de outra compilação. A trava estava certa; voltou.
- **O resto** continua como estava em [BUILD.md](BUILD.md#as-bibliotecas-da-nintendo-ligadas-do-fonte):
  `CPP_locale` e `C_math_arm` (funções em outra ordem), `CPP_iostream` e `C_ansi_fp`
  (uma função do meio não bate) e o `gx_vramcnt.c` do SDK. Juntos são uns 1,9% do
  código; ficam para depois, porque o objetivo agora é o código do jogo.

## 23. A `CExoString` inteira (Fase 3.1)
- **A classe.** Doze bytes: a vtable (só o destrutor é virtual), `m_sString` e
  `m_nBufferLength`, com os nomes da `CExoString` do Neverwinter Nights, do mesmo motor.
  São 33 funções seguidas, de `0x02004e00` a `0x020055b8`, e a ordem do jogo é a do
  fonte: os dois auxiliares de memória, os construtores, o destrutor, os operadores, e
  `AsINT`, `CStr`, `Find`, `Format`, `Left`, `SubString`, `UpperCase`, `CompareNoCase`...
  28 bateram na primeira compilação. As outras 5 ensinaram as lições que estão em
  [COMPILADOR.md](COMPILADOR.md#o-que-aprendemos-igualando-funções): chamar uma função
  dentro dos argumentos muda a ordem das leituras, e um valor guardado numa variável é
  lido uma vez só.
- **Uma biblioteca que faltava.** `LowerCase`, `UpperCase` e `CompareNoCase` chamam três
  funções sem nome no meio do MSL. Não estavam no MSL C: estão no **MSL_Extras**
  (`extras_string.h`), que a Fase 2.3 não tinha procurado. São `strlwr`, `strupr` e
  `stricmp`. As duas primeiras têm o mesmo código e só diferem na tabela que leem
  (`__lower_map` ou `__upper_mapC`), e foi assim que ganharam o nome.
- **Os dados também saem do fonte.** O destrutor virtual é a primeira função virtual da
  classe, então o `.o` traz a vtable, o typeinfo e o nome `"10CExoString"`, além da
  string `"%i"` do construtor de `int`. Na ordem do `.o` eles caem exatamente nos
  endereços do jogo (`0x020f5510` a `0x020f553c`).
- **O que fica para depois.** O arquivo de verdade tem mais coisa: o `""` para onde o
  `g_emptyString` aponta (`0x020f5508`), uma variável em `0x020f550c` e o `__sinit` dela
  (`0x020edb7c`), que também constrói dois objetos da biblioteca C++ em `0x021609e8`.
  Como no `CStr` antes, a parte provada entrou isolada; o resto continua no assembly.
- **Um marcador em cada arquivo.** A variável de `0x020f550c` é `FX_Div(2.0, π)`,
  calculada no `__sinit`. E os 149 `__sinit` do jogo fazem a mesma conta, cada um para a
  sua variável: é uma constante declarada num header que quase todo arquivo inclui, e
  cada arquivo tem a sua cópia, como as strings. São 149 marcadores, um por arquivo, na
  `.data` e na `.init`. Não dizem onde fica cada função, mas ligam cada núcleo ao seu
  `__sinit` e mostram quando não há arquivo nenhum entre dois núcleos. É o próximo passo
  da Fase 2.4.
- **Uma ferramenta para pôr arquivos no build.** `nomes_do_fonte.py` encaixa o `.o` na
  faixa do arquivo, comparando os bytes, e troca os nomes em `symbols.txt` pelos do
  compilador (passo a passo em [BUILD.md](BUILD.md#um-arquivo-decompilado-entra-no-build)).
- **Erro nº 14:** a ferramenta transformava toda relocação para o meio de um símbolo em
  "símbolo + deslocamento", inclusive os ponteiros para funções Thumb, que têm o bit 0
  ligado (`0x02004f71` é a função de `0x02004f70`). O linker somaria 1 e ainda ligaria o
  bit: o ponteiro andaria 1 byte. O build acusou; agora esses ficam como estavam.
- **Erro nº 15:** o construtor padrão existe duas vezes no `.o` (`C2` e `C1`, mesmos
  bytes) e uma vez no jogo. A ferramenta deu ao endereço o nome do primeiro, o `C2`, mas o
  próprio `.o` chama o `C1` (`CExoString result;` no `operator+`). Com os dois citados, o
  linker guardou os dois, e tudo depois andou 16 bytes. Agora ela prefere a cópia que o
  `.o` usa, e depois o `C1`.
- **Resultado:** ROM idêntica, 35 funções conferidas no CI, e o progresso foi de 13,48%
  para **13,67%** (o código do jogo, de 84 para 2.044 bytes).

## 24. Os marcadores e o alocador (Fases 2.4 e 3.2)
- **Os marcadores conferidos.** A constante `2/π` de cada arquivo (DIARIO 23) foi achada
  nos 149 `__sinit`, um deles em ARM. As 149 variáveis são diferentes e estão, na `.data`,
  na mesma ordem dos `__sinit` na `.init`: a conferência podia falhar e não falhou. E
  nenhuma cai no meio das strings de um núcleo, como o modelo `[variáveis][strings]`
  exige. Daí sai uma prova nova: **se há um marcador entre as strings de dois núcleos,
  eles são arquivos diferentes**, porque o marcador é uma variável e não pode vir depois
  das strings do próprio arquivo. Dos 170 núcleos com strings, 77 têm marcadores entre
  eles e o anterior, e 34 têm dois ou mais, o que quer dizer que há pelo menos um arquivo
  inteiro sem strings no meio.
- **Um desses arquivos é o alocador.** Entre o núcleo `02002918` e o do som há dois
  marcadores. O primeiro, `0x020f5424`, é do `__sinit_020eda24`, que configura 5 "pools"
  de blocos pequenos e registra o destrutor de cada um. A `.bss` dele fecha certinho:
  4 bytes de um sinalizador, 5 × 12 das variáveis do `__register_global_object` e
  5 × 0x18 dos pools, de `0x02110f34` a `0x02110fec`. O código é o `new`/`delete` do
  jogo (que pega um bloco de pool ou, se não der, do heap do NitroSDK) e uma função que
  o crt0 do SDK chama antes do `main`: `NitroStartUp`, que monta os heaps.
- **O primeiro arquivo do jogo com tudo.** `src/Aurora/Memory.cpp` traz o código (14
  funções), o `__sinit`, a entrada na `.ctor`, o marcador na `.data` e a `.bss`, e a ROM
  sai idêntica. 9 funções bateram de primeira; as lições das outras estão em
  [COMPILADOR.md](COMPILADOR.md#o-que-aprendemos-igualando-funções). A do marcador: ele só
  vai para a `.data` declarado `const` (`const fx32 X = FX_Div(...)`), o que confirma que
  é uma constante de header, local a cada arquivo.
- **Um erro do jogo.** Quem devolve um bloco ao pool grava o índice dele na lista, mas
  não diminui o contador de blocos usados. O bloco nunca é reaproveitado, e quando um pool
  enche, os pedidos vão para o heap. Ficou no C++ como está, com um comentário. *(A frase
  "o bloco nunca é reaproveitado" está errada: Erro nº 18, seção 27.)*
- **As pontas continuam abertas.** As duas funções vazias depois do `NitroStartUp`
  (`0x020034ec` e `0x020034f0`) e as que vêm antes do destrutor podem ser deste arquivo ou
  dos vizinhos; ficaram no assembly.
- **Resultado:** ROM idêntica, 50 funções conferidas no CI, e o progresso foi de 13,67%
  para **13,76%**.

## 25. As variáveis escondidas, de novo: 84% do código separado por arquivo (Fase 2.4)
- **Erro nº 16: a conclusão do Erro nº 12 estava errada.** Decompilando o sistema de
  arquivos, achei o `Singleton<CDSFileSystem>::Instance()` copiado (inline) dentro de
  uma função: a guarda e o objeto ficam na `.bss` do `main.cpp`, mas a variável de 12
  bytes passada ao `__register_global_object` fica na `.bss` deste arquivo. No Erro
  nº 12 eu tinha visto o `main` usar a mesma variável três vezes e concluído que ela era
  compartilhada entre arquivos. Três usos no mesmo arquivo não dizem nada sobre arquivos
  diferentes. Um `.o` de teste resolve: numa função inline com um `static`, o objeto e a
  guarda saem "multidef" (uma cópia por programa), e a variável sai **local**, uma por
  arquivo, a mesma para todas as funções do arquivo.
- **A regra nova.** Duas funções que passam a mesma variável ao `__register_global_object`
  estão no mesmo arquivo, como duas funções que leem a mesma cópia de uma string. Como o
  `Instance()` está copiado em mais de 3 mil lugares, ela alcança quase todo o código.
  `mapa_arquivos.py` agora junta as funções pelas duas regras (990 funções passam 438
  variáveis) e também pela ordem da `.bss`: se um pedaço de cima tem variáveis depois das
  de um de baixo, são o mesmo arquivo (dentro de um arquivo o compilador ordena essas
  variáveis do jeito dele, como nos pools do alocador).
- **As conferências, todas podiam falhar:** os 337 pares de cópias da mesma string
  continuam em pedaços diferentes; nenhum dos 149 marcadores cai no meio das strings de um
  pedaço; e, pela contiguidade da `.bss`, 8 pedaços contêm variáveis de um `__sinit`, cada
  um de um só, na ordem dos `__sinit`.
- **Arquivos sem código.** O primeiro pedaço vai do `main` até `0x02002d0c`, e há três
  marcadores antes das strings dele. Dois são arquivos sem nenhuma função no jogo. Faz
  sentido: o linker descarta as funções que ninguém chama, mas o `__sinit` de um arquivo
  fica sempre (a tabela `.ctor` aponta para ele), e com ele os dados que ele grava. E a
  ordem dentro do bloco de variáveis de um arquivo mistura variáveis e typeinfo, então o
  marcador não marca o começo do arquivo, só que o arquivo existe.
- **Resultado:** 161 pedaços cobrem **84%** dos bytes do código do jogo (antes, 175
  pedaços e 38%), 125 deles com a faixa da `.bss`; a ROM continua idêntica. Um pedaço que
  atravessa um arquivo já decompilado (o `HashResourceName` no meio do sistema de
  arquivos) fica em duas partes. O que sobra (16%) são as pontas e os arquivos que não
  usam strings nem `Singleton`.

## 26. O sistema de arquivos em C++ (Fase 3.3)
- **O que o arquivo faz.** `src/Aurora/CDSFileSystem.cpp` junta quatro classes:
  `CDSFileSystem` abre os arquivos do cartucho (NitroFS), conta quantas vezes cada um
  foi aberto, lê de uma vez ou em segundo plano (uma fila de pedidos, com estado e um
  tempo-limite de 60 quadros) e descomprime LZ, RL e Huffman; `CErfTable` é um pacote
  HERF (o índice de 12 bytes por recurso, achado por busca binária no hash); `CErfMan`
  abre o `test.herf` e o do idioma do console (francês, italiano, alemão, espanhol; o
  resto usa o inglês); e `CErfResource` é um recurso pedido pelo nome, que procura
  `nome.small` (comprimido) quando pedem. São 52 funções, 0x02008bb0-0x0200a308.
- **Erros do jogo, mantidos como estão:** o destrutor de `CErfTable` só apaga o índice
  quando ele é NULL (`==` no lugar de `!=`); `CErfTable::Open` não apaga o cabeçalho de
  8 bytes que leu; os 12 caminhos que `Init` guarda nunca são usados.
- **Os templates.** O jogo tem as cópias de `CExoArrayList<T>` (a lista do motor) e dos
  construtores inline espalhadas no fim de vários arquivos: cada `.o` gera a sua cópia
  "multidef" e o linker fica com a primeira que vê. Das 72 funções comparadas, 20 são
  cópias que o jogo guardou no `main.cpp` e no `CExoString.cpp`, e todas batem. O
  compilador põe **cada função numa seção própria**, então dá para fazer a nossa cópia
  ceder o lugar à do jogo sem sobrar byte: o `preparar_link.py`, que já fazia isso com as
  bibliotecas, agora faz com o `src/` também, olhando os nomes de todo o código cortado
  do jogo (não só dos `_dsd_gap`: o destrutor de `CDSFileSystem` está num pedaço `jogo/`,
  e o linker acusou "Multiply-defined" até isso ser corrigido).
- **`Singleton<T>` tem um destrutor virtual vazio.** Três coisas independentes mostram
  isso, e as três só batem com ele: (1) no `Instance()` copiado, só o endereço do
  primeiro campo sai como constante pronta (sem destrutor na base, saem os dois
  primeiros); (2) no construtor de `CErfMan`, o endereço do objeto é carregado antes da
  vtable; (3) na `.data`, typeinfos e vtables saem ordenados por tamanho, e os empates
  (`CErfMan`/`CDSFileSystem`, nome de `CDSFileSystem`/vtable de `CErfTable`) só ficam na
  ordem do jogo com ele. A vtable de `Singleton<T>` que isso cria não é usada por
  ninguém, e o linker a descarta, no jogo e aqui.
- **O `FSFile` do NitroSDK.** O jogo copia um `FSFile` palavra por palavra nos primeiros
  0x20 bytes e em dois blocos depois (16 e 24 bytes): é o formato do SDK, com `prop` e
  `arg` como `union`s. O `include/nitro.h` agora o declara assim.
- **O `memcpy` do jogo é o intrínseco do compilador.** Com tamanho constante e pequeno
  ele vira cópias de byte no lugar (os campos do índice HERF, o `"./"` de 3 bytes);
  senão, uma chamada. O jogo tem as duas formas, então o `include/msl.h` manda o
  `memcpy` para o `__memcpy`. As 50 funções de antes continuam iguais.
- **O `DelIndex` não bateu, e ficou no assembly.** `CExoArrayList<T>::DelIndex` (tira um
  elemento e puxa os outros) existe no jogo para vários `T`. O nosso template reproduz
  byte a byte a cópia para um tipo de 16 bytes (`0x0202d788`), mas não as dos tipos de
  0x18, 0x1c e 0x60 bytes: nelas o jogo calcula o endereço do elemento seguinte como
  "deslocamento atual + tamanho" (redução de força), e o nosso refaz a multiplicação.
  Com 16 bytes é um deslocamento de bits, e nem o jogo reduz. Testei 960 formas
  equivalentes do laço (decremento, tipo de laço, incremento, expressão do elemento),
  as versões 1.2, 2.0 e DSi do compilador, opções de otimização e uma função auxiliar
  inline: nenhuma deu os quatro casos. As duas cópias deste arquivo (`0x0200a1b0` e
  `0x0200a258`) estão no fim do `.text`, no meio de outras cinco cópias de template que
  batem; o trecho `0x0200a1b0-0x0200a308` (7 funções) continua do assembly, e o nosso `.o`
  cobre o resto. Fica para quando aparecer outra pista.
- **Erro nº 17: dois "acertos" que não eram.** Testando opções, `-O4,s` e `-ipa program`
  deram "0 diferenças" no `DelIndex`. Era porque, com elas, a função nem saía no `.o`, e o
  meu laço contava as linhas marcadas com `!!` de uma saída de erro. Os scripts de teste
  agora escrevem `ERR` quando a comparação não roda.
- **O alinhamento da `.data`.** Os nomes de classe e as strings saem no `.o` com
  alinhamento 1, mas no jogo cada um começa num múltiplo de 4: o linker alinha. O
  `nomes_do_fonte.py` agora conta assim, e pula as cópias multidef que ficaram em outro
  arquivo e as que ninguém usa.
- **Resultado:** ROM idêntica na primeira ligação. O arquivo vem do C++ com 45 funções,
  o `__sinit`, a `.ctor`, a `.data` (typeinfos, vtables e 21 strings) e a `.bss`; o
  `HashResourceName`, que estava isolado, entrou nele. 119 funções conferidas no CI
  (eram 50) e o progresso foi de 13,76% para **14,32%**.

## 27. O C++ do jogo rodando sobre os dados da ROM (Fase 3)
- **O teste.** A Fase 3 pede, para cada parte, um teste que leia dados reais. O mais forte
  é rodar o próprio C++ que gera os bytes do jogo, sem reescrever nada: o
  `decomp/tests/test_arquivos.cpp` compila `src/Aurora/Memory.cpp`, `CExoString.cpp` e
  `CDSFileSystem.cpp` com o `g++`, junto com `decomp/tests/nitro_pc.cpp`, que faz para o PC
  as funções do NitroSDK e do MSL que eles chamam (o cartucho vira a pasta
  `work/extract/files`, sem diferenciar maiúsculas; as três descompressões da BIOS; o heap).
  O `include/msl.h` ganhou um ramo para o PC, separado por `__MWERKS__` (o `mwccarm` o
  define); os bytes do DS não mudam (119 de 119 no `conferir_sem_rom.py`).
- **O que ele confere:** o `erf.dict` sai pela cadeia inteira do jogo (`CErfResource` →
  `CErfMan` → `CErfTable` → `CDSFileSystem`); para cada um dos nomes, o hash confere, o
  recurso é achado e lido, e os bytes batem com uma leitura do índice feita à parte; os
  `.small` saem descomprimidos com o tamanho do cabeçalho; a leitura de um pedaço, a fila em
  segundo plano (`ReadFileAsync`, `Update` e o callback) e o pacote de cada idioma do
  console. Tudo com o alocador do jogo atendendo cada `new` e `delete`.
- **Resultado:** os **8.689 recursos** do `test.herf` (5.111 `.small` em LZ, 200 sem
  compressão) saem certos; os idiomas 0 a 6 abrem o pacote certo (inglês para japonês,
  inglês e o resto). O `erf.dict` diz ter 8.691 nomes: os 2 últimos são entradas zeradas.
- **Testado ao contrário:** três erros plantados no fonte (a posição dentro do índice, o
  teste do tipo de compressão, a busca binária) foram todos pegos.
- **Erro nº 18: o comentário do alocador estava errado.** Eu tinha escrito que um bloco
  devolvido ao pool "nunca volta a ser usado". Rodando o código do jogo: o `Free` grava o
  bloco na vaga seguinte da lista de livres, então ele é o **próximo** a sair; o que se
  perde é o bloco que ocupava aquela vaga, e o contador só sobe, então o pool atende no
  máximo `m_nCount` pedidos na vida do jogo. O teste agora confere isso (pede, devolve e
  pede de novo: sai o mesmo bloco; o vizinho de cima nunca sai), e o comentário foi
  corrigido.
- **Um achado sobre o jogo.** Com o pool esgotado, devolver um bloco grava 2 bytes logo
  depois do fim da lista de livres. No PC isso estragou o heap na saída do programa. No DS
  é inofensivo por sorte: o heap do NitroSDK arredonda cada bloco para 32 bytes, e as
  listas dos 5 pools têm de 8 a 24 bytes de folga. O `OS_AllocFromHeap` de PC passou a
  arredondar do mesmo jeito.
- **Como rodar:** `make -C decomp arquivos` depois do `montar_rom.sh`. O CI compila o
  teste (assim pega se o C++ do jogo deixar de compilar fora do DS), mas não o roda: não
  tem a ROM.

## O que ainda não sabemos
Vídeos `.vx` (codec Actimagine), layout das telas `.gui`, paletas dos Chao, 311 nomes de
colunas GDA, se um item novo numa loja funciona, os limites que o código impõe (número de
itens, de personagens), o service pack exato do compilador (2.0 base ou sp1+) e as partes do combate listadas em
[COMBATE.md](COMBATE.md#16-o-que-ainda-não-sabemos). Os próximos passos estão no
[plano](PLANO-DECOMPILACAO.md).
