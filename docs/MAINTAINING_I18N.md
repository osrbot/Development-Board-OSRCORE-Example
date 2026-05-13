# Maintaining the Bilingual Documentation

The documentation now uses a source-plus-generated-chapters workflow.

## Source Files

The two monolithic tutorial files remain the canonical editable sources:

- Chinese source: `tutorial_zh.md`
- English source: `en/tutorial.md`

These files preserve the full long-form tutorial text and keep older links working.

## Generated Split Pages

Chapter pages are generated before local preview and before GitHub Pages deployment:

- Chinese generated pages: `zh/chapter-00.md` ... `zh/chapter-11.md`
- English generated pages: `en/chapter-00.md` ... `en/chapter-11.md`
- Chinese chapter index: `zh/index.md`

Generation command:

```bash
npm run docs:split
```

The generated pages are based on chapter headings in the two source files. Keep chapter headings in this form:

```markdown
## 第 0 章  入门：ESP-IDF 工具链与 OSRCORE 硬件
## Chapter 0: ESP-IDF Toolchain and OSRCORE Hardware
```

## Alignment Map

`i18n-map.json` is the source of truth for chapter pairing. Each chapter has:

- a stable chapter id
- Chinese title
- English title
- Chinese generated file path
- English generated file path

When adding or renaming a chapter, update `i18n-map.json` first.

## Alignment Check

Run:

```bash
npm run i18n:check
```

This command:

1. regenerates chapter files from the source tutorials;
2. verifies that each mapped Chinese and English file exists;
3. verifies that generated H1 headings match `i18n-map.json`;
4. warns when section counts differ between Chinese and English chapters.

## Build Flow

The normal build command already performs splitting and alignment checks:

```bash
npm run build
```

GitHub Pages also uses this command, so the deployed site will fail to build if the bilingual chapter mapping is broken.

## Translation Rule

For strict bilingual maintenance, every Chinese paragraph should have a corresponding English paragraph in the same chapter and section. When editing one language, update the matching language in the same commit or mark the item in `I18N_TASKS.md`.
