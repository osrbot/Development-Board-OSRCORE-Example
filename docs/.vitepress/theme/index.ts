import LearningLab from './InteractiveLab.vue'
import DocsLayout from './DocsLayout.vue'
import DefaultTheme from 'vitepress/theme'
import './custom.css'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  Layout: DocsLayout,
  enhanceApp({ app }) {
    app.component('LearningLab', LearningLab)
  }
} satisfies Theme
