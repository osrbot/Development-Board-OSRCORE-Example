import test from 'node:test';
import assert from 'node:assert/strict';
import {hardware} from '../.vitepress/theme/hardware-map.mjs';
import {hardwareInEnglish} from '../.vitepress/theme/hardware-en.mjs';
import {lessons, initialValues, createState, lessonCode, stepSimulation} from '../.vitepress/theme/lab-simulation.mjs';

const han = /\p{Script=Han}/u;
test('every English component retains its descriptions without Chinese fallback', () => {
  for (const part of hardware) {
    const en = hardwareInEnglish(part);
    assert.ok(en.name, part.id);
    assert.ok(en.orientation, `${part.id}: orientation`);
    if (part.note) assert.ok(en.note, `${part.id}: component note`);
    for (let i = 0; i < part.pins.length; i++) {
      if (part.pins[i].note) assert.ok(en.pins[i].note, `${part.id}: pin ${part.pins[i].n} note`);
    }
    assert.ok(!han.test(JSON.stringify(en)), `${part.id}: untranslated text`);
    assert.deepEqual(en.position, part.position);
    assert.equal(en.side, part.side);
    assert.equal(en.reverse, part.reverse);
    assert.deepEqual(en.pins.map(({note, ...pin}) => pin), part.pins.map(({note, ...pin}) => pin));
  }
});
test('English power descriptions retain supply limits and rail distinctions', () => {
  const part = id => hardwareInEnglish(hardware.find(p => p.id === id));
  assert.match(part('VIN-A').pins[3].note, /9–26 V/);
  assert.match(part('VO').pins[1].note, /main power input/);
  for (const pin of part('EX2').pins.filter(p => p.net === 'VCC_ESC')) {
    assert.match(pin.note, /Independent supply/);
    assert.match(pin.note, /not the onboard 5 V/);
  }
  assert.match(part('EX5').pins[1].note, /not GPIO45 directly/);
});
test('English lessons, source excerpts and simulation outputs contain no Chinese strings', () => {
  for (const lesson of lessons) {
    assert.ok(!han.test(lesson.en + lesson.descriptionEn + lesson.params.map(p => p.en).join('')));
    for (const limit of ['value', 'min', 'max']) {
      const values = {...initialValues(lesson.id), ...Object.fromEntries(lesson.params.map(p => [p.key, p[limit]]))};
      const state = createState();
      assert.ok(!han.test(lessonCode(lesson.id, values).join('\n')), `lesson ${lesson.id}: code`);
      for (const time of [0, 500, 2500, 5000]) {
        assert.ok(!han.test(JSON.stringify(stepSimulation(lesson.id, values, time, .05, state))), `lesson ${lesson.id}: output at ${time}`);
      }
    }
  }
});
