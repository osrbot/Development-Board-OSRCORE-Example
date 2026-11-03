# Maintaining Bilingual Documentation

The OSRCORE documentation keeps long-form source tutorials and generates the public chapter pages during preview and build.

## Source Files

Edit these source files:

- Chinese: `zh/tutorial.md`
- English: `tutorial.md`

These files are source inputs for generation. They are not exposed as primary navigation entries on the site.

## Public Chapter Pages

Generated public pages:

- Chinese chapters: `zh/chapter-00.md` ... `zh/chapter-13.md`
- English root-locale chapters: `chapter-00.md` ... `chapter-13.md`
- Chinese chapter index: `zh/chapters.md`
- English chapter index: `chapters.md`

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
