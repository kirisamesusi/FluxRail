import test from 'node:test';
import assert from 'node:assert/strict';
import { mountModuleNavigation } from '../src/layout.js';

function fixture(hash = '') {
  const ids = ['profiles', 'keyboard', 'lighting', 'hall', 'knobs', 'diagnostics', 'update', 'experimental'];
  const nodes = new Map(), listeners = new Map();
  function node(id, tagName = 'SECTION', parentElement = null) {
    const attributes = new Map(), handlers = new Map();
    const item = { id, tagName, parentElement, hidden: false, open: false, draft: { value: 'unchanged' },
      closest() { for (let p = this; p; p = p.parentElement) if (ids.includes(p.id)) return p; return null; },
      focus() { state.focus = id; }, scrollIntoView() { state.scrolledTarget = id; },
      setAttribute(k, v) { attributes.set(k, v); }, getAttribute(k) { return attributes.get(k) ?? null; },
      removeAttribute(k) { attributes.delete(k); },
      addEventListener(k, fn) { handlers.set(k, fn); }, removeEventListener(k) { handlers.delete(k); },
      click(extra = {}) { const event = { button: 0, preventDefault() { state.prevented = true; }, ...extra }; handlers.get('click')?.(event); }
    };
    nodes.set(id, item); return item;
  }
  const state = { focus: null, scrolledTarget: null, top: null, changes: [] };
  const links = ids.map(id => { const panel = node(id); node(id + '-title', 'H2', panel); const link = node('nav-' + id, 'A'); link.dataset = { module: id }; link.setAttribute('href', '#' + id); return link; });
  const lab = node('experimental-panel', 'DETAILS', nodes.get('experimental'));
  const calibration = node('experiment-calibration', 'DETAILS', lab);
  node('calibration-field', 'INPUT', calibration);
  const document = { querySelectorAll() { return links; }, getElementById(id) { return nodes.get(id) ?? null; } };
  const window = { location: { hash }, scrollTo({ top }) { state.top = top; }, addEventListener(k, fn) { listeners.set(k, fn); }, removeEventListener(k) { listeners.delete(k); } };
  const dispose = mountModuleNavigation({ document, window, onNavigate: id => state.changes.push(id) });
  return { nodes, links, state, window, dispose, navigate(hash) { window.location.hash = hash; listeners.get('hashchange')?.(); }, visible() { return ids.filter(id => !nodes.get(id).hidden); } };
}

test('默认模块与无效片段回退到模板，初次加载不移动焦点', () => {
  for (const hash of ['', '#unknown', '#%E0%A4%A']) {
    const f = fixture(hash);
    assert.deepEqual(f.visible(), ['profiles']); assert.equal(f.state.focus, null);
    assert.equal(f.links[0].getAttribute('aria-current'), 'page');
  }
});
test('切换模块保留同一编辑器及草稿，活动链接与焦点同步', () => {
  const f = fixture(); const editor = f.nodes.get('keyboard'), draft = editor.draft;
  draft.value = 'B'; f.links[1].click();
  assert.equal(f.window.location.hash, '#keyboard'); assert.deepEqual(f.visible(), ['keyboard']);
  assert.equal(f.state.focus, 'keyboard-title'); assert.equal(f.state.top, 0);
  assert.equal(f.links[0].getAttribute('aria-current'), null);
  f.links[2].click(); f.links[1].click();
  assert.equal(f.nodes.get('keyboard'), editor); assert.equal(editor.draft, draft); assert.equal(draft.value, 'B');
});
test('内部深链接显示实验室并展开祖先分组；回到普通模块关闭展示', () => {
  const f = fixture('#calibration-field');
  assert.deepEqual(f.visible(), ['experimental']);
  assert.equal(f.nodes.get('experimental-panel').open, true); assert.equal(f.nodes.get('experiment-calibration').open, true);
  f.navigate('#calibration-field'); assert.equal(f.state.scrolledTarget, 'calibration-field');
  f.navigate('#update'); assert.deepEqual(f.visible(), ['update']); assert.equal(f.state.focus, 'update-title');
});
test('历史导航恢复模块，跳转到主内容保留当前模块', () => {
  const f = fixture('#knobs'); f.navigate('#hall'); f.navigate('#knobs');
  assert.deepEqual(f.visible(), ['knobs']); f.navigate('#main-content'); assert.deepEqual(f.visible(), ['knobs']);
  assert.deepEqual(f.state.changes, ['knobs', 'hall', 'knobs']);
});
test('修饰键点击保留浏览器链接行为，卸载后取消事件监听', () => {
  const f = fixture(); f.links[1].click({ ctrlKey: true });
  assert.equal(f.window.location.hash, ''); assert.deepEqual(f.visible(), ['profiles']);
  f.dispose(); f.navigate('#keyboard'); f.links[1].click(); assert.deepEqual(f.visible(), ['profiles']);
});
