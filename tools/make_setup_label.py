#!/usr/bin/env python3
"""Capture a first-run credential over USB and create a printable label."""

import argparse
import base64
from html import escape
from io import BytesIO
import json
from pathlib import Path
import sys
from urllib.request import ProxyHandler, build_opener


def qr_escape(value):
    return "".join(("\\" + char) if char in "\\;,:" else char for char in value)


parser = argparse.ArgumentParser()
parser.add_argument("--url", default="http://192.168.55.1")
parser.add_argument("--output", type=Path, default=Path("provisioning-output/setup-label.html"))
parser.add_argument("--device")
parser.add_argument("--ssid")
parser.add_argument("--credential")
parser.add_argument("--require-qr", action="store_true")
args = parser.parse_args()

if args.device and args.ssid and args.credential:
    data = {"device": args.device, "ssid": args.ssid,
            "credential": args.credential, "usb_url": "http://192.168.55.1"}
else:
    endpoint = args.url.rstrip("/") + "/api/v1/setup-credential"
    with build_opener(ProxyHandler({})).open(endpoint, timeout=10) as response:
        data = json.load(response)

wifi_payload = (f"WIFI:T:WPA;S:{qr_escape(data['ssid'])};"
                f"P:{qr_escape(data['credential'])};;")
qr_data_uri = ""
try:
    import qrcode
    image = qrcode.make(wifi_payload)
    buffer = BytesIO()
    image.save(buffer, format="PNG")
    qr_data_uri = "data:image/png;base64," + base64.b64encode(buffer.getvalue()).decode()
except ImportError:
    if args.require_qr:
        print('QR support is missing. Install with: python -m pip install "qrcode[pil]"',
              file=sys.stderr)
        raise SystemExit(2)

qr_html = (f'<img alt="Wi-Fi setup QR" src="{qr_data_uri}">'
           if qr_data_uri else '<p class="warning">QR module is not installed</p>')
document = f"""<!doctype html><html><meta charset="utf-8"><title>SlideLink setup label</title>
<style>body{{font:16px system-ui;margin:0}}.label{{width:86mm;min-height:54mm;padding:6mm;
border:1px dashed #555;display:grid;grid-template-columns:1fr 30mm;gap:4mm}}
h1,p{{margin:0 0 2mm}}code{{font-weight:700}}img{{width:30mm;height:30mm;image-rendering:pixelated}}
.small{{font-size:10px}}.warning{{font-size:9px;color:#900}}@media print{{.label{{border:0}}}}</style>
<section class="label"><div><h1>SlideLink</h1><p>{escape(data['device'])}</p>
<p>Wi-Fi: <code>{escape(data['ssid'])}</code></p>
<p>Password: <code>{escape(data['credential'])}</code></p>
<p class="small">USB: {escape(data['usb_url'])}<br>Open / Откройте: slidelink.local</p></div>{qr_html}</section></html>"""
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(document, encoding="utf-8")
print(f"wrote {args.output.resolve()}")
if not qr_data_uri:
    print('Install QR support with: python -m pip install "qrcode[pil]"')
