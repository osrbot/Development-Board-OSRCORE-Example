<script setup lang="ts">
import { computed, ref, onUnmounted } from 'vue'
import { withBase } from 'vitepress'

const props = defineProps<{ locale?: string }>()
const isEn = computed(() => props.locale === 'en')

type Mode = 'ota' | 'recovery'
type State = 'idle' | 'connecting' | 'connected' | 'running' | 'done' | 'error'

const RELEASE_BASE = '/firmware/'
const DEFAULT_APP = 'osrbot_ESP32S3_IDF_App.bin'
const DEFAULT_FULL = 'osrbot_ESP32S3_IDF_FullFlash.bin'

const USB_FILTERS = [
  { usbVendorId: 0x303a, usbProductId: 0x1001 },
  { usbVendorId: 0x303a, usbProductId: 0x0009 },
  { usbVendorId: 0x303a, usbProductId: 0x1002 },
  { usbVendorId: 0x10c4, usbProductId: 0xea60 },
  { usbVendorId: 0x1a86, usbProductId: 0x55d4 },
  { usbVendorId: 0x1a86, usbProductId: 0x7523 },
]

const mode = ref<Mode>('ota')
const state = ref<State>('idle')
const progress = ref(0)
const errorMsg = ref('')
const logLines = ref<string[]>([])
const useRemote = ref(true)
const forceLowVoltage = ref(false)
const appUrl = ref(RELEASE_BASE + DEFAULT_APP)
const fullUrl = ref(RELEASE_BASE + DEFAULT_FULL)
const localApp = ref<ArrayBuffer | null>(null)
const localFull = ref<ArrayBuffer | null>(null)
const localAppName = ref('')
const localFullName = ref('')
const commandInput = ref('')

let port: SerialPort | null = null
let reader: ReadableStreamDefaultReader<Uint8Array> | null = null
let writer: WritableStreamDefaultWriter<Uint8Array> | null = null
let readAbort = false
let lineBuffer = ''
let waiters: Array<(line: string) => boolean> = []

const isSupported = computed(() => typeof navigator !== 'undefined' && 'serial' in navigator)
const selectedFileName = computed(() => mode.value === 'ota' ? localAppName.value : localFullName.value)
const actionText = computed(() => {
  if (mode.value === 'ota') return isEn.value ? 'Update app without BOOT' : '普通升级：不进 BOOT，只更新 app'
  return isEn.value ? 'Recovery full flash' : '恢复刷机：进 BOOT，全量恢复'
})

function t(zh: string, en: string) {
  return isEn.value ? en : zh
}

function log(line: string) {
  logLines.value.push(line)
  if (logLines.value.length > 400) logLines.value.shift()
}

function reset(clearLog = false) {
  state.value = 'idle'
  progress.value = 0
  errorMsg.value = ''
  if (clearLog) logLines.value = []
}

async function readFile(file: File, target: 'app' | 'full') {
  const data = await file.arrayBuffer()
  if (target === 'app') {
    localApp.value = data
    localAppName.value = file.name
  } else {
    localFull.value = data
    localFullName.value = file.name
  }
}

async function onFile(e: Event, target: 'app' | 'full') {
  const file = (e.target as HTMLInputElement).files?.[0]
  if (file) await readFile(file, target)
}

async function fetchBinary(url: string) {
  const resolvedUrl = /^https?:\/\//.test(url) ? url : withBase(url)
  const resp = await fetch(resolvedUrl)
  if (!resp.ok) throw new Error(`HTTP ${resp.status}: ${url}`)
  return await resp.arrayBuffer()
}

async function getImageData() {
  if (mode.value === 'ota') {
    if (useRemote.value) return await fetchBinary(appUrl.value)
    if (!localApp.value) throw new Error(t('请选择 app bin 文件', 'Select an app bin file'))
    return localApp.value
  }

  if (useRemote.value) return await fetchBinary(fullUrl.value)
  if (!localFull.value) throw new Error(t('请选择 full flash bin 文件', 'Select a full flash bin file'))
  return localFull.value
}

function toHex(buffer: ArrayBuffer) {
  return Array.from(new Uint8Array(buffer))
    .map(v => v.toString(16).padStart(2, '0'))
    .join('')
}

async function sha256(buffer: ArrayBuffer) {
  return toHex(await crypto.subtle.digest('SHA-256', buffer))
}

function chunkHex(bytes: Uint8Array) {
  let out = ''
  for (const b of bytes) out += b.toString(16).padStart(2, '0')
  return out
}

async function closePort() {
  readAbort = true
  try { await reader?.cancel() } catch {}
  try { reader?.releaseLock() } catch {}
  try { writer?.releaseLock() } catch {}
  try { await port?.close() } catch {}
  reader = null
  writer = null
  port = null
  waiters = []
}

async function openSerial(baudRate = 460800) {
  port = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
  await port!.open({ baudRate })
  reader = (port as any).readable.getReader()
  writer = (port as any).writable.getWriter()
  readAbort = false
  readLoop()
}

async function readLoop() {
  const decoder = new TextDecoder()
  try {
    while (!readAbort && reader) {
      const { value, done } = await reader.read()
      if (done || !value) break
      lineBuffer += decoder.decode(value, { stream: true })
      const lines = lineBuffer.split(/\r?\n/)
      lineBuffer = lines.pop() ?? ''
      for (const line of lines) {
        const clean = line.trim()
        if (!clean) continue
        log(clean)
        waiters = waiters.filter(fn => !fn(clean))
      }
    }
  } catch (e: any) {
    if (!readAbort) log(`SERIAL ERROR: ${e?.message || e}`)
  }
}

async function writeLine(line: string) {
  if (!writer) throw new Error('serial port is not open')
  await writer.write(new TextEncoder().encode(line + '\n'))
  log(`> ${line}`)
}

function waitForFw(timeoutMs = 5000) {
  return new Promise<string>((resolve, reject) => {
    const timer = setTimeout(() => {
      waiters = waiters.filter(w => w !== waiter)
      reject(new Error(t('等待固件响应超时', 'Timeout waiting for firmware response')))
    }, timeoutMs)
    const waiter = (line: string) => {
      if (!line.startsWith('OK fw') && !line.startsWith('ERROR fw')) return false
      clearTimeout(timer)
      if (line.startsWith('ERROR fw')) reject(new Error(line))
      else resolve(line)
      return true
    }
    waiters.push(waiter)
  })
}

async function sendFw(line: string, timeoutMs = 5000) {
  const pending = waitForFw(timeoutMs)
  await writeLine(line)
  return await pending
}

async function runOta() {
  reset(true)
  state.value = 'connecting'
  log(t('连接运行中的设备，普通升级不需要进入 BOOT。', 'Connecting to a running device. BOOT mode is not required for app OTA.'))
  const data = await getImageData()
  const bytes = new Uint8Array(data)
  const digest = await sha256(data)

  await openSerial(460800)
  state.value = 'running'
  progress.value = 0

  try {
    await writeLine('stream off')
    await new Promise(r => setTimeout(r, 200))
    try { await sendFw('fw abort', 1000) } catch {}

    const force = forceLowVoltage.value ? ' force' : ''
    await sendFw(`fw begin ${bytes.length} ${digest}${force}`)

    const chunkSize = 128
    const total = Math.ceil(bytes.length / chunkSize)
    for (let seq = 0, offset = 0; offset < bytes.length; seq++, offset += chunkSize) {
      const chunk = bytes.slice(offset, Math.min(offset + chunkSize, bytes.length))
      await sendFw(`fw data ${seq} ${chunkHex(chunk)}`)
      progress.value = Math.round(((seq + 1) / total) * 100)
    }

    await sendFw('fw end', 8000)
    state.value = 'done'
    log(t('升级完成，设备正在重启。', 'Update complete. Device is rebooting.'))
  } finally {
    await closePort()
  }
}

async function runRecovery() {
  reset(true)
  state.value = 'connecting'
  log(t('恢复刷机需要设备处于 BOOT 下载模式。', 'Recovery flashing requires BOOT download mode.'))
  const data = await getImageData()
  const { ESPLoader, Transport } = await import('esptool-js')
  const device = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
  const transport = new Transport(device)
  const terminal = {
    clean() {},
    writeLine(line: string) { log(line) },
    write(data: string) { log(data) },
  }

  try {
    const loader = new ESPLoader({ transport, baudrate: 460800, terminal })
    const chip = await loader.main()
    if (!String(chip).startsWith('ESP32-S3')) throw new Error(`Unsupported chip: ${chip}`)
    state.value = 'running'
    await loader.writeFlash({
      fileArray: [{ data: arrayBufferToBinaryString(data), address: 0 }],
      flashSize: 'keep',
      eraseAll: true,
      compress: true,
      reportProgress(_: number, written: number, total: number) {
        progress.value = Math.round((written / total) * 100)
      },
    })
    await loader.after()
    state.value = 'done'
    log(t('恢复刷机完成，设备正在重启。', 'Recovery flash complete. Device is rebooting.'))
  } finally {
    try { await transport.disconnect() } catch {}
  }
}

function arrayBufferToBinaryString(buffer: ArrayBuffer) {
  const bytes = new Uint8Array(buffer)
  let out = ''
  const step = 0x8000
  for (let i = 0; i < bytes.length; i += step) {
    out += String.fromCharCode(...bytes.subarray(i, i + step))
  }
  return out
}

async function start() {
  if (!isSupported.value) return
  try {
    if (mode.value === 'ota') await runOta()
    else await runRecovery()
  } catch (e: any) {
    errorMsg.value = e?.message || String(e)
    log(`ERROR: ${errorMsg.value}`)
    state.value = 'error'
    await closePort()
  }
}

async function sendManualCommand() {
  if (!commandInput.value) return
  if (!port) await openSerial(460800)
  await writeLine(commandInput.value)
  commandInput.value = ''
}

onUnmounted(closePort)
</script>

<template>
  <ClientOnly>
    <div class="fw-updater">
      <div class="mode-row">
        <button :class="{ active: mode === 'ota' }" @click="mode = 'ota'; reset()">
          {{ t('普通升级', 'App update') }}
        </button>
        <button :class="{ active: mode === 'recovery' }" @click="mode = 'recovery'; reset()">
          {{ t('恢复刷机', 'Recovery flash') }}
        </button>
      </div>

      <div class="explain">
        <strong>{{ actionText }}</strong>
        <p v-if="mode === 'ota'">
          {{ t('用于已经运行新固件的设备，只更新 app 分区。设备正常开机后直接连接，不需要按 BOOT。', 'For devices already running the new firmware. It updates the app partition only and does not require BOOT mode.') }}
        </p>
        <p v-else>
          {{ t('用于第一次刷机、恢复出厂或固件无法启动。请先让设备进入 BOOT 下载模式，再开始全量刷机。', 'For first flash, factory recovery, or broken firmware. Put the device into BOOT download mode before flashing.') }}
        </p>
      </div>

      <div class="source-row">
        <label><input type="radio" :checked="useRemote" @change="useRemote = true"> {{ t('使用远端固件 URL', 'Use remote firmware URL') }}</label>
        <label><input type="radio" :checked="!useRemote" @change="useRemote = false"> {{ t('选择本地 .bin 文件', 'Select local .bin file') }}</label>
      </div>

      <div v-if="useRemote" class="field">
        <label>{{ mode === 'ota' ? 'App URL' : 'Full flash URL' }}</label>
        <input v-if="mode === 'ota'" v-model="appUrl" />
        <input v-else v-model="fullUrl" />
      </div>

      <div v-else class="field">
        <label>{{ mode === 'ota' ? t('App bin 文件', 'App bin file') : t('Full flash bin 文件', 'Full flash bin file') }}</label>
        <input v-if="mode === 'ota'" type="file" accept=".bin" @change="onFile($event, 'app')" />
        <input v-else type="file" accept=".bin" @change="onFile($event, 'full')" />
        <span v-if="selectedFileName" class="file-name">{{ selectedFileName }}</span>
      </div>

      <label v-if="mode === 'ota'" class="check-row">
        <input type="checkbox" v-model="forceLowVoltage">
        {{ t('低电压测试时强制升级', 'Force update during low-voltage testing') }}
      </label>

      <div v-if="!isSupported" class="error">
        {{ t('当前浏览器不支持 Web Serial API，请使用 Chrome 或 Edge。', 'This browser does not support Web Serial API. Use Chrome or Edge.') }}
      </div>

      <div class="action-row">
        <button class="primary" :disabled="state === 'connecting' || state === 'running' || !isSupported" @click="start">
          {{ state === 'running' || state === 'connecting' ? t('执行中...', 'Running...') : t('开始', 'Start') }}
        </button>
        <button class="secondary" @click="reset(true)">{{ t('清空日志', 'Clear log') }}</button>
      </div>

      <div v-if="state === 'running' || state === 'done'" class="progress-wrap">
        <div class="progress-bar" :style="{ width: progress + '%' }"></div>
      </div>

      <div v-if="state === 'done'" class="ok">{{ t('完成', 'Done') }}</div>
      <div v-if="state === 'error'" class="error">{{ errorMsg }}</div>

      <div class="manual">
        <input v-model="commandInput" :placeholder="t('串口命令，例如 status', 'Serial command, e.g. status')" @keydown.enter="sendManualCommand" />
        <button class="secondary" @click="sendManualCommand">{{ t('发送', 'Send') }}</button>
      </div>

      <pre class="log">{{ logLines.join('\n') }}</pre>
    </div>
  </ClientOnly>
</template>

<style scoped>
.fw-updater {
  margin: 24px 0;
  padding: 18px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  background: var(--vp-c-bg-soft);
}
.mode-row, .source-row, .action-row, .manual {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: center;
  margin: 12px 0;
}
button {
  border: 1px solid var(--vp-c-divider);
  border-radius: 7px;
  padding: 8px 14px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  cursor: pointer;
}
button.active, button.primary {
  background: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
  color: #fff;
}
button:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}
.secondary {
  background: transparent;
}
.explain {
  margin: 14px 0;
  color: var(--vp-c-text-2);
}
.explain p {
  margin: 6px 0 0;
}
.field {
  display: grid;
  grid-template-columns: 140px 1fr;
  gap: 10px;
  align-items: center;
  margin: 12px 0;
}
.field input, .manual input {
  width: 100%;
  min-width: 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 7px;
  padding: 8px 10px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}
.file-name {
  grid-column: 2;
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.check-row {
  display: flex;
  gap: 8px;
  align-items: center;
  margin: 10px 0;
  color: var(--vp-c-text-2);
}
.progress-wrap {
  height: 8px;
  border-radius: 4px;
  background: var(--vp-c-divider);
  overflow: hidden;
  margin: 14px 0;
}
.progress-bar {
  height: 100%;
  background: var(--vp-c-brand-1);
  transition: width 0.2s;
}
.ok {
  color: var(--vp-c-brand-1);
  font-weight: 600;
}
.error {
  color: #dc2626;
  margin: 10px 0;
}
.manual input {
  flex: 1;
}
.log {
  min-height: 220px;
  max-height: 420px;
  overflow: auto;
  margin: 12px 0 0;
  padding: 12px;
  border-radius: 8px;
  background: #0d1117;
  color: #e6edf3;
  font-size: 12px;
  white-space: pre-wrap;
}
@media (max-width: 640px) {
  .field {
    grid-template-columns: 1fr;
  }
  .file-name {
    grid-column: 1;
  }
}
</style>
