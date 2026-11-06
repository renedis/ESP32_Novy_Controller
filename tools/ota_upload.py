#!/usr/bin/env python3
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
"""Upload a firmware image to /api/ota and wait until the device runs it from the other slot.

usage: ota_upload.py [--force] <host> <firmware.bin>
  --force   accept an image of another project (for example the Arduino firmware)
Run it on a computer in the same network as the device.
"""
import json
import sys
import time
import urllib.error
import urllib.request


def status(host):
    with urllib.request.urlopen("http://%s/api/status" % host, timeout=5) as r:
        return json.load(r)


def main():
    args = [a for a in sys.argv[1:] if a != "--force"]
    force = len(args) != len(sys.argv) - 1
    if len(args) != 2:
        sys.exit(__doc__)
    host, path = args
    before = status(host)["partition"]
    with open(path, "rb") as f:
        data = f.read()
    url = "http://%s/api/ota%s" % (host, "?force=1" if force else "")
    req = urllib.request.Request(url, data=data, method="POST",
                                 headers={"Content-Type": "application/octet-stream"})
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            print("upload:", r.read().decode().strip())
    except urllib.error.HTTPError as e:
        sys.exit("upload refused (%d): %s" % (e.code, e.read().decode().strip()))

    time.sleep(5)
    deadline = time.time() + 120
    while time.time() < deadline:
        try:
            s = status(host)
            if s["partition"] != before:
                print("running %s from %s (was %s)" % (s["version"], s["partition"], before))
                return
        except OSError:
            pass
        time.sleep(2)
    sys.exit("device did not come back on a new slot (still %s)" % before)


if __name__ == "__main__":
    main()
