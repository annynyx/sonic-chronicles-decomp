"""Confere que o jogo foi compilado com -ipa file, pela ordem dos dados.

O mwccarm põe o nome de cada classe (_ZTS, a string "4Time" que o typeinfo _ZTI
aponta) num lugar da .data que depende da opção -ipa:

  padrão (-ipa function): no fim do arquivo, depois das strings do código
  com -ipa file:          junto das variáveis e dos typeinfo, antes das strings

(-ipa file é a "análise interprocedural por arquivo": o compilador lê o arquivo
inteiro antes de gerar o código.) Duas consequências que dá para medir no jogo:

  a. no padrão, o nome vem sempre DEPOIS do typeinfo; com -ipa file, o nome de uma
     classe derivada vem antes (a ordem é: nome da derivada, typeinfo da base,
     typeinfo da derivada, nome da base);
  b. com -ipa file, nunca há strings do código entre um typeinfo e o nome dele.

O script compila um exemplo com as duas opções (a previsão) e conta (a) e (b) no
jogo. A conferência pode falhar: no padrão, (a) daria zero. Em (b), uma string de
função inline é a exceção esperada: ela vira um objeto com nome próprio (_ZZ...Es)
e, com -ipa file, vai para o bloco das variáveis, no meio dos typeinfo.

Uso: python3 decomp/tools/conferir_ipa.py   (precisa de work/extract: montar_rom.sh)
"""
import bisect, os, re, struct, subprocess, sys, tempfile
from elftools.elf.elffile import ELFFile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CFG = os.path.join(REPO, "config/YWSE/arm9")
BASE = 0x02000000
# as vtables de __vmi_class_type_info, __si_class_type_info e __class_type_info
# (+8), que a primeira palavra de todo typeinfo aponta
VTABLES_TI = (0x02109020, 0x02109030, 0x02109040)

EXEMPLO = """
extern void usa(const char*);
template<class T> class Singleton { public: Singleton() {} virtual ~Singleton() {} };
class Time : public Singleton<Time> { public: Time(); virtual ~Time(); };
Time::Time() {}
Time::~Time() {}
void f() { usa("uma string do arquivo"); }
"""


def ordem_no_o(flags):
    """A ordem dos dados (.data) num .o do exemplo compilado com estas flags."""
    with tempfile.TemporaryDirectory() as tmp:
        fonte, obj = os.path.join(tmp, "t.cpp"), os.path.join(tmp, "t.o")
        open(fonte, "w").write(EXEMPLO)
        # compilar.sh já tem -ipa file; um -ipa no fim troca. (O padrão do compilador é
        # -ipa function; -ipa off é outra coisa, que até inverte a ordem das funções.)
        subprocess.run([os.path.join(REPO, "decomp/tools/compilar.sh"), fonte, obj, *flags],
                       check=True, stdout=subprocess.DEVNULL)
        elf = ELFFile(open(obj, "rb"))
        secoes = list(elf.iter_sections())
        simbolos = [s for s in elf.get_section_by_name(".symtab").iter_symbols()
                    if s.name and s["st_info"]["type"] == "STT_OBJECT"]
        ordem = []
        for i, sec in enumerate(secoes):
            if sec.name == ".data":
                for s in sorted((s for s in simbolos if s["st_shndx"] == i), key=lambda s: s["st_value"]):
                    nome = s.name
                    if nome.startswith("_ZTS"):
                        nome = "nome"
                    elif nome.startswith("_ZTI"):
                        nome = "typeinfo"
                    elif nome.startswith("@"):
                        nome = "string"
                    else:
                        continue
                    if not ordem or ordem[-1] != nome:
                        ordem.append(nome)
        return " ".join(ordem)


def main():
    print("1. a previsão (o exemplo compilado):")
    print(f"   padrão (-ipa function): {ordem_no_o(['-ipa', 'function'])}")
    print(f"   com -ipa file:          {ordem_no_o([])}")

    arm9 = open(os.path.join(REPO, "work/extract/arm9/arm9.bin"), "rb").read()
    palavra = lambda a: struct.unpack_from("<I", arm9, a - BASE)[0]
    texto = lambda a: arm9[a - BASE:a - BASE + 64].split(b"\0")[0]
    secoes = {m.group(1): (int(m.group(2), 16), int(m.group(3), 16)) for m in re.finditer(
        r"(\.\w+)\s+start:(0x\w+) end:(0x\w+)", open(os.path.join(CFG, "delinks.txt")).read().split("\n\n")[0])}
    # a .data do jogo vem antes da .data das bibliotecas (a primeira é a da NitroSystem)
    fim_jogo = min(int(m.group(1), 16) for m in re.finditer(
        r"^(?:NitroSystem|NitroSDK|MSL)/[^\n]*:\n(?:    complete\n)?(?:    \.\w+ +start:\w+ end:\w+\n)*?"
        r"    \.data +start:(0x\w+)", open(os.path.join(CFG, "delinks.txt")).read(), re.M))
    dados = (secoes[".data"][0], fim_jogo)
    codigo = [secoes[".text"], secoes[".init"]]
    simbolos = [int(m.group(1), 16) for m in re.finditer(
        r"^\S+ kind:data\S* addr:(0x\w+)", open(os.path.join(CFG, "symbols.txt")).read(), re.M)]

    # strings lidas pelo código: alvos de relocações que saem do .text, com texto
    lidas = {}
    for m in re.finditer(r"from:(0x\w+) kind:\w+ to:(0x\w+)", open(os.path.join(CFG, "relocs.txt")).read()):
        de, para = int(m.group(1), 16), int(m.group(2), 16)
        if any(a <= de < b for a, b in codigo) and dados[0] <= para < dados[1]:
            s = texto(para)
            if len(s) >= 3 and all(32 <= c < 127 for c in s) and palavra(para) >> 24 != 0x02:
                lidas[para] = s
    strings = sorted(lidas)

    typeinfos = [a for a in simbolos if dados[0] <= a < dados[1] and palavra(a) in VTABLES_TI]
    antes = sum(1 for t in typeinfos if palavra(t + 4) < t)
    print(f"\n2a. no jogo: {antes} de {len(typeinfos)} nomes vêm antes do typeinfo "
          "(no padrão seriam 0)")
    print("2b. strings lidas pelo código entre cada typeinfo e o nome dele:")
    limpos, excecoes = 0, []
    for t in typeinfos:
        nome = palavra(t + 4)
        a, b = min(t, nome), max(t, nome)
        meio = strings[bisect.bisect_right(strings, a):bisect.bisect_left(strings, b)]
        if meio:
            excecoes.append((t, texto(nome), [lidas[s] for s in meio]))
        else:
            limpos += 1
    print(f"   {limpos} de {len(typeinfos)} typeinfo sem nenhuma string no meio (o que -ipa file prevê)")
    for t, nome, meio in excecoes:
        print(f"   exceção: {t:#010x} {nome.decode()}: {[s.decode() for s in meio]}")
    # as exceções têm de ser a mesma string, vinda de uma função inline: uma só
    # string, lida por uma função só, com outra cópia normal em outro arquivo
    textos = {s for _, _, meio in excecoes for s in meio}
    if antes == 0 or limpos < 0.9 * len(typeinfos) or len(textos) > 1:
        print("A ordem do jogo NÃO bate com -ipa file.")
        return 1
    print("A ordem do jogo bate com -ipa file." + (
        f" A exceção ({textos.pop().decode()!r}) é uma string de função inline: ela fica no bloco"
        " das variáveis." if textos else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
