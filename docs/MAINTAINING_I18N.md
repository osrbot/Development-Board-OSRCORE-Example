# Maintaining Bilingual Documentation

The OSRCORE documentation keeps long-form source tutorials and generates the public chapter pages during preview and build.

## Source Files

Edit these source files:

- Chinese: `tutorial_zh.md`
- English: `en/tutorial.md`

These files are source inputs for generation. They are not exposed as primary navigation entries on the site.

## Public Chapter Pages

Generated public pages:

- Chinese root-locale chapters: `chapter-00.md` ... `chapter-11.md`
- English chapters: `en/chapter-00.md` ... `en/chapter-11.md`
- Chinese chapter index: `chapters.md`
- English chapter index: `en/chapters.md`

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
