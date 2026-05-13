import { defineConfig } from 'vitepress'

const isGitHubPages = process.env.GITHUB_ACTIONS === 'true'
const base = isGitHubPages ? '/Development-Board-OSRCORE-Example/' : '/'
const repo = 'https://github.com/osrbot/Development-Board-OSRCORE-Example'

const zhChapters = [
  ['第 0 章：ESP-IDF 与硬件入门', '/chapter-00'],
  ['第 1 章：WS2812B 彩色 LED', '/chapter-01'],
  ['第 2 章：无源蜂鸣器', '/chapter-02'],
  ['第 3 章：PWM 舵机与 ESC', '/chapter-03'],
  ['第 4 章：SBUS 遥控接收', '/chapter-04'],
  ['第 5 章：QMI8658 IMU', '/chapter-05'],
  ['第 6 章：正交编码器测速', '/chapter-06'],
  ['第 7 章：NVS 参数持久化', '/chapter-07'],
  ['第 8 章：FreeRTOS 多任务', '/chapter-08'],
  ['第 9 章：PID 电机闭环', '/chapter-09'],
  ['第 10 章：Madgwick AHRS', '/chapter-10'],
  ['第 11 章：完整机器人示例', '/chapter-11']
] as const

const enChapters = [
  ['0. ESP-IDF & Hardware', '/en/chapter-00'],
  ['1. WS2812B LED', '/en/chapter-01'],
  ['2. Passive Buzzer', '/en/chapter-02'],
  ['3. Servo & ESC PWM', '/en/chapter-03'],
  ['4. SBUS Receiver', '/en/chapter-04'],
  ['5. QMI8658 IMU', '/en/chapter-05'],
  ['6. Quadrature Encoder', '/en/chapter-06'],
  ['7. NVS Persistence', '/en/chapter-07'],
  ['8. FreeRTOS Tasks', '/en/chapter-08'],
  ['9. PID Speed Loop', '/en/chapter-09'],
  ['10. Madgwick AHRS', '/en/chapter-10'],
  ['11. Full Robot Example', '/en/chapter-11']
] as const

export default defineConfig({
  title: 'OSRCORE Docs',
  description: 'OSRCORE development board ESP-IDF examples and robot tutorial',
  base,
  cleanUrls: true,
  lastUpdated: true,
  metaChunk: true,
  markdown: {
    lineNumbers: true
  },
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}logo.svg` }],
    ['link', { rel: 'apple-touch-icon', href: `${base}logo.svg` }],
    ['meta', { name: 'theme-color', content: '#16a34a' }],
    ['meta', { name: 'viewport', content: 'width=device-width,initial-scale=1' }]
  ],
  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'OSRCORE',
    socialLinks: [
      { icon: 'github', link: repo }
    ],
    search: {
      provider: 'local'
    }
  },
  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
      title: 'OSRCORE 文档',
      description: 'OSRCORE 开发板 ESP-IDF 示例与机器人嵌入式教程',
      themeConfig: {
        nav: [
          { text: '教程', link: '/chapters' },
          { text: '兼容单页版', link: '/tutorial_zh' },
          { text: '示例仓库', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE 教程',
            items: [
              { text: '首页', link: '/' },
              { text: '章节索引', link: '/chapters' },
              { text: '兼容单页版', link: '/tutorial_zh' }
            ]
          },
          {
            text: '章节导航',
            items: zhChapters.map(([text, link]) => ({ text, link }))
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
              root: {
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
          message: 'Built for OSRCORE robot development board.',
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
    },
    en: {
      label: 'English',
      lang: 'en-US',
      link: '/en/',
      title: 'OSRCORE Docs',
      description: 'OSRCORE development board ESP-IDF examples and robot tutorial',
      themeConfig: {
        nav: [
          { text: 'Guide', link: '/en/chapters' },
          { text: 'Compatibility Single Page', link: '/en/tutorial' },
          { text: 'Examples', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE Guide',
            items: [
              { text: 'Overview', link: '/en/' },
              { text: 'Chapter Index', link: '/en/chapters' },
              { text: 'Compatibility Single Page', link: '/en/tutorial' }
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
          message: 'Built for OSRCORE robot development board.',
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
    }
  }
})
