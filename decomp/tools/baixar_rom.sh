#!/usr/bin/env bash
# Baixa a ROM base para work/rom/ywse.nds (fora do Git) e confere o SHA-1.
#
# A ROM nunca vai para o repositório. Na nuvem, quem tem a cópia põe um link
# privado de download na variável de ambiente SONIC_ROM_URL (e, se o link pedir,
# um token em SONIC_ROM_TOKEN, mandado como "Authorization: Bearer").
#
# Link do Google Drive: o link de compartilhamento (".../file/d/<id>/view") abre
# uma página, não o arquivo. O script tira o <id> do link e pede o arquivo ao
# endereço de download do Drive. Para arquivos grandes, o Drive ainda responde
# com uma página de aviso ("não foi possível verificar se há vírus") que tem um
# formulário; o script lê os campos dele e pede de novo, como o botão "Fazer
# download mesmo assim" faria. O arquivo precisa estar compartilhado como
# "Qualquer pessoa com o link": aqui não há login na conta Google.
#
# O link e o token nunca são impressos: vão para o curl pela entrada padrão
# (-K -), e não pela linha de comando, onde outros processos os veriam.
#
# Uso: decomp/tools/baixar_rom.sh        (já baixada e certa: não baixa de novo)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="$REPO/work/rom/ywse.nds"
SHA1=f4ff8291b38bf9a33ab280fb9f7625cf4d549473

confere() { echo "$SHA1  $1" | sha1sum -c --status 2>/dev/null; }

if [ -f "$ROM" ] && confere "$ROM"; then
    echo "ROM já está em work/rom/ywse.nds, SHA-1 confere"
    exit 0
fi
[ -n "${SONIC_ROM_URL:-}" ] || {
    echo "SONIC_ROM_URL não está definida nesta sessão."
    echo "Defina-a nas variáveis do ambiente da nuvem; ela só vale para sessões novas."
    exit 1
}
mkdir -p "$(dirname "$ROM")"
TMP="$ROM.tmp"
trap 'rm -f "$TMP" "$TMP.cfg"' EXIT

# Converte link do Drive no endereço de download. Escreve num arquivo, não na
# tela. Um link que não é do Drive passa como está.
python3 - "$TMP.cfg" <<'EOF'
import os, re, sys
url = os.environ["SONIC_ROM_URL"].strip()
m = re.search(r"drive\.google\.com/(?:file/d/|open\?id=|uc\?(?:.*&)?id=)([\w-]+)", url)
if m:
    url = ("https://drive.usercontent.google.com/download?id=" + m.group(1)
           + "&export=download&confirm=t")
with open(sys.argv[1], "w") as f:
    f.write('url = "%s"\n' % url.replace('"', '%22'))
    if os.environ.get("SONIC_ROM_TOKEN"):
        f.write('header = "Authorization: Bearer %s"\n' % os.environ["SONIC_ROM_TOKEN"])
EOF
HOST=$(sed -n 's#^url = "https\?://\([^/]*\).*#\1#p' "$TMP.cfg")

baixar() {
    # %{http_connect} é a resposta do proxy da nuvem ao pedido de conexão: 403 ali
    # quer dizer que a regra de rede do ambiente barrou o domínio, antes de
    # chegar ao servidor. Com -f, o curl dá o mesmo código (22) para os dois casos.
    local codigo=0 resp
    resp=$(curl -fL --retry 3 -sS -o "$TMP" -w '%{http_connect}' -K - < "$TMP.cfg") || codigo=$?
    [ "$codigo" -eq 0 ] && return
    if [ "$resp" = 403 ]; then
        echo "A rede do ambiente bloqueou $HOST (o proxy respondeu 403)."
        echo "Libere o domínio em Network access > Allowed domains; vale para sessões novas."
    elif [ "$codigo" -eq 22 ]; then
        echo "$HOST recusou o pedido (erro HTTP). No Drive, o arquivo precisa estar"
        echo "compartilhado como 'Qualquer pessoa com o link'."
    else
        echo "O download falhou (curl, código $codigo)."
    fi
    exit 1
}

echo "== baixando a ROM de $HOST"
baixar
# Página de aviso do Drive: refaz o pedido com os campos do formulário dela.
if head -c 512 "$TMP" | grep -qi '<html\|<!doctype'; then
    python3 - "$TMP" "$TMP.cfg" <<'EOF' || { echo "O servidor devolveu uma página, não a ROM. O arquivo está compartilhado como 'Qualquer pessoa com o link'?"; exit 1; }
import html, re, sys, urllib.parse
pagina = open(sys.argv[1], encoding="utf-8", errors="replace").read()
form = re.search(r'<form[^>]+action="([^"]+)"(.*?)</form>', pagina, re.S)
if not form:
    sys.exit(1)
campos = dict(re.findall(r'<input[^>]+type="hidden"[^>]+name="([^"]+)"[^>]+value="([^"]*)"', form.group(2)))
if "id" not in campos:
    sys.exit(1)
url = html.unescape(form.group(1)) + "?" + urllib.parse.urlencode(campos)
linhas = [l for l in open(sys.argv[2]) if not l.startswith("url = ")]
with open(sys.argv[2], "w") as f:
    f.write('url = "%s"\n' % url)
    f.writelines(linhas)
EOF
    echo "   o Drive pediu confirmação (arquivo grande); baixando de novo"
    baixar
fi

if ! confere "$TMP"; then
    echo "SHA-1 não confere: $(sha1sum "$TMP" | cut -c1-40) ($(stat -c %s "$TMP") bytes),"
    echo "esperado $SHA1 (Sonic Chronicles, EUA, YWSE). Nada foi gravado."
    exit 1
fi
mv "$TMP" "$ROM"
echo "ROM em work/rom/ywse.nds, SHA-1 confere ($SHA1)"
