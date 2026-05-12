import { defineConfig } from 'vitepress'

const isGitHubPages = process.env.GITHUB_ACTIONS === 'true'
const base = isGitHubPages ? '/Development-Board-OSRCORE-Example/' : '/'
const repo = 'https://github.com/osrbot/Development-Board-OSRCORE-Example'

const chapterAnchors = [
  ['第 0 章：ESP-IDF 与硬件入门', '/tutorial_zh#第-0-章-入门-esp-idf-工具链与-osrcore-硬件'],
  ['第 1 章：WS2812B 彩色 LED', '/tutorial_zh#第-1-章-ws2812b-彩色-led-rmt-驱动'],
  ['第 2 章：无源蜂鸣器', '/tutorial_zh#第-2-章-无源蜂鸣器-ledc-pwm-音调控制'],
  ['第 3 章：PWM 舵机与 ESC', '/tutorial_zh#第-3-章-pwm-舵机与-esc-控制'],
  ['第 4 章：SBUS 遥控接收', '/tutorial_zh#第-4-章-sbus-遥控接收'],
  ['第 5 章：QMI8658 IMU', '/tutorial_zh#第-5-章-qmi8658-imu-i2c-读取'],
  ['第 6 章：正交编码器测速', '/tutorial_zh#第-6-章-正交编码器速度测量-pcnt'],
  ['第 7 章：NVS 参数持久化', '/tutorial_zh#第-7-章-nvs-参数持久化'],
  ['第 8 章：FreeRTOS 多任务', '/tutorial_zh#第-8-章-freertos-多任务架构'],
  ['第 9 章：PID 电机闭环', '/tutorial_zh#第-9-章-pid-电机速度闭环控制'],
  ['第 10 章：Madgwick AHRS', '/tutorial_zh#第-10-章-madgwick-ahrs-姿态解算'],
  ['第 11 章：完整机器人示例', '/tutorial_zh#第-11-章-完整机器人示例']
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
          { text: '教程', link: '/tutorial_zh' },
          { text: '示例仓库', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE 教程',
            items: [
              { text: '总览', link: '/' },
              { text: '完整中文教程', link: '/tutorial_zh' }
            ]
          },
          {
            text: '章节导航',
            items: chapterAnchors.map(([text, link]) => ({ text, link }))
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
          { text: 'Guide', link: '/en/tutorial' },
          { text: 'Examples', link: repo }
        ],
        sidebar: [
          {
            text: 'OSRCORE Guide',
            items: [
              { text: 'Overview', link: '/en/' },
              { text: 'Tutorial', link: '/en/tutorial' }
            ]
          },
          {
            text: 'Chapters',
            items: [
              { text: '0. ESP-IDF & Hardware', link: '/en/tutorial#chapter-0-esp-idf-toolchain-and-osrcore-hardware' },
              { text: '1. WS2812B LED', link: '/en/tutorial#chapter-1-ws2812b-rgb-led-rmt' },
              { text: '2. Passive Buzzer', link: '/en/tutorial#chapter-2-passive-buzzer-ledc-pwm' },
              { text: '3. Servo & ESC PWM', link: '/en/tutorial#chapter-3-servo-and-esc-control' },
              { text: '4. SBUS Receiver', link: '/en/tutorial#chapter-4-sbus-rc-receiver' },
              { text: '5. QMI8658 IMU', link: '/en/tutorial#chapter-5-qmi8658-imu-over-i2c' },
              { text: '6. Quadrature Encoder', link: '/en/tutorial#chapter-6-quadrature-encoder-speed-measurement' },
              { text: '7. NVS Persistence', link: '/en/tutorial#chapter-7-nvs-parameter-persistence' },
              { text: '8. FreeRTOS Tasks', link: '/en/tutorial#chapter-8-freertos-multitasking' },
              { text: '9. PID Speed Loop', link: '/en/tutorial#chapter-9-pid-motor-speed-control' },
              { text: '10. Madgwick AHRS', link: '/en/tutorial#chapter-10-madgwick-ahrs-attitude-estimation' },
              { text: '11. Full Robot Example', link: '/en/tutorial#chapter-11-complete-robot-example' }
            ]
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
