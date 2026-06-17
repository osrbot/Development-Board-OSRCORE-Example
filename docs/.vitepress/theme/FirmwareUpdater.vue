<script setup lang="ts">
import { computed, ref, onMounted, onUnmounted } from 'vue'
import { withBase } from 'vitepress'

const props = defineProps<{ locale?: string }>()
const isEn = computed(() => props.locale === 'en')

type Mode = 'ota' | 'recovery'
type State = 'idle' | 'connecting' | 'connected' | 'running' | 'done' | 'error'

const RELEASE_BASE = '/firmware/'
const DEFAULT_APP = 'osrbot_ESP32S3_IDF_App.bin'
const DEFAULT_FULL = 'osrbot_ESP32S3_IDF_FullFlash.bin'
const POST_FLASH_RELOAD_DELAY_MS = 1000
const CUSTOM_ID = 'custom'
const FACTORY_FULL_ID = 'factory_full'

interface ManifestEntry {
  id: string
  label: string
  bin: string
}

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
const firmwareChoice = ref('')
const manifestExamples = ref<ManifestEntry[]>([])
const manifestError = ref('')
const localApp = ref<ArrayBuffer | null>(null)
const localFull = ref<ArrayBuffer | null>(null)
const localAppName = ref('')
const localFullName = ref('')
const commandInput = ref('')
const selectedPort = ref<SerialPort | null>(null)
const selectedPortLabel = ref('')

let port: SerialPort | null = null
let reader: ReadableStreamDefaultReader<Uint8Array> | null = null
let writer: WritableStreamDefaultWriter<Uint8Array> | null = null
let readAbort = false
let lineBuffer = ''
let waiters: Array<(line: string) => boolean> = []
let sawRomBoot = false
let reloadTimer: ReturnType<typeof setTimeout> | null = null

const isSupported = computed(() => typeof navigator !== 'undefined' && 'serial' in navigator)
const isSerialOpen = computed(() => !!port)
const selectedFileName = computed(() => mode.value === 'ota' ? localAppName.value : localFullName.value)
const isFirmwareLoading = computed(() => useRemote.value && mode.value === 'ota' && firmwareChoice.value === 'loading')
const urlHelp = computed(() => {
  if (mode.value === 'ota') {
    return t('自定义 URL 必须填写完整 https:// 地址。', 'Custom URL must be a full https:// address.')
  }
  return t('恢复出厂使用站内 full flash 固件，不需要填写链接。', 'Factory restore uses the built-in full flash image; no URL is required.')
})
const firmwareOptions = computed(() => {
  if (mode.value === 'ota') {
    const options = manifestExamples.value.map(ex => ({
      value: `example:${ex.id}`,
      label: ex.label,
      url: appBinFromFull(ex.bin),
    }))
    if (!options.length) {
      options.push({
        value: 'loading',
        label: manifestError.value ? t('例程清单加载失败', 'Failed to load example list') : t('正在加载当前仓库例程...', 'Loading repository examples...'),
        url: '',
      })
    }
    options.push({
      value: CUSTOM_ID,
      label: t('自定义 HTTPS URL', 'Custom HTTPS URL'),
      url: '',
    })
    return options
  }
  return [
  {
    value: FACTORY_FULL_ID,
    label: t('恢复出厂固件', 'Factory restore firmware'),
    url: RELEASE_BASE + DEFAULT_FULL,
  },
]
})
const actionText = computed(() => {
  if (mode.value === 'ota') return isEn.value ? 'Flashing: update app while the device is running' : '在线烧录：设备正常运行时只更新 app'
  return isEn.value ? 'Factory restore full flash' : '恢复出厂：进 BOOT，全量恢复'
})

function t(zh: string, en: string) {
  return isEn.value ? en : zh
}

function log(line: string) {
  logLines.value.push(line)
  if (logLines.value.length > 400) logLines.value.shift()
}

function reset(clearLog = false) {
  if (reloadTimer) {
    clearTimeout(reloadTimer)
    reloadTimer = null
  }
  state.value = 'idle'
  progress.value = 0
  errorMsg.value = ''
  sawRomBoot = false
  if (clearLog) logLines.value = []
}

function appBinFromFull(bin: string) {
  if (/-full\.bin$/i.test(bin)) return bin.replace(/-full\.bin$/i, '-app.bin')
  return bin
}

function setDefaultFirmwareChoice() {
  if (mode.value === 'ota') {
    firmwareChoice.value = manifestExamples.value[0] ? `example:${manifestExamples.value[0].id}` : 'loading'
  } else {
    firmwareChoice.value = FACTORY_FULL_ID
  }
  selectFirmwareUrl()
}

function selectFirmwareUrl() {
  const item = firmwareOptions.value.find(opt => opt.value === firmwareChoice.value)
  if (!item || item.value === CUSTOM_ID || item.value === 'loading') return
  if (mode.value === 'ota') appUrl.value = item.url
  else fullUrl.value = item.url
}

function selectedExampleFullUrl() {
  if (!firmwareChoice.value.startsWith('example:')) return ''
  const id = firmwareChoice.value.slice('example:'.length)
  return manifestExamples.value.find(ex => ex.id === id)?.bin || ''
}

function switchMode(next: Mode) {
  mode.value = next
  setDefaultFirmwareChoice()
  reset()
}

onMounted(async () => {
  try {
    const res = await fetch(withBase('/firmware/manifest.json'))
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const data = await res.json()
    manifestExamples.value = Array.isArray(data.examples) ? data.examples : []
  } catch (e: any) {
    manifestError.value = e?.message || String(e)
  }
  setDefaultFirmwareChoice()
})

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
    if (useRemote.value) {
      if (firmwareChoice.value === 'loading') throw new Error(t('例程固件清单还没有加载完成', 'Example firmware list is still loading'))
      if (firmwareChoice.value === CUSTOM_ID && !/^https:\/\//.test(appUrl.value)) {
        throw new Error(t('自定义 URL 必须是完整 https:// 地址', 'Custom URL must be a full https:// address'))
      }
      return await fetchBinary(appUrl.value)
    }
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
  lineBuffer = ''
  waiters = []
}

async function finishFlashAndReload(doneMessage: string) {
  state.value = 'done'
  progress.value = 100
  log(doneMessage)
  log(t(
    '烧录完成，正在关闭串口连接；页面将在 1 秒后刷新。刷新后请重新选择串口，再打开串口监视器。',
    'Flashing complete. Closing the serial connection; this page will reload in 1 second. Select the serial port again after reload, then open the monitor.'
  ))
  await closePort()
  selectedPort.value = null
  selectedPortLabel.value = ''
  reloadTimer = setTimeout(() => {
    window.location.reload()
  }, POST_FLASH_RELOAD_DELAY_MS)
}

async function disconnectSerial() {
  await closePort()
  state.value = 'idle'
  log(t('串口已断开。', 'Serial disconnected.'))
}

function describePort(p: SerialPort) {
  const info = (p as any).getInfo?.() ?? {}
  const vid = info.usbVendorId !== undefined ? `VID ${info.usbVendorId.toString(16).padStart(4, '0')}` : ''
  const pid = info.usbProductId !== undefined ? `PID ${info.usbProductId.toString(16).padStart(4, '0')}` : ''
  return [vid, pid].filter(Boolean).join(' / ') || t('已选择串口', 'Serial port selected')
}

function isEspUsbJtagPort(p: SerialPort) {
  const info = (p as any).getInfo?.() ?? {}
  return info.usbVendorId === 0x303a && info.usbProductId === 0x1001
}

async function selectSerialPort() {
  selectedPort.value = await (navigator as any).serial.requestPort({ filters: USB_FILTERS })
  selectedPortLabel.value = describePort(selectedPort.value!)
  log(t(`已选择串口：${selectedPortLabel.value}`, `Selected serial port: ${selectedPortLabel.value}`))
}

async function openSerial(baudRate = 460800, requestIfMissing = true) {
  if (port) return
  if (!selectedPort.value && requestIfMissing) await selectSerialPort()
  if (!selectedPort.value) throw new Error(t('请先选择串口', 'Select a serial port first'))
  port = selectedPort.value
  await port!.open({ baudRate })
  reader = (port as any).readable.getReader()
  writer = (port as any).writable.getWriter()
  readAbort = false
  state.value = 'connected'
  log(t(`串口已连接：${selectedPortLabel.value || describePort(port!)}`, `Serial connected: ${selectedPortLabel.value || describePort(port!)}`))
  readLoop()
}

async function connectMonitor() {
  errorMsg.value = ''
  try {
    await openSerial(460800, false)
  } catch (e: any) {
    errorMsg.value = e?.message || String(e)
    state.value = 'error'
    log(`ERROR: ${errorMsg.value}`)
  }
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
        if (/^ESP-ROM:/i.test(clean)) sawRomBoot = true
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
      if (/^ERROR\b.*unknown command/i.test(line)) {
        clearTimeout(timer)
        reject(Object.assign(new Error(t(
          '当前设备固件不支持在线烧录协议。请切换到“恢复出厂”，让设备进入 BOOT 下载模式后再烧录。',
          'The current device firmware does not support the online flashing protocol. Switch to Factory restore, put the device into BOOT download mode, and flash again.'
        )), { code: 'FW_UNSUPPORTED' }))
        return true
      }
      if (!line.startsWith('OK fw') && !line.startsWith('ERROR fw')) return false
      clearTimeout(timer)
      if (line.startsWith('ERROR fw')) reject(Object.assign(new Error(line), { code: 'FW_ERROR', line }))
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
  log(t('连接设备：优先使用 fw 协议更新 app；如果检测到 ESP-ROM 下载模式，将自动刷写所选例程 full flash。', 'Connecting to the device: use the fw protocol for app update first; if ESP-ROM download mode is detected, automatically flash the selected example full image.'))
  const data = await getImageData()
  const bytes = new Uint8Array(data)
  const digest = await sha256(data)

  await openSerial(460800)
  state.value = 'running'
  progress.value = 0

  try {
    await writeLine('stream off')
    await new Promise(r => setTimeout(r, 200))
    try {
      await sendFw('fw abort', 1000)
    } catch (e: any) {
      if (e?.code === 'FW_UNSUPPORTED') {
        if (sawRomBoot) {
          const fullData = await getSelectedExampleFullImage()
          log(t('检测到设备已在 ESP-ROM 下载模式，自动改用 full flash 烧录当前选择的例程。', 'ESP-ROM download mode detected; switching automatically to full flash for the selected example.'))
          await closePort()
          await runFullFlash(fullData, t('正在全量烧录所选例程。', 'Flashing the selected example full image.'), t('烧录完成，设备正在重启。', 'Flashing complete. Device is rebooting.'))
          return
        }
        throw e
      }
    }

    const force = forceLowVoltage.value ? ' force' : ''
    try {
      await sendFw(`fw begin ${bytes.length} ${digest}${force}`)
    } catch (e: any) {
      if (/^ERROR fw begin invalid/.test(e?.line || e?.message || '')) {
        const fullData = await getSelectedExampleFullImage()
        log(t('当前固件的 fw begin 响应异常，自动改用 full flash 烧录当前选择的例程。', 'The current firmware rejected fw begin; switching automatically to full flash for the selected example.'))
        await closePort()
        await runFullFlash(fullData, t('正在全量烧录所选例程。', 'Flashing the selected example full image.'), t('烧录完成，设备正在重启。', 'Flashing complete. Device is rebooting.'))
        return
      }
      throw e
    }

    const chunkSize = 128
    const total = Math.ceil(bytes.length / chunkSize)
    for (let seq = 0, offset = 0; offset < bytes.length; seq++, offset += chunkSize) {
      const chunk = bytes.slice(offset, Math.min(offset + chunkSize, bytes.length))
      await sendFw(`fw data ${seq} ${chunkHex(chunk)}`)
      progress.value = Math.round(((seq + 1) / total) * 100)
    }

    await sendFw('fw end', 8000)
    await finishFlashAndReload(t('升级完成，设备正在重启。', 'Update complete. Device is rebooting.'))
  } finally {
    if (state.value !== 'done') await closePort()
  }
}

async function getSelectedExampleFullImage() {
  if (!useRemote.value) {
    throw new Error(t('本地 app 文件没有对应 full flash 镜像，无法自动从 ESP-ROM 模式烧录。请切换到“恢复出厂”并选择 full flash bin。', 'A local app file has no matching full flash image, so it cannot be flashed automatically from ESP-ROM mode. Switch to Factory restore and select a full flash bin.'))
  }
  if (firmwareChoice.value === CUSTOM_ID) {
    throw new Error(t('自定义 App URL 没有对应 full flash 镜像，无法自动从 ESP-ROM 模式烧录。请切换到“恢复出厂”。', 'A custom app URL has no matching full flash image, so it cannot be flashed automatically from ESP-ROM mode. Switch to Factory restore.'))
  }
  const full = selectedExampleFullUrl()
  if (!full) throw new Error(t('未找到当前例程的 full flash 固件。', 'Full flash image for the selected example was not found.'))
  return await fetchBinary(full)
}

async function tryExtraUsbJtagReset(transport: any, UsbJtagSerialResetCtor: any, device: SerialPort) {
  if (!isEspUsbJtagPort(device)) return
  try {
    log(t('再次尝试 USB-JTAG 硬复位...', 'Trying an extra USB-JTAG hard reset...'))
    await new UsbJtagSerialResetCtor(transport).reset()
  } catch (e: any) {
    log(t(
      `额外 USB-JTAG 复位失败，页面仍会刷新；如无输出请手动按 RST。${e?.message || e}`,
      `Extra USB-JTAG reset failed; the page will still reload. Press RST if there is no output. ${e?.message || e}`
    ))
  }
}

async function runFullFlash(data: ArrayBuffer, startMessage: string, doneMessage: string) {
  state.value = 'connecting'
  log(startMessage)
  const { ESPLoader, Transport, UsbJtagSerialReset } = await import('esptool-js')
  if (port) await closePort()
  if (!selectedPort.value) await selectSerialPort()
  const device = selectedPort.value!
  const transport = new Transport(device)
  const terminal = {
    clean() {},
    writeLine(line: string) { log(line) },
    write(data: string) { log(data) },
  }

  try {
    const options: any = { transport, baudrate: 460800, terminal }
    if (isEspUsbJtagPort(device)) {
      options.resetConstructors = {
        hardReset: (resetTransport: any) => new UsbJtagSerialReset(resetTransport),
      }
    }
    const loader = new ESPLoader(options)
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
    await tryExtraUsbJtagReset(transport, UsbJtagSerialReset, device)
  } finally {
    try { await transport.disconnect() } catch {}
  }
  await finishFlashAndReload(doneMessage)
}

async function runRecovery() {
  reset(true)
  const data = await getImageData()
  await runFullFlash(
    data,
    t('恢复出厂需要设备处于 BOOT 下载模式。', 'Factory restore requires BOOT download mode.'),
    t('恢复出厂完成，设备正在重启。', 'Factory restore complete. Device is rebooting.')
  )
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

onUnmounted(() => {
  if (reloadTimer) clearTimeout(reloadTimer)
  closePort()
})
</script>

<template>
  <ClientOnly>
    <div class="fw-updater">
      <div class="mode-row">
        <button :class="{ active: mode === 'ota' }" @click="switchMode('ota')">
          {{ t('在线烧录', 'Flashing') }}
        </button>
        <button :class="{ active: mode === 'recovery' }" @click="switchMode('recovery')">
          {{ t('恢复出厂', 'Factory restore') }}
        </button>
      </div>

      <div class="explain">
        <strong>{{ actionText }}</strong>
        <p v-if="mode === 'ota'">
          {{ t('设备正常运行时优先通过 fw 协议只更新 app 分区；如果设备已在 ESP-ROM 下载模式，会自动刷写当前选择例程的 full flash。', 'When the device is running normally, this updates only the app partition through the fw protocol. If the device is already in ESP-ROM download mode, it automatically flashes the selected example full image.') }}
        </p>
        <p v-else>
          {{ t('用于第一次刷机、恢复出厂或固件无法启动。请先让设备进入 BOOT 下载模式，再开始烧录 full flash。', 'For first flash, factory restore, or broken firmware. Put the device into BOOT download mode before flashing the full image.') }}
        </p>
      </div>

      <div class="source-row">
        <label><input type="radio" :checked="useRemote" @change="useRemote = true"> {{ t('使用远端固件', 'Use remote firmware') }}</label>
        <label><input type="radio" :checked="!useRemote" @change="useRemote = false"> {{ t('选择本地 .bin 文件', 'Select local .bin file') }}</label>
      </div>

      <div v-if="useRemote" class="field">
        <label>{{ mode === 'ota' ? t('当前仓库例程', 'Repository example') : t('恢复出厂固件', 'Factory firmware') }}</label>
        <select v-model="firmwareChoice" @change="selectFirmwareUrl">
          <option v-for="item in firmwareOptions" :key="item.value" :value="item.value">
            {{ item.label }}
          </option>
        </select>
        <div v-if="manifestError" class="hint error-text">
          {{ t(`例程清单加载失败：${manifestError}`, `Failed to load example list: ${manifestError}`) }}
        </div>
      </div>

      <div v-if="useRemote && firmwareChoice === CUSTOM_ID" class="field">
        <label>{{ t('自定义 App URL', 'Custom app URL') }}</label>
        <input v-if="mode === 'ota'" v-model="appUrl" />
        <input v-else v-model="fullUrl" />
        <div class="hint">{{ urlHelp }}</div>
      </div>

      <div v-if="!useRemote" class="field">
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
        <button class="secondary" :disabled="state === 'running' || !isSupported" @click="selectSerialPort">
          {{ selectedPort ? t('重新选择串口', 'Select another port') : t('选择串口', 'Select serial port') }}
        </button>
        <button class="secondary" :disabled="state === 'running' || !isSupported || isSerialOpen || !selectedPort" @click="connectMonitor">
          {{ t('连接串口监视器', 'Connect monitor') }}
        </button>
        <button class="secondary" :disabled="state === 'running' || !isSerialOpen" @click="disconnectSerial">
          {{ t('断开', 'Disconnect') }}
        </button>
        <button class="primary" :disabled="state === 'connecting' || state === 'running' || !isSupported || isFirmwareLoading" @click="start">
          {{ state === 'running' || state === 'connecting' ? t('执行中...', 'Running...') : t('开始烧录', 'Start flashing') }}
        </button>
        <button class="secondary" @click="reset(true)">{{ t('清空日志', 'Clear log') }}</button>
      </div>

      <div class="port-state">
        {{ selectedPort ? t(`串口：${selectedPortLabel}`, `Port: ${selectedPortLabel}`) : t('未选择串口。在线烧录和串口监视器需要先选择串口；恢复出厂也可以先选择，开始时会复用。', 'No serial port selected. Flashing and monitor require a selected port; factory restore can also reuse it.') }}
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
.field input, .field select, .manual input {
  width: 100%;
  min-width: 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 7px;
  padding: 8px 10px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}
.hint, .port-state {
  color: var(--vp-c-text-2);
  font-size: 12px;
}
.hint {
  grid-column: 2;
}
.error-text {
  color: #dc2626;
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
  margin: 14px 0 10px;
  position: relative;
  z-index: 0;
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
  height: 360px;
  min-height: 360px;
  max-height: 360px;
  overflow: auto;
  margin: 12px 0 0;
  padding: 12px;
  border-radius: 8px;
  background: #0d1117;
  color: #e6edf3;
  font-size: 12px;
  white-space: pre-wrap;
  box-sizing: border-box;
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
