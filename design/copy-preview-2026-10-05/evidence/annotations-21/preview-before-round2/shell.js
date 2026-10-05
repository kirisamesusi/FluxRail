// Local presentation and interaction demos only; no connection or device commands.
(() => {
  const links = [...document.querySelectorAll('[data-module]')];
  const panels = new Map(links.map(link => [link.dataset.module, document.getElementById(link.dataset.module)]));
  const show = event => {
    let targetId;
    try { targetId = decodeURIComponent(location.hash.slice(1)); } catch { targetId = ''; }
    if (targetId === 'main-content') return;
    const target = document.getElementById(targetId);
    const selected = panels.has(targetId) ? targetId : target?.closest('.module-panel')?.id || 'profiles';
    for (const [id, panel] of panels) panel.hidden = id !== selected;
    for (const link of links) {
      if (link.dataset.module === selected) link.setAttribute('aria-current', 'page');
      else link.removeAttribute('aria-current');
    }
    if (selected === 'experimental') document.getElementById('experimental-panel').open = true;
    if (target && target !== panels.get(selected)) {
      for (let ancestor = target; ancestor && ancestor !== panels.get(selected); ancestor = ancestor.parentElement) {
        if (ancestor.tagName === 'DETAILS') ancestor.open = true;
      }
    }
    if (event) {
      document.getElementById(selected + '-title').focus({ preventScroll:true });
      if (target && target !== panels.get(selected)) target.scrollIntoView({ block:'start' });
      else window.scrollTo({top:0,behavior:'instant'});
    }
  };
  addEventListener('hashchange', show);
  show();
  for (const slider of document.querySelectorAll('input[type="range"][data-local-gain]')) {
    const id = slider.dataset.localGain;
    const number = document.getElementById(id + '-number'), error = document.getElementById(id + '-error');
    const toPosition = value => value < 1 ? (value - 0.1) / 0.9 : value;
    const fromPosition = position => position === 0 ? 0.1 : position;
    const update = (value, { reflectInput = true } = {}) => {
      if (reflectInput) number.value = String(value);
      slider.value = String(toPosition(value)); error.textContent = ''; number.removeAttribute('aria-invalid');
      slider.setAttribute('aria-valuetext', '倍率 ' + value);
      const settings = slider.closest('.knob-card')?.querySelector('.preview-settings');
      if (settings) settings.textContent = `当前预览倍率为 ${value}。这里只调整页面示意，不改变设备设置。`;
    };
    slider.addEventListener('input', () => update(fromPosition(Math.round(Number(slider.value)))));
    slider.addEventListener('keydown', event => {
      if (!['ArrowRight','ArrowUp','ArrowLeft','ArrowDown','Home','End'].includes(event.key)) return;
      event.preventDefault(); const current = Number(slider.value);
      const next = event.key === 'Home' ? 0 : event.key === 'End' ? 10 : ['ArrowRight','ArrowUp'].includes(event.key) ? Math.min(10,Math.floor(current)+1) : Math.max(0,Math.ceil(current)-1);
      update(fromPosition(next));
    });
    number.addEventListener('input', () => {
      const value = Number(number.value);
      if (number.value.trim() && Number.isFinite(value) && value >= 0.1 && value <= 10) update(value, { reflectInput:false });
      else { error.textContent = '请输入 0.1～10 之间的数值'; number.setAttribute('aria-invalid','true'); }
    });
    update(5);
  }
  const restart = document.getElementById('restart-preview'), dialog = document.getElementById('restart-confirmation');
  restart.addEventListener('click', () => dialog.showModal());
  document.getElementById('restart-cancel').addEventListener('click', () => dialog.close());
  document.getElementById('restart-confirm').addEventListener('click', () => {
    dialog.close(); document.getElementById('notice').textContent = '这里只演示重启确认，未操作设备。';
  });
  dialog.addEventListener('close', () => restart.focus());
})();
