// Teste com dados reais da Fase 3: o C++ do sistema de arquivos que gera os bytes do
// jogo (src/Aurora/CDSFileSystem.cpp e CExoString.cpp, sem mudar nada), compilado no
// PC e rodando sobre os pacotes HERF extraídos da ROM.
//
// O que ele confere:
//  1. o erf.dict (os nomes oficiais de todos os recursos) sai pela cadeia inteira do
//     jogo: CErfResource -> CErfMan -> CErfTable -> CDSFileSystem;
//  2. para cada nome: HashResourceName dá o hash do erf.dict; CErfResource acha o
//     recurso e dá o tamanho; a leitura devolve os mesmos bytes que uma leitura
//     independente do índice (feita aqui, sem o código do jogo); um .small sai
//     descomprimido, com o tamanho do cabeçalho;
//  3. a leitura de um pedaço (CErfResource::Read com posição) e a fila de leitura em
//     segundo plano (ReadFileAsync + Update + callback);
//  4. o pacote do idioma: para cada idioma do console, LoadTables abre o test_X.herf
//     certo (inglês para japonês, inglês e qualquer outro).
//  5. as tabelas GDA (src/Aurora/G2DA.cpp): cada .gda do erf.dict é aberto pelo nome, e
//     linhas, colunas, o hash e o tipo de cada coluna, o id de cada linha e o valor de
//     cada célula (pela posição e pela chave) batem com uma leitura independente do GFF4.
//  6. as tabelas 2DA em texto (src/Aurora/C2DA.cpp): cada recurso que começa com "2DA " é
//     aberto pelo nome, e os nomes das colunas e das linhas, o texto e o inteiro de cada
//     célula batem com uma leitura independente do texto.
// Tudo isso com o alocador do jogo (src/Aurora/Memory.cpp) atendendo cada new e delete.
//
// Uso: decomp/build/test_arquivos work/extract/files   (depois do montar_rom.sh)
#include <map>
#include <string>
#include <vector>
#include "Aurora/CErfMan.h"
#include "Aurora/C2DA.h"
#include "Aurora/G2DA.h"
#include "msl.h"

extern const char* g_pastaPC;
extern int g_idiomaPC;

static int falhas = 0;

#define CONFERE(cond, ...)                                     \
    do {                                                       \
        if (!(cond)) {                                         \
            if (falhas++ < 20) {                               \
                printf("FALHA (%s:%d): ", __FILE__, __LINE__); \
                printf(__VA_ARGS__);                           \
                printf("\n");                                  \
            }                                                  \
        }                                                      \
    } while (0)

// Leitura independente de um pacote: o arquivo inteiro e o índice num map.
struct Pacote {
    std::vector<u8> dados;
    std::map<u32, std::pair<u32, u32> > indice;   // hash -> (tamanho, posição)

    bool Abre(const char* nome)
    {
        std::string caminho = std::string(g_pastaPC) + "/" + nome;
        FILE* f = fopen(caminho.c_str(), "rb");
        if (f == NULL)
            return false;
        fseek(f, 0, SEEK_END);
        dados.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        if (fread(&dados[0], 1, dados.size(), f) != dados.size())
            return false;
        fclose(f);
        u32 n = Palavra(4);
        for (u32 i = 0; i < n; i++)
            indice[Palavra(8 + 12 * i)] = std::make_pair(Palavra(12 + 12 * i), Palavra(16 + 12 * i));
        return Palavra(0) == 0x00f1a5c0;
    }
    u32 Palavra(u32 p) const { return dados[p] | dados[p + 1] << 8 | dados[p + 2] << 16 | (u32)dados[p + 3] << 24; }
};

// Leitura independente de um GFF4, sem o código do jogo: os campos de cada modelo são
// procurados um por um (o jogo faz busca binária).
struct Gff {
    const u8* d;
    u32 P(u32 pos) const { return d[pos] | d[pos + 1] << 8 | d[pos + 2] << 16 | (u32)d[pos + 3] << 24; }
    u32 Dados() const { return P(0x18); }
    u32 Tamanho(u32 modelo) const { return P(0x1c + 16 * modelo + 12); }
    bool Campo(u32 modelo, u32 rotulo, u32* tipo, u32* pos) const
    {
        u32 n = P(0x1c + 16 * modelo + 4), campos = P(0x1c + 16 * modelo + 8);
        for (u32 i = 0; i < n; i++) {
            if (P(campos + 12 * i) == rotulo) {
                *tipo = P(campos + 12 * i + 4);
                *pos = P(campos + 12 * i + 8);
                return true;
            }
        }
        return false;
    }
    // Uma lista de structs da raiz: posição do primeiro elemento, quantos, modelo e tamanho.
    u32 Lista(u32 rotulo, u32* modelo, u32* tamanho) const
    {
        u32 tipo, pos;
        if (!Campo(0, rotulo, &tipo, &pos) || P(Dados() + pos) == 0xffffffff)
            return 0;
        *modelo = tipo & 0xffff;
        *tamanho = Tamanho(*modelo);
        return Dados() + P(Dados() + pos);
    }
};

// Leitura independente de um 2DA em texto, sem o código do jogo: as linhas que têm alguma
// palavra (separadas por espaço ou tab). A primeira traz os nomes das colunas; cada uma das
// seguintes, o nome da linha e as células. "****" é uma célula vazia; uma célula que falta
// fica vazia (o valor padrão) e uma que sobra é ignorada. Os nomes ficam em minúsculas.
// Os arquivos do jogo não têm aspas nem a linha "DEFAULT", então esta leitura não as trata.
struct Texto2DA {
    std::vector<std::string> colunas, linhas;
    std::vector<std::vector<std::string> > celulas;
    u32 sobras;

    static std::string Minusculas(std::string s)
    {
        for (size_t i = 0; i < s.size(); i++)
            s[i] = tolower((unsigned char)s[i]);
        return s;
    }
    void Le(const std::string& texto)
    {
        sobras = 0;
        size_t i = 8;   // depois de "2DA V2.0"
        bool primeira = true;
        while (i < texto.size()) {
            size_t fim = texto.find_first_of("\r\n", i);
            if (fim == std::string::npos)
                fim = texto.size();
            std::vector<std::string> palavras;
            for (size_t j = i; j < fim;) {
                size_t k = texto.find_first_of(" \t", j);
                if (k == std::string::npos || k > fim)
                    k = fim;
                if (k > j)
                    palavras.push_back(texto.substr(j, k - j));
                j = k + 1;
            }
            i = fim + 1;
            if (palavras.empty())
                continue;
            if (primeira) {
                for (size_t c = 0; c < palavras.size(); c++)
                    colunas.push_back(Minusculas(palavras[c]));
                primeira = false;
                continue;
            }
            linhas.push_back(Minusculas(palavras[0]));
            std::vector<std::string> linha;
            for (size_t c = 0; c < colunas.size(); c++) {
                std::string v = c + 1 < palavras.size() ? palavras[c + 1] : "";
                linha.push_back(v == "****" ? "" : v);
            }
            if (palavras.size() > colunas.size() + 1)
                sobras += palavras.size() - colunas.size() - 1;
            celulas.push_back(linha);
        }
    }
};

// O nome do arquivo aberto com este id (o ResName do CDSFileSystem, em maiúsculas).
static std::string NomeAberto(u16 id)
{
    CDSFileSystem& fs = CDSFileSystem::Instance();
    int i = fs.FindFile(&id);
    return i < 0 ? std::string("?") : std::string(fs.m_files[i].name.m_name.CStr());
}

static bool s_chegou;
static void AoLer(u16* id, u8* data, u32 size, void* userData)
{
    std::vector<u8>* esperado = (std::vector<u8>*)userData;
    s_chegou = true;
    CONFERE(size == esperado->size() && memcmp(data, &(*esperado)[0], size) == 0,
            "a leitura em segundo plano devolveu outros bytes (id %d)", *id);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "uso: %s PASTA_COM_test.herf (work/extract/files)\n", argv[0]);
        return 2;
    }
    g_pastaPC = argv[1];

    // 0. o alocador do jogo (src/Aurora/Memory.cpp) atende todos os new deste teste. Um
    // pedido de até 16 bytes sai do pool de blocos de 16. Devolver um bloco o põe na
    // vaga seguinte da lista de livres, sem voltar o contador: ele é o próximo a sair, e
    // o bloco que ocupava a vaga (o vizinho de cima) nunca mais sai.
    char* a = new char[10];
    delete[] a;
    char* b = new char[10];
    char* c = new char[10];
    CONFERE(b == a, "o bloco devolvido devia ser o próximo a sair");
    CONFERE(c == a + 32, "o vizinho de cima do bloco devolvido devia ter se perdido");
    delete[] b;
    delete[] c;
    printf("alocador do jogo: o bloco devolvido sai de novo, o vizinho se perde\n");

    Pacote comum, ingles;
    if (!comum.Abre("test.herf") || !ingles.Abre("test_e.herf")) {
        fprintf(stderr, "não achei test.herf e test_e.herf em %s\n", g_pastaPC);
        return 2;
    }

    // 1. o erf.dict pela cadeia do jogo
    g_idiomaPC = 1;
    CErfMan::Instance().LoadTables();
    CErfResource dict(CExoString("erf.dict"), 0);
    u8* bytes = NULL;
    int tamanho = 0;
    CONFERE(dict.Read(&bytes, &tamanho), "o erf.dict não foi lido");
    if (bytes == NULL)
        return 1;
    u32 n = bytes[4] | bytes[5] << 8 | bytes[6] << 16 | (u32)bytes[7] << 24;
    printf("erf.dict: %d bytes, %u nomes\n", tamanho, n);
    CONFERE(tamanho >= 8 && 8 + 132ull * n <= (u32)tamanho, "o erf.dict não tem o tamanho que diz ter");
    if (falhas)
        return 1;

    // 2. cada recurso pelo nome
    std::vector<std::string> tabelas;
    std::vector<std::pair<std::string, std::string> > textos2da;   // nome, bytes
    u32 lidos = 0, comprimidos = 0, crus = 0, fora = 0, vazias = 0;
    for (u32 i = 0; i < n; i++) {
        const u8* e = bytes + 8 + 132 * i;
        u32 hash = e[0] | e[1] << 8 | e[2] << 16 | (u32)e[3] << 24;
        std::string nome((const char*)e + 4);
        if (hash == 0 && nome.empty()) {
            vazias++;                // o erf.dict termina com entradas zeradas
            continue;
        }
        CONFERE(HashResourceName(CExoString(nome.c_str())) == hash, "hash de %s", nome.c_str());
        if (nome.size() > 4 && strcasecmp(nome.c_str() + nome.size() - 4, ".gda") == 0)
            tabelas.push_back(nome);

        const Pacote* p = comum.indice.count(hash) ? &comum : ingles.indice.count(hash) ? &ingles : NULL;
        if (p == NULL) {
            fora++;                  // está num pacote de outro idioma
            continue;
        }
        u32 tam = p->indice.find(hash)->second.first, pos = p->indice.find(hash)->second.second;
        bool small = nome.size() > 6 && nome.compare(nome.size() - 6, 6, ".small") == 0;
        CErfResource r(CExoString(small ? nome.substr(0, nome.size() - 6).c_str() : nome.c_str()), small);
        CONFERE(r.m_hash == hash, "CErfResource(%s) calculou outro hash", nome.c_str());
        CONFERE(r.GetSize() == (int)tam, "tamanho de %s: %d, o índice diz %u", nome.c_str(), r.GetSize(), tam);

        u8* buffer = NULL;
        int obtido = 0;
        CONFERE(r.Read(&buffer, &obtido), "%s não foi lido", nome.c_str());
        if (buffer == NULL)
            continue;
        const u8* cru = &p->dados[pos];
        if (small) {
            u32 cabecalho = cru[0] | cru[1] << 8 | cru[2] << 16 | (u32)cru[3] << 24;
            CONFERE((u32)obtido == cabecalho >> 8, "%s: %d bytes descomprimidos, o cabeçalho diz %u",
                    nome.c_str(), obtido, cabecalho >> 8);
            if ((cabecalho & 0xf0) == 0) {
                CONFERE(memcmp(buffer, cru + 4, obtido) == 0, "%s (sem compressão) mudou", nome.c_str());
                crus++;
            } else {
                comprimidos++;
            }
        } else {
            CONFERE((u32)obtido == tam && memcmp(buffer, cru, tam) == 0, "%s: bytes diferentes", nome.c_str());
            if (tam >= 8 && memcmp(cru, "2DA ", 4) == 0)
                textos2da.push_back(std::make_pair(nome, std::string((const char*)cru, tam)));
        }
        delete[] buffer;
        lidos++;

        // 3a. um pedaço do meio, de um recurso normal
        if (!small && tam >= 12) {
            u8 pedaco[8];
            u8* destino = pedaco;
            CONFERE(r.Read(&destino, 4, 8), "pedaço de %s", nome.c_str());
            CONFERE(memcmp(pedaco, cru + 4, 8) == 0, "pedaço de %s: bytes diferentes", nome.c_str());
        }
    }
    printf("recursos lidos pelo nome: %u (%u .small em LZ, %u .small sem compressão); "
           "%u em pacotes de outros idiomas; %u entradas vazias no fim\n", lidos, comprimidos, crus, fora, vazias);
    CONFERE(fora == 0 && lidos + vazias == n, "%u nomes sem recurso", n - lidos - vazias);
    delete[] bytes;

    // 3b. a fila de leitura em segundo plano, num arquivo que ninguém abriu ainda (o
    // test_g.herf): ao fechar, o contador chega a 0 e o id volta a 0xffff
    CDSFileSystem& fs = CDSFileSystem::Instance();
    Pacote alemao;
    CONFERE(alemao.Abre("test_g.herf"), "test_g.herf");
    u16 id = 0xffff;
    CONFERE(fs.OpenFile("test_g.herf", &id, FALSE), "OpenFile(test_g.herf)");
    std::vector<u8> esperado(alemao.dados.begin() + 100, alemao.dados.begin() + 100 + 4000);
    u8* destino = NULL;
    s_chegou = false;
    CONFERE(fs.ReadFileAsync(&id, &destino, 100, 4000, AoLer, &esperado), "ReadFileAsync");
    for (int quadro = 0; quadro < 10 && !s_chegou; quadro++)
        fs.Update();
    CONFERE(s_chegou, "a leitura em segundo plano não terminou");
    CONFERE(fs.CloseFile(&id) == 0 && id == 0xffff, "CloseFile");
    delete[] destino;

    // 4. o pacote do idioma
    const char* esperados[] = {"TEST_E.HERF", "TEST_E.HERF", "TEST_F.HERF", "TEST_G.HERF", "TEST_I.HERF",
                               "TEST_S.HERF", "TEST_E.HERF"};
    for (int idioma = 0; idioma <= 6; idioma++) {
        g_idiomaPC = idioma;
        CErfMan::Instance().LoadTables();
        std::string aberto = NomeAberto(CErfMan::Instance().m_tables[1].m_fileId);
        CONFERE(aberto == esperados[idioma], "idioma %d abriu %s", idioma, aberto.c_str());
    }
    printf("idiomas 0 a 6: o pacote certo\n");

    // 5. as tabelas GDA
    g_idiomaPC = 1;
    CErfMan::Instance().LoadTables();
    u32 nLinhas = 0, nCelulas = 0, nTipoFalso = 0;
    for (size_t k = 0; k < tabelas.size(); k++) {
        const char* nome = tabelas[k].c_str();
        G2DA tabela((CExoString(nome)));
        CONFERE(tabela.IsLoaded(), "%s não carregou", nome);
        if (!tabela.IsLoaded())
            continue;
        Gff g = {tabela.m_data};
        u32 mc = 0, tc = 0, ml = 0, tl = 0;
        u32 colunas = g.Lista(10002, &mc, &tc), linhas = g.Lista(10003, &ml, &tl);
        u32 nc = colunas ? g.P(colunas) : 0, nl = linhas ? g.P(linhas) : 0;
        CONFERE(tabela.GetColumnCount() == (int)nc && tabela.GetRowCount() == (int)nl,
                "%s: %d x %d, o arquivo diz %u x %u", nome, tabela.GetRowCount(), tabela.GetColumnCount(), nl, nc);
        // as colunas pelo hash: a primeira com aquele hash
        std::vector<u32> hashes;
        for (u32 c = 0; c < nc; c++) {
            u32 tipo, pos;
            u32 coluna = colunas + 4 + c * tc;
            hashes.push_back(g.Campo(mc, 10001, &tipo, &pos) ? g.P(coluna + pos) : 0);
            u32 primeira = 0;
            while (hashes[primeira] != hashes[c])
                primeira++;
            CONFERE(tabela.GetColumnIndex(hashes[c]) == (int)primeira, "%s: coluna %u pelo hash", nome, c);
        }
        for (u32 r = 0; r < nl; r++) {
            u32 linha = linhas + 4 + r * tl;
            for (u32 c = 0; c < nc; c++) {
                u32 tipo, pos;
                if (!g.Campo(ml, 10005 + c, &tipo, &pos))
                    continue;
                u32 v = g.P(linha + pos);
                if (tipo == 5 || tipo == 18) {
                    // nenhuma tabela tem coluna float (tipo 8); a leitura de float só copia os 4
                    // bytes do campo, então ela é conferida aqui, nas células de 32 bits
                    float bits = 0;
                    CONFERE(tabela.GetFloat(r, c, &bits) && memcmp(&bits, &v, 4) == 0, "%s[%u][%u] como float", nome, r, c);
                }
                int tipoColuna = -1;
                BOOL temTipo = tabela.GetColumnType(r, c, &tipoColuna);
                if (tipo == 5) {
                    int lido = 0;
                    CONFERE(tabela.GetInt(r, c, &lido) && lido == (int)v, "%s[%u][%u]: %d, devia ser %d", nome, r, c, lido, (int)v);
                    CONFERE(temTipo && tipoColuna == 1, "%s: tipo da coluna %u", nome, c);
                } else if (tipo == 8) {
                    float lido = 0;
                    CONFERE(tabela.GetFloat(r, c, &lido) && memcmp(&lido, &v, 4) == 0, "%s[%u][%u] (float)", nome, r, c);
                    CONFERE(temTipo && tipoColuna == 2, "%s: tipo da coluna %u", nome, c);
                } else if (tipo == 18) {
                    CFixed lido(0);
                    CONFERE(tabela.GetFixed(r, c, &lido) && lido.m_value == (fx32)v, "%s[%u][%u] (fixo)", nome, r, c);
                    CONFERE(temTipo && tipoColuna == 2, "%s: tipo da coluna %u", nome, c);
                } else if (tipo == 0) {
                    BOOL lido = 2;
                    CONFERE(tabela.GetBool(r, c, &lido) && lido == ((v & 0xff) != 0), "%s[%u][%u] (bool)", nome, r, c);
                    CONFERE(temTipo && tipoColuna == 3, "%s: tipo da coluna %u", nome, c);
                } else if (tipo == 0x20000014) {
                    // texto ASCII por referência: o tipo, com as flags, não está no switch do
                    // jogo, e a GetColumnType devolve FALSE
                    CExoString lido;
                    const char* esperado = v == 0xffffffff ? "" : (const char*)tabela.m_data + g.Dados() + v + 4;
                    CONFERE(tabela.GetString(r, c, lido) && strcmp(lido.CStr(), esperado) == 0,
                            "%s[%u][%u]: \"%s\", devia ser \"%s\"", nome, r, c, lido.CStr(), esperado);
                    CONFERE(!temTipo, "%s: tipo da coluna %u (texto)", nome, c);
                    nTipoFalso++;
                } else {
                    CONFERE(false, "%s[%u][%u]: tipo 0x%x não esperado", nome, r, c, tipo);
                }
                nCelulas++;
            }
            // a linha pelo id (o valor da coluna 0): a primeira com aquele id; e um valor
            // pela chave (id, hash da coluna) é o mesmo que pela posição
            u32 tipo0, pos0;
            if (nc > 0 && g.Campo(ml, 10005, &tipo0, &pos0) && tipo0 == 5) {
                int id = (int)g.P(linha + pos0);
                u32 primeira = 0;
                while ((int)g.P(linhas + 4 + primeira * tl + pos0) != id)
                    primeira++;
                CONFERE(tabela.GetRowIndex(id) == (int)primeira, "%s: linha %u pelo id %d", nome, r, id);
                int a = 0, b = 0;
                if (primeira == r && hashes[0] != 0) {
                    CONFERE(tabela.GetIntByKey(id, hashes[0], &a) && tabela.GetInt(r, 0, &b) && a == b,
                            "%s: linha %u pela chave", nome, r);
                }
            }
            nLinhas++;
        }
    }
    printf("tabelas GDA: %u, %u linhas, %u células (%u de texto, cuja GetColumnType dá FALSE)\n",
           (u32)tabelas.size(), nLinhas, nCelulas, nTipoFalso);

    // 6. as tabelas 2DA em texto: C2DA::Load lê o recurso, Parse monta a tabela de
    // CExoString numa C2DA temporária, e CopyFrom passa tudo para o bloco compacto
    u32 nLinhas2da = 0, nCelulas2da = 0, nVazias = 0, nSobras = 0, nInteiros = 0;
    for (size_t k = 0; k < textos2da.size(); k++) {
        const char* nome = textos2da[k].first.c_str();
        Texto2DA esperado;
        esperado.Le(textos2da[k].second);
        CONFERE(textos2da[k].second.compare(4, 4, "V2.0") == 0, "%s não é \"2DA V2.0\"", nome);
        C2DA tabela;
        CONFERE(tabela.Load(nome), "%s não carregou", nome);
        CONFERE(tabela.m_bLoaded && tabela.m_bCompact && tabela.m_bOwnsBuffer, "%s: não ficou compacta", nome);
        int nl = (int)esperado.linhas.size(), nc = (int)esperado.colunas.size();
        CONFERE(tabela.GetNumRows() == nl && tabela.GetNumColumns() == nc,
                "%s: %d x %d, o texto tem %d x %d", nome, tabela.GetNumRows(), tabela.GetNumColumns(), nl, nc);
        if (tabela.GetNumRows() != nl || tabela.GetNumColumns() != nc)
            continue;
        CExoString lido;
        for (int c = 0; c < nc; c++) {
            tabela.GetColumnLabel(c, lido);
            CONFERE(esperado.colunas[c] == lido.CStr(), "%s: coluna %d \"%s\", devia ser \"%s\"", nome, c,
                    lido.CStr(), esperado.colunas[c].c_str());
        }
        for (int r = 0; r < nl; r++) {
            tabela.GetRowLabel(r, lido);
            CONFERE(esperado.linhas[r] == lido.CStr(), "%s: linha %d \"%s\", devia ser \"%s\"", nome, r,
                    lido.CStr(), esperado.linhas[r].c_str());
            for (int c = 0; c < nc; c++) {
                const std::string& v = esperado.celulas[r][c];
                BOOL tem = tabela.GetCExoStringEntry(r, c, lido);
                CONFERE(v == lido.CStr() && tem == !v.empty(), "%s[%d][%d]: \"%s\", devia ser \"%s\"", nome, r, c,
                        lido.CStr(), v.c_str());
                // o inteiro é o "%i" do sscanf: decimal, 0x hexadecimal ou 0 octal; um texto que
                // não é número dá 0 (com TRUE); uma célula vazia dá 0 (com FALSE)
                char* fim = NULL;
                long n = strtol(v.c_str(), &fim, 0);
                int inteiro = -12345;
                BOOL ok = tabela.GetINTEntry(r, c, &inteiro);
                CONFERE(ok == !v.empty() && inteiro == (fim == v.c_str() ? 0 : (int)n),
                        "%s[%d][%d]: inteiro %d, devia ser %ld (\"%s\")", nome, r, c, inteiro, n, v.c_str());
                if (fim != v.c_str())
                    nInteiros++;
                if (v.empty())
                    nVazias++;
                nCelulas2da++;
            }
            nLinhas2da++;
        }
        // fora da tabela: o valor padrão (vazio, depois da CopyFrom) e FALSE
        int inteiro = -1;
        CONFERE(!tabela.GetCExoStringEntry(nl, 0, lido) && lido.GetLength() == 0, "%s: linha fora da tabela", nome);
        CONFERE(!tabela.GetCExoStringEntry(0, -1, lido) && lido.GetLength() == 0, "%s: coluna fora da tabela", nome);
        CONFERE(!tabela.GetINTEntry(0, nc, &inteiro) && inteiro == 0, "%s: inteiro fora da tabela", nome);
        nSobras += esperado.sobras;
    }
    printf("tabelas 2DA: %u, %u linhas, %u células (%u números, %u vazias com \"****\"); "
           "%u células a mais no fim das linhas, ignoradas\n",
           (u32)textos2da.size(), nLinhas2da, nCelulas2da, nInteiros, nVazias, nSobras);
    CONFERE(textos2da.size() > 0, "nenhum 2DA no pacote");

    printf(falhas ? "%d FALHA(S)\n" : "OK\n", falhas);
    return falhas ? 1 : 0;
}
