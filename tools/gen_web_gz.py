#!/usr/bin/env python3
"""Génère Solar_Router_V17_29/WebGz.h : versions gzip des pages web embarquées.

Les sources lisibles restent dans Page*.h / JS_*.h (chaînes brutes R"====( ... )====").
Le firmware sert les tableaux *_gz avec l'en-tête HTTP `Content-Encoding: gzip`.

Usage :
  python tools/gen_web_gz.py          # (re)génère WebGz.h
  python tools/gen_web_gz.py --check  # vérifie que WebGz.h est à jour et que chaque
                                      # tableau se décompresse exactement vers la source
"""
import gzip
import re
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "Solar_Router_V17_29"
OUT = ROOT / "WebGz.h"

# (fichier source, [noms de chaînes à compresser])
SOURCES = [
    ("PageAccueil.h", ["MainHtml"]),
    ("JS_Accueil.h", ["MainJS1", "MainJS2", "MainJS3", "Favicon", "Favicon192", "Manifest"]),
    ("PageActions.h", ["ActionsHtml"]),
    ("JS_Actions.h", ["ActionsJS1", "ActionsJS2", "ActionsJS3", "ActionsJS4"]),
    ("PagePara.h", ["ParaHtml"]),
    ("JS_Para.h", ["ParaJS1", "ParaJS2"]),
    ("PageBrute.h", ["PageBrute"]),
    ("JS_Brute.h", ["PageBruteJS1", "PageBruteJS2"]),
    ("PageCommun.h", ["ParaCleHtml"]),
    ("JS_Commun.h", ["ParaCommunJS"]),
    ("PageHtmlJS_OTA.h", ["OtaHtml"]),
    ("PageHtmlJS_Heure.h", ["HeureHtml"]),
    ("PageHtmlJS_Couleurs.h", ["CouleursHtml", "CommunCouleurJS"]),
    ("PageHtmlJS_Export.h", ["ExportHtml"]),
    ("PageHtmlJS_Connect.h", ["ConnectAP_Html"]),
]

RE_LIT = re.compile(r'const\s+char\s*\*\s*(\w+)\s*=\s*R"====\((.*?)\)====";', re.S)


def extract(path: Path) -> dict:
    """Retourne {nom: contenu} des littéraux bruts d'un fichier."""
    text = path.read_bytes().decode("utf-8")
    return {m.group(1): m.group(2) for m in RE_LIT.finditer(text)}


def compress(data: bytes) -> bytes:
    # mtime=0 pour un résultat reproductible (comparaison en --check)
    return gzip.compress(data, compresslevel=9, mtime=0)


def build() -> tuple[str, list]:
    lines = [
        "// Fichier généré par tools/gen_web_gz.py — NE PAS ÉDITER.",
        "// Pages web compressées (gzip) servies avec Content-Encoding: gzip.",
        "#pragma once",
        "#include <pgmspace.h>",
        "",
    ]
    stats = []
    for fname, names in SOURCES:
        lits = extract(ROOT / fname)
        for name in names:
            if name not in lits:
                sys.exit(f"Littéral {name} introuvable dans {fname}")
            raw = lits[name].encode("utf-8")
            gz = compress(raw)
            stats.append((name, len(raw), len(gz)))
            lines.append(f"// {name} : {len(raw)} -> {len(gz)} octets (source {fname})")
            lines.append(f"const uint32_t {name}_gz_len = {len(gz)};")
            lines.append(f"const uint8_t {name}_gz[] PROGMEM = {{")
            for i in range(0, len(gz), 32):
                chunk = ",".join(f"0x{b:02x}" for b in gz[i:i + 32])
                lines.append(f"  {chunk},")
            lines.append("};")
            lines.append("")
    return "\n".join(lines) + "\n", stats


def check() -> int:
    content, stats = build()
    ok = True
    if not OUT.exists() or OUT.read_text(encoding="utf-8") != content:
        print("ERREUR : WebGz.h n'est pas à jour (relancer tools/gen_web_gz.py)")
        ok = False
    # Vérifie la réversibilité : décompression == source
    for fname, names in SOURCES:
        lits = extract(ROOT / fname)
        for name in names:
            raw = lits[name].encode("utf-8")
            if zlib.decompress(compress(raw), 16 + zlib.MAX_WBITS) != raw:
                print(f"ERREUR : {name} ne se décompresse pas à l'identique")
                ok = False
    tot_raw = sum(s[1] for s in stats)
    tot_gz = sum(s[2] for s in stats)
    print(f"{len(stats)} pages : {tot_raw} -> {tot_gz} octets ({100 * (1 - tot_gz / tot_raw):.0f} % de gain)")
    return 0 if ok else 1


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    content, stats = build()
    OUT.write_text(content, encoding="utf-8", newline="\n")
    for name, r, g in stats:
        print(f"{name:16s} {r:7d} -> {g:6d}")
    print(f"Écrit {OUT} ({sum(s[2] for s in stats)} octets compressés)")
