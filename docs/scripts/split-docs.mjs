import fs from 'node:fs/promises'
import path from 'node:path'
import { lessons } from '../.vitepress/theme/lab-simulation.mjs'

const root = process.cwd()
const mapPath = path.join(root, 'i18n-map.json')
const map = JSON.parse(await fs.readFile(mapPath, 'utf8'))
map.chapters = lessons.map(lesson => {
  const id = String(lesson.id).padStart(2, '0')
  return {
    ...map.chapters.find(chapter => chapter.id === id),
    id,
    zhTitle: `${id} · ${lesson.zh}`,
    enTitle: `${id} · ${lesson.en}`,
    zh: `zh/chapter-${id}.md`,
    en: `chapter-${id}.md`
  }
})
await fs.writeFile(mapPath,JSON.stringify(map,null,2)+'\n')

const sources = {
  zh: path.join(root, 'zh', 'tutorial.md'),
  en: path.join(root, 'tutorial.md')
}

const splitRules = {
  zh: /^##\s+第\s*(\d+)\s*章[^\n]*$/gm,
  en: /^##\s+Chapter\s+(\d+)\s*:[^\n]*$/gm
}

function normalizeId(value) {
  return String(Number(value)).padStart(2, '0')
}

function splitByChapter(content, locale) {
  const rule = splitRules[locale]
  const matches = [...content.matchAll(rule)]
  const result = new Map()

  for (let i = 0; i < matches.length; i += 1) {
    const match = matches[i]
    const id = normalizeId(match[1])
    const start = match.index
    const end = i + 1 < matches.length ? matches[i + 1].index : content.length
    result.set(id, content.slice(start, end).trim() + '\n')
  }

  return result
}

function promoteChapterHeading(markdown, title, locale) {
  const id=markdown.match(/^##\s+(?:第\s*|Chapter\s+)(\d+)/m)?.[1] || '0'
  const url=(locale==='zh'?'/zh/':'/')+'#lesson-'+id.padStart(2,'0')
  const promoted=markdown.replace(/^##\s+[^\n]+/m, `# ${title}`)
  return promoted.replace(/^(#\s+[^\n]+\n)/m,`$1\n[${locale==='zh'?'打开本课交互实验与设备下载':'Open this interactive lesson and device downloads'}](${url})\n`)
}

function indexPage(locale) {
  const isZh = locale === 'zh'
  const title = isZh ? '课程资料' : 'Course resources'
  const intro = isZh
    ? '与交互课程一一对应：同样的编号、名称与顺序。选择课程阅读原理、接线与源码说明。'
    : 'Start with the hardware, then explore peripheral drivers, sensors, and robot control.'

  const lines = [`# ${title}`, '', intro, '']
  for (const chapter of map.chapters) {
    const text = locale === 'zh' ? chapter.zhTitle : chapter.enTitle
    const file = locale === 'zh' ? chapter.zh : chapter.en
    const href = path.basename(file).replace(/\.md$/, '')
    lines.push(`- [${text}](./${href})`)
  }
  lines.push('', isZh ? '## 硬件资料' : '## Hardware resources', '', `[${isZh ? '产品手册' : 'Product manual'}](/hardware/manual-${locale}.pdf) · [${isZh ? '原理图' : 'Schematic'}](/hardware/schematic-${locale}.pdf)`, '')
  return lines.join('\n')
}

async function writeFileEnsured(relativePath, content) {
  const absolutePath = path.join(root, relativePath)
  await fs.mkdir(path.dirname(absolutePath), { recursive: true })
  await fs.writeFile(absolutePath, content, 'utf8')
}

const sourceText = {
  zh: await fs.readFile(sources.zh, 'utf8'),
  en: await fs.readFile(sources.en, 'utf8')
}

const chaptersByLocale = {
  zh: splitByChapter(sourceText.zh, 'zh'),
  en: splitByChapter(sourceText.en, 'en')
}

const missing = []

for (const chapter of map.chapters) {
  const zh = chaptersByLocale.zh.get(chapter.id)
  const en = chaptersByLocale.en.get(chapter.id)

  if (!zh) missing.push(`zh chapter ${chapter.id}`)
  if (!en) missing.push(`en chapter ${chapter.id}`)

  if (zh) {
    await writeFileEnsured(chapter.zh, promoteChapterHeading(zh, chapter.zhTitle, 'zh'))
  }
  if (en) {
    await writeFileEnsured(chapter.en, promoteChapterHeading(en, chapter.enTitle, 'en'))
  }
}

await writeFileEnsured('zh/chapters.md', indexPage('zh'))
await writeFileEnsured('chapters.md', indexPage('en'))

if (missing.length > 0) {
  console.warn(`[split-docs] Missing chapters: ${missing.join(', ')}`)
  process.exitCode = 1
} else {
  console.log(`[split-docs] Generated ${map.chapters.length * 2} chapter pages and chapter indexes`)
}
