#!/usr/bin/env python3
"""Patch module datasheet PDFs for cross-audit consistency (AIO UI, RGB relay type)."""
from pathlib import Path

import fitz

REPO = Path(__file__).resolve().parent.parent


def replace_text_on_page(page, old: str, new: str) -> bool:
    rects = page.search_for(old)
    if not rects:
        return False
    for rect in rects:
        page.add_redact_annot(rect, fill=(1, 1, 1))
    page.apply_redactions()
    for rect in rects:
        page.insert_text(
            (rect.x0, rect.y1 - 2),
            new,
            fontsize=11,
            fontname="helv",
            color=(0, 0, 0),
        )
    return True


def patch_rgb_relay(pdf_path: Path) -> None:
    doc = fitz.open(pdf_path)
    ok = replace_text_on_page(doc[1], "1 × SPDT dry-contact relays", "1 × SPST-NO dry-contact relay")
    if not ok:
        raise RuntimeError(f"RGB relay text not found in {pdf_path}")
    doc.save(pdf_path, incremental=True, encryption=fitz.PDF_ENCRYPT_KEEP)
    doc.close()
    print(f"  patched RGB relay type in {pdf_path.name}")


def patch_aio_user_interface(pdf_path: Path) -> None:
    doc = fitz.open(pdf_path)
    page = doc[1]
    label = "User Interface"
    detail = "4 buttons, 7× LEDs (Power, 4 user LEDs, RX, TX)"
    if page.search_for(label):
        print(f"  skip AIO UI (already present) in {pdf_path.name}")
        doc.close()
        return
    y = 410.0
    page.insert_text((50.5, y), label, fontsize=11, fontname="helv", color=(0, 0, 0))
    page.insert_text((266.5, y), detail, fontsize=11, fontname="helv", color=(0, 0, 0))
    doc.save(pdf_path, incremental=True, encryption=fitz.PDF_ENCRYPT_KEEP)
    doc.close()
    print(f"  added AIO User Interface in {pdf_path.name}")


def main() -> None:
    patch_rgb_relay(REPO / "RGB-621-R1/Manuals/RGB-621-R1_Datasheet.pdf")
    patch_aio_user_interface(REPO / "AIO-422-R1/Manuals/AIO-422-R1_Datasheet.pdf")


if __name__ == "__main__":
    main()
