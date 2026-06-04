#!/usr/bin/env bash
# tests/test.sh
# Tests pour le projet Huffman :
#   1. Compile si nécessaire
#   2. Round-trip avec tes binaires (compresse puis décompresse, doit redonner l'original)
#   3. Compatibilité avec les exécutables de référence (les deux sens), si présents
#
# Usage :
#   cd tests && bash test.sh
# ou :
#   bash chemin/vers/tests/test.sh
#
# Les binaires de référence (fournis par l'UE) sont attendus, s'ils existent,
# dans tests/reference/ . Ils ne sont pas versionnés (voir .gitignore).

set -u

# ---------- Chemins (relatifs au script) ----------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
HC="$ROOT/hcompress_test/hcompress"
UC="$ROOT/unhcompress_test/unhcompress"
REF_DIR="$SCRIPT_DIR/reference"

# Dossier temporaire pour les sorties intermédiaires
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ---------- Couleurs ----------
if [ -t 1 ]; then
  G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; N=$'\033[0m'
else
  G=''; R=''; Y=''; B=''; N=''
fi

PASSED=0; FAILED=0
ok()   { printf "  ${G}OK${N}   %s\n" "$1"; PASSED=$((PASSED+1)); }
ko()   { printf "  ${R}FAIL${N} %s\n" "$1"; FAILED=$((FAILED+1)); }
info() { printf "  ${Y}--${N}   %s\n" "$1"; }
hdr()  { printf "\n${B}== %s ==${N}\n" "$1"; }

# Calcule un ratio de compression lisible, en évitant la division par zéro.
ratio() {
  local orig="$1" comp="$2"
  if [ "$orig" -gt 0 ]; then
    awk -v o="$orig" -v c="$comp" 'BEGIN { printf "%.0f%%", 100 * c / o }'
  else
    printf -- "-"
  fi
}

# ---------- Build si besoin ----------
if [ ! -x "$HC" ] || [ ! -x "$UC" ]; then
  hdr "Compilation"
  if (cd "$ROOT/hcompress_test" && make) >/dev/null 2>&1; then
    ok "hcompress compilé"
  else
    ko "compilation de hcompress échouée"; exit 1
  fi
  if (cd "$ROOT/unhcompress_test" && make) >/dev/null 2>&1; then
    ok "unhcompress compilé"
  else
    ko "compilation de unhcompress échouée"; exit 1
  fi
fi

[ -x "$HC" ] || { echo "Introuvable : $HC"; exit 1; }
[ -x "$UC" ] || { echo "Introuvable : $UC"; exit 1; }

# ---------- Préparation des fichiers de test ----------
# Crée les cas limites s'ils n'existent pas
[ -f "$SCRIPT_DIR/empty.txt" ] || : > "$SCRIPT_DIR/empty.txt"
[ -f "$SCRIPT_DIR/one.txt" ]   || printf "A" > "$SCRIPT_DIR/one.txt"
[ -f "$SCRIPT_DIR/rep.txt" ]   || python3 -c "print('A'*10000, end='')" > "$SCRIPT_DIR/rep.txt"

# Liste des fichiers à tester (uniquement ceux présents)
FILES=()
for f in empty.txt one.txt rep.txt lesmiserables.txt; do
  [ -f "$SCRIPT_DIR/$f" ] && FILES+=("$f")
done

# ---------- TEST 1 : round-trip avec tes binaires ----------
hdr "Round-trip (ton hcompress -> ton unhcompress)"
for f in "${FILES[@]}"; do
  src="$SCRIPT_DIR/$f"
  if ! "$HC" "$src" "$TMP/c.bin" 2>/dev/null; then
    ko "$f (compression a échoué)"; continue
  fi
  if ! "$UC" "$TMP/c.bin" "$TMP/d.out" 2>/dev/null; then
    ko "$f (décompression a échoué)"; continue
  fi
  if cmp -s "$src" "$TMP/d.out"; then
    osz=$(wc -c < "$src"); csz=$(wc -c < "$TMP/c.bin")
    ok "$f  ($osz o -> $csz o, $(ratio "$osz" "$csz"))"
  else
    ko "$f (les fichiers diffèrent)"
  fi
done

# Cas bonus : binaire arbitraire (couvre tous les octets 0-255)
if [ -x "$HC" ]; then
  if "$HC" "$HC" "$TMP/c.bin" 2>/dev/null && \
     "$UC" "$TMP/c.bin" "$TMP/d.out" 2>/dev/null && \
     cmp -s "$HC" "$TMP/d.out"; then
    ok "binaire hcompress lui-même"
  else
    ko "binaire hcompress lui-même"
  fi
fi

# ---------- TEST 2 : compatibilité avec la référence (si présente) ----------
if [ -d "$REF_DIR" ]; then
  hdr "Compat : ton hcompress -> unhcompress de référence"
  shopt -s nullglob
  for ref_uc in "$REF_DIR"/unhcompress*; do
    [ -x "$ref_uc" ] || continue
    name=$(basename "$ref_uc")
    match=0; total=0; details=""
    for f in "${FILES[@]}"; do
      src="$SCRIPT_DIR/$f"; total=$((total+1))
      if "$HC" "$src" "$TMP/c.bin" 2>/dev/null && \
         "$ref_uc" "$TMP/c.bin" "$TMP/d.out" 2>/dev/null && \
         cmp -s "$src" "$TMP/d.out"; then
        match=$((match+1))
      else
        details="$details $f"
      fi
    done
    if [ "$match" -eq "$total" ]; then
      ok "$name accepte ta sortie ($match/$total)"
    else
      info "$name : $match/$total — échecs sur:$details"
    fi
  done

  hdr "Compat : hcompress de référence -> ton unhcompress"
  for ref_hc in "$REF_DIR"/hcompress*; do
    [ -x "$ref_hc" ] || continue
    name=$(basename "$ref_hc")
    match=0; total=0; details=""
    for f in "${FILES[@]}"; do
      src="$SCRIPT_DIR/$f"; total=$((total+1))
      if "$ref_hc" "$src" "$TMP/c.bin" 2>/dev/null && \
         "$UC" "$TMP/c.bin" "$TMP/d.out" 2>/dev/null && \
         cmp -s "$src" "$TMP/d.out"; then
        match=$((match+1))
      else
        details="$details $f"
      fi
    done
    if [ "$match" -eq "$total" ]; then
      ok "ton unhcompress accepte la sortie de $name ($match/$total)"
    else
      info "$name : $match/$total — échecs sur:$details"
    fi
  done
  shopt -u nullglob
else
  info "Dossier de référence absent ($REF_DIR) — tests de compatibilité ignorés."
  info "Place les binaires de l'UE dans tests/reference/ pour les activer."
fi

# ---------- Résumé ----------
hdr "Résumé"
printf "  ${G}%d réussis${N}, ${R}%d échoués${N}\n" "$PASSED" "$FAILED"
[ "$FAILED" -eq 0 ] && exit 0 || exit 1
