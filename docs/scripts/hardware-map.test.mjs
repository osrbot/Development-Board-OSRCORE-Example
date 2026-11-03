import test from 'node:test';import assert from 'node:assert/strict';
import {hardware,headerPins,lessonHardware,lessonGPIO} from '../.vitepress/theme/hardware-map.mjs';
const get=id=>hardware.find(h=>h.id===id),nets=id=>get(id).pins.map(p=>p.net);
test('EX2 preserves the GPIO-only first row and isolated servo supply',()=>{
 assert.equal(headerPins.length,24);
 assert.deepEqual(headerPins.filter(p=>p.row===1).map(p=>p.gpio),[13,12,14]);
 assert.deepEqual(headerPins.filter(p=>p.col===1).map(p=>p.net),['GPIO 12','5 V','5 V','5 V','5 V','5 V','VCC_ESC','VCC_ESC']);
 assert.deepEqual(headerPins.filter(p=>p.col===0).map(p=>p.gpio),[13,15,43,44,39,38,1,2]);
});
test('EX4 encoder differs from EX2 encoder and socket orders preserve source',()=>{
 assert.deepEqual(nets('EX4'),['5 V','GND','GPIO 9','GPIO 3']);
 assert.deepEqual(nets('EX3'),['5 V','GND','GPIO 47','GPIO 48']);
 assert.deepEqual(nets('USBIN'),['GND','USB D+','USB D−','USB 5 V']);
 for(const id of ['USB1','USB2','USB3'])assert.deepEqual(nets(id),['GND','USB D+','USB D−','5 V']);
 assert.equal(get('LDS').reverse,true);assert.equal(get('EX5').reverse,true);
 assert.deepEqual(nets('EX5'),['GND','WS2812 DO','5 V']);
 assert.equal(get('EX5').pins[1].gpio,null);
});
test('XT30 power and small contacts are not mixed',()=>{
 for(const id of ['VIN-A','VIN-B'])assert.deepEqual(nets(id),['CTRL_A','CTRL_B','GND_BAT','VCC_BAT']);
 for(const id of ['VOUT-A','VOUT-B'])assert.deepEqual(nets(id),['CAN L','CAN H','GND','V_IN']);
});
test('all interfaces have unique pin keys and valid lesson mappings',()=>{
 assert.equal(new Set(hardware.map(h=>h.id)).size,hardware.length);
 for(const h of hardware)assert.equal(new Set(h.pins.map(p=>p.n)).size,h.pins.length);
 for(const id of Object.values(lessonHardware))assert.ok(get(id));
 assert.deepEqual(lessonGPIO[6],[3,9]);
});
