#!/usr/bin/env python3
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
"""Write .local/rf_codes.h (the format of main/rf_codes_example.h) from the config.h of the Arduino
version of this project: TRANSMIT_433MHZ_PIN, KNOWN_DEVICE_ID and the fan *_CODES / *_CYCLE_SIZE
arrays. The Novy hood channel is not in config.h: pass the channel your hood listens on (1-10).

usage: make_rf_codes.py path/to/config.h --channel N [--out .local/rf_codes.h]
Then: idf.py reconfigure && idf.py build
"""
import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FAN = ["ALL_OFF", "FAN_OFF", "LIGHT_TOGGLE", "FAN_FORWARD", "FAN_REVERSE", "FAN_WAVE",
       "SPEED_1", "SPEED_2", "SPEED_3", "SPEED_4", "SPEED_5", "SPEED_6",
       "TIMER_1H", "TIMER_2H", "TIMER_4H"]


def fail(msg):
    sys.exit("make_rf_codes.py: " + msg)


def number(text, name):
    m = re.search(r"^\s*const\s+(?:unsigned\s+long|unsigned\s+int|int|long)\s+%s\s*=\s*(0[xX][0-9a-fA-F]+|\d+)\s*;"
                  % name, text, re.M)
    if not m:
        fail("%s not found" % name)
    return int(m.group(1), 0)


def array(text, name):
    m = re.search(r"\b%s\s*\[\s*\]\s*=\s*\{([^}]*)\}" % name, text)
    if not m:
        fail("%s not found (does this config.h have the ceiling fan codes?)" % name)
    return [int(v, 0) for v in (x.strip() for x in m.group(1).split(",")) if v]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("config")
    ap.add_argument("--channel", type=int, required=True, help="Novy hood channel 1-10")
    ap.add_argument("--out", default=str(ROOT / ".local" / "rf_codes.h"))
    a = ap.parse_args()
    if not 1 <= a.channel <= 10:
        fail("--channel must be 1-10")
    text = Path(a.config).read_text(encoding="utf-8")
    pin, dev = number(text, "TRANSMIT_433MHZ_PIN"), number(text, "KNOWN_DEVICE_ID")
    if dev >= 1 << 20:
        fail("KNOWN_DEVICE_ID has more than 20 bits")
    out = ["/* Your RF settings, from %s by tools/make_rf_codes.py. Keep this file out of git. */"
           % os.path.basename(a.config),
           "#pragma once", "#include <stdint.h>", "",
           "#define RF_TX_GPIO   %d" % pin,
           "#define NOVY_CHANNEL %d" % a.channel, "",
           "#define KNOWN_DEVICE_ID 0x%xul" % dev, ""]
    sizes = []
    for f in FAN:
        codes, size = array(text, f + "_CODES"), number(text, f + "_CYCLE_SIZE")
        if len(codes) != size or not all(0 <= c < 4096 for c in codes):
            fail("%s_CODES must have %s_CYCLE_SIZE values of 12 bits" % (f, f))
        out.append("static const uint16_t %s_CODES[] = {%s};" % (f, ", ".join(str(c) for c in codes)))
        sizes.append("#define %s_CYCLE_SIZE %d" % (f, size))
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text("\n".join(out + [""] + sizes) + "\n")
    print("wrote %s (channel %d, %d fan buttons)" % (a.out, a.channel, len(FAN)))


if __name__ == "__main__":
    main()
