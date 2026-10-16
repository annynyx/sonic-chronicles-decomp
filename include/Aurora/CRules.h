// CRules: as regras do jogo, um singleton (o nome e a base Singleton<CRules> vêm do RTTI).
// Ela guarda o registro de todas as tabelas GDA do jogo e carrega as regras do combate
// (combatrules.gda) em variáveis globais.
//
// O registro é o membro em +4. A classe dele não tem virtuais, então não tem RTTI e o
// jogo não guarda o nome: CTwoDimArrays é o nome da classe que faz o mesmo papel no
// Neverwinter Nights, do mesmo motor. Os nomes dos campos vêm das colunas de 2DA.GDA
// (ID, Table, ExploreCache, CombatCache); os dos métodos são nossos.
#ifndef AURORA_CRULES_H
#define AURORA_CRULES_H

#include "Aurora/Singleton.h"
#include "Aurora/G2DA.h"
#include "Aurora/CExoArrayList.h"

// Uma tabela de valores tipados ("I" inteiro, "F" fracionário), aberta pelo nome. A
// n2da.gda lista as tabelas deste tipo (no jogo, só Levels.GDA). O código dela está no
// arquivo do StatRedirect (0x02008580...); o nome é nosso, tirado da n2da.gda.
class CN2DA {
public:
    CN2DA();
    ~CN2DA();
    void Clear();
    BOOL Load(const CExoString& name);

    int m_rowCount;      // +0x00
    int m_columnCount;   // +0x04
    u8* m_types;         // +0x08
    void* m_names;       // +0x0c
    int* m_values;       // +0x10
};

// O registro das tabelas GDA (2DA.GDA). Cada tabela pode ficar na memória durante a
// exploração, durante o combate, nos dois (carregada já no começo) ou em nenhum (carregada
// quando alguém pede).
class CTwoDimArrays {
public:
    CTwoDimArrays()
    {
        m_count = 0;
        m_exploreCache = 0;
        m_combatCache = 0;
    }
    ~CTwoDimArrays() { delete[] m_tables; }
    void Load2DArrays();
    void SetMode(u32 mode, int load);
    G2DA* GetCached2DA(int id);
    BOOL Load2DArray(int id);

    G2DA* m_tables;                      // +0x00: uma G2DA por linha de 2DA.GDA
    int m_count;                         // +0x04
    CExoArrayList<CExoString> m_names;   // +0x08: os nomes dos arquivos
    u32 m_exploreCache;                  // +0x14: um bit por tabela (coluna ExploreCache)
    u32 m_combatCache;                   // +0x18: um bit por tabela (coluna CombatCache)
    CN2DA* m_n2da;                       // +0x1c
    int m_n2daCount;                     // +0x20
};

class CRules : public Singleton<CRules> {
public:
    virtual void Update() {}
    void LoadRules();

    CTwoDimArrays m_2DArrays;   // +0x04
};

#endif
