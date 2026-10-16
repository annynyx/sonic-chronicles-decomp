CC ?= cc
CXX ?= c++
CFLAGS = -std=c99 -Wall -Wextra -O2 -Iinclude
# o C++ do jogo (src/Aurora), compilado no PC para o teste com dados reais. -w e
# -fpermissive porque ele é escrito para o compilador do DS, de 32 bits: as conversões de
# ponteiro para u32 (só no NitroStartUp, que no PC não roda) são as do jogo
CXXFLAGS = -std=gnu++11 -O1 -w -fpermissive -fno-strict-aliasing -I../include
AURORA = ../src/Aurora/Memory.cpp ../src/Aurora/CExoString.cpp ../src/Aurora/CDSFileSystem.cpp ../src/Aurora/G2DA.cpp ../src/Aurora/C2DA.cpp
# make test HERF_DIR=work/herf            (saída do run_all.sh)
MANIFEST ?= $(HERF_DIR)/_manifest.json

test: build/test_hash build/test_language
	./build/test_language
	python3 -I ../analise/tools/manifest_pairs.py $(MANIFEST) | ./build/test_hash

build/test_hash: src/resource_hash.c src/exo_string.c tests/test_hash.c | build
	$(CC) $(CFLAGS) $^ -o $@
build/test_language: src/language.c tests/test_language.c | build
	$(CC) $(CFLAGS) $^ -o $@
# make arquivos PASTA=../work/extract/files   (depois do montar_rom.sh; veja tests/test_arquivos.cpp)
PASTA ?= ../work/extract/files
arquivos: build/test_arquivos
	./build/test_arquivos $(PASTA)
build/test_arquivos: $(AURORA) tests/nitro_pc.cpp tests/test_arquivos.cpp ../include/*.h ../include/Aurora/*.h | build
	$(CXX) $(CXXFLAGS) $(AURORA) tests/nitro_pc.cpp tests/test_arquivos.cpp -o $@
build:
	mkdir -p build
clean:
	rm -rf build
.PHONY: test clean arquivos
