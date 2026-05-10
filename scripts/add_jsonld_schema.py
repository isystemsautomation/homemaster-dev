"""
Manage HowTo JSON-LD for product pages inside Odoo's s_embed_code snippet.

- Migrates pages to: one <section class="s_embed_code"> containing
  <template class="s_embed_code_saved"> and
  <div class="s_embed_code_embedded ...">, each with an identical HowTo
  <script type="application/ld+json"> block.
- Removes supplementary Product JSON-LD (the block with @type Product and
  @id ending in #product) to avoid Google "missing price" issues.

Run from the repo root:

    python scripts/add_jsonld_schema.py

Idempotent: skips files that already have exactly two JSON-LD scripts,
both HowTo, identical bodies, and s_embed_code present.

Legacy mode (bare scripts after hm-plc): if a file has no Product block
but still uses bare scripts, migration wraps the HowTo in s_embed_code.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# (relative path, product code, product name, type, url)
PRODUCTS: list[tuple[str, str, str, str, str]] = [
    ("AIO-422-R1/productpage/productpage.html",
     "AIO-422-R1", "HomeMaster AIO-422-R1", "module",
     "https://www.home-master.eu/shop/aio-422-r1-721"),
    ("ALM-173-R1/productpage/productpage.html",
     "ALM-173-R1", "HomeMaster ALM-173-R1", "module",
     "https://www.home-master.eu/shop/alm-173-r1-719"),
    ("DIM-420-R1/productpage/productpage.html",
     "DIM-420-R1", "HomeMaster DIM-420-R1", "module",
     "https://www.home-master.eu/shop/dim-420-r1-722"),
    ("DIO-430-R1/productpage/productpage.html",
     "DIO-430-R1", "HomeMaster DIO-430-R1", "module",
     "https://www.home-master.eu/shop/dio-430-r1-relay-module-58"),
    ("ENM-223-R1/productpage/productpage.html",
     "ENM-223-R1", "HomeMaster ENM-223-R1", "module",
     "https://www.home-master.eu/shop/enm-223-r1-723"),
    ("MicroPLC/productpage/productpage.html",
     "MicroPLC", "HomeMaster MicroPLC", "controller",
     "https://www.home-master.eu/shop/microplc-56"),
    ("MiniPLC/ProductPage/productpage.html",
     "MiniPLC", "HomeMaster MiniPLC", "controller",
     "https://www.home-master.eu/shop/miniplc-55"),
    ("OpenthermGateway/productpage/productpage.html",
     "OpenthermGateway", "HomeMaster OpenTherm Gateway", "controller",
     "https://www.home-master.eu/shop/opentherm-gateway-59"),
    ("RGB-621-R1/productpage/productpage.html",
     "RGB-621-R1", "HomeMaster RGB-621-R1", "module",
     "https://www.home-master.eu/shop/rgb-621-r1-57"),
    ("STR-3221-R1/productpage/productpage.html",
     "STR-3221-R1", "HomeMaster STR-3221-R1", "module",
     "https://www.home-master.eu/shop/str-3221-r1-stair-leds-module-66"),
    ("WLD-521-R1/productpage/productpage.html",
     "WLD-521-R1", "HomeMaster WLD-521-R1", "module",
     "https://www.home-master.eu/shop/wld-521-r1-1021"),
]

SCRIPT_LD_JSON = re.compile(
    r'<script type="application/ld\+json">\s*(.*?)\s*</script>',
    re.DOTALL,
)

# Inner content of <div class="hm-plc"> through opening <div class="plc-wrap">
HM_PLC_INNER = re.compile(
    r'(<div class="hm-plc">)\s*([\s\S]*?)(\s*<div class="plc-wrap">)',
)


def howto_controller(product_code: str) -> dict:
    return {
        "@context": "https://schema.org",
        "@type": "HowTo",
        "name": f"How to set up HomeMaster {product_code}",
        "description": (
            f"Quick setup of HomeMaster {product_code} via Improv Wi-Fi "
            "and Home Assistant."
        ),
        "totalTime": "PT5M",
        "step": [
            {"@type": "HowToStep", "position": 1,
             "name": "Mount and Power",
             "text": "Install on 35 mm DIN rail and connect 24 V DC supply."},
            {"@type": "HowToStep", "position": 2,
             "name": "Open Improv",
             "text": "Go to improv-wifi.com in Chrome or Edge."},
            {"@type": "HowToStep", "position": 3,
             "name": "Connect Device",
             "text": "Connect via USB-C (Serial) or Bluetooth LE."},
            {"@type": "HowToStep", "position": 4,
             "name": "Enter Wi-Fi",
             "text": "Enter SSID and password, press Connect."},
            {"@type": "HowToStep", "position": 5,
             "name": "Auto-Discovery",
             "text": "Device appears in Home Assistant and ESPHome Dashboard."},
        ],
    }


def howto_module(product_code: str) -> dict:
    return {
        "@context": "https://schema.org",
        "@type": "HowTo",
        "name": f"How to configure HomeMaster {product_code}",
        "description": (
            f"Configure HomeMaster {product_code} via USB-C WebConfig and "
            "integrate via Modbus RTU."
        ),
        "totalTime": "PT5M",
        "step": [
            {"@type": "HowToStep", "position": 1,
             "name": "Mount and Power",
             "text": "Install on 35 mm DIN rail and connect 24 V DC supply."},
            {"@type": "HowToStep", "position": 2,
             "name": "Connect USB-C",
             "text": "Connect USB-C cable from the module to a PC."},
            {"@type": "HowToStep", "position": 3,
             "name": "Open WebConfig",
             "text": "Open WebConfig page in Chrome or Edge browser."},
            {"@type": "HowToStep", "position": 4,
             "name": "Configure Modbus",
             "text": "Set Modbus address (1-255) and baud rate (9600-115200)."},
            {"@type": "HowToStep", "position": 5,
             "name": "Map I/O",
             "text": "Configure input, output, LED and button behavior."},
            {"@type": "HowToStep", "position": 6,
             "name": "Save and Disconnect",
             "text": "Save settings to flash and disconnect USB-C."},
        ],
    }


def render_howto_script(obj: dict, indent: str = "  ") -> str:
    body = json.dumps(obj, indent=2, ensure_ascii=False)
    body_indented = "\n".join(indent + line for line in body.splitlines())
    return (
        f'{indent}<script type="application/ld+json">\n'
        f"{body_indented}\n"
        f"{indent}</script>"
    )


def indent_lines(s: str, n: int) -> str:
    pad = " " * n
    return "\n".join(pad + line for line in s.splitlines())


def build_embed_section(howto_element: str) -> str:
    """howto_element: full <script>...</script> substring."""
    ht = indent_lines(howto_element.rstrip("\n"), 6)
    return (
        '  <section class="s_embed_code text-center pt16 pb16 o_colored_level" '
        'data-snippet="s_embed_code" data-name="Embed Code">\n'
        '    <template class="s_embed_code_saved">\n'
        f"{ht}\n"
        "    </template>\n"
        '    <div class="s_embed_code_embedded container o_not_editable">\n'
        f"{ht}\n"
        "    </div>\n"
        "  </section>"
    )


def extract_howto_script_element(middle: str) -> tuple[str | None, str]:
    """Return (script_element, status)."""
    howto_bodies: list[str] = []
    howto_elements: list[str] = []
    for scr in SCRIPT_LD_JSON.finditer(middle):
        body = scr.group(1).strip()
        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            return None, "invalid_json_ld"
        t = data.get("@type")
        if t == "Product":
            iid = data.get("@id") or ""
            if isinstance(iid, str) and iid.endswith("#product"):
                continue
            return None, "unexpected_product_shape"
        if t == "HowTo":
            howto_bodies.append(body)
            howto_elements.append(scr.group(0))
        else:
            return None, f"unexpected_type:{t}"

    if not howto_elements:
        return None, "no_howto"
    if len(set(howto_bodies)) != 1:
        return None, "howto_bodies_differ"
    return howto_elements[0].rstrip("\n"), "ok"


def needs_embed_indent_fix(text: str) -> bool:
    """Script flush-left right after template / embedded open (broken strip)."""
    return bool(
        re.search(
            r'(?:s_embed_code_saved"|s_embed_code_embedded[^\n]*)\s*>\s*\n'
            r'<script type="application/ld\+json">',
            text,
        )
    )


def is_expected_post_state(text: str) -> bool:
    if "s_embed_code" not in text or "s_embed_code_saved" not in text:
        return False
    if needs_embed_indent_fix(text):
        return False
    matches = list(SCRIPT_LD_JSON.finditer(text))
    if len(matches) != 2:
        return False
    bodies = [m.group(1).strip() for m in matches]
    if bodies[0] != bodies[1]:
        return False
    for b in bodies:
        try:
            if json.loads(b).get("@type") != "HowTo":
                return False
        except json.JSONDecodeError:
            return False
    return True


def migrate_hm_plc_jsonld(text: str) -> tuple[str, str]:
    """
    Replace hm-plc inner (before plc-wrap) with s_embed_code + dual HowTo.
    Idempotent when is_expected_post_state(text).
    """
    if is_expected_post_state(text):
        return text, "SKIP_OK"

    m = HM_PLC_INNER.search(text)
    if not m:
        return text, "NO_HM_PLC_MATCH"

    middle = m.group(2)
    howto_el, st = extract_howto_script_element(middle)
    if howto_el is None:
        return text, st

    embed = build_embed_section(howto_el)
    new_text = (
        text[: m.start()]
        + m.group(1)
        + "\n"
        + embed
        + m.group(3)
        + text[m.end() :]
    )
    return new_text, "MIGRATED"


def seed_howto_only(rel_path: str, code: str, ptype: str) -> str:
    """
    Insert HowTo-only embed when hm-plc has no JSON-LD yet (matches plc-wrap
    directly after hm-plc open).
    """
    abs_path = ROOT / rel_path
    if not abs_path.exists():
        return f"MISSING: {rel_path}"

    text = abs_path.read_text(encoding="utf-8")
    if is_expected_post_state(text):
        return f"SKIP (already migrated): {rel_path}"

    if SCRIPT_LD_JSON.search(text):
        return f"SKIP (has other ld+json): {rel_path}"

    if ptype == "controller":
        howto = howto_controller(code)
    elif ptype == "module":
        howto = howto_module(code)
    else:
        return f"BAD_TYPE: {rel_path}"

    script_el = render_howto_script(howto)
    embed = build_embed_section(script_el)

    def repl(match: re.Match[str]) -> str:
        return f"{match.group(1)}\n{embed}{match.group(2)}"

    pattern = re.compile(
        r'(<div class="hm-plc">)(\s*<div class="plc-wrap">)',
        re.DOTALL,
    )
    new_text, n = pattern.subn(repl, text, count=1)
    if n != 1:
        return f"NO_MATCH: {rel_path}"

    abs_path.write_text(new_text, encoding="utf-8", newline="\n")
    return f"SEED: {rel_path}"


def process_file(rel_path: str) -> str:
    abs_path = ROOT / rel_path
    if not abs_path.exists():
        return f"MISSING: {rel_path}"

    text = abs_path.read_text(encoding="utf-8")
    new_text, status = migrate_hm_plc_jsonld(text)
    if status == "SKIP_OK":
        return f"SKIP: {rel_path}"
    if status != "MIGRATED":
        return f"FAIL {status}: {rel_path}"

    abs_path.write_text(new_text, encoding="utf-8", newline="\n")
    return f"OK: {rel_path}"


def validate_jsonld_product_pages() -> list[str]:
    errors: list[str] = []
    for rel_path, *_ in PRODUCTS:
        abs_path = ROOT / rel_path
        if not abs_path.exists():
            errors.append(f"MISSING: {rel_path}")
            continue
        text = abs_path.read_text(encoding="utf-8")
        if not is_expected_post_state(text):
            errors.append(f"NOT_EXPECTED_STATE: {rel_path}")
            continue
        matches = list(SCRIPT_LD_JSON.finditer(text))
        bodies = [m.group(1).strip() for m in matches]
        if len(matches) != 2 or bodies[0] != bodies[1]:
            errors.append(f"HOWTO_MISMATCH: {rel_path}")
            continue
        for i, b in enumerate(bodies, 1):
            try:
                json.loads(b)
            except json.JSONDecodeError as e:
                errors.append(f"{rel_path} block {i}: {e}")
    return errors


def main() -> int:
    print("== Migrate: remove Product JSON-LD, wrap HowTo in s_embed_code ==")
    for rel_path, *_ in PRODUCTS:
        print(process_file(rel_path))

    print("\n== Validating JSON-LD (2x HowTo, identical, parse) ==")
    errors = validate_jsonld_product_pages()
    if errors:
        for err in errors:
            print(f"  ERROR: {err}")
        return 1
    print("All product pages match expected state; JSON parses cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
