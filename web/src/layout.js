/** 顶部三个页签只改变展示。设备状态 / 固件 / 实验性功能共用同一 DOM 与草稿，
 * 切换页签不会重建编辑器，也不会丢失网页草稿。旧链接（#keyboard、#lighting、#hall、
 * #knobs、#profiles 等）回到设备状态；#update 为固件页；内部深链接会展开其所在的分组。
 */
const legacyToStatus = new Set(['keyboard', 'lighting', 'hall', 'knobs', 'profiles', 'diagnostics', 'input-status']);

export function mountModuleNavigation({ onNavigate = () => {}, document = globalThis.document, window = globalThis.window } = {}) {
  const links = [...document.querySelectorAll('.module-nav [data-module]')];
  const panels = new Map(links.map(link => [link.dataset.module, document.getElementById(link.dataset.module)]));
  let current = null;

  function show({ focus = false } = {}) {
    let targetId;
    try { targetId = decodeURIComponent(window.location.hash.slice(1)); } catch { targetId = ''; }
    // The skip link moves focus to main without changing the selected page.
    if (targetId === 'main-content' && current) return;
    if (legacyToStatus.has(targetId)) targetId = 'device-status';
    const target = document.getElementById(targetId);
    const id = panels.has(targetId) ? targetId : target?.closest('.module-panel')?.id;
    const selected = panels.has(id) ? id : 'device-status';
    const changed = current !== selected;
    current = selected;
    for (const [name, panel] of panels) panel.hidden = name !== selected;
    for (const link of links) {
      if (link.dataset.module === selected) link.setAttribute('aria-current', 'page');
      else link.removeAttribute('aria-current');
    }
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
