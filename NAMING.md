# HomeMaster product naming

Read this before writing a product name, a model code or an Internal
Reference anywhere: Odoo, the shop, this repository, documents, labels,
customs paperwork. Never derive a code by analogy with another product.

## Current products

| Product | Full name | Model / Odoo Internal Reference | Printed part no. |
|---|---|---|---|
| OpenTherm Thermostat | HomeMaster Thermostat for OpenTherm | HM-OTT-R1 | OTT-R1 |

## The rule

The model code is `HM-<three letters>-<hardware revision>`. The revision
is the same `R1` that the documents and the silkscreen carry. Documents
print the short form, without the `HM-` prefix.

`HM-OTG-001` and the other sequential `-001` references belong to an
older scheme. Do not extend it to new products.

## Retired, never use

- `HM-OTT-001`
- `HomeMaster Thermostat for OpenTherm boilers`
- `OpenTherm Thermostat` as a product name — it is a description

## Where a name has to agree

The Odoo product card (name, Internal Reference, MPN), the shop URL, the
module README, the datasheet, the user manual, the Declaration of
Conformity, `project.name` in the ESPHome configuration, the ESPHome
device page, the hub card, the label and the packaging.
