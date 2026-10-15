"""Põe em symbols.txt os nomes que o nosso C++ gera, no lugar dos nomes automáticos.

Quando um arquivo do jogo é decompilado (marcado "complete" em delinks.txt), o build
usa o .o compilado no lugar do pedaço cortado do jogo. O resto do código do jogo, que
ainda é assembly, chama essas funções pelo nome que está em symbols.txt: esse nome tem
de ser o que o compilador gera (o "mangled", como _ZN10CExoStringC1EPKc), senão o
linker não acha a função.

O script compila o fonte, pega em delinks.txt a faixa de cada seção do arquivo e
encaixa nela, em ordem, cada função e cada dado do .o, comparando os bytes com os do
jogo (os bytes de relocação não contam). O que não encaixa é pulado: é uma cópia que o
linker vai descartar porque ninguém usa (o compilador gera dois construtores, o C1 e o
C2, e o jogo só tem os que alguém chama). Se sobrar um pedaço do jogo sem nada do .o,
ou se uma seção do .o não couber, o script para: o fonte ainda não bate com o jogo.

Funções e dados "multidef" (templates, funções inline que não foram expandidas,
typeinfos, o objeto de um Singleton) saem em todo .o que os usa, e o linker fica com uma
cópia só. Se o nome já está em symbols.txt fora das faixas do arquivo, a cópia que fica
no jogo é aquela (de um arquivo ligado antes) e esta é pulada; o preparar_link.py faz o
linker usar a do jogo. Um dado multidef que não encaixa é pulado também: ninguém o usa, e
o linker o descarta (a vtable de Singleton<T>).

Os símbolos dentro das faixas são trocados pelos do .o; os "static" (e as strings, que
o compilador chama de @123) ficam "local", como no .o. Uma relocação que aponta para o
meio de um símbolo (o código guarda o endereço da vtable + 8, onde começam as funções)
passa a ser "símbolo + deslocamento" em relocs.txt, porque o dsd só aceita nomes.

Uso: python3 decomp/tools/nomes_do_fonte.py src/Aurora/CExoString.cpp [--aplicar]
Sem --aplicar, só mostra. Precisa de work/extract (montar_rom.sh o cria).
"""
import os, re, subprocess, sys, tempfile
from elftools.elf.elffile import ELFFile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CFG = os.path.join(REPO, "config/YWSE/arm9")
BASE = 0x02000000
ARM9 = open(os.path.join(REPO, "work/extract/arm9/arm9.bin"), "rb").read()


def faixas(fonte):
    """As seções do arquivo em delinks.txt: {".text": (início, fim), ...}."""
    texto = open(os.path.join(CFG, "delinks.txt")).read()
    m = re.search(rf"^{re.escape(fonte)}:\n((?:    .*\n)+)", texto, re.M)
    if not m:
        sys.exit(f"{fonte} não está em delinks.txt")
    return {s.group(1): (int(s.group(2), 16), int(s.group(3), 16))
            for s in re.finditer(r"(\.\w+)\s+start:(0x\w+) end:(0x\w+)", m.group(1))}


def pedacos(obj):
    """Cada seção de dados/código do .o, na ordem do arquivo: (nome da seção, bytes,
    alinhamento, posições de relocação, [(símbolo, deslocamento, tamanho, local, tipo)],
    multidef)."""
    elf = ELFFile(open(obj, "rb"))
    secoes = list(elf.iter_sections())
    simbolos = list(elf.get_section_by_name(".symtab").iter_symbols())
    rels = {}
    for s in secoes:
        if s.header.sh_type in ("SHT_REL", "SHT_RELA"):
            rels.setdefault(s.header.sh_info, set()).update(
                p for r in s.iter_relocations() for p in range(r["r_offset"], r["r_offset"] + 4))
    # os nomes que o próprio .o cita (um construtor chamado por outra função dele)
    citados = {simbolos[r["r_info_sym"]].name for s in secoes if s.header.sh_type in ("SHT_REL", "SHT_RELA")
               for r in s.iter_relocations()}
    saida = []
    for i, s in enumerate(secoes):
        if s.name not in (".text", ".data", ".rodata", ".bss", ".init", ".ctor") or not s.data_size:
            continue
        corpo = bytes(s.data_size) if s.header.sh_type == "SHT_NOBITS" else s.data()
        nomes = []
        for x in simbolos:
            if x["st_shndx"] != i or not x.name or x.name.startswith("$") or x["st_info"]["type"] == "STT_SECTION":
                continue
            tipo = "thumb" if x["st_value"] & 1 else ("arm" if x["st_info"]["type"] == "STT_FUNC" else "dado")
            if tipo == "arm" and any(m.name == "$t" and m["st_shndx"] == i for m in simbolos):
                tipo = "thumb"
            nomes.append((x.name, x["st_value"] & ~1, x["st_size"], x["st_info"]["bind"] == "STB_LOCAL", tipo))
        multidef = bool(nomes) and all(x["st_info"]["bind"] == "STB_LOPROC" for x in simbolos
                                       if x["st_shndx"] == i and x.name and not x.name.startswith("$")
                                       and x["st_info"]["type"] != "STT_SECTION")
        # o linker põe cada seção de dados num múltiplo de 4, mesmo as de alinhamento 1
        # (as strings): no jogo, toda string começa num múltiplo de 4
        alinhamento = max(s.header.sh_addralign, 1 if s.name in (".text", ".init") else 4)
        saida.append((s.name, corpo, alinhamento, rels.get(i, set()), nomes, multidef))
    return saida, citados


def preferencia(pedaco, citados):
    """Entre cópias de bytes iguais, qual o jogo guardou: a que o .o cita; senão o C1
    (construtor do objeto inteiro, o que o código chama ao criar uma variável)."""
    nomes = [n for n, *_ in pedaco[4]]
    return (not any(n in citados for n in nomes), not any("C1E" in n or "D1E" in n for n in nomes))


def bate(corpo, mascara, end):
    jogo = ARM9[end - BASE:end - BASE + len(corpo)]
    return len(jogo) == len(corpo) and all(i in mascara or a == b for i, (a, b) in enumerate(zip(corpo, jogo)))


def em_outro_lugar(alvo):
    """Os nomes que symbols.txt põe fora das faixas do arquivo."""
    fora = set()
    for l in open(os.path.join(CFG, "symbols.txt")):
        m = re.match(r"(\S+) kind:\S+ addr:0x(\w+)", l)
        if m and not any(i <= int(m.group(2), 16) < f for i, f in alvo.values()):
            fora.add(m.group(1))
    return fora


def encaixar(fonte, obj):
    """{endereço: (nome, tamanho, local, tipo)} de tudo o que o .o põe no jogo."""
    alvo = faixas(fonte)
    todos, citados = pedacos(obj)
    fora = em_outro_lugar(alvo)
    # a cópia multidef que o jogo guardou em outro arquivo não entra nesta faixa
    todos = [x for x in todos if not (x[5] and all(n in fora for n, *_ in x[4]))]
    achados = {}
    for sec, (ini, fim) in alvo.items():
        pos = ini
        lista = [x for x in todos if x[0] == sec]
        i = 0
        while i < len(lista):
            # cópias seguidas com os mesmos bytes (o C2 e o C1 de um construtor): o jogo
            # pode ter as duas, uma só ou nenhuma
            # (só no código: dois typeinfos têm os mesmos bytes no .o, todos de relocação)
            grupo = [lista[i]]
            while sec == ".text" and i + len(grupo) < len(lista) and lista[i + len(grupo)][1] == lista[i][1]:
                grupo.append(lista[i + len(grupo)])
            i += len(grupo)
            p = pos
            no_jogo = 0
            while no_jogo < len(grupo):
                a = grupo[0][2]
                q = (p + a - 1) & ~(a - 1)
                if not (sec == ".bss" or bate(grupo[0][1], grupo[0][3], q)):
                    break
                no_jogo += 1
                p = q + len(grupo[0][1])
            if no_jogo == 0:
                if sec != ".text" and not grupo[0][5]:
                    nomes = grupo[0][4]
                    sys.exit(f"{sec} de {fonte}: {nomes[0][0] if nomes else '?'} não bate em {pos:#010x}")
                continue
            ficam = sorted(grupo, key=lambda x: preferencia(x, citados))[:no_jogo]
            for _, corpo, alinhamento, _, nomes, _ in [x for x in grupo if x in ficam]:
                pos = (pos + alinhamento - 1) & ~(alinhamento - 1)
                for nome, desl, tam, local, tipo in nomes:
                    achados[pos + desl] = (nome, tam, local, tipo)
                pos += len(corpo)
        # o resto da faixa só pode ser o enchimento até o múltiplo de 4
        if (pos + 3) & ~3 < fim or pos > fim:
            sys.exit(f"{sec} de {fonte}: o .o cobre até {pos:#010x}, mas a faixa vai até {fim:#010x}")
    return alvo, achados


def main():
    args = [a for a in sys.argv[1:] if a != "--aplicar"]
    fonte = args[0]
    with tempfile.TemporaryDirectory() as tmp:
        obj = os.path.join(tmp, "x.o")
        subprocess.run([os.path.join(REPO, "decomp/tools/compilar.sh"), fonte, obj],
                       cwd=REPO, check=True, stdout=subprocess.DEVNULL)
        alvo, achados = encaixar(fonte, obj)
    for end in sorted(achados):
        nome, tam, local, tipo = achados[end]
        print(f"   {end:#010x} {nome}{' (local)' if local else ''}")
    if "--aplicar" not in sys.argv:
        return 0
    caminho = os.path.join(CFG, "symbols.txt")
    linhas = open(caminho).read().splitlines()
    dentro = lambda a: any(i <= a < f for i, f in alvo.values())
    novas, tirados = [], 0
    for l in linhas:
        m = re.search(r"addr:0x(\w+)", l)
        if m and dentro(int(m.group(1), 16)):
            tirados += 1
            continue
        novas.append(l)
    for end, (nome, tam, local, tipo) in achados.items():
        if tipo == "dado":
            kind = f"kind:data(byte[{tam:#x}])" if tam else "kind:data(any)"
        else:
            kind = f"kind:function({tipo},size={tam:#x})"
        novas.append(f"{nome} {kind} addr:{end:#010x}{' local' if local else ''}")
    novas.sort(key=lambda l: int(re.search(r"addr:0x(\w+)", l).group(1), 16))
    open(caminho, "w").write("\n".join(novas) + "\n")
    print(f"symbols.txt: {tirados} nomes trocados por {len(achados)} do .o")

    # relocações para o meio de um símbolo: "to:símbolo add:deslocamento"
    inicios = sorted(achados)
    caminho = os.path.join(CFG, "relocs.txt")
    trocadas = 0
    saida = []
    for l in open(caminho).read().splitlines():
        m = re.search(r"to:0x(\w+)(?! add)", l)
        if m and " add:" not in l:
            para = int(m.group(1), 16)
            # um ponteiro para função Thumb vem com o bit 0 ligado (0x02004f71): o dsd
            # já o entende como a função, e "+ 1" faria o linker somar 1 de novo
            thumb = para & 1 and achados.get(para & ~1, (0, 0, 0, ""))[3] == "thumb"
            if dentro(para) and para not in achados and not thumb:
                base = max(a for a in inicios if a <= para)
                l = l.replace(f"to:{m.group(0)[3:]}", f"to:{base:#010x} add:{para - base:#x}")
                trocadas += 1
        saida.append(l)
    open(caminho, "w").write("\n".join(saida) + "\n")
    print(f"relocs.txt: {trocadas} relocações viraram símbolo + deslocamento")
    return 0


if __name__ == "__main__":
    sys.exit(main())
