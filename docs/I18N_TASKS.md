# OSRCORE Docs i18n Task Checklist

This checklist tracks the documentation split, bilingual alignment, and navigation maintenance work.

## Phase 1 — Structure

- [x] Keep the original monolithic Chinese tutorial as `tutorial_zh.md` for compatibility.
- [x] Keep the original monolithic English tutorial as `en/tutorial.md` for compatibility.
- [x] Generate split Chinese chapter pages under `zh/` before preview/build.
- [x] Generate split English chapter pages under `en/` before preview/build.
- [x] Generate language-specific chapter index pages.
- [x] Update VitePress sidebar and navigation to point to split chapter pages.

## Phase 2 — Alignment

- [x] Add `docs/i18n-map.json` as the source of truth for bilingual chapter mapping.
- [x] Add `docs/scripts/split-docs.mjs` to generate bilingual chapter pages from source tutorials.
- [x] Add `docs/scripts/check-i18n.mjs` to verify mapped files exist and headings are aligned.
- [x] Add `npm run docs:split`.
- [x] Add `npm run i18n:check`.
- [x] Run split and i18n checks before `npm run build`.
- [ ] Convert each English chapter from structure-aligned text to paragraph-by-paragraph translation of the matching Chinese chapter.

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
- [x] Add generated `/zh/` and existing `/en/` index pages.
- [x] Update sidebars to expose chapter pages directly.
- [x] Update Chinese and English home pages to point to split chapter navigation.
- [ ] Optionally add redirect notices from monolithic pages to split chapter indexes.

## Operating Rule

Every documentation change should update both language versions or explicitly mark the unmatched item in this checklist. The `i18n-map.json` file should be treated as the canonical chapter pairing map.
