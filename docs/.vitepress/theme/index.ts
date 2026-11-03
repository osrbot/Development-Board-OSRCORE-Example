import LearningLab from './InteractiveLab.vue'
import DefaultTheme from 'vitepress/theme'
import './custom.css'
import type { Theme } from 'vitepress'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('LearningLab', LearningLab)
  }
} satisfies Theme
