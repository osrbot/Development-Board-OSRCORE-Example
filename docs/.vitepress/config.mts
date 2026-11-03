import { defineConfig } from 'vitepress'
import { lessons } from './theme/lab-simulation.mjs'

const isGitHubPages = process.env.OSRCORE_PAGES === 'true'
const base = isGitHubPages ? '/osrcore-learn/' : '/'
const repo = 'https://github.com/osrbot/osrcore-learn'

const zhChapters = lessons.map(l => [String(l.id).padStart(2,'0')+' · '+l.zh, '/chapter-'+String(l.id).padStart(2,'0')])

const enChapters = lessons.map(l => [String(l.id).padStart(2,'0')+' · '+l.en, '/chapter-'+String(l.id).padStart(2,'0')])

export default defineConfig({
  title: 'OSRCORE Learn',
  description: 'Interactive lessons and ESP-IDF examples for the OSRCORE ESP32-S3 robot controller.',
  base,
  cleanUrls: false,
  appearance: false,
  lastUpdated: true,
  metaChunk: true,
  markdown: {
    lineNumbers: true
  },
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}brand/favicon.svg` }],
    ['link', { rel: 'apple-touch-icon', href: `${base}brand/favicon.svg` }],
    ['meta', { name: 'theme-color', content: '#0076DF' }],
    ['meta', { name: 'viewport', content: 'width=device-width,initial-scale=1' }]
  ],
  themeConfig: {
    logo: '/brand/logo.svg',
    siteTitle: false,
    socialLinks: [
      { icon: 'github', link: repo }
    ],
    search: {
      provider: 'local'
    }
  },
  locales: {
    root: {
      label: 'English',
      lang: 'en-US',
      link: '/',
      title: 'OSRCORE Learn',
      description: 'Interactive lessons and ESP-IDF examples for the OSRCORE ESP32-S3 robot controller.',
      themeConfig: {
        nav: [
          { text: 'Guide', link: '/chapters' },
          { text: 'Connect & download', link: '/updater' },
          { text: 'Examples', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE Guide',
            items: [
              { text: 'Overview', link: '/' },
              { text: 'Chapter Index', link: '/chapters' },
              { text: 'Connect & download', link: '/updater' }
            ]
          },
          {
            text: 'Chapters',
            items: enChapters.map(([text, link]) => ({ text, link }))
          }
        ],
        outline: {
          level: [2, 3],
          label: 'On this page'
        },
        footer: {
          message: 'Learn, build and control robots with OSRCORE.',
          copyright: 'MIT License'
        },
        docFooter: {
          prev: 'Previous page',
          next: 'Next page'
        },
        lastUpdated: {
          text: 'Last updated',
          formatOptions: {
            dateStyle: 'medium',
            timeStyle: 'short'
          }
        },
        editLink: {
          pattern: `${repo}/edit/main/docs/:path`,
          text: 'Edit this page on GitHub'
        }
      }
    },
    zh: {
      label: '简体中文',
      lang: 'zh-CN',
      link: '/zh/',
      title: 'OSRCORE 文档',
      description: 'OSRCORE ESP32-S3 机器人控制器交互教程与 ESP-IDF 例程',
      themeConfig: {
        nav: [
          { text: '教程', link: '/zh/chapters' },
          { text: '连接与下载', link: '/zh/updater' },
          { text: '示例仓库', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE 教程',
            items: [
              { text: '首页', link: '/zh/' },
              { text: '章节索引', link: '/zh/chapters' },
              { text: '连接与下载', link: '/zh/updater' }
            ]
          },
          {
            text: '章节导航',
            items: zhChapters.map(([text, link]) => ({ text, link: '/zh'+link }))
          }
        ],
        outline: {
          level: [2, 3],
          label: '本页目录'
        },
        search: {
          provider: 'local',
          options: {
            locales: {
              zh: {
                translations: {
                  button: {
                    buttonText: '搜索文档',
                    buttonAriaLabel: '搜索文档'
                  },
                  modal: {
                    displayDetails: '显示详情',
                    resetButtonTitle: '清除搜索',
                    backButtonTitle: '关闭搜索',
                    noResultsText: '没有找到相关内容',
                    footer: {
                      selectText: '选择',
                      navigateText: '切换',
                      closeText: '关闭'
                    }
                  }
                }
              }
            }
          }
        },
        footer: {
          message: 'Learn, build and control robots with OSRCORE.',
          copyright: 'MIT License'
        },
        docFooter: {
          prev: '上一页',
          next: '下一页'
        },
        lastUpdated: {
          text: '最后更新',
          formatOptions: {
            dateStyle: 'medium',
            timeStyle: 'short'
          }
        },
        editLink: {
          pattern: `${repo}/edit/main/docs/:path`,
          text: '在 GitHub 上编辑此页'
        }
      }
    }
  }
})
