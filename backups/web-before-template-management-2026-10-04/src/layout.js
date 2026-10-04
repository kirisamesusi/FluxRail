/** Module navigation affects presentation only. All editors keep the same DOM and draft. */
export function mountModuleNavigation({ onNavigate = () => {}, document = globalThis.document, window = globalThis.window } = {}) {
  const links = [...document.querySelectorAll('[data-module]')];
  const panels = new Map(links.map(link => [link.dataset.module, document.getElementById(link.dataset.module)]));
  let current = null;

  function show({ focus = false } = {}) {
    let targetId;
    try { targetId = decodeURIComponent(window.location.hash.slice(1)); } catch { targetId = ''; }
    // The skip link moves focus to main without changing the selected module.
    if (targetId === 'main-content' && current) return;
    const target = document.getElementById(targetId);
    const id = panels.has(targetId) ? targetId : target?.closest('.module-panel')?.id;
    const selected = panels.has(id) ? id : 'profiles';
    const changed = current !== selected;
    current = selected;
    for (const [name, panel] of panels) panel.hidden = name !== selected;
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
    if (changed) onNavigate(selected);
    if (focus) {
      document.getElementById(selected + '-title').focus({ preventScroll: true });
      if (target && target !== panels.get(selected)) target.scrollIntoView({ block: 'start', behavior: 'instant' });
      else window.scrollTo({ top: 0, behavior: 'instant' });
    }
  }

  const onHashChange = () => show({ focus: true });
  const handlers = links.map(link => {
    const handler = event => {
      if (event.button > 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
      event.preventDefault();
      window.location.hash = link.getAttribute('href');
      show({ focus: true });
    };
    link.addEventListener('click', handler);
    return [link, handler];
  });
  window.addEventListener('hashchange', onHashChange);
  show();
  return () => {
    window.removeEventListener('hashchange', onHashChange);
    for (const [link, handler] of handlers) link.removeEventListener('click', handler);
  };
}
