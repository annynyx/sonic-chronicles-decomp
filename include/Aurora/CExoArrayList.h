// CExoArrayList<T>: a lista dinâmica do motor (um vetor que dobra de tamanho).
//
// Layout (12 bytes), deduzido das cópias que o jogo tem para vários T
// (0x020030b0, 0x02002f84, 0x02002e5c...):
//   +0x0  num         quantos elementos estão em uso
//   +0x4  array_size  quantos cabem no bloco alocado
//   +0x8  element     o bloco (new T[array_size])
// Os nomes dos campos vêm da CExoArrayList do Neverwinter Nights (a ordem lá é
// outra); os dos métodos são nossos.
//
// Os métodos ficam fora da classe, sem "inline": o compilador não os expande, e cada
// arquivo que usa um deles ganha uma cópia "multidef" (o linker fica com a primeira).
// É por isso que o jogo tem as cópias espalhadas no fim de vários arquivos.
//
// A ordem das declarações abaixo aparece no jogo: no fim de cada arquivo, as cópias saem
// na ordem em que os métodos são declarados (os que só outro método do template pede
// vêm numa segunda passada). No arquivo da CTlkTable, Allocate vem antes de Add.
#ifndef AURORA_CEXOARRAYLIST_H
#define AURORA_CEXOARRAYLIST_H

#ifndef NULL
#define NULL 0
#endif

template <class T> class CExoArrayList {
public:
    CExoArrayList(int size = 0);
    ~CExoArrayList();
    T& operator[](int index);
    void Allocate(int size);
    void Add(const T& element);
    void AddEmpty();
    void DelIndex(int index);
    void SetSize(int size);
    void Clear();

    int num;          // +0x0
    int array_size;   // +0x4
    T* element;       // +0x8
};

template <class T> CExoArrayList<T>::CExoArrayList(int size)
{
    num = 0;
    array_size = 0;
    element = NULL;
    if (size != 0)
        Allocate(size);
}

template <class T> CExoArrayList<T>::~CExoArrayList()
{
    if (element != NULL)
        delete[] element;
}

template <class T> T& CExoArrayList<T>::operator[](int index)
{
    return element[index];
}

template <class T> void CExoArrayList<T>::Add(const T& t)
{
    if (num == array_size)
        Allocate(array_size != 0 ? array_size * 2 : 4);
    element[num++] = t;
}

// Acrescenta um elemento sem valor (o chamador o preenche com operator[]).
template <class T> void CExoArrayList<T>::AddEmpty()
{
    if (num == array_size)
        Allocate(array_size > 0 ? array_size * 2 : 4);
    num++;
}

template <class T> void CExoArrayList<T>::DelIndex(int index)
{
    num--;
    for (; index < num; index++)
        element[index] = element[index + 1];
}

// Troca o bloco por um de "size" elementos, copiando os que couberem.
template <class T> void CExoArrayList<T>::Allocate(int size)
{
    T* old = element;
    array_size = size;
    element = new T[array_size];
    int count = array_size < num ? array_size : num;
    for (int i = 0; i < count; i++)
        element[i] = old[i];
    if (old != NULL)
        delete[] old;
}

template <class T> void CExoArrayList<T>::SetSize(int size)
{
    if (size == 0) {
        if (element != NULL) {
            delete[] element;
            element = NULL;
            array_size = 0;
        }
    } else {
        Allocate(size);
    }
    num = size;
}

template <class T> void CExoArrayList<T>::Clear()
{
    SetSize(0);
}

#endif
