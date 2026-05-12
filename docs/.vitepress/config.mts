import { defineConfig } from 'vitepress'

const isGitHubPages = process.env.GITHUB_ACTIONS === 'true'

export default defineConfig({
  title: 'OSRCORE Docs',
  description: 'OSRCORE 开发板 ESP-IDF 示例与机器人嵌入式教程',
  lang: 'zh-CN',
  base: isGitHubPages ? '/Development-Board-OSRCORE-Example/' : '/',
  cleanUrls: true,
  lastUpdated: true,
  metaChunk: true,
  markdown: {
    lineNumbers: true
  },
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/logo.svg' }],
    ['link', { rel: 'apple-touch-icon', href: '/logo.svg' }],
    ['meta', { name: 'theme-color', content: '#16a34a' }],
    ['meta', { name: 'viewport', content: 'width=device-width,initial-scale=1' }]
  ],
  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'OSRCORE',
    nav: [
      { text: '教程', link: '/tutorial_zh' },
      {
        text: '语言',
        items: [
          { text: '简体中文', link: '/' },
          { text: 'English', link: '/en/' }
        ]
      },
      { text: '示例仓库', link: 'https://github.com/osrbot/Development-Board-OSRCORE-Example' }
    ],
    sidebar: [
      {
        text: 'OSRCORE 教程',
        items: [
          { text: '总览', link: '/' },
          { text: '完整中文教程', link: '/tutorial_zh' },
          { text: 'English Overview', link: '/en/' }
        ]
      },
      {
        text: '章节导航',
        items: [
          { text: '第 0 章：ESP-IDF 与硬件入门', link: '/tutorial_zh#第-0-章-入门-esp-idf-工具链与-osrcore-硬件' },
          { text: '第 1 章：WS2812B 彩色 LED', link: '/tutorial_zh#第-1-章-ws2812b-彩色-led-rmt-驱动' },
          { text: '第 2 章：无源蜂鸣器', link: '/tutorial_zh#第-2-章-无源蜂鸣器-ledc-pwm-音调控制' },
          { text: '第 3 章：PWM 舵机与 ESC', link: '/tutorial_zh#第-3-章-pwm-舵机与-esc-控制' },
          { text: '第 4 章：SBUS 遥控接收', link: '/tutorial_zh#第-4-章-sbus-遥控接收' },
          { text: '第 5 章：QMI8658 IMU', link: '/tutorial_zh#第-5-章-qmi8658-imu-i2c-读取' },
          { text: '第 6 章：正交编码器测速', link: '/tutorial_zh#第-6-章-正交编码器速度测量-pcnt' },
          { text: '第 7 章：NVS 参数持久化', link: '/tutorial_zh#第-7-章-nvs-参数持久化' },
          { text: '第 8 章：FreeRTOS 多任务', link: '/tutorial_zh#第-8-章-freertos-多任务架构' },
          { text: '第 9 章：PID 电机闭环', link: '/tutorial_zh#第-9-章-pid-电机速度闭环控制' },
          { text: '第 10 章：Madgwick AHRS', link: '/tutorial_zh#第-10-章-madgwick-ahrs-姿态解算' },
          { text: '第 11 章：完整机器人示例', link: '/tutorial_zh#第-11-章-完整机器人示例' }
        ]
      }
    ],
    outline: {
      level: [2, 3],
      label: '本页目录'
    },
    search: {
      provider: 'local',
      options: {
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
    },
    socialLinks: [
      { icon: 'github', link: 'https://github.com/osrbot/Development-Board-OSRCORE-Example' }
    ],
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
      pattern: 'https://github.com/osrbot/Development-Board-OSRCORE-Example/edit/main/docs/:path',
      text: '在 GitHub 上编辑此页'
    }
  }
})
