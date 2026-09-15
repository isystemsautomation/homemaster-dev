# HomeMaster partner kit

Public marketing and compliance assets for authorised resellers of HomeMaster® DIN-rail modules. No prices or stock status are included.

Version **2026.09** — see [CHANGELOG.md](./CHANGELOG.md). Full download: [homemaster-partner-kit-2026.09.zip](./homemaster-partner-kit-2026.09.zip).

## Audience

Shop operators and integrators who resell genuine HomeMaster hardware and need ready-made listing text, images and compliance PDFs.

## Contents

- [`HomeMaster_Catalogue.pdf`](./HomeMaster_Catalogue.pdf) — full product catalogue: one A4 page per module with purpose, specifications and what makes it different, plus partner terms and compliance summary. Send this to a distributor who asks for a technical catalogue.
- `index.csv` — product codes, GTIN, MPN and canonical shop URLs
- `<MODULE>/description-short.md` — card listing text (40–60 words)
- `<MODULE>/description-long.md` — extended product description (250–350 words)
- `<MODULE>/specs.csv` — flat specification table for shop import
- `<MODULE>/images/` — WebP product photography and diagrams
- `<MODULE>/doc/` — EU Declaration of Conformity, datasheet and user manual (where available)
- `claims-notice.md` — binding limits on public statements

### Listing images

Slots `02-listing-1`, `03-listing-2` and `04-listing-3` for modules that use `mplisting*.png` are **shared line marketing images**, not photographs of the specific module in that folder. RGB-621-R1 and DIM-420-R1 use alternate sources for slots 02 and 03; slot 04 is omitted where no suitable file exists.

## Usage rules

- Materials may be used to market and resell genuine HomeMaster® modules.
- **HomeMaster®** is a registered EU trade mark (EUTM 019082911). Use it only to identify the product — not in your store name, domain or logo.
- Do not recolour product images, alter branding or imply UL, CSA or FCC certification.
- Do not use the phrase “Works with Home Assistant”. Integration may be described as “works with Home Assistant via ESPHome” where applicable.
- **Made for ESPHome** may be stated only for MiniPLC and OpenTherm Gateway.
- Rewriting descriptions in your own words is recommended so your listing does not compete directly with home-master.eu in search results.
- CE marking statements refer to the EU Declaration of Conformity supplied in `doc/`.
- The catalogue PDF may be forwarded to customers as it is, but not edited or rebranded.

## Image map

### MiniPLC
- `Images/miniplc_1600x1600.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/dimension.png` → `images/06-dimensions.webp`
- `system_block_diagram.png` → `images/07-block-diagram.webp`

### MicroPLC
- `Images/microplc_1600x1600.png` → `images/01-main.webp`
- `Images/mplisting1_1.png` → `images/02-listing-1.webp`
- `Images/mplisting2_1.png` → `images/03-listing-2.webp`
- `Images/mplisting3_1.png` → `images/04-listing-3.webp`
- `Images/dimensions.png` → `images/06-dimensions.webp`
- `Images/diagram.png` → `images/07-block-diagram.webp`

### OpenthermGateway
- `Images/opentherm_render.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/OpenTherm_SystemBlock.png` → `images/07-block-diagram.webp`

### ALM-173-R1
- `Images/ALM_2000x2000.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/ALMMDimensions.png` → `images/06-dimensions.webp`
- `Images/ALM_SystemBlockDiagram.png` → `images/07-block-diagram.webp`

### STR-3221-R1
- `Images/photo1.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/STR-3221-R1 Dimensions.png` → `images/06-dimensions.webp`
- `Images/STR_SystemBlockDiagram_New.png` → `images/07-block-diagram.webp`

### AIO-422-R1
- `Images/AIO_1600x1600.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/package1.png` → `images/05-package.webp`
- `Images/Dimensions.png` → `images/06-dimensions.webp`
- `Images/AIO_SystemBlockDiagram.png` → `images/07-block-diagram.webp`

### DIO-430-R1
- `Images/DIO_2000x2000.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/package1.png` → `images/05-package.webp`
- `Images/DIODimensions.png` → `images/06-dimensions.webp`
- `Images/DIO_SystemBlockDiagram.png` → `images/07-block-diagram.webp`

### RGB-621-R1
- `Images/rgb_2000x2000.png` → `images/01-main.webp`
- `Images/photo1.png` → `images/02-listing-1.webp`
- `Images/photo3.png` → `images/03-listing-2.webp`
- `Images/RGB-621-R1Dimensions.png` → `images/06-dimensions.webp`
- `Images/RGB_SystemBlock.png` → `images/07-block-diagram.webp`

### DIM-420-R1
- `Images/DIM_2000x2000.png` → `images/01-main.webp`
- `Images/photo1.png` → `images/02-listing-1.webp`
- `Images/photo3.png` → `images/03-listing-2.webp`
- `Images/Dimension.png` → `images/06-dimensions.webp`
- `Images/DIM_SystemBlockDiagram.png` → `images/07-block-diagram.webp`

### ENM-223-R1
- `Images/ENM_2000x2000.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/package1.png` → `images/05-package.webp`
- `Images/ENMDimensions.png` → `images/06-dimensions.webp`
- `Images/ENM_Diagram.png` → `images/07-block-diagram.webp`

### WLD-521-R1
- `Images/photo1.png` → `images/01-main.webp`
- `Images/mplisting1.png` → `images/02-listing-1.webp`
- `Images/mplisting2.png` → `images/03-listing-2.webp`
- `Images/mplisting3.png` → `images/04-listing-3.webp`
- `Images/package1.png` → `images/05-package.webp`
- `Images/WLD_SystemBLockDiagram.png` → `images/07-block-diagram.webp`

## Missing

### MiniPLC
- images/05-package

### MicroPLC
- images/05-package

### OpenthermGateway
- images/05-package
- images/06-dimensions

### ALM-173-R1
- images/05-package

### STR-3221-R1
- images/05-package

### RGB-621-R1
- images/04-listing-3
- images/05-package

### DIM-420-R1
- images/04-listing-3
- images/05-package

### WLD-521-R1
- images/06-dimensions
