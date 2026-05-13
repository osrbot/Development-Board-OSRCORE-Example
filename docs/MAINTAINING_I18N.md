# Maintaining Bilingual Documentation

The OSRCORE documentation keeps long-form source tutorials and generates split chapter pages during build.

## Source Files

Edit these source files:

- Chinese: `tutorial_zh.md`
- English: `en/tutorial.md`

The generated chapter pages are created from these source files before preview and build.

## Generated Chapter Pages

Generated pages:

- `zh/chapter-00.md` ... `zh/chapter-11.md`
- `en/chapter-00.md` ... `en/chapter-11.md`
- `zh/index.md`

Generate them manually with:

```bash
npm run docs:split
```

## Alignment Map

`i18n-map.json` is the canonical chapter pairing map. Update it when adding, removing, or renaming chapters.

## Checks

Run:

```bash
npm run i18n:check
```

This regenerates chapter pages and verifies that mapped Chinese and English files exist and use the expected generated headings.

## CI

The GitHub Actions documentation workflow runs:

```bash
npm run i18n:check
npm run build
```

The deployment fails when bilingual chapter mapping is broken.

## Rule

Every documentation change should update both language versions in the same commit whenever possible.
