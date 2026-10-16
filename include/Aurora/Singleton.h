// Singleton<T>: o objeto único de uma classe (CDSFileSystem, CErfMan, CRules...).
//
// Instance() é inline: o jogo a tem expandida em mais de 3 mil lugares. Cada cópia
// testa o bit 0 de uma "guarda", constrói o objeto na primeira vez e registra o
// destrutor com __register_global_object, que o chama no fim do programa. O objeto
// e a guarda são "multidef" (um só no jogo inteiro); a variável de 12 bytes que o
// registro usa é local a cada arquivo (docs/DIARIO.md, seção 25).
//
// A classe não tem campos; ela aparece no typeinfo das filhas (o
// "9SingletonI13CDSFileSystemE" do jogo). O destrutor virtual vazio não gera código
// que fique no jogo (a vtable de Singleton<T> não é usada por ninguém e o linker a
// descarta), mas muda como o compilador monta o construtor da filha dentro de
// Instance(), e só com ele o código sai igual ao do jogo:
// - os endereços dos campos: sem destrutor na base, os dois primeiros campos saem
//   como constantes prontas (ldr r0, =objeto+4 e =objeto+0x10); com ele, só o primeiro;
// - a ordem das cargas: com o destrutor virtual, o endereço do objeto é carregado
//   antes da vtable no construtor de CErfMan, como no jogo.
#ifndef AURORA_SINGLETON_H
#define AURORA_SINGLETON_H

template <class T> class Singleton {
public:
    virtual ~Singleton() {}

    static T& Instance()
    {
        static T s_instance;
        return s_instance;
    }
};

#endif
