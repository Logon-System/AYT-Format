#!/usr/bin/env bash
#
# compress-sweep.sh — Explore le meilleur compromis AYT / taille COMPRESSÉE.
#
# Idée : la taille brute de l'.ayt n'est pas toujours corrélée à sa taille une
# fois passée dans un cruncher Z80 (ZX0, Shrinkler, ...). Un heap moins « serré »
# offre parfois plus de redondance longue-portée exploitable par le compresseur.
# Ce script balaye (taille de pattern) x (étape d'optimisation) x (masking) x
# (normalize), compresse chaque candidat avec l'outil fourni, et classe les
# combinaisons par taille compressée. Accepte un OU plusieurs fichiers .ym.
#
# On ne modifie pas le convertisseur : on ne fait que l'invoquer différemment.
#
# Usage :
#   ./compress-sweep.sh [options] <fichier.ym> [autre.ym ...]
#   ./compress-sweep.sh [options] yms/*.ym
#
# Choix du compresseur (l'un OU l'autre) :
#   --preset zx0|shrinkler        Invocations prêtes pour les binaires de ./compressors
#   --compressor 'CMD {in} {out}' Motif libre ; {in}=.ayt candidat, {out}=fichier compressé
#
# Axes de balayage :
#   --sizes MIN:MAX:STEP    Tailles de pattern        (défaut 4:64:4)
#   --stages "LISTE"        Étapes d'optim            (défaut "none greedy ga sa ils tabu")
#   --masking off|on|both   Déduplication par masques (défaut on)   [-Om]
#   --normalize off|on|both Bits ignorés mis à 0      (défaut off)  [-Oz]
#
# Divers :
#   --target ARCH   Cible passée à -t             (défaut CPC)
#   --seed N        Graine RNG (reproductible)    (défaut 1)
#   --out-csv FILE  CSV de sortie                 (défaut compress-sweep.csv)
#   --top N         Lignes du classement global   (défaut 15)
#   --bin PATH      Binaire YmToAyt               (défaut ./YmToAyt)
#   --keep          Conserve le meilleur .ayt par fichier (<nom>.best.ayt + .cmp)
#   -v, --verbose   Affiche chaque combinaison testée (sinon récap par fichier)
#
set -u

# ------------------------------- valeurs par défaut -------------------------
BIN="./YmToAyt"
SIZES="4:64:4"
STAGES="none greedy ga sa ils tabu"
MASKING="on"
NORMALIZE="off"
TARGET="CPC"
SEED="1"
OUT_CSV="compress-sweep.csv"
TOP="15"
KEEP="0"
VERBOSE="0"
COMPRESSOR=""
PRESET=""
FILES=()

die() { echo "Erreur: $*" >&2; exit 1; }

# ------------------------------- parsing des args ---------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --preset)      PRESET="$2"; shift 2 ;;
        --compressor)  COMPRESSOR="$2"; shift 2 ;;
        --sizes)       SIZES="$2"; shift 2 ;;
        --stages)      STAGES="$2"; shift 2 ;;
        --masking)     MASKING="$2"; shift 2 ;;
        --normalize)   NORMALIZE="$2"; shift 2 ;;
        --target)      TARGET="$2"; shift 2 ;;
        --seed)        SEED="$2"; shift 2 ;;
        --out-csv)     OUT_CSV="$2"; shift 2 ;;
        --top)         TOP="$2"; shift 2 ;;
        --bin)         BIN="$2"; shift 2 ;;
        --keep)        KEEP="1"; shift ;;
        -v|--verbose)  VERBOSE="1"; shift ;;
        -h|--help)     sed -n '2,55p' "$0"; exit 0 ;;
        -*)            die "option inconnue: $1" ;;
        *)             FILES+=("$1"); shift ;;
    esac
done

[ "${#FILES[@]}" -gt 0 ] || die "aucun fichier .ym fourni (voir --help)"
[ -x "$BIN" ] || die "binaire YmToAyt introuvable/exécutable: $BIN"

# Résolution du compresseur : preset OU motif libre
if [ -n "$PRESET" ]; then
    case "$PRESET" in
        zx0)       COMPRESSOR="./compressors/zx0.exe -f {in} {out}" ;;
        shrinkler) COMPRESSOR="./compressors/shrinkler.exe -d -b -9 -p {in} {out}" ;;
        *)         die "preset inconnu: $PRESET (zx0|shrinkler)" ;;
    esac
fi
[ -n "$COMPRESSOR" ] || die "spécifier --preset ou --compressor"
case "$COMPRESSOR" in
    *"{in}"*) : ;;
    *) die "le motif --compressor doit contenir {in} (et {out})" ;;
esac

# Expansion des axes on/off/both -> liste de 0/1
expand_flag() {
    case "$1" in
        off)  echo "0" ;;
        on)   echo "1" ;;
        both) echo "0 1" ;;
        *)    die "valeur attendue off|on|both, reçu: $1" ;;
    esac
}
MASK_VALUES=$(expand_flag "$MASKING")
NORM_VALUES=$(expand_flag "$NORMALIZE")

# Expansion MIN:MAX:STEP -> liste de tailles
IFS=: read -r SMIN SMAX SSTEP <<<"$SIZES"
[ -n "${SMIN:-}" ] && [ -n "${SMAX:-}" ] && [ -n "${SSTEP:-}" ] || die "--sizes attend MIN:MAX:STEP"
SIZE_LIST=""
s="$SMIN"
while [ "$s" -le "$SMAX" ]; do SIZE_LIST="$SIZE_LIST $s"; s=$((s + SSTEP)); done

# Nb de combinaisons par fichier
COMBOS_PER_FILE=0
for m in $MASK_VALUES; do for n in $NORM_VALUES; do for st in $STAGES; do for sz in $SIZE_LIST; do
    COMBOS_PER_FILE=$((COMBOS_PER_FILE + 1))
done; done; done; done

# ------------------------------- dossier de travail -------------------------
WORK="$(mktemp -d "${TMPDIR:-/tmp}/aytsweep.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
CAND="$WORK/cand.ayt"
COMP="$WORK/cand.cmp"

echo "file,size,stage,masking,normalize,ayt_bytes,compressed_bytes,ratio_pct" > "$OUT_CSV"

label() { [ "$1" = "1" ] && echo "on" || echo "off"; }

echo "== Balayage compressé (multi-fichiers) =="
echo "Fichiers    : ${#FILES[@]}"
echo "Compresseur : $COMPRESSOR"
echo "Tailles     : $SIZE_LIST"
echo "Étapes      : $STAGES"
echo "Masking     : $MASKING   Normalize: $NORMALIZE   Seed: $SEED   Cible: $TARGET"
echo "Combinaisons: $COMBOS_PER_FILE / fichier"
echo

# ------------------------------- cœur : balayage d'un fichier ---------------
# Ajoute les lignes valides au CSV. Renvoie via stdout la meilleure ligne
# compressée (ou vide si aucune).
sweep_file() {
    local YM="$1" base ok=0 done_n=0
    base="$(basename "$YM")"
    [ -f "$YM" ] || { echo "  !! introuvable, ignoré: $YM" >&2; return 1; }

    for m in $MASK_VALUES; do
     for n in $NORM_VALUES; do
      for st in $STAGES; do
       for sz in $SIZE_LIST; do
        done_n=$((done_n + 1))
        local mflag="" nflag=""
        [ "$m" = "1" ] && mflag="-Om"
        [ "$n" = "1" ] && nflag="-Oz"

        rm -f "$CAND" "$COMP"
        if ! "$BIN" "$YM" -t "$TARGET" -q --seed "$SEED" -p "$sz" -O "$st" \
                    $mflag $nflag -o "$CAND" >/dev/null 2>&1 || [ ! -s "$CAND" ]; then
            [ "$VERBOSE" = "1" ] && printf "  %s [%d/%d] size=%-3s stage=%-6s -> SKIP (pas d'.ayt)\n" \
                "$base" "$done_n" "$COMBOS_PER_FILE" "$sz" "$st"
            continue
        fi
        local ayt_bytes; ayt_bytes=$(stat -c%s "$CAND")

        local cmd="${COMPRESSOR//\{in\}/$CAND}"
        cmd="${cmd//\{out\}/$COMP}"
        if ! eval "$cmd" >/dev/null 2>&1 || [ ! -s "$COMP" ]; then
            [ "$VERBOSE" = "1" ] && printf "  %s [%d/%d] size=%-3s stage=%-6s -> SKIP (compression KO)\n" \
                "$base" "$done_n" "$COMBOS_PER_FILE" "$sz" "$st"
            continue
        fi
        local comp_bytes; comp_bytes=$(stat -c%s "$COMP")
        local ratio; ratio=$(awk "BEGIN{printf \"%.1f\", 100*$comp_bytes/$ayt_bytes}")

        echo "$base,$sz,$st,$(label $m),$(label $n),$ayt_bytes,$comp_bytes,$ratio" >> "$OUT_CSV"
        ok=1
        [ "$VERBOSE" = "1" ] && printf "  %s [%d/%d] size=%-3s stage=%-6s mask=%-3s norm=%-3s  ayt=%-6s comp=%-6s (%.1f%%)\n" \
            "$base" "$done_n" "$COMBOS_PER_FILE" "$sz" "$st" "$(label $m)" "$(label $n)" \
            "$ayt_bytes" "$comp_bytes" "$ratio"

        if [ "$KEEP" = "1" ]; then
            local bf="$WORK/best_$base"
            if [ ! -f "$bf" ] || [ "$comp_bytes" -lt "$(cat "$bf")" ]; then
                echo "$comp_bytes" > "$bf"
                cp "$CAND" "./$base.best.ayt"; cp "$COMP" "./$base.best.ayt.cmp"
            fi
        fi
       done
      done
     done
    done
    return $([ "$ok" = "1" ] && echo 0 || echo 1)
}

# ------------------------------- boucle multi-fichiers ----------------------
idx=0
for YM in "${FILES[@]}"; do
    idx=$((idx + 1))
    printf "[%d/%d] %s ...\n" "$idx" "${#FILES[@]}" "$(basename "$YM")"
    sweep_file "$YM" || true
    # Meilleure ligne compressée pour ce fichier (affichage immédiat)
    base="$(basename "$YM")"
    bl=$(awk -F, -v f="$base" 'NR>1 && $1==f {if(best=="" || $7<bc){bc=$7; best=$0}} END{print best}' "$OUT_CSV")
    [ -n "$bl" ] && echo "        → meilleur compressé : $bl" || echo "        → aucun candidat valide"
done

rows=$(( $(wc -l < "$OUT_CSV") - 1 ))
[ "$rows" -gt 0 ] || die "aucun candidat valide produit"

# ------------------------------- récap par fichier --------------------------
echo
echo "== Récap par fichier : compressé-optimal vs naïf (plus petit .ayt brut) =="
printf "%-28s %-5s %-6s %-4s %-4s %-8s | %-9s %-8s\n" \
    fichier b.size b.stage mask norm "comp*" "comp@raw" "gain%"
awk -F, '
NR>1 {
    file=$1; ayt=$6+0; comp=$7+0;
    if(!(file in mc) || comp<mc[file]) {mc[file]=comp; bc[file]=$0}
    if(!(file in ma) || ayt<ma[file])  {ma[file]=ayt; craw[file]=comp}
}
END {
    n=0; sc=0; sr=0;
    for(f in mc){ order[n++]=f }
    # tri alphabétique simple
    for(i=0;i<n;i++) for(j=i+1;j<n;j++) if(order[j]<order[i]){t=order[i];order[i]=order[j];order[j]=t}
    for(i=0;i<n;i++){
        f=order[i]; split(bc[f],a,",");
        gain = (craw[f]>0)? 100.0*(craw[f]-mc[f])/craw[f] : 0;
        printf "%-28s %-5s %-6s %-4s %-4s %-8d | %-9d %+7.1f\n", f, a[2],a[3],a[4],a[5], mc[f], craw[f], gain;
        sc+=mc[f]; sr+=craw[f];
    }
    printf "%-28s %-5s %-6s %-4s %-4s %-8d | %-9d %+7.1f\n", "TOTAL","","","","", sc, sr, (sr>0?100.0*(sr-sc)/sr:0);
}' "$OUT_CSV"

echo
echo "  comp*    = plus petit compressé trouvé (params optimaux par fichier)"
echo "  comp@raw = compressé obtenu en minimisant NAÏVEMENT la taille .ayt brute"
echo "  gain%    = économie de la sélection compressé-consciente vs naïve"

# ------------------------------- classement global -------------------------
echo
echo "== Top $TOP global par taille COMPRESSÉE (croissant) =="
printf "%-28s %-5s %-6s %-4s %-4s %-8s %-9s %-6s\n" file size stage mask norm ayt comp ratio%
tail -n +2 "$OUT_CSV" | sort -t, -k7,7n | head -n "$TOP" | \
    awk -F, '{printf "%-28s %-5s %-6s %-4s %-4s %-8s %-9s %-6s\n",$1,$2,$3,$4,$5,$6,$7,$8}'

echo
echo "CSV complet : $OUT_CSV"
[ "$KEEP" = "1" ] && echo "Meilleurs candidats conservés : <nom>.best.ayt (+ .cmp)"
exit 0
