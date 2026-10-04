#!/usr/bin/env bash
# =============================================================================
# bench_run.sh - esegue capture_recapture.out sui dataset bftestN in modo
#                CUMULATIVO per giorno (giorno 1, giorni 1-2, ... 1-10) e salva
#                un log per ogni combinazione (test, giorno).
#
# Uso:
#   ./bench_run.sh                 # tutti i test 1..36, giorni 1..10
#   ./bench_run.sh 4 5 6           # solo bftest4, bftest5, bftest6
#   ./bench_run.sh $(seq 10 36)    # solo i dataset nuovi
#   MAX_GIORNI=5 ./bench_run.sh 1  # solo i primi 5 giorni di bftest1
#
# Variabili d'ambiente:
#   JOBS=8         esecuzioni in parallelo        (default: numero di core)
#   FORCE=1        rifa' anche i log gia' presenti (default: li salta)
#   SUDO=          disattiva sudo                 (default: "sudo")
#   BIN, OUTDIR, MAX_GIORNI  come prima
#
# Output: logs/bftestN_giorno<K>.log
# =============================================================================
set -uo pipefail

BIN="${BIN:-./capture_recapture.out}"
OUTDIR="${OUTDIR:-logs}"
MAX_GIORNI="${MAX_GIORNI:-10}"
FORCE="${FORCE:-0}"
SUDO="${SUDO-sudo}"

# Default: un job per core. Ogni esecuzione e' single-thread e CPU-bound, quindi
# la scalabilita' e' quasi lineare finche' i pcap stanno nella cache del disco.
if [ -z "${JOBS:-}" ]; then
    JOBS="$(nproc 2>/dev/null || echo 4)"
fi

# ATTENZIONE: nel dataset il file del pomeriggio si chiama "pomeriggia" (typo
# presente anche nel makefile). Lo script prova entrambe le forme.
MOMENTI=("mattina" "pomeriggia" "pomeriggio" "notte")

TESTS=("$@")
if [ ${#TESTS[@]} -eq 0 ]; then
    TESTS=($(seq 1 36))
fi

if [ ! -x "$BIN" ]; then
    echo "ERRORE: binario '$BIN' non trovato. Esegui prima 'make'." >&2
    exit 1
fi

mkdir -p "$OUTDIR"

# Una sola autenticazione all'inizio: senza questo ogni job in parallelo
# potrebbe chiedere la password per conto suo, mescolando i prompt.
if [ -n "$SUDO" ]; then
    if ! $SUDO -v; then
        echo "ERRORE: autenticazione sudo fallita." >&2
        exit 1
    fi
fi

# --- costruzione della lista di lavori ---------------------------------------
# Si prepara tutto prima di eseguire, cosi' il totale e' noto in anticipo e i
# job possono partire senza attese fra un test e l'altro.
# Inizializzati esplicitamente a vuoto: con "set -u" un array solo dichiarato
# fa fallire ${#array[@]} sulle versioni di bash precedenti alla 4.4.
JOB_TEST=()
JOB_GIORNO=()
JOB_LOG=()
JOB_FILES=()
saltati=0

for n in "${TESTS[@]}"; do
    dir="bftest${n}"
    if [ ! -d "$dir" ]; then
        echo "[skip] directory '$dir' inesistente" >&2
        continue
    fi

    for g in $(seq 1 "$MAX_GIORNI"); do
        files=()
        for d in $(seq 1 "$g"); do
            for m in "${MOMENTI[@]}"; do
                f="${dir}/giorno${d}_${m}.pcap"
                if [ -f "$f" ]; then
                    files+=("$f")
                fi
            done
        done

        if [ ${#files[@]} -eq 0 ]; then
            echo "[skip] $dir giorno $g: nessun pcap trovato" >&2
            continue
        fi

        log="${OUTDIR}/${dir}_giorno${g}.log"
        if [ "$FORCE" != "1" ] && [ -s "$log" ]; then
            saltati=$((saltati + 1))
            continue
        fi

        JOB_TEST+=("$dir")
        JOB_GIORNO+=("$g")
        JOB_LOG+=("$log")
        JOB_FILES+=("${files[*]}")
    done
done

TOT=${#JOB_TEST[@]}
if [ "$saltati" -gt 0 ]; then
    echo "($saltati log gia' presenti, saltati: usa FORCE=1 per rifarli)"
fi
if [ "$TOT" -eq 0 ]; then
    echo "Niente da fare."
    exit 0
fi

echo "Esecuzioni da fare: $TOT, in parallelo su $JOBS job."
inizio=$(date +%s)

# --- esecuzione di un singolo lavoro -----------------------------------------
esegui() {
    local dir="$1" g="$2" log="$3"
    shift 3
    local files=("$@")

    # Intestazione machine-readable letta poi dal parser Python
    {
        echo "#TEST=${dir}"
        echo "#GIORNO=${g}"
        echo "#NFILE=${#files[@]}"
        echo "#FILES=${files[*]}"
    } > "$log"

    if [ -n "$SUDO" ]; then
        $SUDO "$BIN" "${files[@]}" >> "$log" 2>&1
    else
        "$BIN" "${files[@]}" >> "$log" 2>&1
    fi
    local rc=$?

    # L'esito va nel log, non solo a schermo: in parallelo l'output si mescola
    # e non si saprebbe piu' a quale esecuzione appartiene un errore.
    echo "#EXIT=${rc}" >> "$log"
    return $rc
}

# --- ciclo con semaforo -------------------------------------------------------
fatti=0
falliti=0

for i in $(seq 0 $((TOT - 1))); do
    # Attende che si liberi uno slot. 'wait -n' richiede bash >= 4.3.
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do
        wait -n 2>/dev/null || break
    done

    read -r -a files <<< "${JOB_FILES[$i]}"
    esegui "${JOB_TEST[$i]}" "${JOB_GIORNO[$i]}" "${JOB_LOG[$i]}" "${files[@]}" &
done

wait

# Il conteggio si fa sui log, non sui codici di uscita dei job in background:
# e' l'unico modo affidabile con un semaforo basato su 'wait -n'.
for i in $(seq 0 $((TOT - 1))); do
    rc=$(sed -n 's/^#EXIT=//p' "${JOB_LOG[$i]}" | tail -1)
    if [ "$rc" = "0" ]; then
        fatti=$((fatti + 1))
    else
        falliti=$((falliti + 1))
        echo "[errore] ${JOB_LOG[$i]} (exit=${rc:-?})" >&2
    fi
done

durata=$(( $(date +%s) - inizio ))
echo "Fatto in ${durata}s: $fatti riuscite, $falliti fallite. Log in '$OUTDIR/'."

if [ "$falliti" -gt 0 ]; then
    exit 1
fi