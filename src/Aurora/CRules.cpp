// CRules: o registro das tabelas GDA e as regras do combate. 0x0201ef68-0x02020790.
#include "Aurora/Comum.h"
#include "Aurora/CRules.h"
#include "Aurora/CErfMan.h"
#include "msl.h"

// As cores do contorno dos modelos 3D (GXRgb, 5 bits por canal), de edgecolors.gda.
extern u16 g_edgeColorTable[];

// 0x0201ef68: abre 2DA.GDA (a lista das tabelas), carrega já as que ficam na memória nos
// dois modos, depois as tabelas tipadas de n2da.gda e as cores de edgecolors.gda.
void CTwoDimArrays::Load2DArrays()
{
    G2DA list(CExoString("2da.gda"));
    if (list.IsLoaded()) {
        m_count = list.GetRowCount();
        m_exploreCache = 0;
        m_combatCache = 0;
        m_tables = new G2DA[m_count];
        m_names.SetSize(m_count);
        for (int i = 0; i < m_count; i++) {
            list.GetString(i, 1, m_names[i]);
            BOOL explore = FALSE;
            BOOL combat = FALSE;
            list.GetBool(i, 2, &explore);
            m_exploreCache |= explore << i;
            list.GetBool(i, 3, &combat);
            m_combatCache |= combat << i;
            if (explore && combat)
                Load2DArray(i);
        }
    }
    G2DA n2da(CExoString("n2da.gda"));
    if (n2da.IsLoaded()) {
        m_n2daCount = n2da.GetRowCount();
        CExoString name;
        m_n2da = new CN2DA[m_n2daCount];
        for (int i = 0; i < m_n2daCount; i++) {
            n2da.GetString(i, 1, name);
            m_n2da[i].Load(name);
        }
    }
    G2DA colors(CExoString("edgecolors.gda"));
    int count = colors.GetRowCount();
    for (int i = 0; i < count; i++) {
        int red, green, blue;
        colors.GetInt(i, 1, &red);
        colors.GetInt(i, 2, &green);
        colors.GetInt(i, 3, &blue);
        g_edgeColorTable[i] = red | green << 5 | blue << 10;
    }
}

// 0x0201f128: troca de modo (1 = exploração, 0 = combate). Descarrega as tabelas que não
// ficam na memória no modo novo e, com load, carrega as que ficam.
void CTwoDimArrays::SetMode(u32 mode, int load)
{
    u32 keep = 0xffffffff;
    if (load == 0) {
        if (mode == 1)
            keep = m_exploreCache;
        else if (mode == 0)
            keep = m_combatCache;
    } else {
        keep = m_combatCache & m_exploreCache;
    }
    if (keep != 0xffffffff) {
        for (int i = 0; i < m_count; i++) {
            if (!(1 << i & keep) && m_tables[i].IsLoaded())
                m_tables[i].Unload();
        }
    }
    if (load == 1 && mode <= 1) {
        u32 mask;
        if (mode == 1)
            mask = m_exploreCache;
        else
            mask = m_combatCache;
        for (int i = 0; i < m_count; i++) {
            if (1 << i & mask && !m_tables[i].IsLoaded())
                Load2DArray(i);
        }
    }
}

// 0x0201f1dc: a tabela de número id, carregada se ainda não estiver. O teste do limite
// deixa passar id == m_count, uma depois da última.
G2DA* CTwoDimArrays::GetCached2DA(int id)
{
    if (id < 0 || id > m_count)
        return NULL;
    if (!m_tables[id].IsLoaded())
        Load2DArray(id);
    return &m_tables[id];
}

// 0x0201f210
BOOL CTwoDimArrays::Load2DArray(int id)
{
    CErfResource resource(CExoString(m_names[id].CStr()), 0);
    int size = -1;
    u8* data = NULL;
    if (resource.Read(&data, &size))
        return m_tables[id].Load(data, size);
    return FALSE;
}

// 0x020206b4: um número de min a max (sem incluir max), mais o bônus, dentro de [min, max].
int RandomRange(int min, int max, int bonus)
{
    int value = min + rand() % (max - min);
    value += bonus;
    if (value >= max)
        value = max;
    if (value <= min)
        value = min;
    return value;
}

// 0x020206d8: a RandomRange em ponto fixo, de min a max, mais o bônus, dentro de [min, max].
CFixed RandomFixed(int min, int max, int bonus)
{
    CFixed result = CFixed::Random(CFixed(min), CFixed(max));
    result += CFixed(bonus);
    result = result < max ? result : CFixed(max);
    result = result > min ? result : CFixed(min);
    return result;
}
