const names={"VIN-A": "V_IN \u00b7 Left", "VIN-B": "V_IN \u00b7 Right", "VOUT-A": "V_OUT \u00b7 Left", "VOUT-B": "V_OUT \u00b7 Right", "SW": "SW \u00b7 External switch", "VO": "VO \u00b7 Auxiliary power", "CPU": "ESP32-S3", "IMU": "IMU \u00b7 QMI8658A", "BEEP": "Buzzer", "R": "R \u00b7 Reset", "B": "B \u00b7 BOOT", "EX1": "EX1 \u00b7 GPIO expansion", "LDS": "LDS \u00b7 Ethernet", "EX3": "EX3 \u00b7 GPIO expansion", "EX2": "EX2 \u00b7 3\u00d78 header", "USB1": "USB1 \u00b7 Peripheral", "USB2": "USB2 \u00b7 Peripheral", "USB3": "USB3 \u00b7 Peripheral", "USBIN": "USBIN \u00b7 Host", "USBC": "USB-C", "RGB": "RGB \u00b7 WS2812", "EX4": "EX4 \u00b7 Encoder", "5V": "5V \u00b7 Power output", "ETH": "ETH \u00b7 Ethernet", "EX5": "EX5 \u00b7 LED chain", "TF": "TFCard \u00b7 MicroSD", "MAG": "Magnetometer \u00b7 QMC6309"};
const notes={"VIN-A": "Battery input. Both V_IN connectors use the same pinout. Small contacts are switch controls, not CAN. Check polarity before connecting.", "VIN-B": "Battery input. Both V_IN connectors use the same pinout. Small contacts are switch controls, not CAN. Check polarity before connecting.", "VOUT-A": "Power output and CAN signals; CAN TX is GPIO41 and RX is GPIO40. Output voltage follows the main supply.", "VOUT-B": "Power output and CAN signals; CAN TX is GPIO41 and RX is GPIO40. Output voltage follows the main supply.", "SW": "External switch contacts; not ordinary GPIO or a power output.", "VO": "SH1.0 2P auxiliary power connector with a resettable fuse.", "CPU": "ESP32-S3 controller for sensing, communication and robot control.", "IMU": "Six-axis inertial sensor; magnetometer on the reverse side.", "BEEP": "Passive buzzer driven through a transistor.", "R": "Press to pull EN low and reset the controller.", "B": "Boot mode selection.", "EX1": "MX1.25 8P; GPIO uses 3.3 V logic.", "LDS": "SH1.0 8P. Opening view: pins 8 to 1 from left to right. Pins 7 and 8 carry main supply voltage. Shares Ethernet with ETH; use one connector.", "EX3": "MX1.25 4P. GPIO47 also supports an external active buzzer.", "EX2": "Row 1 contains GPIO only. VCC_ESC in rows 7 and 8 is an independent supply. EX2 encoder pins 39/38 differ from EX4 pins 3/9.", "USB1": "USB hub downstream connector; same pin order as USB2 and USB3.", "USB2": "USB hub downstream connector.", "USB3": "USB hub downstream connector.", "USBIN": "USB upstream. Shares the data link with USB-C; use one input.", "USBC": "Connects through the CH339F hub, not directly to GPIO19/20.", "RGB": "LED chain output is available at EX5 on the reverse side.", "EX4": "Rear horizontal MX1.25 4P. Encoder example: A=GPIO3, B=GPIO9.", "5V": "Rear horizontal MX1.25 2P. Do not use as a peripheral supply when powered by USB alone.", "ETH": "Rear horizontal MX1.25 4P. Shares Ethernet with LDS; use one connector.", "EX5": "Opening view: pins 3 to 1 from left to right.", "TF": "USB host connection through CH339F; not an ESP32-S3 SPI slot.", "MAG": "Shares the I\u00b2C bus with QMI8658A."};
const pinNotes = {
  '9–26 V 主电源输入': '9–26 V main power input',
  '主电源输出': 'Main power output',
  '与主电源输入电压一致': 'Same voltage as the main power input',
  '独立电源端；不等同于板载 5 V': 'Independent supply; not the onboard 5 V rail',
  '编码器 B 相': 'Encoder channel B',
  '编码器 A 相': 'Encoder channel A',
  '板载灯的级联输出，不是直接 GPIO45': 'Onboard LED chain output, not GPIO45 directly',
};
const orientations = {
  '板卡正面俯视，USB-C 朝下。': 'Front face viewed from above, USB-C at the bottom.',
  '板端插座开口正视，PCB 在下；左 → 右。不是线束插头视图。': 'Board socket opening viewed straight on, PCB below; left to right. Not the cable plug view.',
  '板卡正面俯视，USB-C 朝下。从上到下 1–8 行，三列为左／中／右。': 'Front face, USB-C at the bottom. Rows 1–8 run from top to bottom; columns are left, middle and right.',
  'USB-C 标准触点名；下表为信号分组，不代表单排线序。': 'Standard USB-C contact names, grouped by signal; not a single row of pins.',
};
export function hardwareInEnglish(part) {
  return {
    ...part,
    name: names[part.id] || part.id,
    note: notes[part.id] || '',
    orientation: orientations[part.orientation] || '',
    ref: part.id === 'USBC' ? 'USB2 (schematic)' : part.ref,
    pins: part.pins.map(pin => ({...pin, note: pin.note ? pinNotes[pin.note] || '' : ''})),
  };
}
