# Security Policy

ISYSTEMS AUTOMATION S.R.L. (HomeMaster®) takes the security of its products
seriously. This document explains how to report a vulnerability and what to
expect in return.

## Reporting a vulnerability

**Email:** support@home-master.eu
**PGP key:** https://www.home-master.eu/.well-known/pgp-key.asc
**Or:** GitHub private vulnerability reporting on this repository

Please include, where you can:

- affected product and hardware version (e.g. MiniPLC V1.0)
- firmware version — shown in the ESPHome logs at boot and in Home Assistant
- what the issue is and what it allows
- how to reproduce it
- whether, to your knowledge, it is being exploited

Please do not open a public issue for a security matter.

## What to expect

| Stage | Target |
|---|---|
| We acknowledge your report | 3 working days |
| We give an initial assessment and severity | 10 working days |
| We keep you updated while we work | every 20 working days |
| We ship a fix or a documented mitigation | per severity, below |

| Severity | Target |
|---|---|
| Critical — remote code execution, or unauthenticated control of a relay switching mains | 30 days |
| High — authentication bypass, credential exposure | 60 days |
| Medium | 90 days |
| Low | next release |

We work to coordinated disclosure and will credit you in the advisory unless you
ask us not to.

## Products in scope

All HomeMaster modules and their firmware:

AIO-422-R1 · ALM-173-R1 · DIM-420-R1 · DIO-430-R1 · ENM-223-R1 · MicroPLC ·
MiniPLC · OpenTherm Gateway · RGB-621-R1 · STR-3221-R1 · WLD-521-R1

Also the WebConfig pages at config.home-master.eu and the firmware update
manifests served from this repository.

Out of scope: Home Assistant, ESPHome and other third-party software — report
those upstream. We forward anything that reaches us by mistake.

## Support period

Security updates are provided for each module for **10 years** from the date the
last unit of that hardware version was placed on the market. This reflects the
service life of DIN-rail equipment in a fixed installation.

## Advisories

Published at
https://github.com/isystemsautomation/HOMEMASTER/security/advisories
and mirrored at https://www.home-master.eu/security

## Regulatory reporting

For products made available on the EU market, actively exploited vulnerabilities
and severe security incidents are reported to ENISA and to the Romanian national
CSIRT (DNSC) through the Single Reporting Platform, under Article 14 of Regulation
(EU) 2024/2847 (Cyber Resilience Act), from 11 September 2026.

## Deploying HomeMaster modules securely

These modules are meant to be installed in a fixed electrical installation by a
qualified person. To run them safely:

- put them on a network segment separate from general and guest devices — a
  dedicated VLAN, for instance
- do not expose a module directly to the internet; do not forward ports to one
- complete the ESPHome adoption ("Take control") after Wi-Fi provisioning — this
  sets the per-device API encryption key and OTA password
- do the initial Wi-Fi provisioning somewhere controlled: the BLE window is open
  until the device first joins a network
- keep firmware current; updates are offered automatically in Home Assistant

Per-module detail is in each User Manual.

---

*Last reviewed: 2026-07-25 · Contact: support@home-master.eu*
