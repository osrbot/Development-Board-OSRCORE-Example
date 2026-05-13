# OSRCORE Docs i18n Review Log

This log records paragraph-level bilingual review progress for the split documentation.

## Review Criteria

A chapter can be marked as reviewed when:

1. The Chinese and English chapters have the same main section structure.
2. Every Chinese paragraph, table, code block, formula block, and summary has a corresponding English item.
3. Code blocks preserve API names, constants, GPIO values, and command lines.
4. Technical wording is consistent with ESP-IDF and embedded systems terminology.
5. The chapter passes `npm run i18n:check` after regeneration.

## Progress

| Chapter | Status | Notes |
|---|---|---|
| 00 | Reviewed | English Chapter 0 mirrors the Chinese 0.1–0.5 structure: key points, course content, ESP-IDF installation, project structure, idf.py commands, OSRCORE resource table, USB CDC configuration, minimal app, and summary. |
| 01 | Pending | Needs paragraph-level comparison against Chinese source. |
| 02 | Pending | Needs paragraph-level comparison against Chinese source. |
| 03 | Pending | Needs paragraph-level comparison against Chinese source. |
| 04 | Pending | Needs paragraph-level comparison against Chinese source. |
| 05 | Pending | Needs paragraph-level comparison against Chinese source. |
| 06 | Pending | Needs paragraph-level comparison against Chinese source. |
| 07 | Pending | Needs paragraph-level comparison against Chinese source. |
| 08 | Pending | Needs paragraph-level comparison against Chinese source. |
| 09 | Pending | Needs paragraph-level comparison against Chinese source. |
| 10 | Pending | Needs paragraph-level comparison against Chinese source. |
| 11 | Pending | Needs paragraph-level comparison against Chinese source. |

## Next Item

Continue with Chapter 01: WS2812B RGB LED (RMT Driver).
