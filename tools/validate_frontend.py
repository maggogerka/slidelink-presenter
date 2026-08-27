#!/usr/bin/env python3
"""Dependency-free release checks for the embedded frontend."""

from collections import Counter
from html.parser import HTMLParser
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


class AuditParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = []
        self.translation_keys = []

    def handle_starttag(self, _tag, attrs):
        values = dict(attrs)
        if "id" in values:
            self.ids.append(values["id"])
        for name in ("data-i18n", "data-i18n-title", "data-i18n-aria"):
            if name in values:
                self.translation_keys.append(values[name])


def fail(message):
    print(f"frontend validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


html = (ROOT / "frontend" / "index.html").read_text(encoding="utf-8")
javascript = (ROOT / "frontend" / "app.js").read_text(encoding="utf-8")
styles = (ROOT / "frontend" / "styles.css").read_text(encoding="utf-8")
parser = AuditParser()
parser.feed(html)

duplicates = [name for name, count in Counter(parser.ids).items() if count > 1]
if duplicates:
    fail(f"duplicate HTML id(s): {', '.join(duplicates)}")
if html.lower().count("<!doctype html>") != 1:
    fail("expected exactly one HTML document")
if "data-lang=\"ru\"" not in html or "data-lang=\"en\"" not in html:
    fail("manual RU | EN language controls are missing")
for key in parser.translation_keys:
    if len(re.findall(rf"['\"]{re.escape(key)}['\"]\s*:", javascript)) != 2:
        fail(f"translation key {key!r} must exist once in RU and once in EN")
for required in ("/api/v1/firmware", "/api/v1/factory-reset",
                 "/api/v1/settings", "192.168.55.1"):
    if required not in javascript and required not in html:
        fail(f"required product feature reference is missing: {required}")
if "�" in html + javascript + styles:
    fail("Unicode replacement character detected")
if not styles.strip() or not javascript.strip():
    fail("embedded asset is empty")

print(f"frontend validation OK: {len(parser.ids)} ids, "
      f"{len(set(parser.translation_keys))} localized static strings")
