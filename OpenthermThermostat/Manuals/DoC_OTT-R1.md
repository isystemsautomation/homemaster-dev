# EU DECLARATION OF CONFORMITY

in accordance with Regulation (EC) No 765/2008 and Decision No 768/2008/EC

**Document No.: DoC-OTT-R1-V1.0**

> **STATUS: DRAFT — NOT TO BE SIGNED OR ISSUED YET.**
> Every field is complete except the conformity evidence in Section 8, which
> does not exist for this product yet. The sections marked **PENDING** must be
> replaced with the real report identifiers before this declaration is signed.
> A declaration signed over evidence that has not been produced is a false
> declaration. See `CE_Technical_File_OTT-R1.md` for what has to be done first.

---

## 1. Manufacturer

| | |
|---|---|
| Company name | ISYSTEMS AUTOMATION S.R.L. |
| Trading as | HomeMaster® |
| Registered office (sediul social) | Str. Domnișori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiești, Jud. Prahova, Romania |
| Office / Contact address | Diligentei 18, Ploiești, Romania |
| CUI / VAT | RO 21537032 |
| EUID / Trade Registry | ROONRC.J2007000919293 |
| EU Trade Mark | HOMEMASTER — EUTM No. 019082911 (EUIPO, registered 15/01/2025) |
| Telephone | +40 747 757 798 |
| Website | https://www.home-master.eu |

## 2. Object of the Declaration

| | |
|---|---|
| Product name | HomeMaster OpenTherm Thermostat |
| Part number | OTT-R1 |
| Hardware version | V1.0 |
| Firmware version | 1.0.0 onwards |
| Cybersecurity conformity (Art. 3(3)(d)) from firmware | 1.0.0 onwards (applies to later versions that do not alter the assessed security mechanisms) |
| Description | ESP32-based wall-mounted room thermostat acting as an OpenTherm master for compatible boilers, with a 2.8″ colour display, rotary encoder and four keys, a 24 GHz presence radar, on-board temperature, humidity and ambient-light sensors, USB-C and Wi-Fi 2.4 GHz connectivity. |
| Power input | 24 V DC nominal (SELV) |
| Radio modules | Pre-certified ESP32 Wi-Fi 2.4 GHz / Bluetooth module; HLK-LD2410C 24 GHz presence radar module |
| Enclosure | Italtronic EYE BOX, ABS, wall mounted |

## 3. Statement of Responsibility

This Declaration of Conformity is issued under the sole responsibility of the
manufacturer named above. The object of the declaration described above is in
conformity with the relevant Union harmonisation legislation listed below.

## 4. Applicable Union Harmonisation Legislation

| Directive / Regulation | Subject |
|---|---|
| Directive 2014/53/EU (RED) | Radio Equipment Directive |
| Directive 2011/65/EU and (EU) 2015/863 (RoHS) | Restriction of Hazardous Substances |

> **Note on the EMC and Low Voltage Directives.** This product is radio
> equipment. Under Article 2(2)(a) of Directive 2014/30/EU and Article 1(2)(f)
> of Directive 2014/35/EU, radio equipment within the scope of Directive
> 2014/53/EU is excluded from both. The corresponding essential requirements are
> satisfied through Article 3.1(b) (electromagnetic compatibility) and
> Article 3.1(a) (health and safety, including the objectives of the Low Voltage
> Directive without a lower voltage limit) of the RED. The product is supplied
> from 24 V DC SELV only and carries no mains circuit.

## 5. Harmonised Standards Applied

Conformity with the legislation listed in Section 4 has been demonstrated
through the application of the following harmonised standards:

| Area | Standards |
|---|---|
| Health and safety — RED Art. 3.1(a) | EN IEC 62368-1:2020 + A11:2020 |
| RF exposure — RED Art. 3.1(a) | EN 62479:2010 (low-power exemption assessment) |
| EMC — RED Art. 3.1(b), general | EN 301 489-1 V2.2.3 |
| EMC — RED Art. 3.1(b), wideband data | EN 301 489-17 V3.2.4 |
| EMC — RED Art. 3.1(b), short-range devices | EN 301 489-3 V2.3.2 |
| Radio spectrum — RED Art. 3.2, 2.4 GHz | EN 300 328 V2.2.2 |
| Radio spectrum — RED Art. 3.2, 24 GHz SRD | EN 305 550-2 V1.2.1 |
| Cybersecurity — RED Art. 3(3)(d) | EN 18031-1:2024 |
| RoHS technical documentation | EN IEC 63000 |

## 6. Radio Modules

The product integrates two pre-certified radio modules.

### 6.1 Wi-Fi / Bluetooth, 2.4 GHz

| | |
|---|---|
| Module model | ESP32-WROOM-32U-N16 |
| Module manufacturer | Espressif Systems (Shanghai) Co., Ltd. |
| EU Type-Examination certificate (Module B) | No. 172141367/AA/02, issued 02 March 2023 |
| Issued by (Notified Body) | Kiwa Nederland B.V. (NB 0063), Wilmersdorf 50, 7327 AC Apeldoorn, The Netherlands |
| Frequency band | 2.4 GHz ISM, 2402–2480 MHz |
| Maximum radiated power | ≤ 20 dBm EIRP |
| Radio standards covered | EN 300 328 V2.2.2 · EN 301 489-1 V2.2.3 · EN 301 489-17 V3.2.4 |

A copy of Espressif's CE certification document for ESP32-WROOM-32U is held in
the technical file maintained by ISYSTEMS AUTOMATION S.R.L.

### 6.2 Presence radar, 24 GHz

| | |
|---|---|
| Module model | HLK-LD2410C |
| Module manufacturer | Shenzhen Hi-Link Electronic Co., Ltd. |
| Frequency band | 24.00–24.25 GHz ISM |
| Maximum radiated power | **PENDING** — take the figure from the module supplier's test report |
| Supplier conformity evidence | **PENDING** — obtain the module's CE test report against EN 305 550-2 and EN 301 489-3 and file it |

> ⚠ The 24 GHz radar is the part of this product most likely to fail a market
> surveillance check. Unlike the ESP32, it carries no notified-body type
> examination. Either obtain the supplier's full test evidence and reference it
> here, or have the finished product tested against EN 305 550-2 and
> EN 301 489-3. Do not sign this declaration on the strength of a supplier CE
> logo alone.

## 7. Conformity Assessment Procedure

For the radio-equipment aspects of the Wi-Fi function, conformity is supported
by the EU type-examination of the integrated module (Module B), carried out by
Kiwa Nederland B.V. (Notified Body No. 0063), certificate No. 172141367/AA/02.

For all other aspects — Article 3.1(a), Article 3.1(b), Article 3.2 as it
applies to the 24 GHz radar, Article 3(3)(d), and RoHS — the conformity
assessment is carried out by the manufacturer using the internal production
control procedure (Module A, Annex II of Directive 2014/53/EU).

Compliance with Article 3(3)(d) of Directive 2014/53/EU, applicable since
1 August 2025 under Delegated Regulation (EU) 2022/30, is established at product
level by the manufacturer under Module A, applying EN 18031-1:2024. Article
3(3)(e) is not declared: whether the data processed by the product falls within
its scope is under legal assessment. Article 3(3)(f) does not apply.

## 8. Additional Information

Technical documentation supporting this declaration is held by ISYSTEMS
AUTOMATION S.R.L. at the address listed in Section 1 and is available to the
relevant authorities on request.

**EMC conformity** is supported by test report **PENDING** — issue number, date
and laboratory. Tests required: radiated and conducted RF emissions, RF field,
conducted RF, EFT/burst and ESD immunity, per EN 301 489-1 and EN 301 489-17.
(The gateway's EMC testing was carried out by Idvorsky Laboratories Ltd.,
Volgina 15, 11060 Belgrade, Serbia; the same laboratory is the obvious route for
this product.)

**Safety conformity under Article 3.1(a)** is supported by in-house assessment
report **PENDING**. The product has no mains circuit and no hazardous voltage;
the assessment covers EN IEC 62368-1 energy-source classification, touch
temperature of the enclosure front, and the single-fault behaviour of the 24 V
input. The dielectric-withstand programme applied to the gateway does not apply
here — there is no insulation barrier to test.

**RF exposure** is addressed by EN 62479 low-power exemption. Record the
calculation in the technical file.

**Cybersecurity** risk assessment and verification records per EN 18031-1 are
held in the technical file as CYB-001 and CYB-002 (to be raised for this
product; the gateway's documents are a usable starting point).

This declaration applies to all units of the product described in Section 2
placed on the market with hardware version V1.0 and remains valid as long as the
product is manufactured without significant modification.

## 9. Signed for and on behalf of the Manufacturer

Place of issue: Ploiești, Romania

Date of issue: _________________

Name: Daniela Drezyulya

Position: Director

Signature: _________________

---

HomeMaster® is a registered European Union trademark (EUTM No. 019082911) of
ISYSTEMS AUTOMATION S.R.L., registered with EUIPO on 15 January 2025.
OpenTherm® is a registered trademark of the OpenTherm Association. All other
trademarks are the property of their respective owners.

*End of Declaration*
