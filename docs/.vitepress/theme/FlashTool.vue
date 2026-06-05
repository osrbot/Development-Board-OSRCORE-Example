<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, computed } from 'vue'
import { withBase } from 'vitepress'

const props = defineProps<{
  example: string  // e.g. "01_blink_led"
  binUrl: string   // URL from manifest (root-relative or absolute)
  label?: string
}>()

// USB filters for ESP32-S3 (USB-JTAG/CDC)
const USB_FILTERS = [
  { usbVendorId: 0x303a, usbProductId: 0x1001 }, // USB_SERIAL_JTAG
  { usbVendorId: 0x303a, usbProductId: 0x0009 }, // USB_CDC
  { usbVendorId: 0x303a, usbProductId: 0x1002 }, // esp-usb-bridge
  { usbVendorId: 0x10c4, usbProductId: 0xea60 }, // CP2102
  { usbVendorId: 0x1a86, usbProductId: 0x55d4 }, // CH9102
  { usbVendorId: 0x1a86, usbProductId: 0x7523 }, // CH340
]

type State = 'idle' | 'connecting' | 'downloading' | 'flashing' | 'done' | 'console' | 'error'

const state = ref<State>('idle')
const errorMsg = ref('')
const isSupported = ref(false)
const progress = ref(0)
const chipName = ref('')
const termVisible = ref(false)
const termRef = ref<HTMLElement | null>(null)
const consoleRunning = ref(false)

let term: any = null
let fitAddon: any = null
let transport: any = null
let esploader: any = null
let device: SerialPort | null = null
let consoleReader: ReadableStreamDefaultReader | null = null
let consoleAbort: AbortController | null = null

const statusLabel = computed(() => {
  switch (state.value) {
    case 'connecting':   return '连接中…'
    case 'downloading':  return '下载固件…'
    case 'flashing':     return `烧录中… ${progress.value}%`
    case 'done':         return '烧录完成'
    case 'error':        return '出错'
    default:             return ''
  }
})

onMounted(async () => {
  if (typeof navigator === 'undefined') return
  isSupported.value = 'serial' in navigator

  if (!isSupported.value) return

  const { Terminal } = await import('xterm')
  const { FitAddon } = await import('xterm-addon-fit')
  term = new Terminal({
    cols: 80, rows: 18, fontSize: 12,
    scrollback: 99999,
    theme: {
      background: '#0d1117',
      foreground: '#e6edf3',
      cursor: '#58a6ff',
    },
  })
  fitAddon = new FitAddon()
  term.loadAddon(fitAddon)
})

onUnmounted(async () => {
  await stopConsole()
  term?.dispose()
  try { await transport?.disconnect() } catch {}
  if (device) { try { await device.close() } catch {} }
})

// Watch example prop changes — reset state when user switches example on hub page
watch(() => props.example, () => {
  if (state.value === 'done' || state.value === 'error') {
    state.value = 'idle'
    termVisible.value = false
    progress.value = 0
    chipName.value = ''
  }
})

const espLoaderTerminal = {
  clean() { term?.clear() },
  writeLine(data: string) { term?.writeln(data) },
  write(data: string) { term?.write(data) },
}

function openTerm() {
  if (!termRef.value || !term) return
  if (!term._initialized) {
    term.open(termRef.value)
    fitAddon?.fit()
    term._initialized = true
  }
  termVisible.value = true
  setTimeout(() => fitAddon?.fit(), 50)
}

async function flash() {
  if (!isSupported.value) return
  if (consoleRunning.value) {
    await stopConsole()
  }
  errorMsg.value = ''
  progress.value = 0
  chipName.value = ''

  const { ESPLoader, Transport } = await import('esptool-js')

  state.value = 'connecting'
  try {
    device = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
    transport = new Transport(device)

    esploader = new ESPLoader({
      transport,
      baudrate: 460800,
      terminal: espLoaderTerminal,
    })

    openTerm()
    chipName.value = await esploader.main()

    // Download firmware
    state.value = 'downloading'
    const resp = await fetch(withBase(props.binUrl))
    if (!resp.ok) throw new Error(`无法下载固件: HTTP ${resp.status}`)

    const blob = await resp.blob()
    const data = await new Promise<string>((resolve, reject) => {
      const reader = new FileReader()
      reader.onload = e => resolve(e.target!.result as string)
      reader.onerror = () => reject(new Error('读取固件失败'))
      reader.readAsBinaryString(blob)
    })

    // Flash
    state.value = 'flashing'
    await esploader.writeFlash({
      fileArray: [{ data, address: 0 }],
      flashSize: 'keep',
      eraseAll: true,
      compress: true,
      reportProgress(fileIndex: number, written: number, total: number) {
        progress.value = Math.round((written / total) * 100)
      },
    })

    // Let esploader reset the chip and release the port properly
    await esploader.after()
    state.value = 'done'
  } catch (e: any) {
    if (e?.name === 'NotFoundError') {
      state.value = 'idle'
      return
    }
    errorMsg.value = e?.message || String(e)
    state.value = 'error'
  }
}

async function openConsole() {
  if (!device || consoleRunning.value) return

  consoleAbort = new AbortController()
  consoleRunning.value = true
  openTerm()
  term?.writeln('\r\n\x1b[32m--- 串口监视器已启动 115200 baud ---\x1b[0m\r\n')
  term?.writeln('\x1b[33m按"停止监视器"关闭\x1b[0m\r\n')

  try {
    // Port was released by esploader.after(); open it fresh for console use
    if ((device as any).readable === null) {
      await device.open({ baudRate: 115200 })
    }
    consoleReader = (device as any).readable.getReader()

    while (!consoleAbort.signal.aborted) {
      const { value, done } = await consoleReader.read()
      if (done) break
      if (value) term?.write(value)
    }
  } catch (e: any) {
    if (!consoleAbort?.signal.aborted) {
      term?.writeln(`\r\n\x1b[31m串口错误: ${e?.message}\x1b[0m`)
    }
  } finally {
    try { consoleReader?.releaseLock() } catch {}
    consoleReader = null
    consoleRunning.value = false
  }
}

async function stopConsole() {
  if (!consoleRunning.value) return
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  // Wait briefly for the read loop to exit
  await new Promise(r => setTimeout(r, 100))
  term?.writeln('\r\n\x1b[33m--- 串口监视器已停止 ---\x1b[0m')
}

async function retry() {
  await stopConsole()
  state.value = 'idle'
  termVisible.value = false
  progress.value = 0
  try { await transport?.disconnect() } catch {}
  if (device) {
    try { await device.close() } catch {}
  }
  transport = null
  esploader = null
  device = null
}
</script>

<template>
  <ClientOnly>
    <div class="flash-tool">
      <!-- Header row -->
      <div class="flash-header">
        <span class="flash-icon">⚡</span>
        <span class="flash-title">
          {{ props.label ?? `在线烧录 — ${props.example}` }}
        </span>
        <span v-if="chipName" class="flash-chip-badge">{{ chipName }}</span>
        <span v-if="state !== 'idle'" class="flash-status-text">{{ statusLabel }}</span>
      </div>

      <!-- Browser not supported -->
      <div v-if="!isSupported" class="flash-unsupported">
        当前浏览器不支持 Web Serial API。请使用 Chrome 或 Edge 打开此页面。
      </div>

      <!-- Controls -->
      <div v-else class="flash-controls">
        <!-- Idle -->
        <button
          v-if="state === 'idle'"
          class="flash-btn flash-btn-primary"
          @click="flash"
        >
          连接并烧录
        </button>

        <!-- Connecting / downloading / flashing -->
        <div v-if="state === 'connecting' || state === 'downloading' || state === 'flashing'" class="flash-progress-row">
          <div class="flash-spinner" />
          <span class="flash-progress-label">{{ statusLabel }}</span>
          <div v-if="state === 'flashing'" class="flash-progress-bar-wrap">
            <div class="flash-progress-bar" :style="{ width: progress + '%' }" />
          </div>
        </div>

        <!-- Done -->
        <div v-if="state === 'done'" class="flash-done-row">
          <span class="flash-done-badge">✓ 烧录成功</span>
          <button
            v-if="!consoleRunning"
            class="flash-btn flash-btn-secondary"
            @click="openConsole"
          >打开串口监视器</button>
          <button
            v-else
            class="flash-btn flash-btn-stop"
            @click="stopConsole"
          >停止监视器</button>
          <button
            class="flash-btn flash-btn-ghost"
            :disabled="consoleRunning"
            @click="retry"
          >重新烧录</button>
        </div>

        <!-- Error -->
        <div v-if="state === 'error'" class="flash-error-row">
          <span class="flash-error-badge">✗ 错误</span>
          <span class="flash-error-msg">{{ errorMsg }}</span>
          <button class="flash-btn flash-btn-ghost" @click="retry">重试</button>
        </div>
      </div>

      <!-- xterm terminal (visible after connect) -->
      <div v-show="termVisible" class="flash-term-wrap">
        <div ref="termRef" class="flash-term" />
      </div>

      <!-- Hint -->
      <div v-if="isSupported && state === 'idle'" class="flash-hint">
        <div>连接前请确保开发板已通过 USB 接入电脑，且 V_IN 已接 9–26 V 电源。</div>
        <div class="flash-hint-boot">
          <span class="flash-hint-boot-icon">💡</span>
          <span>
            如芯片无法连接，请<strong>按住 BOOT 键</strong>，再按一下 RESET 键，松开 RESET 后再松开 BOOT——芯片即进入下载模式。
          </span>
        </div>
      </div>
    </div>
  </ClientOnly>
</template>

<style scoped>
.flash-tool {
  margin: 24px 0;
  border: 1px solid rgba(34, 197, 94, 0.25);
  border-radius: 12px;
  padding: 16px 20px;
  background: linear-gradient(135deg, rgba(34,197,94,0.06), rgba(6,182,212,0.04));
}

.flash-header {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
  flex-wrap: wrap;
}

.flash-icon { font-size: 18px; }

.flash-title {
  font-weight: 600;
  font-size: 15px;
  color: var(--vp-c-text-1);
}

.flash-chip-badge {
  background: rgba(34, 197, 94, 0.15);
  color: var(--vp-c-brand-1);
  border: 1px solid rgba(34, 197, 94, 0.3);
  border-radius: 999px;
  padding: 2px 10px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
}

.flash-status-text {
  font-size: 13px;
  color: var(--vp-c-text-2);
  margin-left: auto;
}

.flash-controls {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px;
}

.flash-btn {
  border: none;
  border-radius: 8px;
  padding: 8px 18px;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: opacity 0.15s;
}
.flash-btn:hover { opacity: 0.85; }

.flash-btn-primary {
  background: var(--vp-c-brand-1);
  color: #fff;
}

.flash-btn-secondary {
  background: rgba(34,197,94,0.12);
  color: var(--vp-c-brand-1);
  border: 1px solid rgba(34,197,94,0.3);
}

.flash-btn-ghost {
  background: transparent;
  color: var(--vp-c-text-2);
  border: 1px solid var(--vp-c-divider);
}

.flash-progress-row {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
}

.flash-spinner {
  width: 16px; height: 16px;
  border: 2px solid var(--vp-c-divider);
  border-top-color: var(--vp-c-brand-1);
  border-radius: 50%;
  animation: spin 0.7s linear infinite;
  flex-shrink: 0;
}
@keyframes spin { to { transform: rotate(360deg); } }

.flash-progress-label { font-size: 13px; color: var(--vp-c-text-2); }

.flash-progress-bar-wrap {
  flex: 1;
  height: 6px;
  background: var(--vp-c-divider);
  border-radius: 3px;
  overflow: hidden;
}

.flash-progress-bar {
  height: 100%;
  background: var(--vp-c-brand-1);
  border-radius: 3px;
  transition: width 0.2s;
}

.flash-done-row {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.flash-done-badge {
  color: #22c55e;
  font-weight: 600;
  font-size: 14px;
}

.flash-error-row {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.flash-error-badge {
  color: #ef4444;
  font-weight: 600;
  font-size: 14px;
}

.flash-error-msg {
  font-size: 13px;
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
  word-break: break-all;
}

.flash-term-wrap {
  margin-top: 14px;
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid var(--vp-c-divider);
}

.flash-term {
  height: 280px;
  background: #0d1117;
}

.flash-btn-stop {
  background: rgba(239,68,68,0.12);
  color: #ef4444;
  border: 1px solid rgba(239,68,68,0.3);
}

.flash-hint {
  margin-top: 10px;
  font-size: 12px;
  color: var(--vp-c-text-3);
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.flash-hint-boot {
  display: flex;
  align-items: flex-start;
  gap: 5px;
  padding: 7px 10px;
  border-radius: 6px;
  background: rgba(250,204,21,0.07);
  border: 1px solid rgba(250,204,21,0.2);
  color: var(--vp-c-text-2);
  font-size: 12px;
  line-height: 1.5;
}

.flash-hint-boot-icon { flex-shrink: 0; }

.flash-unsupported {
  font-size: 13px;
  color: #f59e0b;
  padding: 8px 12px;
  background: rgba(245,158,11,0.1);
  border-radius: 8px;
  border: 1px solid rgba(245,158,11,0.25);
}
</style>
