"""
Insert two JSON-LD <script> blocks (HowTo + Product) into each product
page immediately after the opening `<div class="hm-plc">` tag and before
`<div class="plc-wrap">`.

Run from the repo root:

    python scripts/add_jsonld_schema.py

The script is idempotent: if the HowTo / Product schema blocks for the
product already exist in the page, the file is left unchanged.
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


EU_COUNTRIES_NON_RO = [
    "AT", "BE", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR",
    "HU", "IE", "IT", "LV", "LT", "LU", "MT", "NL", "PL", "PT", "SK", "SI",
    "ES", "SE",
]
EU_COUNTRIES_ALL = ["RO"] + EU_COUNTRIES_NON_RO


def product_block(product_name: str, product_url: str) -> dict:
    return {
        "@context": "https://schema.org",
        "@type": "Product",
        "@id": f"{product_url}#product",
        "name": product_name,
        "brand": {"@type": "Brand", "name": "HomeMaster"},
        "manufacturer": {
            "@type": "Organization",
            "name": "ISYSTEMS AUTOMATION S.R.L.",
        },
        "url": product_url,
        "offers": {
            "@type": "Offer",
            "url": product_url,
            "priceCurrency": "EUR",
            "shippingDetails": [
                {
                    "@type": "OfferShippingDetails",
                    "shippingRate": {
                        "@type": "MonetaryAmount",
                        "minValue": "4.00",
                        "maxValue": "10.00",
                        "currency": "EUR",
                    },
                    "shippingDestination": {
                        "@type": "DefinedRegion",
                        "addressCountry": "RO",
                    },
                    "deliveryTime": {
                        "@type": "ShippingDeliveryTime",
                        "handlingTime": {
                            "@type": "QuantitativeValue",
                            "minValue": 0,
                            "maxValue": 1,
                            "unitCode": "DAY",
                        },
                        "transitTime": {
                            "@type": "QuantitativeValue",
                            "minValue": 1,
                            "maxValue": 3,
                            "unitCode": "DAY",
                        },
                    },
                },
                {
                    "@type": "OfferShippingDetails",
                    "shippingRate": {
                        "@type": "MonetaryAmount",
                        "minValue": "30.00",
                        "maxValue": "100.00",
                        "currency": "EUR",
                    },
                    "shippingDestination": {
                        "@type": "DefinedRegion",
                        "addressCountry": EU_COUNTRIES_NON_RO,
                    },
                    "deliveryTime": {
                        "@type": "ShippingDeliveryTime",
                        "handlingTime": {
                            "@type": "QuantitativeValue",
                            "minValue": 0,
                            "maxValue": 1,
                            "unitCode": "DAY",
                        },
                        "transitTime": {
                            "@type": "QuantitativeValue",
                            "minValue": 3,
                            "maxValue": 10,
                            "unitCode": "DAY",
                        },
                    },
                },
            ],
            "hasMerchantReturnPolicy": {
                "@type": "MerchantReturnPolicy",
                "applicableCountry": EU_COUNTRIES_ALL,
                "returnPolicyCategory":
                    "https://schema.org/MerchantReturnFiniteReturnWindow",
                "merchantReturnDays": 14,
                "returnMethod": "https://schema.org/ReturnByMail",
                "returnFees": "https://schema.org/ReturnShippingFees",
                "returnShippingFeesAmount": {
                    "@type": "MonetaryAmount",
                    "value": "0",
                    "currency": "EUR",
                },
            },
        },
    }


def render_script(obj: dict, indent: str = "  ") -> str:
    body = json.dumps(obj, indent=2, ensure_ascii=False)
    body_indented = "\n".join(indent + line for line in body.splitlines())
    return (
        f'{indent}<script type="application/ld+json">\n'
        f"{body_indented}\n"
        f"{indent}</script>"
    )


def build_blocks(product_code: str, product_name: str,
                 product_type: str, product_url: str) -> str:
    if product_type == "controller":
        howto = howto_controller(product_code)
    elif product_type == "module":
        howto = howto_module(product_code)
    else:
        raise ValueError(f"Unknown type: {product_type}")
    product = product_block(product_name, product_url)
    return render_script(howto) + "\n" + render_script(product)


INSERT_PATTERN = re.compile(
    r'(<div class="hm-plc">)(\s*\n)(\s*)(<div class="plc-wrap">)'
)

# Marker used to detect existing insertion (re-entrance / idempotency).
EXISTING_MARKER = re.compile(
    r'<script type="application/ld+json">\s*\{[^<]*"@type"\s*:\s*"HowTo"',
    re.DOTALL,
)


def process_file(rel_path: str, code: str, name: str,
                 ptype: str, url: str) -> str:
    abs_path = ROOT / rel_path
    if not abs_path.exists():
        return f"MISSING: {rel_path}"

    text = abs_path.read_text(encoding="utf-8")

    if EXISTING_MARKER.search(text):
        return f"SKIP (already has JSON-LD): {rel_path}"

    blocks = build_blocks(code, name, ptype, url)

    def repl(m: re.Match) -> str:
        return f"{m.group(1)}\n{blocks}\n{m.group(3)}{m.group(4)}"

    new_text, n = INSERT_PATTERN.subn(repl, text, count=1)
    if n != 1:
        return f"NO MATCH: {rel_path}"

    abs_path.write_text(new_text, encoding="utf-8", newline="\n")
    return f"OK: {rel_path}"


def validate_jsonld_in_file(rel_path: str) -> list[str]:
    errors: list[str] = []
    abs_path = ROOT / rel_path
    if not abs_path.exists():
        return [f"MISSING: {rel_path}"]
    text = abs_path.read_text(encoding="utf-8")
    pattern = re.compile(
        r'<script type="application/ld\+json">(.*?)</script>',
        re.DOTALL,
    )
    for i, m in enumerate(pattern.finditer(text), 1):
        body = m.group(1).strip()
        try:
            json.loads(body)
        except json.JSONDecodeError as e:
            errors.append(f"{rel_path} block {i}: {e}")
    return errors


def main() -> int:
    print("== Inserting JSON-LD blocks ==")
    for rel_path, code, name, ptype, url in PRODUCTS:
        print(process_file(rel_path, code, name, ptype, url))

    print("\n== Validating JSON-LD ==")
    all_errors: list[str] = []
    for rel_path, *_ in PRODUCTS:
        all_errors.extend(validate_jsonld_in_file(rel_path))
    if all_errors:
        for err in all_errors:
            print(f"  ERROR: {err}")
        return 1
    print("All JSON-LD blocks parse cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
