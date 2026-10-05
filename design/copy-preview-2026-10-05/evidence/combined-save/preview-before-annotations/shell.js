// Presentation only: no connection, configuration, storage or device commands.
(() => {
  const links = [...document.querySelectorAll('[data-module]')];
  const panels = new Map(links.map(link => [link.dataset.module, document.getElementById(link.dataset.module)]));
  const show = () => {
    let targetId;
    try { targetId = decodeURIComponent(location.hash.slice(1)); } catch { targetId = ''; }
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
  };
  addEventListener('hashchange', show);
  show();
})();
