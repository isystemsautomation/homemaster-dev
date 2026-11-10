#!/usr/bin/env python3
"""Insert the HomeMaster Thermostat for OpenTherm card into index.html, after the
OpenTherm Gateway card, and register it in the JSON-LD ItemList.

Usage:  python3 insert.py /path/to/index.html /path/to/thermostat_card.html
Idempotent: refuses to run twice.
"""
import json, re, sys

idx_path, card_path = sys.argv[1], sys.argv[2]
src = open(idx_path, encoding='utf-8').read()
card = open(card_path, encoding='utf-8').read()

if 'id="OpenThermThermostat"' in src:
    sys.exit('already present — nothing to do')

# 1. DOM: the gateway card is the last one in the section; append after it.
start = src.index('<div class="mod" id="OpenThermGateway">')
end = src.index('    </div></div>\n  </section>', start)
# keep the gateway's own closing </div>, then append our card
src = src[:end] + '    </div>' + card + src[end + len('    </div>'):]

# 2. JSON-LD: add the list item and bump the count.
m = re.search(r'<script type="application/ld\+json">(.*?)</script>', src, re.S)
graph = json.loads(m.group(1))
for node in graph['@graph']:
    if node.get('@type') == 'ItemList':
        pos = len(node['itemListElement']) + 1
        node['numberOfItems'] = pos
        node['itemListElement'].append({
            "@type": "ListItem", "position": pos,
            "name": "HomeMaster Thermostat for OpenTherm (OTT-R1)",
            "url": "https://config.home-master.eu/#OpenThermThermostat",
            "item": {
                "@type": "WebPage",
                "@id": "https://config.home-master.eu/#OpenThermThermostat",
                "name": "HomeMaster Thermostat for OpenTherm (OTT-R1)",
                "description": "Wall-mounted OpenTherm master thermostat with colour display, encoder and presence radar.",
                "url": "https://config.home-master.eu/#OpenThermThermostat",
                "image": "https://config.home-master.eu/OpenthermThermostat/Images/thermostat-front.jpg",
                "isPartOf": {"@id": "https://config.home-master.eu/#webpage"},
            },
        })
        break
else:
    sys.exit('ItemList not found')

src = src[:m.start(1)] + json.dumps(graph, ensure_ascii=False, separators=(',', ':')) + src[m.end(1):]
open(idx_path, 'w', encoding='utf-8').write(src)
print('inserted')
