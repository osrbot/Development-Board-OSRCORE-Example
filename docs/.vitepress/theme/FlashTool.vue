<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, computed } from 'vue'
import { withBase } from 'vitepress'

const props = defineProps<{
  example: string
  binUrl?: string   // remote URL (manifest examples)
  binData?: string  // pre-read binary string (custom local file)
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

type State = 'idle' | 'connecting' | 'connected' | 'downloading' | 'flashing' | 'done' | 'console' | 'disconnected' | 'error'

const state = ref<State>('idle')
const errorMsg = ref('')
const isSupported = ref(false)
const progress = ref(0)
const chipName = ref('')
const termVisible = ref(false)
const termRef = ref<HTMLElement | null>(null)
const consoleOrigin = ref<'flash' | 'direct'>('direct')

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
    case 'connected':    return `已识别：${chipName.value}`
    case 'downloading':  return '下载固件…'
    case 'flashing':     return `烧录中… ${progress.value}%`
    case 'done':         return '烧录完成'
    case 'console':      return '串口监视中'
    case 'disconnected': return '设备已断开'
    case 'error':        return '出错'
    default:             return ''
  }
})

const disconnectHandler = (e: Event) => {
  const port = (e as any).port as SerialPort
  if (port !== device) return
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  term?.writeln('\r\n\x1b[31m⚠  设备已断开 — 请检查 USB 连接\x1b[0m')
  openTerm()
  state.value = 'disconnected'
}

onMounted(async () => {
  if (typeof navigator === 'undefined') return
  isSupported.value = 'serial' in navigator
  if (!isSupported.value) return

  ;(navigator as any).serial.addEventListener('disconnect', disconnectHandler)

  const { Terminal } = await import('xterm')
  const { FitAddon } = await import('xterm-addon-fit')
  term = new Terminal({
    cols: 80, rows: 18, fontSize: 12,
    scrollback: 99999,
    theme: { background: '#0d1117', foreground: '#e6edf3', cursor: '#58a6ff' },
  })
  fitAddon = new FitAddon()
  term.loadAddon(fitAddon)
})

onUnmounted(async () => {
  ;(navigator as any).serial?.removeEventListener('disconnect', disconnectHandler)
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  term?.dispose()
  try { await transport?.disconnect() } catch {}
  if (device) { try { await device.close() } catch {} }
})

// Watch example prop changes — reset state when user switches example on hub page
watch(() => props.example, () => {
  if (['done', 'error', 'connected', 'disconnected'].includes(state.value)) retry()
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

async function connect() {
  if (!isSupported.value) return
  errorMsg.value = ''
  progress.value = 0
  chipName.value = ''

  const { ESPLoader, Transport } = await import('esptool-js')
  state.value = 'connecting'
  try {
    device = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
    transport = new Transport(device)
    esploader = new ESPLoader({ transport, baudrate: 460800, terminal: espLoaderTerminal })
    openTerm()
    chipName.value = await esploader.main()

    // Chip whitelist: only ESP32-S3 is supported (OSRCORE hardware)
    if (!chipName.value.startsWith('ESP32-S3')) {
      errorMsg.value = `仅支持 OSRCORE（ESP32-S3）开发板，检测到：${chipName.value}`
      try { await transport.disconnect() } catch {}
      state.value = 'error'
      return
    }

    state.value = 'connected'
  } catch (e: any) {
    if (e?.name === 'NotFoundError') { state.value = 'idle'; return }
    errorMsg.value = e?.message || String(e)
    state.value = 'error'
  }
}

async function startFlash() {
  if (state.value !== 'connected' || !esploader) return
  errorMsg.value = ''
  progress.value = 0

  try {
    let data: string

    if (props.binData) {
      // Custom local file — already read by FlashHub before passing in
      data = props.binData
      state.value = 'flashing'
    } else if (props.binUrl) {
      state.value = 'downloading'
      const resp = await fetch(withBase(props.binUrl))
      if (!resp.ok) throw new Error(`无法下载固件: HTTP ${resp.status}`)
      const blob = await resp.blob()
      data = await new Promise<string>((resolve, reject) => {
        const reader = new FileReader()
        reader.onload = e => resolve(e.target!.result as string)
        reader.onerror = () => reject(new Error('读取固件失败'))
        reader.readAsBinaryString(blob)
      })
      state.value = 'flashing'
    } else {
      throw new Error('未指定固件来源')
    }

    await esploader.writeFlash({
      fileArray: [{ data, address: 0 }],
      flashSize: 'keep',
      eraseAll: true,
      compress: true,
      reportProgress(_: number, written: number, total: number) {
        progress.value = Math.round((written / total) * 100)
      },
    })

    // Reset chip; transport.disconnect() releases the internal reader lock on device.readable
    await esploader.after()
    try { await transport.disconnect() } catch {}
    state.value = 'done'
  } catch (e: any) {
    errorMsg.value = e?.message || String(e)
    state.value = 'error'
  }
}

// ---- Console flow ----

async function openConsoleFromIdle() {
  if (!isSupported.value) return
  errorMsg.value = ''
  state.value = 'connecting'
  try {
    device = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
    consoleOrigin.value = 'direct'
    openTerm()
    await runConsole()
  } catch (e: any) {
    if (e?.name === 'NotFoundError') { state.value = 'idle'; return }
    errorMsg.value = e?.message || String(e)
    state.value = 'error'
  }
}

async function openConsoleAfterFlash() {
  if (!device) return
  consoleOrigin.value = 'flash'
  openTerm()
  await runConsole()
}

async function runConsole() {
  consoleAbort = new AbortController()
  state.value = 'console'
  term?.writeln('\r\n\x1b[32m--- 串口监视器已启动 115200 baud ---\x1b[0m\r\n')

  try {
    await device!.open({ baudRate: 115200 })
    consoleReader = (device as any).readable.getReader()

    while (!consoleAbort.signal.aborted) {
      const { value, done } = await consoleReader.read()
      if (done) break
      if (value) term?.write(value)
    }
  } catch (e: any) {
    if (!consoleAbort?.signal.aborted && state.value !== 'disconnected') {
      term?.writeln(`\r\n\x1b[31m串口错误: ${e?.message}\x1b[0m`)
    }
  } finally {
    try { consoleReader?.releaseLock() } catch {}
    consoleReader = null
    if (device && state.value !== 'disconnected') {
      try { await device.close() } catch {}
    }
    if (state.value === 'console') {
      state.value = consoleOrigin.value === 'flash' ? 'done' : 'idle'
      if (consoleOrigin.value !== 'flash') termVisible.value = false
    }
  }
}

async function stopConsole() {
  if (state.value !== 'console') return
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  await new Promise(r => setTimeout(r, 150))
  term?.writeln('\r\n\x1b[33m--- 串口监视器已停止 ---\x1b[0m')
}

async function retry() {
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  await new Promise(r => setTimeout(r, 80))
  state.value = 'idle'
  termVisible.value = false
  progress.value = 0
  chipName.value = ''
  try { await transport?.disconnect() } catch {}
  if (device) { try { await device.close() } catch {} }
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
        <span v-if="chipName && (state === 'done' || state === 'console')" class="flash-chip-badge">{{ chipName }}</span>
        <span
          v-if="['downloading','flashing','done','console','disconnected','error'].includes(state)"
          class="flash-status-text"
        >{{ statusLabel }}</span>
      </div>

      <!-- Browser not supported -->
      <div v-if="!isSupported" class="flash-unsupported">
        当前浏览器不支持 Web Serial API。请使用 Chrome 或 Edge 打开此页面。
      </div>

      <!-- Controls -->
      <div v-else class="flash-controls">
        <!-- Idle: two entry points -->
        <template v-if="state === 'idle'">
          <button class="flash-btn flash-btn-primary" @click="connect">连接开发板</button>
          <button class="flash-btn flash-btn-secondary" @click="openConsoleFromIdle">串口监视器</button>
        </template>

        <!-- Connecting -->
        <div v-if="state === 'connecting'" class="flash-progress-row">
          <div class="flash-spinner" />
          <span class="flash-progress-label">连接中…</span>
        </div>

        <!-- Connected — confirm before flash -->
        <div v-if="state === 'connected'" class="flash-connected-row">
          <span class="flash-connected-badge">● 已连接</span>
          <span class="flash-chip-inline">{{ chipName }}</span>
          <button class="flash-btn flash-btn-primary" @click="startFlash">开始烧录</button>
          <button class="flash-btn flash-btn-ghost" @click="retry">取消</button>
        </div>

        <!-- Downloading / flashing -->
        <div v-if="state === 'downloading' || state === 'flashing'" class="flash-progress-row">
          <div class="flash-spinner" />
          <span class="flash-progress-label">{{ statusLabel }}</span>
          <div v-if="state === 'flashing'" class="flash-progress-bar-wrap">
            <div class="flash-progress-bar" :style="{ width: progress + '%' }" />
          </div>
        </div>

        <!-- Done: offer console or reflash -->
        <div v-if="state === 'done'" class="flash-done-row">
          <span class="flash-done-badge">✓ 烧录成功</span>
          <button class="flash-btn flash-btn-secondary" @click="openConsoleAfterFlash">打开串口监视器</button>
          <button class="flash-btn flash-btn-ghost" @click="retry">重新烧录</button>
        </div>

        <!-- Console running -->
        <div v-if="state === 'console'" class="flash-console-row">
          <span class="flash-console-badge">● 监视中 115200</span>
          <button class="flash-btn flash-btn-stop" @click="stopConsole">停止监视器</button>
          <button v-if="consoleOrigin === 'flash'" class="flash-btn flash-btn-ghost" disabled>重新烧录</button>
        </div>

        <!-- Disconnected -->
        <div v-if="state === 'disconnected'" class="flash-disconnected-row">
          <span class="flash-disconnected-badge">⚠ 设备已断开</span>
          <button class="flash-btn flash-btn-ghost" @click="retry">返回</button>
        </div>

        <!-- Error -->
        <div v-if="state === 'error'" class="flash-error-row">
          <span class="flash-error-badge">✗ 错误</span>
          <span class="flash-error-msg">{{ errorMsg }}</span>
          <button class="flash-btn flash-btn-ghost" @click="retry">重试</button>
        </div>
      </div>

      <!-- xterm terminal -->
      <div v-show="termVisible" class="flash-term-wrap">
        <div ref="termRef" class="flash-term" />
      </div>

      <!-- Hint (idle only) -->
      <div v-if="isSupported && state === 'idle'" class="flash-hint">
        <div>连接前请确保开发板已通过 USB 接入电脑，且 V_IN 已接 9–26 V 电源。</div>
        <div class="flash-hint-boot">
          <span class="flash-hint-boot-icon">💡</span>
          <span>
            烧录需要进入下载模式：<strong>按住 BOOT 键 → 按一下 RESET 键 → 松开 RESET → 松开 BOOT</strong>，再点「连接开发板」。
            串口监视器无需此操作，直接连接正常运行的开发板即可。
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

.flash-connected-row {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.flash-connected-badge {
  color: #22c55e;
  font-size: 13px;
  font-weight: 600;
}

.flash-chip-inline {
  font-family: var(--vp-font-family-mono);
  font-size: 13px;
  color: var(--vp-c-brand-1);
  background: rgba(34,197,94,0.1);
  border: 1px solid rgba(34,197,94,0.25);
  border-radius: 6px;
  padding: 2px 8px;
}

.flash-console-row {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.flash-console-badge {
  color: #22c55e;
  font-size: 13px;
  font-weight: 600;
  font-family: var(--vp-font-family-mono);
}

.flash-disconnected-row {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.flash-disconnected-badge {
  color: #f59e0b;
  font-weight: 600;
  font-size: 14px;
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
