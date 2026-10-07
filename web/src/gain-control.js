import { gainView, normalizeSensitivity, sensitivityToGain } from './sensitivity.js';

/** Only an explicit edit converts the product scale to the existing gain field. */
export function mountGainControl(container, { object, key, id, label, onChange }) {
  const make = (tag, text = '', cls = '') => { const e = document.createElement(tag); e.textContent = text; e.className = cls; return e; };
  const root = make('div', '', 'gain-control'), title = make('label', label, 'gain-title');
  title.htmlFor = id + '-number'; root.append(title);
  const body = make('div', '', 'gain-body'), scale = make('div', '', 'gain-scale');
  const ends = make('div', '', 'gain-endpoints'); ends.append(make('span', '低'), make('span', '高'));
  const ruler = make('input'); ruler.type = 'range'; ruler.min = 1; ruler.max = 10; ruler.step = '0.1'; ruler.id = id + '-ruler'; ruler.setAttribute('aria-label', label + '刻度尺');
  const ticks = make('div', '', 'gain-ticks'); for (let n = 1; n <= 10; n++) ticks.append(make('span', String(n)));
  scale.append(ends, ruler, ticks);
  const exact = make('div', '', 'gain-exact'), number = make('input'); number.type = 'number'; number.step = '0.1'; number.min = 1; number.max = 10; number.id = id + '-number'; number.placeholder = '保留旧配置';
  exact.append(make('span', '灵敏度'), number); body.append(scale, exact);
  const hint = make('small', '', 'gain-error'); hint.id = id + '-help'; number.setAttribute('aria-describedby', hint.id); ruler.setAttribute('aria-describedby', hint.id);
  root.append(body, hint); container.append(root);
  function sync() {
    const view = gainView(object[key], key === 'center_gain' ? 'f32' : 'f64');
    number.setCustomValidity('');
    number.value = view.legacy ? '' : view.value.toFixed(1);
    ruler.value = view.legacy ? '5' : String(view.value); ruler.classList.toggle('outside-scale', view.legacy);
    ruler.setAttribute('aria-valuetext', view.legacy ? '旧配置原倍率 ' + object[key] + '，原值保留；选择刻度才会替换' : view.value.toFixed(1));
    hint.textContent = view.legacy ? '旧配置原倍率 ' + object[key] + ' 无法用新刻度准确表示，原值保留。重新选择 1.0～10.0 才会替换。' : '';
  }
  function commit(value) {
    try {
      const normalized = normalizeSensitivity(value);
      object[key] = sensitivityToGain(normalized); sync(); onChange();
    } catch (error) {
      number.setCustomValidity(error.message); hint.textContent = error.message + '，请修正灵敏度。';
    }
  }
  // The existing apply/save validity gate keeps rejecting invalid input until
  // an explicit valid change commits it to the same draft.
  // step=0.1 alone does not limit typed/pasted precision in a number input.
  // Keep native editing (including a trailing decimal point) intact, but remove
  // extra fractional digits immediately, before blur/change or any submission.
  number.inputMode = 'decimal';
  number.addEventListener('beforeinput', event => {
    if (event.data && /[eE+\-]/.test(event.data)) event.preventDefault();
  });
  number.addEventListener('input', () => {
    const value = number.value;
    if (value && !/^(?:\d+(?:\.\d*)?|\.\d+)$/.test(value)) {
      number.value = ''; return;
    }
    const limited = value.replace(/^(\d*\.\d)\d+$/, '$1');
    if (limited !== value) number.value = limited;
  });
  number.addEventListener('change', () => commit(number.valueAsNumber));
  ruler.addEventListener('input', () => { number.value = ruler.value; commit(Number(ruler.value)); });
  sync(); return { sync, number, ruler };
}
