import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { gainView, normalizeSensitivity, sensitivityToGain } from '../src/sensitivity.js';
import { mountGainControl } from '../src/gain-control.js';
import { validateMotion } from '../src/config.js';
import { projectPreview } from '../src/knob-preview.js';
import { decodeSnapshot } from '../src/snapshot.js';
import { Device } from '../src/device.js';
import { DemoHost } from '../src/demo.js';
import { applyAndSave } from '../src/workflows.js';

test('全部91个刻度合法、递增，5保持原1倍，只有一位小数', async () => {
  const bytes = await readFile(new URL('../../tests/integration/schema2_defaults.bin', import.meta.url));
  const motion = decodeSnapshot(bytes).profiles[3].motion;
  let previous = 0;
  for (let tenth = 10; tenth <= 100; tenth++) {
    const scale = tenth / 10, gain = sensitivityToGain(scale);
    assert(gain > previous); previous = gain;
    motion.hall.center_gain = Math.fround(gain);
    motion.knobs.forEach(k => { k.gain = gain; });
    validateMotion(motion);
    assert.equal(gainView(motion.hall.center_gain, 'f32').value, scale);
    assert.equal(gainView(gain).value, scale);
    assert.equal(projectPreview(1, { gain, axis: 0, invert: false }).scaledTurns, gain);
  }
  assert.equal(sensitivityToGain(1), .2); assert.equal(sensitivityToGain(5), 1); assert.equal(sensitivityToGain(10), 2);
  assert.equal(normalizeSensitivity(5.14), 5.1); assert.equal(normalizeSensitivity(4.96), 5);
  assert.equal(normalizeSensitivity(5.15), 5.2);
  for (const value of [0, .9, 10.01, NaN, Infinity, '', '5']) assert.throws(() => sensitivityToGain(value));
});

// Small DOM boundary checks the actual component callbacks; rendered forms and
// native number validity are covered separately in the browser regression.
function control(gain, key = 'gain') {
  class Element {
    constructor() { this.children = []; this.handlers = new Map(); this.classes = new Set(); this.attributes = {}; this.value = ''; }
    append(...nodes) { this.children.push(...nodes); }
    setAttribute(k, v) { this.attributes[k] = v; }
    setCustomValidity(v) { this.invalid = v; }
    addEventListener(k, fn) { this.handlers.set(k, fn); }
    get classList() { return { toggle: (k, enabled) => enabled ? this.classes.add(k) : this.classes.delete(k) }; }
    get valueAsNumber() { return this.value === '' ? NaN : Number(this.value); }
    fire(name) { this.handlers.get(name)?.(); }
  }
  globalThis.document = { createElement: () => new Element() };
  const object = { [key]: gain }, container = new Element(); let changes = 0;
  const controls = mountGainControl(container, { object, key, id: 'test-gain', label: '灵敏度', onChange: () => changes++ });
  return { ...controls, object, changes: () => changes, hint: container.children[0].children[2] };
}

test('旧值、读回和无关保存不截断；明确编辑才替换原倍率', () => {
  for (const gain of [.01, .1, 1.003, 8, 12, 16]) {
    const f = control(gain);
    assert.equal(f.number.value, ''); assert.equal(f.object.gain, gain); assert.equal(f.changes(), 0);
    assert(f.ruler.classes.has('outside-scale')); assert.match(f.hint.textContent, /原值保留/);
    assert.equal(f.number.invalid, ''); f.sync(); assert.equal(f.object.gain, gain);
    f.number.value = '5.14'; f.number.fire('change');
    assert.equal(f.object.gain, 1.02); assert.equal(f.number.value, '5.1'); assert.equal(f.changes(), 1);
    assert(!f.ruler.classes.has('outside-scale'));
  }
  const f = control(Math.fround(.98), 'center_gain');
  assert.equal(f.number.value, '4.9'); assert.equal(f.object.center_gain, Math.fround(.98)); assert.equal(f.changes(), 0);
  assert.equal(control(1.0000005).number.value, '');
  assert.equal(gainView(1.0000005).legacy, true);
  assert.equal(gainView(Math.fround(1.000000119), 'f32').legacy, true);
});

test('越界和空输入阻止提交且不改草稿，合法输入/尺恢复并换算一次', () => {
  const f = control(1);
  assert.equal(f.number.step, '0.1'); assert.equal(f.ruler.step, '0.1'); assert.equal(f.number.value, '5.0');
  for (const value of ['10.04', '0.9', '']) {
    f.number.value = value; f.number.fire('change');
    assert(f.number.invalid); assert.equal(f.object.gain, 1); assert.equal(f.changes(), 0);
  }
  f.ruler.value = '4.9'; f.ruler.fire('input');
  assert.equal(f.object.gain, .98); assert.equal(f.number.value, '4.9'); assert.equal(f.number.invalid, '');
  f.number.value = '5.1'; f.number.fire('change'); assert.equal(f.object.gain, 1.02);
});

test('真实Device应用/保存/再读回及快照保留实际倍率，不重复乘除5', async () => {
  const host = new DemoHost();
  const client = { alive: true, revision: 1, last: null, async request(op, payload = {}) {
    const response = host.handle({ protocol_version: 1, id: 1, op, payload });
    assert(response.ok); this.revision = response.revision; this.last = response; return response.result;
  } };
  const device = new Device(client); await device.handshake();
  for (const scale of [1, 4.9, 5, 5.1, 10]) {
    const gain = sensitivityToGain(scale);
    device.draft.motion.hall.center_gain = gain;
    device.draft.motion.knobs.forEach(k => { k.gain = gain; }); device.localDirty = true;
    await applyAndSave(device, { validateDraft: () => validateMotion(device.draft.motion) });
    const write = host.requests.filter(r => r.op === 'set_profile').at(-1);
    assert.equal(write.payload.motion.hall.center_gain, gain);
    assert(write.payload.motion.knobs.every(k => k.gain === gain));
    const stored = decodeSnapshot(host.snapshot()).profiles.find(p => p.profile_id === device.selected);
    assert.equal(gainView(stored.motion.hall.center_gain, 'f32').value, scale);
    assert.equal(stored.motion.knobs[0].gain, gain); assert.equal(device.localDirty, false);
    await device.refresh(); assert.equal(gainView(device.draft.motion.hall.center_gain).value, scale);
  }
  host.device.profiles[3].motion.hall.center_gain = .1;
  host.device.profiles[3].motion.knobs[0].gain = 12;
  host.device.profiles[3].motion.knobs[1].gain = 1.003;
  await device.refresh(); device.draft.config.lighting.brightness_level = 37; device.localDirty = true;
  await applyAndSave(device, { validateDraft: () => validateMotion(device.draft.motion) });
  assert.equal(device.profile.motion.hall.center_gain, .1);
  assert.equal(device.profile.motion.knobs[0].gain, 12); assert.equal(device.profile.motion.knobs[1].gain, 1.003);
});

test('每个模块的SVG引用都定义，设备状态导航和标题使用同一个有内容的图标', async () => {
  const html = await readFile(new URL('../index.html', import.meta.url), 'utf8');
  const symbols = new Set([...html.matchAll(/<symbol id="([^"]+)"/g)].map(m => m[1]));
  for (const match of html.matchAll(/<use href="#([^"]+)"/g)) assert(symbols.has(match[1]), match[1]);
  assert.equal([...html.matchAll(/<use href="#art-device-status"/g)].length, 2);
  assert.match(html, /<symbol id="art-device-status"[^>]*>.*<circle.*<path.*<\/symbol>/);
});
