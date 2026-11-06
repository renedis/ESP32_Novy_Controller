#!/usr/bin/env python3
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
"""Extract the host-test reference fixtures from the Arduino firmware 2.3 sources.

  extract_ref.py config <config.h>   RF part of config.h (pin, NOVY and fan codes, HOSTNAME, MQTT_PREFIX)
  extract_ref.py sketch <file.ino>   verbatim RF, button and press sections of the sketch

Output goes to stdout. Secrets in config.h are never emitted: the script refuses any line that
mentions a credential.
"""
import re
import sys

HEADER = [
    "// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0",
    "// Extracted by tools/extract_ref.py from the Arduino firmware 2.3 sources; do not edit.",
    "// `make -C tests/host check-sources` compares this file with a fresh extraction.",
]
SECRET = re.compile(r"SSID|PASSWORD|OTAPASSWORD|MQTT_USER|MQTT_SERVER|MQTT_PORT", re.I)


def fail(msg):
    sys.exit("extract_ref.py: " + msg)


def config(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    out = []
    for name in ("HOSTNAME", "MQTT_PREFIX"):
        hits = [l for l in lines if re.match(r"\s*const\s+String\s+%s\s*=" % name, l)]
        if len(hits) != 1:
            fail("%s not found exactly once" % name)
        out.append(hits[0])
    try:
        start = next(i for i, l in enumerate(lines) if l.strip() == "// NOVY Hardware Configuration")
        end = next(i for i, l in enumerate(lines) if i > start and l.startswith("#endif"))
    except StopIteration:
        fail("RF section markers not found")
    out.append("")
    out.extend(lines[start:end])
    while out and not out[-1].strip():
        out.pop()
    for l in out:
        if SECRET.search(l):
            fail("refusing to emit a line that mentions a credential")
    return out


def strip_literals(line):
    """Line without string/char literals and // comments, for brace counting."""
    line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)
    line = re.sub(r"'(?:[^'\\]|\\.)*'", "''", line)
    return line.split("//", 1)[0]


def block(lines, start):
    """Lines from start up to the line that closes the first brace opened at or after start."""
    depth, opened = 0, False
    for i in range(start, len(lines)):
        s = strip_literals(lines[i])
        depth += s.count("{") - s.count("}")
        opened = opened or "{" in s
        if opened and depth == 0:
            return lines[start:i + 1]
    fail("unbalanced block at line %d" % (start + 1))


def find(lines, prefix, after=0):
    for i in range(after, len(lines)):
        if lines[i].strip().startswith(prefix):
            return i
    fail("not found: " + prefix)


def sketch(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    out = []

    def section(title, chunk):
        out.append("")
        out.append("// --- " + title)
        out.extend(chunk)

    section("button commands", block(lines, find(lines, "enum ButtonCommand {")))
    section("fan cycle index", [lines[find(lines, p)] for p in
                                ("#define FAN_CMD_COUNT", "#define FAN_CMD_OFFSET", "int fanCycleIndex[")])
    section("transmitter", [lines[find(lines, "RCSwitch transmitter")]])
    i = find(lines, "// Device state tracking")
    j = find(lines, "static String fanDirection", i)
    section("device state", lines[i + 1:j + 1])
    section("button table", block(lines, find(lines, "struct ButtonConfig {")) +
            block(lines, find(lines, "ButtonConfig buttons[] = {")) +
            [lines[find(lines, "const int BUTTON_COUNT")]])
    decl = find(lines, "// Function declarations")
    section("declarations", [lines[find(lines, p, decl)] for p in (
        "void handleButtonPress(ButtonCommand cmd);",
        "void sendRFCommand(ButtonCommand cmd, int channelIndex = 0);",
        "void sendFanRFCommand(ButtonCommand cmd);",
        "void publishMQTTStatus(String button, String state = \"ON\");",
        "void mqttCallback(char* topic, byte* payload, unsigned int length);",
        "String getButtonNameByCommand(ButtonCommand cmd);",
        "String toLowerCase(String str);")])
    for sig in ("String toLowerCase(String str) {",
                "void handleButtonPress(ButtonCommand cmd) {",
                "String getButtonNameByCommand(ButtonCommand cmd) {",
                "void sendRFCommand(ButtonCommand cmd, int channelIndex) {",
                "void sendFanRFCommand(ButtonCommand cmd) {",
                "void publishMQTTStatus(String button, String state) {",
                "void mqttCallback(char* topic, byte* payload, unsigned int length) {"):
        section(sig.split("(")[0], block(lines, find(lines, sig)))
    setup = find(lines, "void setup() {")
    t = find(lines, "transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);", setup)
    if not (lines[t + 1].strip() == "transmitter.setPulseLength(350);"
            and lines[t + 2].strip() == "transmitter.setProtocol(12);"):
        fail("setup() transmitter lines changed")
    section("setup() transmitter lines, wrapped", ["void sketch_setup_rf() {"] + lines[t:t + 3] + ["}"])
    return out


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("config", "sketch"):
        sys.exit(__doc__)
    body = config(sys.argv[2]) if sys.argv[1] == "config" else sketch(sys.argv[2])
    sys.stdout.write("\n".join(HEADER + [""] + body) + "\n")


if __name__ == "__main__":
    main()
