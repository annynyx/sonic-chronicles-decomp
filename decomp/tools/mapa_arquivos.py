"""Acha pedaços do código do jogo que com certeza vieram do mesmo arquivo-fonte.

O jogo foi compilado arquivo por arquivo (cada .cpp vira um .o, uma "translation
unit") e o linker pôs o .text de cada .o inteiro, um depois do outro. A ROM não guarda
onde um arquivo acaba e o outro começa. Este script recupera parte disso com uma regra
que dá para provar, e não com palpite:

  Uma string literal ("BackButton") é um dado local do arquivo que a usa. O compilador
  junta as repetidas dentro de um arquivo (uma cópia só), mas não entre arquivos: no
  jogo, "BackButton" aparece 9 vezes. Logo, duas funções que leem a MESMA cópia de uma
  string estão no mesmo arquivo. E, como o .text de um arquivo é contínuo, tudo o que
  fica entre elas também está.

A segunda regra vem do mesmo jeito de o compilador trabalhar. Um objeto global (ou um
"static" dentro de uma função) com destrutor é registrado com __register_global_object,
e o terceiro argumento é uma variável de 12 bytes que o compilador cria, LOCAL ao
arquivo. Uma função inline com um static dentro (Singleton<T>::Instance(), copiada em
mais de 3 mil lugares) usa, em cada arquivo, a variável daquele arquivo. Logo, duas
funções que passam a MESMA variável estão no mesmo arquivo.

Juntando as funções por string e por variável, e fechando os intervalos, saem os
"núcleos": faixas do .text que são de um arquivo só. (Dois núcleos com o código numa
ordem e as strings, ou as variáveis na .bss, na outra também se juntam: o linker usa a
mesma ordem de arquivos em todas as seções.) Um núcleo pode ser menor que o arquivo de
verdade, mas nunca mistura dois arquivos. As conferências, e o script para se uma falhar:
  - funções que leem cópias DIFERENTES da mesma string caem em núcleos diferentes;
  - nenhum "marcador" cai no meio das strings de um núcleo. Todo arquivo que inclui o
    header comum tem uma constante (2/pi) na .data, calculada no __sinit dele; ela vem
    antes das strings do arquivo (docs/DIARIO.md, 23 e 24);
  - a faixa da .bss das variáveis de um núcleo não cobre a de um arquivo já ligado.

Cada núcleo vira um arquivo em delinks.txt, "jogo/<Classe>_<endereço>.cpp" (a classe
mais citada nos nomes das funções, quando há uma), com o .text e, quando dá, o .data
das suas strings e a .bss das suas variáveis. Não é "complete": o build continua usando o código cortado do jogo,
só que agora em um .o por arquivo. O que fica entre núcleos continua nos _dsd_gap.

Uso: python3 decomp/tools/mapa_arquivos.py [--aplicar]
Sem --aplicar, só mostra os núcleos. Precisa de work/extract (montar_rom.sh o cria).
"""
import bisect, os, re, struct, sys
from collections import Counter, defaultdict
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
from capstone.arm import (ARM_OP_IMM, ARM_OP_MEM, ARM_OP_REG, ARM_REG_LR, ARM_REG_PC,
                          ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R12)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
CFG = os.path.join(REPO, "config/YWSE/arm9")
BASE = 0x02000000
ARM9 = open(os.path.join(REPO, "work/extract/arm9/arm9.bin"), "rb").read()
PASTA = "jogo/"          # os arquivos que este script cria em delinks.txt


def palavra(a):
    return struct.unpack_from("<I", ARM9, a - BASE)[0]


# --- o que já se sabe: seções, arquivos, funções, relocações ---------------------------
delinks = open(os.path.join(CFG, "delinks.txt")).read()
cabecalho, *arquivos = re.split(r"\n\n(?=\S)", delinks.strip("\n"))
SECOES = {m.group(1): (int(m.group(2), 16), int(m.group(3), 16))
          for m in re.finditer(r"(\.\w+)\s+start:(0x\w+) end:(0x\w+)", cabecalho)}
BIBLIOTECAS = ("NitroSystem/", "NitroSDK/", "MSL/")
ocupado = defaultdict(list)      # seção -> faixas que outros arquivos já têm
de_biblioteca = defaultdict(list)
for arq in arquivos:
    if arq.startswith(PASTA):
        continue
    for m in re.finditer(r"(\.\w+)\s+start:(0x\w+) end:(0x\w+)", arq):
        ocupado[m.group(1)].append((int(m.group(2), 16), int(m.group(3), 16)))
        if arq.startswith(BIBLIOTECAS):
            de_biblioteca[m.group(1)].append(ocupado[m.group(1)][-1])
# o .data do jogo vem antes do das bibliotecas (NitroSystem, NitroSDK, MSL); o .text do
# jogo é o maior trecho sem biblioteca (entre o crt0 do SDK e o NitroSystem)
DADOS = (SECOES[".data"][0], min(a for a, _ in de_biblioteca[".data"]))
libs = sorted([SECOES[".text"][:1] * 2] + de_biblioteca[".text"] + [SECOES[".text"][1:] * 2])
TEXTO = max(((x[1], y[0]) for x, y in zip(libs, libs[1:])), key=lambda f: f[1] - f[0])

funcoes = []                     # (endereço, tamanho, nome), só as do jogo
MODO, ENDERECO = {}, {}          # endereço -> "arm"/"thumb"; nome -> endereço
for l in open(os.path.join(CFG, "symbols.txt")):
    m = re.match(r"(\S+) kind:function\((\w+),size=(0x\w+)\) addr:(0x\w+)", l)
    if m:
        a, t = int(m.group(4), 16), int(m.group(3), 16)
        MODO[a], ENDERECO[m.group(1)] = (m.group(2), t), a
        if TEXTO[0] <= a < TEXTO[1] and not any(x <= a < y for x, y in ocupado[".text"]):
            funcoes.append((a, t, m.group(1)))
funcoes.sort()
INICIOS = [f[0] for f in funcoes]


def funcao_de(a):
    i = bisect.bisect_right(INICIOS, a) - 1
    return i if i >= 0 and a < funcoes[i][0] + funcoes[i][1] else None


relocs = [(int(m.group(1), 16), int(m.group(2), 16)) for m in
          re.finditer(r"from:(0x\w+) kind:load to:(0x\w+)", open(os.path.join(CFG, "relocs.txt")).read())]

# --- 1. as strings do .data do jogo ----------------------------------------------------
def texto(c):
    return 32 <= c < 127 or c in (9, 10, 13) or c >= 0x80


def tamanho_string(a):
    """Bytes da string em a (com o 0 do fim), ou None se ali não parece haver uma."""
    o = a - BASE
    s = ARM9[o:o + 1024]
    n = s.find(b"\0")
    if n < 1 or not all(texto(c) for c in s[:n]):
        return None
    acentos = sum(1 for c in s[:n] if c >= 0x80)
    if acentos and (n < 4 or acentos * 2 > n):      # bytes altos soltos: é outro dado
        return None
    if (a + n) % 4 == 2 and ARM9[o + n + 1] == 0x02:  # é um ponteiro 0x0200xxxx
        return None
    return n + 1


eh_string = bytearray(DADOS[1] - DADOS[0])          # 1 = byte de string
citados = {t for _, t in relocs if DADOS[0] <= t < DADOS[1]}
citados |= {palavra(a) for a in range(DADOS[0], DADOS[1], 4) if DADOS[0] <= palavra(a) < DADOS[1]}
for t in citados:
    n = tamanho_string(t)
    if n and not (n <= 2 and t % 4 == 0):          # "A\0" alinhado: pode ser um número
        eh_string[t - DADOS[0]:t - DADOS[0] + n] = b"\1" * n
# uma string colada no fim de outra também é string (citada de outro jeito)
for i in range(len(eh_string) - 1):
    if eh_string[i] and not eh_string[i + 1]:
        n = tamanho_string(DADOS[0] + i + 1)
        if n and n >= 3:
            eh_string[i + 1:i + 1 + n] = b"\1" * n
# zeros entre strings são strings vazias ("") ou o 0 de alinhamento
i = 0
while i < len(eh_string):
    if not eh_string[i]:
        j = i
        while j < len(eh_string) and not eh_string[j] and ARM9[DADOS[0] + j - BASE] == 0:
            j += 1
        if 0 < i and j < len(eh_string) and eh_string[j] and j - i <= 8:
            eh_string[i:j] = b"\1" * (j - i)
        i = max(j, i + 1)
    else:
        i += 1


def inicio_string(t):
    while t > DADOS[0] and eh_string[t - 1 - DADOS[0]] and ARM9[t - 1 - BASE] != 0:
        t -= 1
    return t


def conteudo(t):
    return ARM9[t - BASE:t - BASE + 1024].split(b"\0")[0]


# --- 1b. as variáveis do __register_global_object ------------------------------------
_CS = {"thumb": Cs(CS_ARCH_ARM, CS_MODE_THUMB), "arm": Cs(CS_ARCH_ARM, CS_MODE_ARM)}
for _c in _CS.values():
    _c.detail = True
VOLATEIS = (ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R12, ARM_REG_LR)


def argumentos(ini, alvo):
    """Os registradores conhecidos em cada chamada direta a `alvo` dentro da função que
    começa em `ini`: o que foi carregado de uma constante (ldr rX, [pc, #n]) e copiado
    sem mudar. Um rótulo (destino de desvio) zera o que se sabe."""
    modo, tam = MODO[ini]
    pc = 4 if modo == "thumb" else 8
    insns = list(_CS[modo].disasm(ARM9[ini - BASE:ini + tam - BASE], ini))
    rotulos = {op.imm for i in insns if i.mnemonic.startswith("b")
               for op in i.operands if op.type == ARM_OP_IMM}
    reg, saida = {}, []
    for i in insns:
        if i.address in rotulos:
            reg = {}
        ops, mn = i.operands, i.mnemonic
        if mn == "ldr" and len(ops) == 2 and ops[1].type == ARM_OP_MEM and ops[1].mem.base == ARM_REG_PC:
            reg[ops[0].reg] = palavra(((i.address + pc) & ~3) + ops[1].mem.disp)
            continue
        if (mn in ("mov", "movs", "add", "adds") and len(ops) >= 2 and ops[1].type == ARM_OP_REG
                and ops[1].reg != ARM_REG_PC and (len(ops) == 2 or (ops[2].type == ARM_OP_IMM and ops[2].imm == 0))):
            if ops[1].reg in reg:
                reg[ops[0].reg] = reg[ops[1].reg]
            else:
                reg.pop(ops[0].reg, None)
            continue
        if mn in ("bl", "blx"):
            if ops and ops[0].type == ARM_OP_IMM and ops[0].imm == alvo:
                saida.append(dict(reg))
            for r in VOLATEIS:
                reg.pop(r, None)
            continue
        try:
            for r in i.regs_access()[1]:
                reg.pop(r, None)
        except Exception:
            reg = {}
    return saida


RGO = ENDERECO["__register_global_object"]
cadeia = defaultdict(set)        # variável (o 3º argumento, r2) -> funções que a passam
for k, (a, t, _) in enumerate(funcoes):
    for regs in argumentos(a, RGO):
        if ARM_REG_R2 in regs:
            cadeia[regs[ARM_REG_R2]].add(k)

# os marcadores: a constante que cada __sinit calcula com FX_Div(2.0, pi) e guarda
FX_DIV = ENDERECO["FX_Div"]
marcadores = []
for a in sorted(x for x in MODO if SECOES[".init"][0] <= x < SECOES[".init"][1]):
    modo, tam = MODO[a]
    insns = list(_CS[modo].disasm(ARM9[a - BASE:a + tam - BASE], a))
    for k, i in enumerate(insns):
        if i.mnemonic in ("bl", "blx") and i.operands and i.operands[0].imm == FX_DIV:
            j = next(j for j in insns[k + 1:] if j.mnemonic == "ldr" and j.operands[1].type == ARM_OP_MEM)
            pc = 4 if modo == "thumb" else 8
            marcadores.append(palavra(((j.address + pc) & ~3) + j.operands[1].mem.disp))
            break
if marcadores != sorted(set(marcadores)):
    sys.exit("os marcadores não estão na ordem dos __sinit: a ordem dos arquivos não é uma só?")

# --- 2. os núcleos ----------------------------------------------------------------------
usam = defaultdict(set)          # string -> funções que a leem
for s, t in relocs:
    # "" fica de fora: um 0 citado pode ser também uma variável global que vale 0
    if DADOS[0] <= t < DADOS[1] and eh_string[t - DADOS[0]] and ARM9[t - BASE]:
        f = funcao_de(s)
        if f is not None:
            usam[inicio_string(t)].add(f)
# Cada grupo de funções (uma string, uma variável) é um intervalo do .text, com a faixa
# dos dados que o ligam: as strings na .data, ou a variável na .bss. Grupos que se cruzam
# no .text se juntam. E se juntam também quando a ordem dos dados contradiz a do código:
# o linker põe os arquivos na mesma ordem no .text, na .data e na .bss, então se um
# núcleo de cima no .text tem strings (ou variáveis) depois das de um de baixo, os dois
# são o mesmo arquivo. (Acontece com função inline: o compilador a escreve no fim do
# .text do arquivo, mas a string dela vai para o começo do .data; e dentro de um arquivo
# o compilador ordena as variáveis da .bss do jeito dele.)
VAZIO = (1 << 40, -1)
grupos = [[min(fs), max(fs), (t, t + len(conteudo(t)) + 1), VAZIO] for t, fs in usam.items()]
grupos += [[min(fs), max(fs), VAZIO, (c, c + 12)] for c, fs in cadeia.items()]
grupos.sort()


def unir(p, q):
    return [min(p[0], q[0]), max(p[1], q[1]), (min(p[2][0], q[2][0]), max(p[2][1], q[2][1])),
            (min(p[3][0], q[3][0]), max(p[3][1], q[3][1]))]


nucleos = []                     # [primeira função, última função, faixa .data, faixa .bss]
for g in grupos:
    if nucleos and g[0] <= nucleos[-1][1]:
        nucleos[-1] = unir(nucleos[-1], g)
    else:
        nucleos.append(g)
mudou = True
while mudou:
    mudou = False
    for i in range(len(nucleos)):
        for j in range(i + 1, len(nucleos)):
            if nucleos[i][2][1] > nucleos[j][2][0] or nucleos[i][3][1] > nucleos[j][3][0]:
                junto = nucleos[i]
                for q in nucleos[i + 1:j + 1]:
                    junto = unir(junto, q)
                nucleos[i:j + 1] = [junto]
                mudou = True
                break
        if mudou:
            break
faixas_bss = {k: n[3] for k, n in enumerate(nucleos) if n[3] != VAZIO}
faixas_str = {k: n[2] for k, n in enumerate(nucleos) if n[2] != VAZIO}
nucleos = [n[:2] for n in nucleos]
DE = [a for a, _ in nucleos]


def nucleo_de(f):
    return bisect.bisect_right(DE, f) - 1

# a prova: cópias diferentes da mesma string em núcleos diferentes
copias = defaultdict(list)
for t, fs in usam.items():
    copias[conteudo(t)].append({nucleo_de(f) for f in fs})
provas = falhas = 0
for texto_, grupos in copias.items():
    for i in range(len(grupos)):
        for j in range(i + 1, len(grupos)):
            if grupos[i] & grupos[j]:
                falhas += 1
                print(f"FALHA: duas cópias de {texto_!r} no mesmo núcleo")
            else:
                provas += 1
if falhas:
    sys.exit("a regra não vale para este jogo: confira a detecção de strings")
# um marcador (uma variável do header comum) não pode cair no meio das strings de um
# núcleo: no arquivo, as variáveis vêm antes das strings
for k, (a, b) in faixas_str.items():
    if any(a < m < b for m in marcadores):
        sys.exit(f"um marcador cai no meio das strings do núcleo {funcoes[nucleos[k][0]][0]:#x}")
# a .bss das variáveis de um núcleo não cobre a de um arquivo já ligado
for k, (a, b) in faixas_bss.items():
    if any(a < y and x < b for x, y in ocupado[".bss"]):
        sys.exit(f"a .bss do núcleo {funcoes[nucleos[k][0]][0]:#x} cobre a de um arquivo já ligado")

# --- 3. o .data de cada núcleo: as suas strings, se forem contínuas e só delas --------
faixa = {}
for t, fs in usam.items():
    k = nucleo_de(min(fs))
    fim = t + len(conteudo(t)) + 1
    a, b = faixa.get(k, (t, fim))
    faixa[k] = (min(a, t), max(b, fim))
# o dsd dá a um dado sem tamanho os bytes até o próximo símbolo: o fim da faixa tem de
# cair num começo de símbolo. O que sobra até ele (o 0 de alinhamento, uma string que
# ninguém cita direto) vai junto, se for só isso
SIMBOLOS_DADOS = sorted(int(m.group(1), 16) for m in re.finditer(
    r"^\S+ kind:(?:data|bss)\S* addr:(0x\w+)", open(os.path.join(CFG, "symbols.txt")).read(), re.M))
so_strings = {k: all(eh_string[a - DADOS[0]:b - DADOS[0]]) for k, (a, b) in faixa.items()}
for k, (a, b) in list(faixa.items()):
    i = bisect.bisect_left(SIMBOLOS_DADOS, b)
    proximo = SIMBOLOS_DADOS[i] if i < len(SIMBOLOS_DADOS) else DADOS[1]
    if all(eh_string[x - DADOS[0]] or ARM9[x - BASE] == 0 for x in range(b, proximo)):
        faixa[k] = (a, proximo)
dados = {}
ordem = sorted(faixa)
for k in ordem:
    a, b = faixa[k]
    sozinha = all(faixa[x][1] <= a or b <= faixa[x][0] for x in ordem if x != k)
    livre = all(b <= x or y <= a for x, y in ocupado[".data"])
    if so_strings[k] and sozinha and livre:
        dados[k] = (a, b)

# a .bss: da primeira à última variável do núcleo, se o fim cair num começo de símbolo
# (o dsd dá a um dado sem tamanho os bytes até o próximo símbolo)
bss = {}
INICIOS_DADOS = set(SIMBOLOS_DADOS)
for k, (a, b) in faixas_bss.items():
    if b in INICIOS_DADOS and SECOES[".bss"][0] <= a and b <= SECOES[".bss"][1]:
        bss[k] = (a, b)

# --- 4. nome e relatório -----------------------------------------------------------------
def classe(nome):
    m = re.match(r"([A-Za-z]\w*?)__(?:vfunc\d+|ctor|dtor|\w+?)_[0-9a-f]{8}$", nome)
    return m.group(1) if m else None


entradas = []
for k, (a, b) in enumerate(nucleos):
    # o fim vai até o múltiplo de 4: a função seguinte começa alinhada, e os 2 bytes
    # de enchimento são do .o desta (o linker não os poria no começo do próximo)
    ini, fim = funcoes[a][0], (funcoes[b][0] + funcoes[b][1] + 3) & ~3
    classes = Counter(c for f in funcoes[a:b + 1] if (c := classe(f[2])))
    # um pedaço do mesmo arquivo já decompilado (isolado, como o HashResourceName)
    # divide o núcleo em partes; as funções delas continuam sendo de um arquivo só
    partes, p = [], ini
    for x, y in sorted(ocupado[".text"]):
        if ini < y and x < fim:
            if p < x:
                partes.append((p, x))
            p = max(p, y)
    if p < fim:
        partes.append((p, fim))
    for n_parte, (pi, pf) in enumerate(partes):
        nome = f"{pi:08x}"
        if classes:
            c, n = classes.most_common(1)[0]
            if n * 2 >= sum(classes.values()):
                nome = f"{c}_{pi:08x}"
        linhas = [f"{PASTA}{nome}.cpp:", f"    .text       start:{pi:#010x} end:{pf:#010x}"]
        if k in dados and n_parte == 0:
            linhas.append(f"    .data       start:{dados[k][0]:#010x} end:{dados[k][1]:#010x}")
        if k in bss and n_parte == 0:
            linhas.append(f"    .bss        start:{bss[k][0]:#010x} end:{bss[k][1]:#010x}")
        entradas.append((pi, "\n".join(linhas)))

total = sum(f[1] for f in funcoes)
coberto = sum(f[1] for a, b in nucleos for f in funcoes[a:b + 1])
print(f"{len(usam)} strings lidas por {len(set().union(*usam.values()))} funções")
print(f"{len(cadeia)} variáveis do __register_global_object passadas por "
      f"{len(set().union(*cadeia.values()))} funções; {len(marcadores)} marcadores")
print(f"{len(nucleos)} núcleos, {sum(b - a + 1 for a, b in nucleos)} de {len(funcoes)} funções, "
      f"{coberto} de {total} bytes ({100 * coberto / total:.1f}%)")
print(f"{len(dados)} núcleos com o .data das strings junto")
print(f"{len(bss)} núcleos com a .bss das variáveis junto")
print(f"provas da regra: {provas} pares de cópias da mesma string, todos em núcleos diferentes;"
      " nenhum marcador no meio das strings de um núcleo")

if "--aplicar" not in sys.argv:
    for _, e in entradas[:5]:
        print(e)
    sys.exit(0)

# --- 5. delinks.txt: os arquivos do jogo em ordem de endereço, antes das bibliotecas ----
def inicio_texto(arq):
    m = re.search(r"\.text\s+start:(0x\w+)", arq)
    return int(m.group(1), 16) if m else 1 << 40


jogo = [(inicio_texto(a), a) for a in arquivos
        if not a.startswith(PASTA) and not a.startswith(BIBLIOTECAS)]
bibliotecas = [a for a in arquivos if a.startswith(BIBLIOTECAS)]
novos = [a for _, a in sorted(jogo + entradas)]
open(os.path.join(CFG, "delinks.txt"), "w").write(
    "\n\n".join([cabecalho] + novos + bibliotecas) + "\n")
print(f"delinks.txt: {len(entradas)} arquivos em {PASTA}")
