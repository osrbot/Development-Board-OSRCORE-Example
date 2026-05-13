import fs from 'node:fs/promises'
import path from 'node:path'

const root = process.cwd()
const map = JSON.parse(await fs.readFile(path.join(root, 'i18n-map.json'), 'utf8'))

function headingPattern(title) {
  const escaped = title.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
  return new RegExp(`^#\\s+${escaped}\\s*$`, 'm')
}

function countHeadings(content, level) {
  const re = new RegExp(`^${'#'.repeat(level)}\\s+`, 'gm')
  return [...content.matchAll(re)].length
}

const errors = []
const warnings = []

async function exists(relativePath) {
  try {
    await fs.access(path.join(root, relativePath))
    return true
  } catch {
    return false
  }
}

for (const chapter of map.chapters) {
  for (const locale of ['zh', 'en']) {
    const file = chapter[locale]
    const title = chapter[`${locale}Title`]

    if (!(await exists(file))) {
      errors.push(`Missing ${locale} file for chapter ${chapter.id}: ${file}`)
      continue
    }

    const content = await fs.readFile(path.join(root, file), 'utf8')
    if (!headingPattern(title).test(content)) {
      errors.push(`Heading mismatch in ${file}; expected H1: ${title}`)
    }

    const h3Count = countHeadings(content, 3)
    if (h3Count < 5) {
      warnings.push(`${file} has only ${h3Count} H3 sections; expected tutorial structure may be incomplete.`)
    }
  }

  if ((await exists(chapter.zh)) && (await exists(chapter.en))) {
    const zh = await fs.readFile(path.join(root, chapter.zh), 'utf8')
    const en = await fs.readFile(path.join(root, chapter.en), 'utf8')
    const zhH3 = countHeadings(zh, 3)
    const enH3 = countHeadings(en, 3)
    if (zhH3 !== enH3) {
      warnings.push(`H3 count differs for chapter ${chapter.id}: zh=${zhH3}, en=${enH3}`)
    }
  }
}

for (const page of ['zh/index.md', 'en/index.md', 'tutorial_zh.md', 'en/tutorial.md']) {
  if (!(await exists(page))) {
    errors.push(`Missing required compatibility/index page: ${page}`)
  }
}

if (warnings.length > 0) {
  console.warn('\n[i18n warnings]')
  for (const warning of warnings) console.warn(`- ${warning}`)
}

if (errors.length > 0) {
  console.error('\n[i18n errors]')
  for (const error of errors) console.error(`- ${error}`)
  process.exit(1)
}

console.log(`[i18n] OK: ${map.chapters.length} bilingual chapter pairs checked.`)
