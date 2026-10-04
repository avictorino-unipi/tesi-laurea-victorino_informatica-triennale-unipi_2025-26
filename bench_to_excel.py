#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
bench_to_excel.py - legge i log prodotti da bench_run.sh, ne estrae le metriche
e le scrive nei fogli "Data Set 1" (bftest1..9) e "Data Set 2" (bftest10..18)
del workbook di benchmark.

Uso tipico:
    python3 bench_to_excel.py --logs logs --xlsx Benchmark_grafici.xlsx
    python3 bench_to_excel.py --logs logs --csv risultati.csv --dry-run
    # scrive anche la colonna Z (popolazione reale) dei nuovi test:
    python3 bench_to_excel.py --logs logs --xlsx Benchmark_grafici.xlsx \\
        --pop 10=1003 11=1003 12=1003 13=503 14=503 15=503 16=103 17=103 18=103

Scrive SOLO le celle numeriche delle colonne B,C,D..M,O..X (e Z con --pop):
il file viene modificato a livello di XML, quindi grafici, stili e formattazione
condizionale restano intatti (openpyxl, al contrario, cancellerebbe i grafici).
"""

import argparse
import csv
import os
import re
import shutil
import sys
import time
import zipfile
import xml.etree.ElementTree as ET

# ---------------------------------------------------------------------------
# CONFIGURAZIONE — l'unica parte da adattare
# ---------------------------------------------------------------------------
# Riga in cui inizia il blocco di 10 giorni di ogni test. I due fogli hanno
# esattamente lo stesso layout:
#   righe   2-31  -> primo blocco  (molto_frequente / bilanciato / poco_frequente)
#   righe  34-63  -> secondo blocco
#   righe  66-95  -> terzo blocco
def row_start(slot):
    """Riga iniziale del blocco di 10 giorni per lo slot i-esimo del foglio.

    Layout dei fogli: 3 profili per gruppo (molto_frequente / bilanciato /
    poco_frequente) a distanza di 10 righe, e gruppi a distanza di 32 righe.
        slot 0,1,2 -> righe   2, 12, 22
        slot 3,4,5 -> righe  34, 44, 54
        slot 6,7,8 -> righe  66, 76, 86
        slot 9,...  -> righe  98, 108, 118, 130, ...   (estensione naturale)
    """
    return 2 + 32 * (slot // 3) + 10 * (slot % 3)


SLOTS_PER_SHEET_STD = 9      # slot occupati dal layout originale (righe 2..95)
LEGEND_ROW = 97              # dallo slot 9 in poi si sovrascrive la legenda

SHEET_DATASET1 = "Data Set 1"
SHEET_DATASET2 = "Data Set 2"

# Nomi alternativi accettati (il foglio e' stato rinominato nel tempo).
SHEET_ALIASES = {
    SHEET_DATASET1: ["Data Set 1", "Dati", "Data Set1", "DataSet1"],
    SHEET_DATASET2: ["Data Set 2", "Dati 2", "Data Set2", "DataSet2"],
}

DEFAULT_MAP = ["Data Set 1=1-9", "Data Set 2=10-18"]
TEST_LAYOUT = {}   # popolato in main() da build_layout()


def parse_range(spec):
    """'1-9' -> [1..9];  '3' -> [3];  '1-3,7,10-12' -> lista ordinata."""
    out = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            out.extend(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def build_layout(map_specs):
    """['Data Set 1=1-9', ...] -> {test: (foglio, riga)}  +  avvisi."""
    layout, warnings = {}, []
    for spec in map_specs:
        if "=" not in spec:
            sys.exit("ERRORE: --map vuole FOGLIO=RANGE (es. 'Data Set 1=1-9')")
        sheet, rng = spec.split("=", 1)
        sheet = sheet.strip()
        tests = parse_range(rng)
        for slot, t in enumerate(tests):
            if t in layout:
                sys.exit("ERRORE: bftest%d mappato due volte" % t)
            layout[t] = (sheet, row_start(slot))
        if len(tests) > SLOTS_PER_SHEET_STD:
            warnings.append(
                "'%s': %d test richiesti ma il foglio ha solo %d blocchi "
                "(righe 2..95). Dal 10o blocco in poi si scrive da riga %d in giu', "
                "sopra la legenda a riga %d, e i grafici NON copriranno le righe nuove."
                % (sheet, len(tests), SLOTS_PER_SHEET_STD,
                   row_start(SLOTS_PER_SHEET_STD), LEGEND_ROW))
    return layout, warnings


# La popolazione reale e' GIA' presente nel workbook, in colonna X
# ("popolazione_reale" nell'intestazione di ogni blocco). Lo script la LEGGE da
# li' per calcolare l'errore relativo: non va piu' passata con --pop.
# La colonna Z ne e' una copia, usata dalla formattazione condizionale (=$Z
# della stessa riga); dove manca viene riallineata a X.
POP_COLUMN = "X"
POP_MIRROR_COLUMN = "Z"

# campo estratto -> colonna Excel
COLUMNS = {
    "giorno":          "B",
    "t":               "C",
    "s_obs":           "D",
    "f1":              "E",
    "f2":              "F",
    "f3":              "G",
    "f4":              "H",
    "f5":              "I",
    "fn":              "J",
    "jackknife":       "K",
    "chao":            "L",
    "huggins":         "M",
    "jack_lo":         "O",
    "jack_hi":         "P",
    "chao_lo":         "Q",
    "chao_hi":         "R",
    "chao_log_lo":     "S",
    "chao_log_hi":     "T",
    "hug_lo":          "U",
    "hug_hi":          "V",
    # ATTENZIONE: hug_log_lo/hug_log_hi NON vengono scritti nel foglio.
    # La versione precedente li mappava su W e X, ma X e' popolazione_reale:
    # con i log attuali (in cui Huggins stampa anche l'IC log-trasformato) la
    # scrittura avrebbe sovrascritto la popolazione, rompendo errore relativo,
    # formattazione condizionale e grafici. Il foglio non ha colonne per questa
    # coppia; resta disponibile nel CSV.
}

# Garanzia strutturale: nessuna colonna di dati puo' finire sulla popolazione.
_collisioni = [k for k, v in COLUMNS.items() if v in (POP_COLUMN, POP_MIRROR_COLUMN)]
if _collisioni:
    raise SystemExit("ERRORE interno: %s scriverebbe sulla colonna della "
                     "popolazione reale (%s)." % (", ".join(_collisioni), POP_COLUMN))

# ---------------------------------------------------------------------------
# PARSING DEI LOG
# ---------------------------------------------------------------------------
NUM = r"(-?\d+(?:\.\d+)?)"

RE_HEADER_TEST = re.compile(r"^#TEST=bftest(\d+)", re.M)
RE_HEADER_GIORNO = re.compile(r"^#GIORNO=(\d+)", re.M)
RE_HEADER_NFILE = re.compile(r"^#NFILE=(\d+)", re.M)

RE_SOBS = re.compile(r"Nodi unici totali osservati \(S_obs\):\s*(\d+)")

# Le etichette delle frequenze sono passate da (n1)..(n5)/(nn) a (f1)..(f5),
# e la riga delle frequenze alte non ha piu' un marcatore fra parentesi.
# I pattern accettano ENTRAMBE le forme, cosi' i log storici restano leggibili.
RE_FK = {k: re.compile(r"\([nf]%s\):\s*(\d+)" % k) for k in ("1", "2", "3", "4", "5")}
RE_FN = re.compile(r"\(nn\):\s*(\d+)"
                   r"|Nodi visti in piu' di cinque catture:\s*(\d+)")

RE_JACK_SEL = re.compile(
    r"Ordine Jackknife selezionato: J(\d)\s*\(Nj=" + NUM + r",\s*se=" + NUM +
    r",\s*IC95%=\[" + NUM + r",\s*" + NUM + r"\]\)")
RE_JACK_INT = re.compile(
    r"Stimatore Jackknife interpolato:\s*" + NUM + r"\s*\(se=" + NUM +
    r",\s*IC95%=\[" + NUM + r",\s*" + NUM + r"\],\s*c=" + NUM + r"\)")

# Chao: "Chao2 Eterogeneo Classico" e' diventato "Stima eq. (8) [N_Ch]".
RE_CHAO = re.compile(
    r"(?:Chao2 Eterogeneo Classico|Stima eq\. \(8\) \[N_Ch\]):\s*" + NUM)
RE_CHAO_MIN = re.compile(r"Limite inferiore eq\. \(9\) \[N_min\]:\s*" + NUM)
RE_CHAO_CO = re.compile(r"Approssimazione di Cormack eq\. \(10\) \[N_Co\]:\s*" + NUM)

RE_HUGGINS = re.compile(r"Stimatore di Huggin[g]?s:\s*" + NUM)

# IC simmetrico. Chao stampa "Intervallo di fiducia 95% (simmetrico, troncato
# a S_obs)", Huggins "IC 95% (normale asintotico, troncato a S_obs)"; le forme
# storiche, senza qualificatore e con maiuscole diverse, restano accettate.
RE_CI = re.compile(
    r"(?:Intervallo di [Ff]iducia 95%|IC 95% \(normale asintotico)"
    r"[^\[\n]*:\s*\[" + NUM + r",\s*" + NUM + r"\]")
RE_CI_LOG = re.compile(
    r"(?:Intervallo di [Ff]iducia 95% \([Ll]og-[Tt]rasformato\)"
    r"|IC 95% \(log-trasformato)"
    r"[^\[\n]*:\s*\[" + NUM + r",\s*" + NUM + r"\]")
RE_CI_BOOT = re.compile(
    r"IC 95% \(bootstrap[^\[\n]*:\s*\[" + NUM + r",\s*" + NUM + r"\]")

# Modello di Huggins effettivamente selezionato e sua bonta' di adattamento.
RE_HUG_MODELLO = re.compile(r"Modello selezionato \(AIC minimo\):\s*(.+?)\s*---")
RE_HUG_PAR = re.compile(r"parametri s = (\d+)")
RE_HUG_AIC = re.compile(r"AIC = " + NUM)
RE_HUG_NCOV = re.compile(r"Indicatrici costruite:\s*(\d+)")

RE_HUG_SKIP = re.compile(
    r"Nessun modello e' arrivato a convergenza|Nessuna ricattura:"
    r"|Stima non calcolata|Memoria insufficiente")

HUGGINS_HEADER = "STIMATORE DI HUGGINS"


def parse_log(path):
    """Restituisce un dict con i valori estratti da un singolo log."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        txt = fh.read()

    rec = {"file": os.path.basename(path)}

    # --- identificazione test/giorno: prima dall'intestazione, poi dal nome file
    m = RE_HEADER_TEST.search(txt)
    n = RE_HEADER_GIORNO.search(txt)
    if m and n:
        rec["test"], rec["giorno"] = int(m.group(1)), int(n.group(1))
    else:
        m2 = re.search(r"bftest(\d+)_giorno(\d+)", rec["file"])
        if not m2:
            raise ValueError("impossibile dedurre test/giorno da %s" % path)
        rec["test"], rec["giorno"] = int(m2.group(1)), int(m2.group(2))

    m = RE_HEADER_NFILE.search(txt)
    rec["t"] = int(m.group(1)) if m else 3 * rec["giorno"]

    # --- frequenze
    m = RE_SOBS.search(txt)
    rec["s_obs"] = int(m.group(1)) if m else None
    for k, rgx in RE_FK.items():
        m = rgx.search(txt)
        rec["f" + k] = int(m.group(1)) if m else None
    m = RE_FN.search(txt)
    rec["fn"] = int(m.group(1) or m.group(2)) if m else None

    # --- jackknife (ordine selezionato dal test sequenziale)
    m = RE_JACK_SEL.search(txt)
    if m:
        rec["jack_order"] = int(m.group(1))
        rec["jackknife"] = float(m.group(2))
        rec["jack_se"] = float(m.group(3))
        rec["jack_lo"] = float(m.group(4))
        rec["jack_hi"] = float(m.group(5))
    # Il ripiego "nessun test accettato" non ha piu' una riga propria: lo
    # stimatore stampa comunque la riga "Ordine Jackknife selezionato",
    # marcandola come convenzione. Non serve piu' un pattern separato.

    m = RE_JACK_INT.search(txt)
    if m:
        rec["jack_int"] = float(m.group(1))
        rec["jack_int_se"] = float(m.group(2))
        rec["jack_int_lo"] = float(m.group(3))
        rec["jack_int_hi"] = float(m.group(4))
        rec["jack_int_c"] = float(m.group(5))

    # --- separo la parte Chao dalla parte Huggins: le righe "Intervallo di
    #     Fiducia 95%" hanno lo stesso identico formato nelle due sezioni.
    cut = txt.find(HUGGINS_HEADER)
    txt_chao = txt[:cut] if cut != -1 else txt
    txt_hug = txt[cut:] if cut != -1 else ""

    m = RE_CHAO.search(txt_chao)
    if m:
        rec["chao"] = float(m.group(1))
    m = RE_CHAO_MIN.search(txt_chao)
    if m:
        rec["chao_min"] = float(m.group(1))
    m = RE_CHAO_CO.search(txt_chao)
    if m:
        rec["chao_cormack"] = float(m.group(1))
    m = RE_CI.search(txt_chao)
    if m:
        rec["chao_lo"], rec["chao_hi"] = float(m.group(1)), float(m.group(2))
    m = RE_CI_LOG.search(txt_chao)
    if m:
        rec["chao_log_lo"], rec["chao_log_hi"] = float(m.group(1)), float(m.group(2))

    m = RE_HUGGINS.search(txt_hug)
    if m:
        rec["huggins"] = float(m.group(1))
    m = RE_CI.search(txt_hug)
    if m:
        rec["hug_lo"], rec["hug_hi"] = float(m.group(1)), float(m.group(2))
    m = RE_CI_LOG.search(txt_hug)
    if m:
        rec["hug_log_lo"], rec["hug_log_hi"] = float(m.group(1)), float(m.group(2))
    m = RE_CI_BOOT.search(txt_hug)
    if m:
        rec["hug_boot_lo"], rec["hug_boot_hi"] = float(m.group(1)), float(m.group(2))

    m = RE_HUG_MODELLO.search(txt_hug)
    if m:
        rec["hug_modello"] = m.group(1)
    m = RE_HUG_PAR.search(txt_hug)
    if m:
        rec["hug_s"] = int(m.group(1))
    m = RE_HUG_AIC.search(txt_hug)
    if m:
        rec["hug_aic"] = float(m.group(1))
    m = RE_HUG_NCOV.search(txt_hug)
    if m:
        rec["hug_ncov"] = int(m.group(1))

    if "huggins" not in rec and txt_hug and RE_HUG_SKIP.search(txt_hug):
        rec["hug_skipped"] = True

    return rec


# ---------------------------------------------------------------------------
# SCRITTURA XLSX (a livello di XML: preserva grafici, stili, formattazione)
# ---------------------------------------------------------------------------
NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
NS_R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
CT_NS = "http://schemas.openxmlformats.org/package/2006/content-types"
PR_NS = "http://schemas.openxmlformats.org/package/2006/relationships"


def col_to_num(col):
    n = 0
    for ch in col:
        n = n * 26 + (ord(ch) - 64)
    return n


def sheet_path_for(zf, sheet_name):
    """Trova xl/worksheets/sheetN.xml corrispondente al nome del foglio.

    Accetta anche i nomi alternativi elencati in SHEET_ALIASES: il foglio
    "Dati" e' stato rinominato in "Data Set 1" e lo script deve funzionare
    con entrambe le versioni del workbook.
    """
    wb = ET.fromstring(zf.read("xl/workbook.xml"))
    rels = ET.fromstring(zf.read("xl/_rels/workbook.xml.rels"))
    rid_to_target = {r.get("Id"): r.get("Target") for r in rels}
    present = {sh.get("name"): sh for sh in wb.find("{%s}sheets" % NS)}

    for candidate in SHEET_ALIASES.get(sheet_name, [sheet_name]):
        sh = present.get(candidate)
        if sh is not None:
            target = rid_to_target[sh.get("{%s}id" % NS_R)]
            return "xl/" + target.lstrip("/").replace("xl/", "", 1)

    raise KeyError("foglio '%s' non trovato (presenti: %s)"
                   % (sheet_name, ", ".join(present)))


RE_DECL = re.compile(rb"^\s*<\?xml[^>]*\?>[\r\n]*")
RE_ROOT_TAG = re.compile(rb"<[A-Za-z_][\w.\-]*(?:\s[^>]*)?>")
RE_XMLNS = re.compile(rb'xmlns:([\w.\-]+)="[^"]*"')
RE_IGNORABLE = re.compile(rb'(\s[\w.\-]+:Ignorable=")([^"]*)(")')

RE_SHEETDATA = re.compile(rb"<sheetData\s*/>|<sheetData\b[^>]*>.*?</sheetData>", re.S)
RE_ROW_ITEM = re.compile(rb"<row\b[^>]*?/>|<row\b[^>]*?>.*?</row>", re.S)
RE_CELL_ITEM = re.compile(rb"<c\b[^>]*?/>|<c\b[^>]*?>.*?</c>", re.S)
RE_ATTR_R = re.compile(rb'\br="([^"]+)"')
RE_ATTR_T = re.compile(rb'\st="[^"]*"')


def repair_ignorable(xml):
    """Toglie da mc:Ignorable i prefissi non dichiarati nel tag radice.

    Serve a recuperare i file gia' danneggiati da una versione precedente di
    questo script: mc:Ignorable="x14ac xr xr2 xr3" senza le rispettive
    dichiarazioni xmlns e' XML non valido, e Excel apre il foglio vuoto.
    """
    m = RE_ROOT_TAG.search(RE_DECL.sub(b"", xml))
    if not m:
        return xml
    tag = m.group(0)
    declared = set(RE_XMLNS.findall(tag))

    def fix(mm):
        kept = [p for p in mm.group(2).split() if p in declared]
        return mm.group(1) + b" ".join(kept) + mm.group(3) if kept else b""

    new_tag = RE_IGNORABLE.sub(fix, tag)
    return xml.replace(tag, new_tag, 1) if new_tag != tag else xml


def _fmt(val):
    f = float(val)
    return b"%d" % int(f) if f.is_integer() else ("%.10g" % f).encode()


def _split_tag(item):
    """Divide un elemento in (tag di apertura, contenuto interno)."""
    end = item.index(b">") + 1
    if item[end - 2:end] == b"/>":
        return item[:end - 2] + b">", b""
    name = item[1:item.index(b">")].split()[0]
    return item[:end], item[end:-(len(name) + 3)]


def set_cells(sheet_xml, updates):
    """updates: {(row:int, col:str): float}. Ritorna (xml_bytes, formule_rimosse).

    Modifica il testo XML invece di riserializzarlo con ElementTree: tutto cio'
    che non tocchiamo resta identico byte per byte (namespace, grafici, stili,
    formattazione condizionale), e non serve registrare prefissi.
    """
    sheet_xml = repair_ignorable(sheet_xml)
    m = RE_SHEETDATA.search(sheet_xml)
    if not m:
        raise ValueError("<sheetData> non trovato nel foglio")

    open_tag, inner = _split_tag(m.group(0))
    rows = []                                   # [(numero, testo)]
    for rm in RE_ROW_ITEM.finditer(inner):
        rows.append([int(RE_ATTR_R.search(rm.group(0)).group(1)), rm.group(0)])

    by_row = {}
    for (r, c), v in updates.items():
        by_row.setdefault(r, {})[c] = v

    removed_formula = False
    index = {num: i for i, (num, _) in enumerate(rows)}

    for rnum in sorted(by_row):
        if rnum in index:
            row_text = rows[index[rnum]][1]
        else:                                   # riga assente: la creo
            row_text = b'<row r="%d">' % rnum + b"</row>"
        row_open, row_inner = _split_tag(row_text)

        cells = []                              # [(numero colonna, testo)]
        for cm in RE_CELL_ITEM.finditer(row_inner):
            ref = RE_ATTR_R.search(cm.group(0)).group(1)
            cells.append([col_to_num(re.match(rb"[A-Z]+", ref).group(0).decode()),
                          cm.group(0)])

        for col, val in by_row[rnum].items():
            cnum = col_to_num(col)
            ref = b"%s%d" % (col.encode(), rnum)
            pos = next((i for i, (n, _) in enumerate(cells) if n == cnum), None)
            if pos is None:                     # cella assente: attributi minimi
                cell_open = b'<c r="%s">' % ref
            else:
                cell_open, cell_inner = _split_tag(cells[pos][1])
                if b"<f" in cell_inner:
                    removed_formula = True
                cell_open = RE_ATTR_T.sub(b"", cell_open)   # non e' piu' testo
            new_cell = cell_open + b"<v>" + _fmt(val) + b"</v></c>"
            if pos is None:
                insert = next((i for i, (n, _) in enumerate(cells) if n > cnum),
                              len(cells))
                cells.insert(insert, [cnum, new_cell])
            else:
                cells[pos][1] = new_cell

        cells.sort(key=lambda c: c[0])
        row_text = row_open + b"".join(c[1] for c in cells) + b"</row>"
        if rnum in index:
            rows[index[rnum]][1] = row_text
        else:
            insert = next((i for i, (n, _) in enumerate(rows) if n > rnum), len(rows))
            rows.insert(insert, [rnum, row_text])
            index = {num: i for i, (num, _) in enumerate(rows)}

    new_data = open_tag + b"".join(r[1] for r in rows) + b"</sheetData>"
    return sheet_xml[:m.start()] + new_data + sheet_xml[m.end():], removed_formula


RE_ATTR_T_VAL = re.compile(rb'\st="([^"]*)"')
RE_V_TAG = re.compile(rb"<v>(.*?)</v>", re.S)


def read_column(sheet_xml, col):
    """{riga: valore} per le celle NUMERICHE di una colonna di un foglio.

    Le celle testuali (t="s" con shared string, t="str", t="inlineStr") sono
    ignorate: l'intestazione "popolazione_reale" e' una di queste.
    """
    out = {}
    m = RE_SHEETDATA.search(sheet_xml)
    if m is None:
        return out
    for rm in RE_ROW_ITEM.finditer(m.group(0)):
        row_xml = rm.group(0)
        for cm in RE_CELL_ITEM.finditer(row_xml):
            cell = cm.group(0)
            ref = RE_ATTR_R.search(cell)
            if ref is None:
                continue
            r = ref.group(1).decode()
            c_letters = "".join(ch for ch in r if ch.isalpha())
            if c_letters != col:
                continue
            t = RE_ATTR_T_VAL.search(cell)
            if t is not None and t.group(1) in (b"s", b"str", b"inlineStr"):
                continue
            v = RE_V_TAG.search(cell)
            if v is None:
                continue
            try:
                out[int("".join(ch for ch in r if ch.isdigit()))] = float(v.group(1))
            except ValueError:
                continue
    return out


def read_pop_columns(xlsx, sheets):
    """{foglio: {riga: popolazione_reale}} leggendo la colonna X del workbook."""
    check_xlsx(xlsx)
    out = {}
    with zipfile.ZipFile(xlsx) as zf:
        for name in sheets:
            try:
                path = sheet_path_for(zf, name)
            except KeyError as e:
                print("[warn] %s" % e, file=sys.stderr)
                continue
            xml = zf.read(path)
            out[name] = {
                "pop": read_column(xml, POP_COLUMN),
                "mirror": read_column(xml, POP_MIRROR_COLUMN),
            }
    return out


def err_rel(stima, pop):
    """Errore relativo con segno: negativo = sottostima."""
    if stima is None or pop in (None, 0):
        return None
    return round((stima - pop) / pop, 6)


def drop_calcchain(names, contents):
    """Rimuove calcChain.xml + override + relationship (Excel lo ricostruisce)."""
    if "xl/calcChain.xml" not in names:
        return names, contents
    names = [n for n in names if n != "xl/calcChain.xml"]
    contents.pop("xl/calcChain.xml", None)
    contents["[Content_Types].xml"] = re.sub(
        rb"<Override[^>]*calcChain\.xml[^>]*/>", b"", contents["[Content_Types].xml"])
    contents["xl/_rels/workbook.xml.rels"] = re.sub(
        rb"<Relationship[^>]*calcChain\.xml[^>]*/>", b"",
        contents["xl/_rels/workbook.xml.rels"])
    return names, contents


def check_xlsx(path):
    """Errore chiaro se il file non e' un .xlsx leggibile."""
    if not os.path.exists(path):
        sys.exit("ERRORE: '%s' non esiste." % path)
    head = open(path, "rb").read(8)
    if head[:2] == b"\xd0\xcf":
        sys.exit("ERRORE: '%s' e' un .xls (formato vecchio), non un .xlsx.\n"
                 "Riaprilo in Excel e salvalo come 'Cartella di lavoro di Excel (*.xlsx)'." % path)
    if head[:2] != b"PK":
        sys.exit("ERRORE: '%s' non e' un file .xlsx valido (non e' un archivio zip).\n"
                 "Probabilmente e' troncato o danneggiato: riparti da una copia integra." % path)
    try:
        with zipfile.ZipFile(path) as zf:
            if "xl/workbook.xml" not in zf.namelist():
                sys.exit("ERRORE: '%s' e' uno zip ma non un workbook Excel." % path)
    except zipfile.BadZipFile:
        sys.exit("ERRORE: '%s' e' danneggiato (zip illeggibile).\n"
                 "Riparti da una copia integra del workbook." % path)


def write_xlsx(src, dst, updates_by_sheet):
    """updates_by_sheet: {nome_foglio: {(riga, colonna): valore}}."""
    check_xlsx(src)
    with zipfile.ZipFile(src) as zf:
        names = zf.namelist()
        contents = {n: zf.read(n) for n in names}
        paths = {name: sheet_path_for(zf, name) for name in updates_by_sheet}

    removed_formula = False
    for name, updates in updates_by_sheet.items():
        if not updates:
            continue
        sheet = paths[name]
        contents[sheet], removed = set_cells(contents[sheet], updates)
        removed_formula = removed_formula or removed
    if removed_formula:
        names, contents = drop_calcchain(names, contents)

    # scrittura atomica: se qualcosa va storto, il file di destinazione
    # esistente non viene troncato a meta'
    tmp = dst + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as out:
        for n in names:
            out.writestr(n, contents[n])
    os.replace(tmp, dst)


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--logs", default="logs", help="cartella dei log (default: logs)")
    ap.add_argument("--xlsx", help="workbook da aggiornare")
    ap.add_argument("--out", help="file di destinazione (default: sovrascrive --xlsx)")
    ap.add_argument("--csv", help="salva anche i valori estratti in CSV")
    ap.add_argument("--dry-run", action="store_true", help="stampa e basta")
    ap.add_argument("--pop", nargs="*", default=[], metavar="TEST=POPOLAZIONE",
                    help="OVERRIDE della popolazione reale. Di norma NON serve: "
                         "il valore viene letto dalla colonna X del workbook, "
                         "dove e' gia' presente. Usare solo per righe ancora "
                         "vuote (es. --pop 37-39=1003)")
    ap.add_argument("--map", nargs="*", default=None, metavar="FOGLIO=RANGE",
                    help="quali bftest vanno in quale foglio, nell'ordine dei blocchi "
                         "(default: %s)" % " ".join(DEFAULT_MAP))
    args = ap.parse_args()

    global TEST_LAYOUT
    TEST_LAYOUT, layout_warnings = build_layout(args.map or DEFAULT_MAP)
    for w in layout_warnings:
        print("[warn] " + w, file=sys.stderr)

    pop = {}
    for item in args.pop:
        try:
            rng, v = item.split("=")
            for t in parse_range(rng):
                pop[t] = float(v)
        except ValueError:
            sys.exit("ERRORE: --pop vuole coppie RANGE=POPOLAZIONE (es. 1-3=1003)")

    logs = sorted(f for f in os.listdir(args.logs) if f.endswith(".log"))
    if not logs:
        sys.exit("Nessun .log in %s" % args.logs)

    records = []
    for f in logs:
        try:
            records.append(parse_log(os.path.join(args.logs, f)))
        except Exception as e:                     # log corrotto o run fallita
            print("[warn] %s: %s" % (f, e), file=sys.stderr)
    records.sort(key=lambda r: (r["test"], r["giorno"]))

    # --- popolazione reale: letta dal workbook (colonna X), non da --pop.
    pop_sheet = {}
    if args.xlsx:
        pop_sheet = read_pop_columns(args.xlsx, sorted({sh for sh, _ in TEST_LAYOUT.values()}))

    mancanti = []
    for r in records:
        layout = TEST_LAYOUT.get(r["test"])
        valore = pop.get(r["test"])          # override esplicito, se dato
        if valore is None and layout is not None and 1 <= r["giorno"] <= 10:
            sheet, r0 = layout
            valore = pop_sheet.get(sheet, {}).get("pop", {}).get(r0 + r["giorno"] - 1)
        r["pop_reale"] = valore
        if valore is None:
            mancanti.append((r["test"], r["giorno"]))
        r["err_rel_jack"] = err_rel(r.get("jackknife"), valore)
        r["err_rel_chao"] = err_rel(r.get("chao"), valore)
        r["err_rel_hug"] = err_rel(r.get("huggins"), valore)

    if mancanti:
        print("[warn] popolazione reale assente in colonna %s per %d righe "
              "(prime: %s). Usa --pop per quelle righe."
              % (POP_COLUMN, len(mancanti),
                 ", ".join("bftest%d/g%d" % m for m in mancanti[:5])),
              file=sys.stderr)

    fields = ["test", "giorno", "t", "s_obs", "f1", "f2", "f3", "f4", "f5", "fn",
              "jack_order", "jackknife", "jack_se", "jack_lo", "jack_hi",
              "chao", "chao_lo", "chao_hi", "chao_log_lo", "chao_log_hi",
              "jack_int", "jack_int_se", "jack_int_lo", "jack_int_hi", "jack_int_c",
              "chao_min", "chao_cormack",
              "huggins", "hug_lo", "hug_hi", "hug_log_lo", "hug_log_hi",
              "hug_boot_lo", "hug_boot_hi",
              "hug_modello", "hug_s", "hug_aic", "hug_ncov",
              "pop_reale", "err_rel_jack", "err_rel_chao", "err_rel_hug"]

    for r in records:
        layout = TEST_LAYOUT.get(r["test"])
        if layout and 1 <= r["giorno"] <= 10:
            dest = "%s!%d" % (layout[0], layout[1] + r["giorno"] - 1)
        else:
            dest = "?"
        nota = "  [Huggins: nessuna stima]" if r.get("hug_skipped") else ""

        def pct(x):
            return "  n/d" if x is None else "%+5.1f%%" % (100.0 * x)

        print("bftest%-2d giorno %2d -> %-14s | S_obs=%-5s N=%-6s | "
              "jack=%-8s %s  chao=%-8s %s  hug=%-8s %s%s" % (
                  r["test"], r["giorno"], dest, r.get("s_obs"),
                  "%.0f" % r["pop_reale"] if r.get("pop_reale") else "?",
                  r.get("jackknife"), pct(r.get("err_rel_jack")),
                  r.get("chao"), pct(r.get("err_rel_chao")),
                  r.get("huggins"), pct(r.get("err_rel_hug")), nota))

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=fields, extrasaction="ignore")
            w.writeheader()
            w.writerows(records)
        print("CSV scritto: %s" % args.csv)

    if not args.xlsx or args.dry_run:
        return

    updates_by_sheet = {}
    for r in records:
        if r["test"] not in TEST_LAYOUT:
            print("[warn] bftest%d non mappato in TEST_LAYOUT" % r["test"],
                  file=sys.stderr)
            continue
        if not 1 <= r["giorno"] <= 10:
            print("[warn] giorno %s fuori range" % r["giorno"], file=sys.stderr)
            continue
        sheet, row_start = TEST_LAYOUT[r["test"]]
        row = row_start + r["giorno"] - 1
        updates = updates_by_sheet.setdefault(sheet, {})
        for key, col in COLUMNS.items():
            val = r.get(key)
            if val is not None:
                updates[(row, col)] = val
        # La popolazione si scrive SOLO se e' stata fornita come override e la
        # cella e' ancora vuota: la colonna X e' un dato d'ingresso, non un
        # risultato, e sovrascriverla falserebbe tutti gli errori relativi.
        gia_presente = pop_sheet.get(sheet, {}).get("pop", {})
        if r["test"] in pop and row not in gia_presente:
            updates[(row, POP_COLUMN)] = pop[r["test"]]

        # La colonna Z e' la copia di X letta dalla formattazione condizionale
        # (=$Z della stessa riga). Sui fogli nuovi puo' mancare: la si allinea.
        mirror = pop_sheet.get(sheet, {}).get("mirror", {})
        if r.get("pop_reale") is not None and row not in mirror:
            updates[(row, POP_MIRROR_COLUMN)] = r["pop_reale"]

    for t in pop:
        if t not in TEST_LAYOUT:
            print("[warn] --pop: bftest%d non mappato, ignorato" % t, file=sys.stderr)

    if not updates_by_sheet:
        sys.exit("Nessun dato da scrivere.")

    check_xlsx(args.xlsx)
    out = args.out or args.xlsx
    if out == args.xlsx:
        # backup con data/ora: non sovrascrive mai un backup precedente
        bak = "%s.%s.bak" % (args.xlsx, time.strftime("%Y%m%d-%H%M%S"))
        shutil.copy2(args.xlsx, bak)
        print("Backup: %s" % bak)
        write_xlsx(bak, out, updates_by_sheet)
    else:
        write_xlsx(args.xlsx, out, updates_by_sheet)

    for sheet, updates in sorted(updates_by_sheet.items()):
        print("Aggiornate %d celle nel foglio '%s' di '%s'."
              % (len(updates), sheet, out))


if __name__ == "__main__":
    main()