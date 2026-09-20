#!/usr/bin/env python3
"""Compile et exécute les tests de non-régression du firmware sur PC.

Usage :  python test/run_tests.py
Code de sortie = celui de l'exécutable de test (0 = tout passe).
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST = os.path.join(ROOT, "test")
BUILD = os.path.join(TEST, "build")
FW = os.path.join(ROOT, "Solar_Router_V17_29")

MINGW_BIN = r"C:\msys64\mingw64\bin"
GXX = os.path.join(MINGW_BIN, "g++.exe")
EXE = os.path.join(BUILD, "tests.exe")

CMD = [
    GXX,
    "-std=gnu++17", "-O0", "-g", "-fpermissive", "-w",
    # ArduinoJson : on active uniquement l'adaptateur String Arduino.
    # (ne pas définir ARDUINO, sinon PROGMEM/Print/Stream seraient exigés)
    "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1",
    "-I", os.path.join(TEST, "mock"),
    "-I", os.path.join(TEST, "third_party"),
    "-I", FW,
    os.path.join(TEST, "firmware_host.cpp"),
    # Actions.h n'a pas de garde d'inclusion : Actions.cpp doit rester une
    # unité de compilation séparée.
    os.path.join(FW, "Actions.cpp"),
    os.path.join(TEST, "test_main.cpp"),
    "-o", EXE,
]


def main():
    if not os.path.isfile(GXX):
        print("g++ introuvable : %s" % GXX, file=sys.stderr)
        return 2
    if not os.path.isfile(os.path.join(TEST, "third_party", "ArduinoJson.h")):
        print("ArduinoJson.h manquant dans test/third_party/ "
              "(cf. test/README.md)", file=sys.stderr)
        return 2

    os.makedirs(BUILD, exist_ok=True)
    # Le répertoire bin de MSYS2 doit être dans le PATH : cc1plus a besoin
    # des DLL qui s'y trouvent, sinon g++ échoue en silence.
    env = dict(os.environ)
    env["PATH"] = MINGW_BIN + os.pathsep + env.get("PATH", "")

    print("Compilation...", flush=True)
    r = subprocess.run(CMD, cwd=ROOT, env=env)
    if r.returncode != 0:
        print("Echec de compilation", file=sys.stderr)
        return r.returncode

    print("Execution...\n")
    return subprocess.run([EXE], cwd=ROOT, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
