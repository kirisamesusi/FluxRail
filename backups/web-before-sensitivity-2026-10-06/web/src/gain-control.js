/** Ruler is a view of the draft; legacy values are never clamped or marked dirty. */
export function mountGainControl(container, { object, key, id, label, min, max, onChange }) {
  const make = (tag, text = '', cls = '') => { const e = document.createElement(tag); e.textContent = text; e.className = cls; return e; };
  const root = make('div', '', 'gain-control'), title = make('label', label, 'gain-title');
  title.htmlFor = id + '-number'; root.append(title);
  const body = make('div', '', 'gain-body'), scale = make('div', '', 'gain-scale');
  const ends = make('div', '', 'gain-endpoints'); ends.append(make('span', '低'), make('span', '高'));
  const ruler = make('input'); ruler.type = 'range'; ruler.min = 1; ruler.max = 10; ruler.step = '0.01'; ruler.id = id + '-ruler'; ruler.setAttribute('aria-label', label + '刻度尺');
  const ticks = make('div', '', 'gain-ticks'); for (let n = 1; n <= 10; n++) ticks.append(make('span', String(n)));
  scale.append(ends, ruler, ticks);
  const exact = make('div', '', 'gain-exact'), number = make('input'); number.type = 'number'; number.step = 'any'; number.min = min; number.max = max; number.id = id + '-number';
  exact.append(make('span', '精确倍率'), number); body.append(scale, exact);
  const hint = make('small', '', 'gain-error'); hint.id = id + '-help'; number.setAttribute('aria-describedby', hint.id); ruler.setAttribute('aria-describedby', hint.id);
  root.append(body, hint); container.append(root);
  function sync() {
    number.value = String(object[key]);
    const outside = object[key] < 1 || object[key] > 10;
    ruler.value = String(object[key]); ruler.classList.toggle('outside-scale', outside);
    ruler.setAttribute('aria-valuetext', outside ? '真实值 ' + object[key] + '，超出刻度范围；请看输入框' : String(object[key]));
    hint.textContent = outside ? '读回倍率 ' + object[key] + ' 超出 1～10 刻度，原值保留。可输入 ' + min + '～' + max + '。' : max < 10 ? '当前固件滑轨支持 ' + min + '～' + max + '；9～10 尚不支持。' : '';
  }
  function commit(value) {
    if (!Number.isFinite(value) || value < min || value > max) { number.setCustomValidity('当前支持 ' + min + '～' + max); hint.textContent = '当前支持 ' + min + '～' + max + '，请修正倍率。'; return; }
    number.setCustomValidity(''); object[key] = value; sync(); onChange();
  }
  number.addEventListener('input', () => { number.setCustomValidity(''); const value = number.valueAsNumber; if (number.value !== '' && Number.isFinite(value) && value >= min && value <= max) { ruler.value = String(value); hint.textContent = value < 1 || value > 10 ? '超出刻度范围，输入值将原样保留。' : max < 10 ? '当前固件滑轨支持 ' + min + '～' + max + '；9～10 尚不支持。' : ''; } });
  number.addEventListener('change', () => commit(number.valueAsNumber));
  ruler.addEventListener('input', () => { number.value = ruler.value; commit(Number(ruler.value)); });
  sync(); return { sync, number, ruler };
}
