# OSRCORE Docs i18n Task Checklist

This checklist tracks the documentation split, bilingual alignment, and navigation maintenance work.

## Phase 1 — Structure

- [x] Keep the original monolithic Chinese tutorial as `tutorial_zh.md` for compatibility.
- [x] Keep the original monolithic English tutorial as `en/tutorial.md` for compatibility.
- [x] Add split Chinese chapter pages under `zh/`.
- [x] Add split English chapter pages under `en/`.
- [x] Add language-specific chapter index pages.
- [x] Update VitePress sidebar and navigation to point to split chapter pages.

## Phase 2 — Alignment

- [x] Add `docs/i18n-map.json` as the source of truth for bilingual chapter mapping.
- [x] Add `docs/scripts/check-i18n.mjs` to verify mapped files exist and headings are aligned.
- [x] Add `npm run i18n:check`.
- [ ] Convert each English chapter from summary-level text to paragraph-by-paragraph translation of the matching Chinese chapter.
- [ ] Require `npm run i18n:check` before publishing documentation changes.

## Phase 3 — Content Quality

- [ ] Chapter 00 paragraph-level translation review.
- [ ] Chapter 01 paragraph-level translation review.
- [ ] Chapter 02 paragraph-level translation review.
- [ ] Chapter 03 paragraph-level translation review.
- [ ] Chapter 04 paragraph-level translation review.
- [ ] Chapter 05 paragraph-level translation review.
- [ ] Chapter 06 paragraph-level translation review.
- [ ] Chapter 07 paragraph-level translation review.
- [ ] Chapter 08 paragraph-level translation review.
- [ ] Chapter 09 paragraph-level translation review.
- [ ] Chapter 10 paragraph-level translation review.
- [ ] Chapter 11 paragraph-level translation review.

## Phase 4 — Navigation and Compatibility

- [x] Preserve `/tutorial_zh` and `/en/tutorial` as compatibility pages.
- [x] Add `/zh/` and `/en/` chapter index pages.
- [x] Update sidebars to expose chapter pages directly.
- [ ] Optionally add redirect notices from monolithic pages to split chapter indexes.

## Operating Rule

Every documentation change should update both language versions or explicitly mark the unmatched item in this checklist. The `i18n-map.json` file should be treated as the canonical chapter pairing map.
