#!/usr/bin/env python3
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
"""Write sdkconfig.secrets (git-ignored, mode 0600) from the config.h of the Arduino version of this
project (config.example.h format): WiFi SSID/password, MQTT broker/port/user/password, plus a setup
AP password (random the first time, kept on later runs). Prints no secret.

usage: make_secrets.py path/to/config.h
Then: idf.py set-target esp32c3 && idf.py build   (set-target re-reads the defaults)
"""
import os
import re
import secrets
import string
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "sdkconfig.secrets"


def c_string(text, name):
    """Value of `const <type> NAME = "...";` with C escapes resolved."""
    m = re.search(r'^\s*const\s+(?:String|char\s*\*)\s+%s\s*=\s*"((?:[^"\\]|\\.)*)"\s*;' % name, text, re.M)
    if not m:
        sys.exit("make_secrets.py: %s not found in config.h" % name)
    raw, out, i = m.group(1), [], 0
    simple = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
    while i < len(raw):
        c = raw[i]
        if c == "\\" and i + 1 < len(raw):
            n = raw[i + 1]
            if n == "x":
                hx = re.match(r"[0-9a-fA-F]{1,2}", raw[i + 2:])
                out.append(chr(int(hx.group(0), 16)))
                i += 2 + len(hx.group(0))
                continue
            out.append(simple.get(n, n))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def c_int(text, name):
    m = re.search(r"^\s*const\s+int\s+%s\s*=\s*(\d+)\s*;" % name, text, re.M)
    if not m:
        sys.exit("make_secrets.py: %s not found in config.h" % name)
    return int(m.group(1))


def quote(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def existing_ap_password():
    if not OUT.exists():
        return None
    m = re.search(r'^CONFIG_NOVY_AP_PASSWORD="(.*)"$', OUT.read_text(), re.M)
    return m.group(1).replace('\\"', '"').replace("\\\\", "\\") if m else None


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    text = Path(sys.argv[1]).read_text(encoding="utf-8")
    ssid, password = c_string(text, "SSID"), c_string(text, "PASSWORD")
    server, port = c_string(text, "MQTT_SERVER"), c_int(text, "MQTT_PORT")
    user, mqtt_pass = c_string(text, "MQTT_USER"), c_string(text, "MQTT_PASSWORD")
    if not 8 <= len(password) <= 64:
        sys.exit("make_secrets.py: the WiFi password in config.h is not 8-64 characters")
    ap_pass = existing_ap_password()
    kept = ap_pass is not None
    if not kept:
        ap_pass = "".join(secrets.choice(string.ascii_letters + string.digits) for _ in range(16))
    lines = [
        "CONFIG_NOVY_WIFI_SSID=" + quote(ssid),
        "CONFIG_NOVY_WIFI_PASSWORD=" + quote(password),
        "CONFIG_NOVY_AP_PASSWORD=" + quote(ap_pass),
        "CONFIG_NOVY_MQTT_URI=" + quote("mqtt://%s:%d" % (server, port)),
        "CONFIG_NOVY_MQTT_USER=" + quote(user),
        "CONFIG_NOVY_MQTT_PASSWORD=" + quote(mqtt_pass),
    ]
    fd = os.open(str(OUT), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(lines) + "\n")
    OUT.chmod(0o600)
    print("wrote %s (WiFi, MQTT, setup AP password %s); values not shown"
          % (OUT, "kept" if kept else "new, random"))
    host, prefix = c_string(text, "HOSTNAME"), c_string(text, "MQTT_PREFIX")
    print("To keep the MQTT topics and Home Assistant entities of the Arduino version, put\n"
          "  CONFIG_NOVY_DEVICE_NAME=%s\ninto .local/sdkconfig.local and keep the MQTT prefix %s (MQTT tab)."
          % (quote(host), prefix))


if __name__ == "__main__":
    main()
