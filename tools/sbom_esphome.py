#!/usr/bin/env python3
"""
Generate a CycloneDX SBOM for a HomeMaster ESP32 module built with ESPHome.

Unlike the Arduino line, these are configured in YAML and built by ESPHome on top
of ESP-IDF. The authoritative version list is what `esphome compile` resolves:
the ESPHome release, the ESP-IDF / framework version, and every external
component and library the YAML pulls in.

Two input modes:

1. --manifest <compile_commands or idf build manifest>  (preferred, exact)
   If the build directory has been produced, pass the ESPHome build manifest
   (.esphome/build/<name>/... ) so versions come from what was actually compiled.

2. --yaml <config.yaml> --esphome-version <x> --framework-version <y>  (fallback)
   Parse the YAML for `esphome.libraries`, `external_components`, and known
   platform components, and take the ESPHome/framework versions from the args
   (which the CI knows because it pins them).

Mode 2 is what the CI uses, because it runs before a full IDF build and the
pinned versions are known up front. Anything not parseable is reported.

Usage (CI, mode 2):
    sbom_esphome.py --module MiniPLC --yaml MiniPLC/Firmware/miniplc.yaml \
        --esphome-version 2026.7.0 --framework-version 5.3.1 \
        --fw-version 1.2.0 --out MiniPLC.cdx.json
"""

import argparse, json, re, sys, uuid, datetime

try:
    import yaml
except ImportError:
    yaml = None


def load_yaml(path):
    if yaml is None:
        print('ERROR: pyyaml not available; install pyyaml', file=sys.stderr)
        sys.exit(1)
    with open(path, encoding='utf-8') as f:
        return yaml.safe_load(f)


def purl_pypi(name, version):
    return f'pkg:pypi/{name}@{version}'


def comp(ctype, name, version, origin, purl=None):
    c = {"type": ctype, "name": name, "version": version,
         "properties": [{"name": "homemaster:origin", "value": origin}]}
    if version:
        if purl:
            c["purl"] = purl
    else:
        c["version"] = "unspecified"
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--module', required=True)
    ap.add_argument('--yaml', required=True)
    ap.add_argument('--esphome-version', required=True)
    ap.add_argument('--framework-version', default='')
    ap.add_argument('--fw-version', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    cfg = load_yaml(a.yaml) or {}
    comps = []
    unresolved = []

    # ESPHome itself
    comps.append(comp('application', 'esphome', a.esphome_version, 'esphome',
                      purl=purl_pypi('esphome', a.esphome_version)))

    # ESP-IDF / framework
    if a.framework_version:
        comps.append(comp('framework', 'esp-idf', a.framework_version, 'framework'))
    else:
        # try to read it from the yaml esp32.framework block
        fw = (cfg.get('esp32') or {}).get('framework') or {}
        v = fw.get('version')
        if v:
            comps.append(comp('framework', 'esp-idf', str(v), 'framework'))
        else:
            unresolved.append('esp-idf framework version (not in args or yaml)')

    # esphome.libraries — arbitrary Arduino/PlatformIO libs the config declares
    esph = cfg.get('esphome') or {}
    for lib in esph.get('libraries', []) or []:
        s = str(lib)
        m = re.match(r'(.+?)[@=]([\w.\-+]+)\s*$', s)
        if m:
            comps.append(comp('library', m.group(1).strip(), m.group(2), 'esphome.libraries'))
        else:
            comps.append(comp('library', s.strip(), '', 'esphome.libraries'))
            unresolved.append(f'library without pinned version: {s}')

    # external_components — from git/local sources
    for ec in cfg.get('external_components', []) or []:
        src = ec.get('source', {})
        if isinstance(src, str):
            name, ver = src, ''
        else:
            name = src.get('url') or src.get('path') or 'external_component'
            ver = src.get('ref', '')
        comps.append(comp('library', str(name), str(ver), 'external_components'))
        if not ver:
            unresolved.append(f'external_component without ref: {name}')

    # note the presence of core platform components that carry security weight
    for key in ('api', 'ota', 'provisioning', 'web_server', 'wifi', 'captive_portal'):
        if key in cfg:
            comps.append(comp('library', f'esphome:{key}', a.esphome_version,
                              'esphome.component'))

    serial = 'urn:uuid:' + str(uuid.uuid4())
    now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

    bom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": serial,
        "version": 1,
        "metadata": {
            "timestamp": now,
            "tools": [{"vendor": "HomeMaster", "name": "sbom_esphome.py", "version": "1.0"}],
            "component": {
                "type": "firmware",
                "name": f"HomeMaster {a.module}",
                "version": a.fw_version,
                "description": f"Firmware for HomeMaster {a.module}, ESP32 / ESPHome",
                "properties": [
                    {"name": "homemaster:module", "value": a.module},
                    {"name": "homemaster:mcu", "value": "ESP32"},
                    {"name": "homemaster:framework", "value": "esphome"},
                ],
            },
        },
        "components": comps,
    }

    with open(a.out, 'w', encoding='utf-8') as f:
        json.dump(bom, f, indent=2, ensure_ascii=False)

    print(f'{a.module}: {len(comps)} components -> {a.out}')
    if unresolved:
        print(f'{a.module}: {len(unresolved)} unresolved:', file=sys.stderr)
        for u in unresolved:
            print(f'  - {u}', file=sys.stderr)


if __name__ == '__main__':
    main()
