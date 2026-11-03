// Pin nets checked against schematic-zh.pdf; physical ordering against PCB silk
// and the supplied OSRCORE product manual. Coordinates use USB-C-down PCB axes.
const p=(n,net,gpio=null,note='')=>({n:String(n),net,gpio,note});
const gpio=(n,g,note='')=>p(n,`GPIO ${g}`,g,note);
const socket='板端插座开口正视，PCB 在下；左 → 右。不是线束插头视图。';
const board='板卡正面俯视，USB-C 朝下。';
const usbPins=[p(1,'GND'),p(2,'USB D+'),p(3,'USB D−'),p(4,'5 V')];
const exRows=[[13,12,14],[15,'5 V','GND'],[43,'5 V','GND'],[44,'5 V','GND'],[39,'5 V','GND'],[38,'5 V','GND'],[1,'VCC_ESC','GND'],[2,'VCC_ESC','GND']];
export const headerPins=exRows.flatMap((row,r)=>row.map((v,c)=>({...((typeof v==='number')?gpio(`${r+1}${['L','M','R'][c]}`,v):p(`${r+1}${['L','M','R'][c]}`,v,null,v==='VCC_ESC'?'独立电源端；不等同于板载 5 V':'')),row:r+1,col:c})));
const item=(id,name,x,y,z,pins=[],extra={})=>({id,name,side:'front',position:[x,y,z],pins,orientation:socket,kind:'socket',...extra});
export const hardware=[
 item('VIN-A','V_IN · 左',7.5,10,7,[p(1,'CTRL_A'),p(2,'CTRL_B'),p(3,'GND_BAT'),p(4,'VCC_BAT',null,'9–26 V 主电源输入')],{kind:'xt30',orientation:board,ref:'CN8 / CN5',note:'两组 V_IN 接口定义相同。小触点为开关控制，不是 CAN。'}),
 item('VIN-B','V_IN · 右',21,10,7,[p(1,'CTRL_A'),p(2,'CTRL_B'),p(3,'GND_BAT'),p(4,'VCC_BAT',null,'9–26 V 主电源输入')],{kind:'xt30',orientation:board,ref:'CN8 / CN5'}),
 item('VOUT-A','V_OUT · 左',79,10,7,[p(1,'CAN L'),p(2,'CAN H'),p(3,'GND'),p(4,'V_IN',null,'主电源输出')],{kind:'xt30',orientation:board,ref:'CN6 / CN7',note:'CAN 收发器：TX GPIO41、RX GPIO40。输出电压跟随主电源。'}),
 item('VOUT-B','V_OUT · 右',93,10,7,[p(1,'CAN L'),p(2,'CAN H'),p(3,'GND'),p(4,'V_IN',null,'主电源输出')],{kind:'xt30',orientation:board,ref:'CN6 / CN7'}),
 item('SW','SW · 外接开关',33.5,5.4,2,[p(1,'CTRL_A'),p(2,'CTRL_B')],{ref:'U48',note:'开关触点。不是普通 GPIO 或供电输出。'}),
 item('VO','VO · 辅助电源',66,5.4,2,[p(1,'GND'),p(2,'V_IN',null,'与主电源输入电压一致')],{ref:'U41',note:'SH1.0 2P；串接自恢复保险丝。'}),
 item('CPU','ESP32-S3',52,41,3,[],{kind:'part',ref:'U25',note:'ESP32-S3-WROOM-1U-N16R8 · 16 MB Flash / 8 MB PSRAM。',lesson:0}),
 item('IMU','IMU · QMI8658A',11,44,2,[gpio('SDA',10),gpio('SCL',11),gpio('INT',21)],{kind:'part',ref:'U39',note:'六轴惯性传感器；磁传感器位于背面同一区域。',lesson:5}),
 item('BEEP','蜂鸣器',50,20,3,[gpio('PWM',42)],{kind:'part',ref:'BUZZER2',note:'无源蜂鸣器，经三极管驱动。',lesson:2}),
 item('R','R · 复位',85,39,2,[p('RESET','CHIP_PU / EN')],{kind:'part',ref:'SW3',note:'按下拉低 EN，复位主控。'}),
 item('B','B · BOOT',94,39,2,[gpio('BOOT',0)],{kind:'part',ref:'SW4',note:'启动模式选择按键。'}),
 item('EX1','EX1 · GPIO 扩展',6,71,3,[p(1,'5 V'),p(2,'GND'),gpio(3,17),gpio(4,18),gpio(5,7),gpio(6,8),gpio(7,5),gpio(8,6)],{ref:'U36',note:'MX1.25 8P；GPIO 为 3.3 V 逻辑。'}),
 item('LDS','LDS · 以太网',24,71,3,[p(1,'ETH RX−'),p(2,'ETH RX+'),p(3,'ETH TX−'),p(4,'ETH TX+'),p(5,'GND'),p(6,'GND'),p(7,'V_IN'),p(8,'V_IN')],{ref:'U32',reverse:true,note:'SH1.0 8P；开口视图从左到右为 8→1。7、8 脚为主电源电压。与背面 ETH 共用网络，择一连接。'}),
 item('EX3','EX3 · GPIO 扩展',72,71,3,[p(1,'5 V'),p(2,'GND'),gpio(3,47),gpio(4,48)],{ref:'U35',note:'MX1.25 4P；GPIO47 也用于扩展有源蜂鸣器信号。'}),
 item('EX2','EX2 · 3×8 排针',89,63,5,headerPins,{kind:'header',ref:'H6 / H5 / H4',orientation:board+'从上到下 1–8 行，三列为左／中／右。',note:'第 1 行全部为 GPIO；第 7、8 行中列 VCC_ESC 是独立电源。EX2 编码器脚为 39/38，和背面 EX4 的 3/9 不同。',lesson:3}),
 item('USB1','USB1 · 外设',24,94,2,usbPins,{ref:'U27 / U28 / U29',note:'USB HUB 下行接口；与 USB2、USB3 的针序相同。'}),
 item('USB2','USB2 · 外设',24,86,2,usbPins,{ref:'U27 / U28 / U29'}),
 item('USB3','USB3 · 外设',75,86,2,usbPins,{ref:'U27 / U28 / U29'}),
 item('USBIN','USBIN · 主机',75,94,2,[p(1,'GND'),p(2,'USB D+'),p(3,'USB D−'),p(4,'USB 5 V')],{ref:'U30',note:'USB 上行接口，与 USB-C 共用数据链路，择一连接。'}),
 item('USBC','USB-C',50,94,2,[p('A1/A12/B1/B12','GND'),p('A4/A9/B4/B9','USB 5 V'),p('A6/B6','USB D+'),p('A7/B7','USB D−'),p('A5','CC1'),p('B5','CC2')],{kind:'part',ref:'USB2（原理图位号）',orientation:'USB-C 标准触点名；下表为信号分组，不代表单排线序。',note:'通过 CH339F HUB 接入主控及外设；不是直接接 GPIO19/20 的插座。'}),
 item('RGB','RGB · WS2812',94,88,1,[gpio('DIN',45)],{kind:'part',ref:'LED7',note:'级联输出从背面 EX5 引出。',lesson:1}),
 item('EX4','EX4 · 编码器',76,94,-3,[p(1,'5 V'),p(2,'GND'),gpio(3,9,'编码器 B 相'),gpio(4,3,'编码器 A 相')],{side:'back',ref:'U37',note:'背面 MX1.25 4P 卧贴；示例编码器使用 EA=GPIO3、EB=GPIO9。',lesson:6}),
 item('5V','5V · 电源输出',50,94,-3,[p(1,'5 V'),p(2,'GND')],{side:'back',ref:'U47',note:'背面 MX1.25 2P 卧贴。仅 USB 供电时不可作为外设电源输出。'}),
 item('ETH','ETH · 以太网',25,94,-3,[p(1,'ETH TX+'),p(2,'ETH TX−'),p(3,'ETH RX+'),p(4,'ETH RX−')],{side:'back',ref:'U38',note:'背面 MX1.25 4P 卧贴；与 LDS 共用同一路以太网，择一连接。'}),
 item('EX5','EX5 · 灯光级联',95,86,-2,[p(1,'GND'),p(2,'WS2812 DO',null,'板载灯的级联输出，不是直接 GPIO45'),p(3,'5 V')],{side:'back',ref:'U45',reverse:true,note:'开口视图从左到右为 3→1。'}),
 item('TF','TFCard · MicroSD',12,71,-2,[],{side:'back',kind:'part',ref:'TF-13',note:'通过 CH339F 接入 USB 主机。不是直接连接 ESP32-S3 SPI 的卡槽。'}),
 item('MAG','磁传感器 · QMC6309',11,40,-1,[gpio('SDA',10),gpio('SCL',11)],{side:'back',kind:'part',ref:'U40',note:'与 QMI8658A 共用 I²C 总线。',lesson:11}),
];
export const lessonHardware={0:'CPU',1:'RGB',2:'BEEP',3:'EX2',4:'EX2',5:'IMU',6:'EX4',7:'CPU',8:'CPU',9:'EX2',10:'IMU',11:'MAG',12:'EX2',13:'RGB'};
export const lessonGPIO={1:[45],2:[42],3:[1,2],4:[44],5:[10,11,21],6:[3,9],9:[1,2],10:[10,11],11:[10,11],12:[1,2,44],13:[45,42]};
