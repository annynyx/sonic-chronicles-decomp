"""Mede o progresso da decompilação: quantos bytes do código saem de fonte.

A métrica do projeto: dos bytes de CÓDIGO do ARM9 (com o ITCM), quantos o build
tira de um .o compilado de fonte (o nosso C++ de src/, o fonte público do NitroSDK
e da NitroSystem, ou os .o do MSL tirados dos .a do CodeWarrior), em vez do
pedaço do jogo cortado pelo dsd.

O que conta como código: as seções "kind:code" de config/YWSE/arm9/delinks.txt
e de itcm/delinks.txt. São três:
  - a .text do ARM9 e a do ITCM: as funções e as constantes que vêm junto delas;
  - a .init do ARM9: as funções __sinit_*, que o compilador gera em cada .cpp que
    tem variáveis globais com construtor. Elas também vão ter de sair do fonte.
O DTCM só tem dados. Os dados (.rodata, .data...) aparecem à parte, só como
informação: eles saem do fonte junto com o código do mesmo arquivo.

O que conta como "de fonte": os arquivos marcados "complete" em delinks.txt. É o
mesmo critério de montar_rom.sh, que compila esses arquivos e usa o .o no lugar do
pedaço do jogo. Um arquivo só é marcado assim com a ROM idêntica.

Bibliotecas ou jogo: pelas regiões que a Fase 2 provou (BUILD.md, DIARIO 16 a 18).
O resto é o jogo, e ali ainda pode haver biblioteca que ninguém identificou.

A conferência: se o mapa do linker existe (work/build/arm9.o.xMAP, gerado por
montar_rom.sh), cada símbolo que o linker pôs dentro de um arquivo "complete" tem
de ter vindo do .o desse arquivo, e não de um pedaço cortado do jogo. Sem o mapa
(no CI, que não tem a ROM), a conta sai só da configuração, e o script avisa.

Uso:
  python3 decomp/tools/progresso.py           a tabela
  python3 decomp/tools/progresso.py --curto   só a porcentagem (para scripts)
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONFIG = os.path.join(REPO, "config/YWSE/arm9")
MAPA = os.path.join(REPO, "work/build/arm9.o.xMAP")
MODULOS = {"main": "delinks.txt", "itcm": "itcm/delinks.txt", "dtcm": "dtcm/delinks.txt"}

# Onde cada biblioteca está (módulo, início, fim). O nome é também o começo do
# caminho dos arquivos dela em delinks.txt.
BIBLIOTECAS = [
    # área segura (as chamadas da BIOS, a libsyscall que a Nintendo entrega com o
    # SDK), o crt0 e a string .version: tudo antes de main(), em 0x02000bec
    ("main", 0x02000000, 0x02000bec, "NitroSDK"),
    ("main", 0x020c8278, 0x020d4394, "NitroSystem"),
    ("main", 0x020d4394, 0x020e09d0, "NitroSDK"),
    ("main", 0x020e09d0, 0x020eccb4, "MSL"),
    # OS_IrqHandler, OSi_DoResetSystem e MIi_DmaSetParams*; o começo do ITCM
    # (0x01ff8000-0x01ffe89c, código ARM sem nome) ainda não tem dono
    ("itcm", 0x01ffe89c, 0x01ffedd0, "NitroSDK"),
]
LIBS = ("NitroSystem", "NitroSDK", "MSL")


def ler_delinks(modulo):
    """Devolve (seções do módulo, arquivos). Seção: nome -> (início, fim, tipo).
    Arquivo: {"nome", "complete", "secoes": [(seção, início, fim)]}."""
    secoes, arquivos, atual = {}, [], None
    for linha in open(os.path.join(CONFIG, MODULOS[modulo])):
        texto = linha.strip()
        if not texto:
            continue
        if not linha[0].isspace() and texto.endswith(":"):
            atual = {"nome": texto[:-1], "complete": False, "secoes": []}
            arquivos.append(atual)
            continue
        if texto == "complete":
            atual["complete"] = True
            continue
        m = re.match(r"(\S+)\s+start:0x(\w+) end:0x(\w+)(?: kind:(\w+))?", texto)
        if not m:
            raise SystemExit(f"{MODULOS[modulo]}: linha que não entendi: {texto}")
        nome, ini, fim = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
        if atual is None:
            secoes[nome] = (ini, fim, m.group(4))
        else:
            atual["secoes"].append((nome, ini, fim))
    return secoes, arquivos


def regiao(modulo, ini, fim):
    """A biblioteca dona do intervalo, ou None (jogo). Recusa um intervalo que
    atravesse a borda de uma região: seria um arquivo mal delimitado."""
    for mod, a, b, lib in BIBLIOTECAS:
        if mod == modulo and ini < b and fim > a:
            if not (a <= ini and fim <= b):
                raise SystemExit(f"{modulo} {ini:#x}-{fim:#x} atravessa a borda de {lib}")
            return lib
    return None


def objeto(nome):
    """O nome do .o que o mapa do linker mostra para um arquivo de delinks.txt.
    As partes "x.itcm.c", "x.dtcm.c" e "x.version.c" vêm todas de x.o."""
    base = os.path.basename(nome)
    for parte in (".itcm.", ".dtcm.", ".version."):
        base = base.replace(parte, ".")
    return os.path.splitext(base)[0] + ".o"


def ler_mapa():
    """Símbolos do mapa: (módulo, endereço, tamanho, .o)."""
    blocos = {"# .arm9": "main", "# .itcm": "itcm", "# .dtcm": "dtcm"}
    modulo, simbolos = None, []
    for linha in open(MAPA, encoding="latin-1"):
        if linha.startswith("#"):
            modulo = blocos.get(linha.strip(), modulo if linha.startswith("#>") else None)
            continue
        m = re.match(r"\s+([0-9A-F]{8}) ([0-9A-F]{8}) \S+\s+.*\t\((.+)\)$", linha.rstrip("\n"))
        if m and modulo:
            simbolos.append((modulo, int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    return simbolos


def conferir_mapa(completos):
    """Todo símbolo dentro de um arquivo complete veio do .o dele. Devolve o
    número de símbolos conferidos; para o script se um falhar.

    O mapa só mostra o nome do .o, sem a pasta, e o pedaço do jogo cortado pelo
    dsd tem o mesmo nome que a versão compilada teria (jogo/X.cpp vira X.o nos
    dois casos). Por isso a lista de .o que o linker recebeu (objects.txt) entra
    na conta: os nomes nela não podem se repetir, e o .o de um arquivo complete
    tem de ser o compilado, não o de work/build/delinks/."""
    simbolos = ler_mapa()
    if not simbolos:
        raise SystemExit(f"{MAPA}: nenhum símbolo lido; o formato do mapa mudou?")
    lista = [l.strip().strip('"') for l in open(os.path.join(os.path.dirname(MAPA), "objects.txt"))]
    nomes = [os.path.basename(c) for c in lista if c]
    repetidos = {n for n in nomes if nomes.count(n) > 1}
    if repetidos:
        raise SystemExit(f"objects.txt tem .o com o mesmo nome: {sorted(repetidos)[:5]}")
    caminho = {os.path.basename(c): c for c in lista if c}
    conferidos, erros = 0, []
    for modulo, arq in completos:
        esperado, achou = objeto(arq["nome"]), False
        if "/delinks/" in caminho.get(esperado, "/delinks/"):
            erros.append(f"{arq['nome']}: o linker recebeu {caminho.get(esperado, 'nada')}, "
                         "não o .o compilado")
        for _, ini, fim in arq["secoes"]:
            for mod, end, _, obj in simbolos:
                if mod == modulo and ini <= end < fim:
                    conferidos += 1
                    achou |= obj == esperado
                    if obj != esperado:
                        erros.append(f"{arq['nome']}: {end:#010x} veio de {obj}, não de {esperado}")
        if not achou:
            erros.append(f"{arq['nome']}: nenhum símbolo de {esperado} no mapa")
    if erros:
        print("O mapa do linker não confirma estes arquivos:", *list(dict.fromkeys(erros))[:20],
              sep="\n   ")
        raise SystemExit(1)
    return conferidos


def fmt(n):
    return f"{n:,}".replace(",", ".")


def pct(a, b):
    return f"{100 * a / b:.2f}".replace(".", ",") + "%" if b else "-"


def main():
    curto = "--curto" in sys.argv
    total = {"bibliotecas": 0, "jogo": 0}          # bytes de código
    fonte = {"bibliotecas": 0, "jogo": 0}          # deles, os que saem de fonte
    por_lib = {lib: [0, 0] for lib in LIBS}        # [de fonte, total]
    so_text = [0, 0]                               # a conta antiga, sem a .init
    dados = [0, 0]                                 # informativo: dados com bytes
    completos, arquivos_fonte = [], {"bibliotecas": 0, "jogo": 0}

    for modulo in MODULOS:
        secoes, arquivos = ler_delinks(modulo)
        codigo = {n for n, (_, _, tipo) in secoes.items() if tipo == "code"}
        for nome, (ini, fim, tipo) in secoes.items():
            if tipo == "bss":
                continue                              # .bss não tem bytes na ROM
            if tipo != "code":
                dados[1] += fim - ini
                continue
            # o total de cada lado: as regiões das bibliotecas, e o resto é o jogo
            dentro = 0
            for mod, a, b, lib in BIBLIOTECAS:
                if mod == modulo:
                    n = max(0, min(fim, b) - max(ini, a))
                    por_lib[lib][1] += n
                    dentro += n
            total["bibliotecas"] += dentro
            total["jogo"] += fim - ini - dentro
            if nome == ".text":
                so_text[1] += fim - ini
        for arq in arquivos:
            if not arq["complete"]:
                continue
            completos.append((modulo, arq))
            prefixo = arq["nome"].split("/")[0]
            lado = "bibliotecas" if prefixo in LIBS else "jogo"
            arquivos_fonte[lado] += 1
            for nome, ini, fim in arq["secoes"]:
                dono = regiao(modulo, ini, fim)
                if nome in codigo and dono != (prefixo if lado == "bibliotecas" else None):
                    raise SystemExit(f"{arq['nome']}: {nome} {ini:#x}-{fim:#x} está na região "
                                     f"de {dono or 'jogo'}")
                if nome not in codigo:
                    if secoes[nome][2] != "bss":
                        dados[0] += fim - ini
                    continue
                fonte[lado] += fim - ini
                if lado == "bibliotecas":
                    por_lib[prefixo][0] += fim - ini
                if nome == ".text":
                    so_text[0] += fim - ini

    soma_total = total["bibliotecas"] + total["jogo"]
    soma_fonte = fonte["bibliotecas"] + fonte["jogo"]
    if curto:
        print(pct(soma_fonte, soma_total))
        return 0

    print("Progresso da decompilação: bytes de código (ARM9 .text e .init, ITCM .text)")
    print("que o build tira de um .o compilado de fonte.\n")
    print(f"   {'':34}{'de fonte':>10}{'total':>12}{'':>9}")
    for lib in LIBS:
        f, t = por_lib[lib]
        print(f"     {lib:32}{fmt(f):>10}{fmt(t):>12}{pct(f, t):>9}")
    for lado, rotulo in (("bibliotecas", "bibliotecas"), ("jogo", "jogo")):
        print(f"   {rotulo + f' ({arquivos_fonte[lado]} arquivos)':34}"
              f"{fmt(fonte[lado]):>10}{fmt(total[lado]):>12}{pct(fonte[lado], total[lado]):>9}")
    print(f"   {'TOTAL':34}{fmt(soma_fonte):>10}{fmt(soma_total):>12}{pct(soma_fonte, soma_total):>9}\n")
    print(f"   sem a .init (a conta usada até a Fase 2.4): {pct(*so_text)} "
          f"({fmt(so_text[0])} de {fmt(so_text[1])})")
    print(f"   dados com bytes (.rodata, .data...), só informação: {pct(*dados)} "
          f"({fmt(dados[0])} de {fmt(dados[1])})")
    if os.path.exists(MAPA):
        n = conferir_mapa(completos)
        print(f"   mapa do linker: os {fmt(n)} símbolos dos {len(completos)} arquivos complete "
              "vieram dos .o compilados")
    else:
        print("   sem o mapa do linker (work/build/arm9.o.xMAP): rode montar_rom.sh para "
              "conferir os arquivos complete contra o link")
    return 0


if __name__ == "__main__":
    sys.exit(main())
