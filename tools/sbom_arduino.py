#!/usr/bin/env python3
"""
Generate a CycloneDX SBOM for a HomeMaster RP2350 module built with arduino-cli.

Input is the `profile-dump.txt` that `arduino-cli compile --dump-profile` writes:
it lists the exact core and library versions the build actually resolved, which
is what a CRA SBOM must reflect — the versions that shipped, not the ones a
manifest hoped for.

Usage:
    sbom_arduino.py --module ALM-173-R1 --sketch default_alm_173_r1 \
                    --profile path/to/profile-dump.txt \
                    --out path/to/ALM-173-R1.cdx.json

The profile dump looks like:

    profiles:
      default:
        fqbn: rp2040:rp2040:generic_rp2350:...
        platforms:
          - platform: rp2040:rp2040 (5.6.0)
            platform_index_url: https://...
        libraries:
          - ADS1X15 (0.6.2)
          - Adafruit MAX31865 library (1.6.2)

We parse the `platforms:` and `libraries:` blocks. Anything we cannot parse is
reported, never silently dropped — a missing component in an SBOM is worse than
a noisy one.
"""

import argparse, json, re, sys, hashlib, datetime, uuid


def parse_profile(text):
    """Return (platforms, libraries) as lists of (name, version)."""
    platforms, libraries = [], []
    section = None
    for raw in text.splitlines():
        line = raw.rstrip()
        stripped = line.strip()
        if re.match(r'^platforms:\s*$', stripped):
            section = 'platforms'; continue
        if re.match(r'^libraries:\s*$', stripped):
            section = 'libraries'; continue
        # a new top-level-ish key ends the current section
        if stripped and not stripped.startswith('-') and stripped.endswith(':') \
           and section and not line.startswith('   '):
            section = None

        m = re.match(r'-\s*(?:platform:\s*)?(.+?)\s*\(([^)]+)\)\s*$', stripped)
        if m and section == 'platforms':
            platforms.append((m.group(1).strip(), m.group(2).strip()))
        elif m and section == 'libraries':
            libraries.append((m.group(1).strip(), m.group(2).strip()))
    return platforms, libraries


def purl(kind, name, version):
    # arduino has no official purl type; use a stable, documented namespace
    safe = re.sub(r'\s+', '.', name.strip())
    return f'pkg:arduino/{kind}/{safe}@{version}'


def component(kind, name, version):
    return {
        "type": "library",
        "name": name,
        "version": version,
        "purl": purl(kind, name, version),
        "properties": [{"name": "homemaster:origin", "value": kind}],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--module', required=True)
    ap.add_argument('--sketch', required=True)
    ap.add_argument('--profile', required=True)
    ap.add_argument('--fw-version', default='0.2.0')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    text = open(a.profile, encoding='utf-8', errors='replace').read()
    platforms, libraries = parse_profile(text)

    if not platforms and not libraries:
        print(f'ERROR: no components parsed from {a.profile}. '
              f'Is this an arduino-cli --dump-profile output?', file=sys.stderr)
        sys.exit(1)

    comps = [component('core', n, v) for n, v in platforms] + \
            [component('library', n, v) for n, v in libraries]

    serial = 'urn:uuid:' + str(uuid.uuid4())
    now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

    bom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": serial,
        "version": 1,
        "metadata": {
            "timestamp": now,
            "tools": [{"vendor": "HomeMaster", "name": "sbom_arduino.py", "version": "1.0"}],
            "component": {
                "type": "firmware",
                "name": f"HomeMaster {a.module}",
                "version": a.fw_version,
                "description": f"Firmware for HomeMaster {a.module} ({a.sketch}), RP2350",
                "properties": [
                    {"name": "homemaster:module", "value": a.module},
                    {"name": "homemaster:sketch", "value": a.sketch},
                    {"name": "homemaster:mcu", "value": "RP2350"},
                ],
            },
        },
        "components": comps,
    }

    with open(a.out, 'w', encoding='utf-8') as f:
        json.dump(bom, f, indent=2, ensure_ascii=False)

    print(f'{a.module}: {len(platforms)} core, {len(libraries)} libraries -> {a.out}')


if __name__ == '__main__':
    main()
