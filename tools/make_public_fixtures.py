#!/usr/bin/env python3
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
"""Write the public test fixtures from your real ones (.local/fixtures) without your private data:
device names are replaced (--map REAL=PUBLIC, also in lower case and with '_' for '-'), the fan
codes, KNOWN_DEVICE_ID and the Novy channel become those of main/rf_codes_example.h.

usage: make_public_fixtures.py --src .local/fixtures --fixtures tests/host/fixtures
                               --ref tests/host/ref --map MY-DEVICE=NOVY-RF [--map ...]
`make -C tests/host local` runs it into a scratch directory and compares the result with the
tracked fixtures.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXAMPLE = ROOT / "main" / "rf_codes_example.h"
HEADER = ["// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0",
          "// Public copy, written by tools/make_public_fixtures.py from a real extraction: device names,",
          "// fan codes, KNOWN_DEVICE_ID and the Novy channel replaced by the example values."]


def fail(msg):
    sys.exit("make_public_fixtures.py: " + msg)


def example():
    t = EXAMPLE.read_text()
    arrays = {m.group(1): m.group(2).strip()
              for m in re.finditer(r"static const uint16_t (\w+_CODES)\[\]\s*=\s*\{([^}]*)\}", t)}
    dev = re.search(r"#define KNOWN_DEVICE_ID (0x[0-9a-fA-F]+)", t).group(1)
    channel = int(re.search(r"#define NOVY_CHANNEL (\d+)", t).group(1))
    return arrays, dev, channel


def mapper(pairs):
    subs = []
    for p in pairs:
        real, public = p.split("=", 1)
        for f in (lambda x: x, str.lower, str.upper, lambda x: x.replace("-", "_"),
                  lambda x: x.lower().replace("-", "_")):
            subs.append((f(real), f(public)))
    subs.sort(key=lambda s: -len(s[0]))

    def apply(text):
        for real, public in subs:
            text = text.replace(real, public)
        return text
    return apply


def strip_header(lines):
    while lines and lines[0].startswith("//"):
        lines.pop(0)
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--fixtures", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--map", action="append", default=[], help="REAL=PUBLIC device name")
    a = ap.parse_args()
    src, fix, ref = Path(a.src), Path(a.fixtures), Path(a.ref)
    fix.mkdir(parents=True, exist_ok=True)
    ref.mkdir(parents=True, exist_ok=True)
    m = mapper(a.map)
    arrays, dev, channel = example()

    for name in ("discovery.txt", "state_topics.txt"):
        (fix / name).write_text(m((src / name).read_text()))

    body = [l for l in (src / "http_live.txt").read_text().splitlines() if l and not l.startswith("#")]
    (fix / "http_live.txt").write_text("\n".join(
        ["# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0",
         "# Responses of the Arduino firmware 2.3 (status line, content type, body)."] + body) + "\n")

    cfg = m((src / "config_rf.h").read_text())
    for arr, values in arrays.items():
        cfg, n = re.subn(r"(\b%s\[\]\s*=\s*\{)[^}]*(\})" % arr, r"\g<1>%s\g<2>" % values, cfg)
        if n != 1:
            fail("%s not found in config_rf.h" % arr)
    cfg, n = re.subn(r"(KNOWN_DEVICE_ID\s*=\s*)0[xX][0-9a-fA-F]+", r"\g<1>%s" % dev, cfg)
    if n != 1:
        fail("KNOWN_DEVICE_ID not found in config_rf.h")
    lines = HEADER + [""] + strip_header(cfg.splitlines())
    lines += ["", "// The sketch's default channelIndex is replaced by the example channel.",
              "#define REF_NOVY_CHANNEL_INDEX %d" % (channel - 1)]
    (fix / "config_rf.h").write_text("\n".join(lines) + "\n")

    sk = m((src / "sketch_ref.inc").read_text())
    sk, n = re.subn(r"(void sendRFCommand\(ButtonCommand cmd, int channelIndex = )\d+\);",
                    r"\g<1>REF_NOVY_CHANNEL_INDEX);", sk)
    if n != 1:
        fail("sendRFCommand declaration not found in sketch_ref.inc")
    lines = [HEADER[0],
             "// Public copy, written by tools/make_public_fixtures.py: the verbatim sketch sections, only the",
             "// default channelIndex comes from config_rf.h (the example Novy channel)."] + strip_header(sk.splitlines())
    (ref / "sketch_ref.inc").write_text("\n".join(lines) + "\n")
    print("public fixtures written to %s and %s" % (fix, ref))


if __name__ == "__main__":
    main()
