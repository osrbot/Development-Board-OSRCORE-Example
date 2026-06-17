<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, computed } from 'vue'
import { withBase, useData } from 'vitepress'

const props = defineProps<{
  example: string
  binUrl?: string   // remote URL (manifest examples)
  binData?: string  // pre-read binary string (custom local file)
  label?: string
}>()

// i18n — derive locale from VitePress so the tool matches the page language
const { lang } = useData()
const isEn = computed(() => lang.value.toLowerCase().startsWith('en'))

const STRINGS = {
  zh: {
    title: (ex: string) => `在线烧录 — ${ex}`,
    connecting: '连接中…',
    detected: (c: string) => `已识别：${c}`,
    downloading: '下载固件…',
    flashing: (p: number) => `烧录中… ${p}%`,
    done: '烧录完成',
    monitoring: '串口监视中',
    disconnected: '设备已断开',
    error: '出错',
    termDisconnect: '⚠  设备已断开 — 请检查 USB 连接',
    monitorStarted: '--- 串口监视器已启动 115200 baud ---',
    monitorStopped: '--- 串口监视器已停止 ---',
    serialError: (m: string) => `串口错误: ${m}`,
    chipUnsupported: (c: string) => `仅支持 OSRCORE（ESP32-S3）开发板，检测到：${c}`,
    dlFail: (s: number) => `无法下载固件: HTTP ${s}`,
    readFail: '读取固件失败',
    noSource: '未指定固件来源',
    sendFail: (m: string) => `发送失败: ${m}`,
    timedNeedInput: '定时发送需要先在输入框填入内容',
    unsupported: '当前浏览器不支持 Web Serial API。请使用 Chrome 或 Edge 打开此页面。',
    connectBtn: '连接开发板',
    monitorBtn: '串口监视器',
    connected: '● 已连接',
    startFlash: '开始烧录',
    cancel: '取消',
    flashOk: '✓ 烧录成功',
    openMonitor: '打开串口监视器',
    reflash: '重新烧录',
    monitoringBadge: '● 监视中 115200',
    stopMonitor: '停止监视器',
    devDisconnected: '⚠ 设备已断开',
    back: '返回',
    errBadge: '✗ 错误',
    retry: '重试',
    sendPlaceholder: '输入发送内容…',
    send: '发送',
    timedLabel: '定时：',
    stopTimed: '停止定时',
    startTimed: '开始定时',
    timedHint: '循环发送输入框内容',
    quickLabel: '快捷：',
    quickEditTitle: '右键编辑',
    quickEditPlaceholder: '指令内容',
    filterLabel: '过滤：',
    filterPlaceholder: '隐藏以此开头的行，逗号分隔，如 DEBUG,INFO',
    autoReplyLabel: '自动回复：',
    addRule: '+ 添加规则',
    arWhen: '收到',
    arTrigger: '开头字符',
    arThen: '→ 回复',
    arReply: '回复内容（\\r \\n 可用）',
    hintPower: '连接前请确保开发板已通过 USB 接入电脑，且 V_IN 已接 9–26 V 电源。',
    hintBoot: '烧录需要进入下载模式：',
    hintBootSteps: '按住 BOOT 键 → 按一下 RESET 键 → 松开 RESET → 松开 BOOT',
    hintBootAfter: '，再点「连接开发板」。串口监视器无需此操作，直接连接正常运行的开发板即可。',
  },
  en: {
    title: (ex: string) => `Online Flash — ${ex}`,
    connecting: 'Connecting…',
    detected: (c: string) => `Detected: ${c}`,
    downloading: 'Downloading firmware…',
    flashing: (p: number) => `Flashing… ${p}%`,
    done: 'Flash complete',
    monitoring: 'Monitoring serial',
    disconnected: 'Device disconnected',
    error: 'Error',
    termDisconnect: '⚠  Device disconnected — check the USB connection',
    monitorStarted: '--- Serial monitor started @ 115200 baud ---',
    monitorStopped: '--- Serial monitor stopped ---',
    serialError: (m: string) => `Serial error: ${m}`,
    chipUnsupported: (c: string) => `Only OSRCORE (ESP32-S3) boards are supported. Detected: ${c}`,
    dlFail: (s: number) => `Failed to download firmware: HTTP ${s}`,
    readFail: 'Failed to read firmware',
    noSource: 'No firmware source specified',
    sendFail: (m: string) => `Send failed: ${m}`,
    timedNeedInput: 'Timed send needs text in the input box first',
    unsupported: 'This browser does not support the Web Serial API. Please open this page in Chrome or Edge.',
    connectBtn: 'Connect Board',
    monitorBtn: 'Serial Monitor',
    connected: '● Connected',
    startFlash: 'Start Flash',
    cancel: 'Cancel',
    flashOk: '✓ Flash successful',
    openMonitor: 'Open Serial Monitor',
    reflash: 'Reflash',
    monitoringBadge: '● Monitoring 115200',
    stopMonitor: 'Stop Monitor',
    devDisconnected: '⚠ Device disconnected',
    back: 'Back',
    errBadge: '✗ Error',
    retry: 'Retry',
    sendPlaceholder: 'Type to send…',
    send: 'Send',
    timedLabel: 'Timed:',
    stopTimed: 'Stop',
    startTimed: 'Start',
    timedHint: 'Repeatedly sends the input box content',
    quickLabel: 'Quick:',
    quickEditTitle: 'right-click to edit',
    quickEditPlaceholder: 'command',
    filterLabel: 'Filter:',
    filterPlaceholder: 'Hide lines starting with…, comma-separated, e.g. DEBUG,INFO',
    autoReplyLabel: 'Auto-reply:',
    addRule: '+ Add rule',
    arWhen: 'On',
    arTrigger: 'prefix',
    arThen: '→ reply',
    arReply: 'reply (\\r \\n supported)',
    hintPower: 'Before connecting, make sure the board is connected via USB and V_IN is powered with 9–26 V.',
    hintBoot: 'Flashing requires download mode: ',
    hintBootSteps: 'hold BOOT → press RESET once → release RESET → release BOOT',
    hintBootAfter: ', then click “Connect Board”. The serial monitor needs none of this — just connect to a running board.',
  },
} as const

const t = computed(() => (isEn.value ? STRINGS.en : STRINGS.zh))

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

// Console control panel
const sendInput = ref('')
const addCRLF = ref(true)

// 6 quick-send slots, persisted to localStorage
const QUICK_KEY = 'osrcore-flash-quicksend'
const quickSends = ref<string[]>(['', '', '', '', '', ''])
const editingQuick = ref(-1)  // index being edited, -1 = none
const editingValue = ref('')

// Timed send
const timedEnabled = ref(false)
const timedInterval = ref(1000)  // ms
let timedTimer: ReturnType<typeof setInterval> | null = null

// Filter: comma-separated line prefixes to hide
const FILTER_KEY = 'osrcore-flash-filter'
const filterText = ref('')
const filterPrefixes = computed(() =>
  filterText.value.split(',').map(s => s.trim()).filter(Boolean)
)

// Auto-reply rules: when a line starts with trigger, send reply
const AUTOREPLY_KEY = 'osrcore-flash-autoreply'
interface AutoReplyRule { trigger: string; reply: string }
const autoReplyRules = ref<AutoReplyRule[]>([])

// Line-buffered processing is only needed when filter or auto-reply is active
const lineProcessing = computed(() =>
  filterPrefixes.value.length > 0 || autoReplyRules.value.length > 0
)
let lineBuffer = ''
const lineDecoder = new TextDecoder()

let term: any = null
let fitAddon: any = null
let transport: any = null
let esploader: any = null
let device: SerialPort | null = null
let consoleReader: ReadableStreamDefaultReader | null = null
let consoleAbort: AbortController | null = null

const statusLabel = computed(() => {
  switch (state.value) {
    case 'connecting':   return t.value.connecting
    case 'connected':    return t.value.detected(chipName.value)
    case 'downloading':  return t.value.downloading
    case 'flashing':     return t.value.flashing(progress.value)
    case 'done':         return t.value.done
    case 'console':      return t.value.monitoring
    case 'disconnected': return t.value.disconnected
    case 'error':        return t.value.error
    default:             return ''
  }
})

const disconnectHandler = (e: Event) => {
  const port = (e as any).port as SerialPort
  if (port !== device) return
  stopTimed()
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  term?.writeln(`\r\n\x1b[31m${t.value.termDisconnect}\x1b[0m`)
  openTerm()
  state.value = 'disconnected'
}

onMounted(async () => {
  if (typeof navigator === 'undefined') return
  isSupported.value = 'serial' in navigator
  if (!isSupported.value) return

  ;(navigator as any).serial.addEventListener('disconnect', disconnectHandler)

  // Load quick-send from localStorage
  try {
    const saved = localStorage.getItem(QUICK_KEY)
    if (saved) {
      const parsed = JSON.parse(saved)
      if (Array.isArray(parsed) && parsed.length === 6) quickSends.value = parsed
    }
  } catch {}

  // Load filter + auto-reply rules
  try {
    const f = localStorage.getItem(FILTER_KEY)
    if (f !== null) filterText.value = f
    const ar = localStorage.getItem(AUTOREPLY_KEY)
    if (ar) {
      const parsed = JSON.parse(ar)
      if (Array.isArray(parsed)) autoReplyRules.value = parsed
    }
  } catch {}

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
  stopTimed()
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
      errorMsg.value = t.value.chipUnsupported(chipName.value)
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
      if (!resp.ok) throw new Error(t.value.dlFail(resp.status))
      const blob = await resp.blob()
      data = await new Promise<string>((resolve, reject) => {
        const reader = new FileReader()
        reader.onload = e => resolve(e.target!.result as string)
        reader.onerror = () => reject(new Error(t.value.readFail))
        reader.readAsBinaryString(blob)
      })
      state.value = 'flashing'
    } else {
      throw new Error(t.value.noSource)
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
  lineBuffer = ''
  term?.writeln(`\r\n\x1b[32m${t.value.monitorStarted}\x1b[0m\r\n`)

  try {
    await device!.open({ baudRate: 115200 })
    consoleReader = (device as any).readable.getReader()

    while (!consoleAbort.signal.aborted) {
      const { value, done } = await consoleReader.read()
      if (done) break
      if (!value) continue
      if (lineProcessing.value) {
        processIncoming(value)
      } else {
        term?.write(value)
      }
    }
  } catch (e: any) {
    if (!consoleAbort?.signal.aborted && state.value !== 'disconnected') {
      term?.writeln(`\r\n\x1b[31m${t.value.serialError(e?.message)}\x1b[0m`)
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
  stopTimed()
  consoleAbort?.abort()
  try { consoleReader?.cancel() } catch {}
  await new Promise(r => setTimeout(r, 150))
  term?.writeln(`\r\n\x1b[33m${t.value.monitorStopped}\x1b[0m`)
}

async function writeToPort(text: string) {
  if (!device || state.value !== 'console' || !text) return
  try {
    const writer = (device as any).writable.getWriter()
    const encoder = new TextEncoder()
    let data = text
    if (addCRLF.value) data += '\r\n'
    await writer.write(encoder.encode(data))
    writer.releaseLock()
    term?.writeln(`\x1b[36m→ ${text}\x1b[0m`)
  } catch (e: any) {
    term?.writeln(`\x1b[31m${t.value.sendFail(e?.message)}\x1b[0m`)
  }
}

// Raw write without CR+LF logic / echo — used by auto-reply
async function writeRaw(text: string) {
  if (!device || state.value !== 'console') return
  try {
    const writer = (device as any).writable.getWriter()
    await writer.write(new TextEncoder().encode(text))
    writer.releaseLock()
  } catch {}
}

// Line-buffered incoming processing: filter + auto-reply
function processIncoming(chunk: Uint8Array) {
  lineBuffer += lineDecoder.decode(chunk, { stream: true })
  const lines = lineBuffer.split(/\r?\n/)
  lineBuffer = lines.pop() ?? ''  // keep trailing partial line

  for (const line of lines) {
    const hidden = filterPrefixes.value.some(p => line.startsWith(p))
    if (!hidden) term?.writeln(line)

    // Auto-reply: first matching rule wins
    for (const rule of autoReplyRules.value) {
      if (rule.trigger && line.startsWith(rule.trigger)) {
        const reply = rule.reply.replace(/\\r/g, '\r').replace(/\\n/g, '\n')
        writeRaw(reply)
        term?.writeln(`\x1b[35m↩ auto: ${rule.reply}\x1b[0m`)
        break
      }
    }
  }
}

function addAutoReply() {
  autoReplyRules.value.push({ trigger: '', reply: '' })
  persistAutoReply()
}

function removeAutoReply(i: number) {
  autoReplyRules.value.splice(i, 1)
  persistAutoReply()
}

function persistAutoReply() {
  localStorage.setItem(AUTOREPLY_KEY, JSON.stringify(autoReplyRules.value))
}

function persistFilter() {
  localStorage.setItem(FILTER_KEY, filterText.value)
}

async function sendData() {
  if (!sendInput.value) return
  await writeToPort(sendInput.value)
  sendInput.value = ''
}

function toggleTimed() {
  if (timedEnabled.value) {
    stopTimed()
  } else {
    if (!sendInput.value) {
      term?.writeln(`\x1b[33m${t.value.timedNeedInput}\x1b[0m`)
      return
    }
    const ms = Math.max(50, timedInterval.value || 1000)
    timedTimer = setInterval(() => { writeToPort(sendInput.value) }, ms)
    timedEnabled.value = true
  }
}

function stopTimed() {
  if (timedTimer) { clearInterval(timedTimer); timedTimer = null }
  timedEnabled.value = false
}

async function sendQuick(i: number) {
  const text = quickSends.value[i]
  if (text) await writeToPort(text)
}

function startEditQuick(i: number) {
  editingQuick.value = i
  editingValue.value = quickSends.value[i]
}

function saveEditQuick() {
  if (editingQuick.value < 0) return
  quickSends.value[editingQuick.value] = editingValue.value
  localStorage.setItem(QUICK_KEY, JSON.stringify(quickSends.value))
  editingQuick.value = -1
  editingValue.value = ''
}

function cancelEditQuick() {
  editingQuick.value = -1
  editingValue.value = ''
}

async function retry() {
  stopTimed()
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
          {{ props.label ?? t.title(props.example) }}
        </span>
        <span v-if="chipName && (state === 'done' || state === 'console')" class="flash-chip-badge">{{ chipName }}</span>
        <span
          v-if="['downloading','flashing','done','console','disconnected','error'].includes(state)"
          class="flash-status-text"
        >{{ statusLabel }}</span>
      </div>

      <!-- Browser not supported -->
      <div v-if="!isSupported" class="flash-unsupported">
        {{ t.unsupported }}
      </div>

      <!-- Controls -->
      <div v-else class="flash-controls">
        <!-- Idle: two entry points -->
        <template v-if="state === 'idle'">
          <button class="flash-btn flash-btn-primary" @click="connect">{{ t.connectBtn }}</button>
          <button class="flash-btn flash-btn-secondary" @click="openConsoleFromIdle">{{ t.monitorBtn }}</button>
        </template>

        <!-- Connecting -->
        <div v-if="state === 'connecting'" class="flash-progress-row">
          <div class="flash-spinner" />
          <span class="flash-progress-label">{{ t.connecting }}</span>
        </div>

        <!-- Connected — confirm before flash -->
        <div v-if="state === 'connected'" class="flash-connected-row">
          <span class="flash-connected-badge">{{ t.connected }}</span>
          <span class="flash-chip-inline">{{ chipName }}</span>
          <button class="flash-btn flash-btn-primary" @click="startFlash">{{ t.startFlash }}</button>
          <button class="flash-btn flash-btn-ghost" @click="retry">{{ t.cancel }}</button>
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
          <span class="flash-done-badge">{{ t.flashOk }}</span>
          <button class="flash-btn flash-btn-secondary" @click="openConsoleAfterFlash">{{ t.openMonitor }}</button>
          <button class="flash-btn flash-btn-ghost" @click="retry">{{ t.reflash }}</button>
        </div>

        <!-- Console running -->
        <div v-if="state === 'console'" class="flash-console-row">
          <span class="flash-console-badge">{{ t.monitoringBadge }}</span>
          <button class="flash-btn flash-btn-stop" @click="stopConsole">{{ t.stopMonitor }}</button>
          <button v-if="consoleOrigin === 'flash'" class="flash-btn flash-btn-ghost" disabled>{{ t.reflash }}</button>
        </div>

        <!-- Disconnected -->
        <div v-if="state === 'disconnected'" class="flash-disconnected-row">
          <span class="flash-disconnected-badge">{{ t.devDisconnected }}</span>
          <button class="flash-btn flash-btn-ghost" @click="retry">{{ t.back }}</button>
        </div>

        <!-- Error -->
        <div v-if="state === 'error'" class="flash-error-row">
          <span class="flash-error-badge">{{ t.errBadge }}</span>
          <span class="flash-error-msg">{{ errorMsg }}</span>
          <button class="flash-btn flash-btn-ghost" @click="retry">{{ t.retry }}</button>
        </div>
      </div>

      <!-- xterm terminal -->
      <div v-show="termVisible" class="flash-term-wrap">
        <div ref="termRef" class="flash-term" />
      </div>

      <!-- Console control panel (visible when in console state) -->
      <div v-if="state === 'console'" class="console-panel">
        <div class="console-send-row">
          <input
            v-model="sendInput"
            class="console-send-input"
            :placeholder="t.sendPlaceholder"
            @keydown.enter="sendData"
          />
          <label class="console-crlf">
            <input type="checkbox" v-model="addCRLF" /> CR+LF
          </label>
          <button class="flash-btn flash-btn-primary console-send-btn" @click="sendData">{{ t.send }}</button>
        </div>

        <!-- Timed send -->
        <div class="console-timed-row">
          <span class="console-quick-label">{{ t.timedLabel }}</span>
          <input
            v-model.number="timedInterval"
            type="number"
            min="50"
            step="50"
            class="console-timed-input"
            :disabled="timedEnabled"
          />
          <span class="console-timed-unit">ms</span>
          <button
            class="console-timed-btn"
            :class="{ 'is-active': timedEnabled }"
            @click="toggleTimed"
          >{{ timedEnabled ? t.stopTimed : t.startTimed }}</button>
          <span v-if="timedEnabled" class="console-timed-hint">{{ t.timedHint }}</span>
        </div>

        <!-- Quick send -->
        <div class="console-quick-row">
          <span class="console-quick-label">{{ t.quickLabel }}</span>
          <template v-for="(qs, i) in quickSends" :key="i">
            <button
              v-if="editingQuick !== i"
              class="console-quick-btn"
              :class="{ 'is-empty': !qs }"
              :title="qs || t.quickEditTitle"
              @click="qs ? sendQuick(i) : startEditQuick(i)"
              @contextmenu.prevent="startEditQuick(i)"
            >{{ qs || `${i + 1}` }}</button>
            <div v-else class="console-quick-edit">
              <input
                v-model="editingValue"
                class="console-quick-edit-input"
                :placeholder="t.quickEditPlaceholder"
                @keydown.enter="saveEditQuick"
                @keydown.escape="cancelEditQuick"
              />
              <button class="console-quick-edit-ok" @click="saveEditQuick">✓</button>
              <button class="console-quick-edit-cancel" @click="cancelEditQuick">✗</button>
            </div>
          </template>
        </div>

        <!-- Filter -->
        <div class="console-filter-row">
          <span class="console-quick-label">{{ t.filterLabel }}</span>
          <input
            v-model="filterText"
            class="console-filter-input"
            :placeholder="t.filterPlaceholder"
            @change="persistFilter"
          />
        </div>

        <!-- Auto-reply -->
        <div class="console-autoreply">
          <div class="console-autoreply-head">
            <span class="console-quick-label">{{ t.autoReplyLabel }}</span>
            <button class="console-ar-add" @click="addAutoReply">{{ t.addRule }}</button>
          </div>
          <div
            v-for="(rule, i) in autoReplyRules"
            :key="i"
            class="console-ar-rule"
          >
            <span class="console-ar-when">{{ t.arWhen }}</span>
            <input
              v-model="rule.trigger"
              class="console-ar-input"
              :placeholder="t.arTrigger"
              @change="persistAutoReply"
            />
            <span class="console-ar-then">{{ t.arThen }}</span>
            <input
              v-model="rule.reply"
              class="console-ar-input"
              :placeholder="t.arReply"
              @change="persistAutoReply"
            />
            <button class="console-ar-del" @click="removeAutoReply(i)">✗</button>
          </div>
        </div>
      </div>

      <!-- Hint (idle only) -->
      <div v-if="isSupported && state === 'idle'" class="flash-hint">
        <div>{{ t.hintPower }}</div>
        <div class="flash-hint-boot">
          <span class="flash-hint-boot-icon">💡</span>
          <span>
            {{ t.hintBoot }}<strong>{{ t.hintBootSteps }}</strong>{{ t.hintBootAfter }}
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

/* Console control panel */
.console-panel {
  margin-top: 12px;
  padding: 12px 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}

.console-send-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.console-send-input {
  flex: 1;
  padding: 7px 10px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  font-size: 13px;
  outline: none;
  font-family: var(--vp-font-family-mono);
}
.console-send-input:focus { border-color: var(--vp-c-brand-1); }

.console-crlf {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  color: var(--vp-c-text-2);
  cursor: pointer;
  user-select: none;
  white-space: nowrap;
}

.console-send-btn {
  padding: 7px 16px;
  font-size: 13px;
}

/* Timed send */
.console-timed-row {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 8px;
  flex-wrap: wrap;
}

.console-timed-input {
  width: 70px;
  padding: 4px 8px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  outline: none;
}
.console-timed-input:focus { border-color: var(--vp-c-brand-1); }
.console-timed-input:disabled { opacity: 0.5; }

.console-timed-unit {
  font-size: 12px;
  color: var(--vp-c-text-3);
}

.console-timed-btn {
  padding: 4px 12px;
  font-size: 12px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  cursor: pointer;
  transition: all 0.15s;
}
.console-timed-btn:hover { border-color: var(--vp-c-brand-1); }
.console-timed-btn.is-active {
  background: rgba(239,68,68,0.12);
  color: #ef4444;
  border-color: rgba(239,68,68,0.3);
}

.console-timed-hint {
  font-size: 11px;
  color: var(--vp-c-text-3);
}

/* Quick send buttons */
.console-quick-row {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 8px;
  flex-wrap: wrap;
}

.console-quick-label {
  font-size: 12px;
  color: var(--vp-c-text-3);
  white-space: nowrap;
}

.console-quick-btn {
  padding: 4px 10px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  cursor: pointer;
  max-width: 80px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  transition: border-color 0.15s;
}
.console-quick-btn:hover { border-color: var(--vp-c-brand-1); }
.console-quick-btn.is-empty {
  color: var(--vp-c-text-3);
  border-style: dashed;
}

.console-quick-edit {
  display: flex;
  align-items: center;
  gap: 3px;
}

.console-quick-edit-input {
  width: 90px;
  padding: 3px 6px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
  border: 1px solid var(--vp-c-brand-1);
  border-radius: 4px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  outline: none;
}

.console-quick-edit-ok,
.console-quick-edit-cancel {
  border: none;
  background: none;
  cursor: pointer;
  font-size: 14px;
  padding: 2px 4px;
}
.console-quick-edit-ok { color: #22c55e; }
.console-quick-edit-cancel { color: #ef4444; }

/* Filter */
.console-filter-row {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 8px;
}

.console-filter-input {
  flex: 1;
  padding: 4px 8px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  outline: none;
}
.console-filter-input:focus { border-color: var(--vp-c-brand-1); }

/* Auto-reply */
.console-autoreply {
  margin-top: 10px;
  padding-top: 10px;
  border-top: 1px dashed var(--vp-c-divider);
}

.console-autoreply-head {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 6px;
}

.console-ar-add {
  font-size: 12px;
  padding: 3px 10px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-brand-1);
  cursor: pointer;
}
.console-ar-add:hover { border-color: var(--vp-c-brand-1); }

.console-ar-rule {
  display: flex;
  align-items: center;
  gap: 5px;
  margin-bottom: 5px;
  flex-wrap: wrap;
}

.console-ar-when,
.console-ar-then {
  font-size: 12px;
  color: var(--vp-c-text-3);
  white-space: nowrap;
}

.console-ar-input {
  width: 120px;
  padding: 4px 8px;
  font-size: 12px;
  font-family: var(--vp-font-family-mono);
  border: 1px solid var(--vp-c-divider);
  border-radius: 5px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  outline: none;
}
.console-ar-input:focus { border-color: var(--vp-c-brand-1); }

.console-ar-del {
  border: none;
  background: none;
  color: #ef4444;
  cursor: pointer;
  font-size: 13px;
  padding: 2px 4px;
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
