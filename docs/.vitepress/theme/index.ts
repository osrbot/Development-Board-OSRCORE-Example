import DefaultTheme from 'vitepress/theme'
import './custom.css'
import 'xterm/css/xterm.css'
import FlashTool from './FlashTool.vue'
import FlashHub from './FlashHub.vue'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('FlashTool', FlashTool)
    app.component('FlashHub', FlashHub)
  }
} satisfies Theme
