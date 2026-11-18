/* MIT. Offline saving of the bundled neutral CAD files. */
(() => {
  if (location.protocol !== 'file:' || typeof DecompressionStream === 'undefined') return;
  let assets;
  function load() {
    return assets ??= new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = '零件数据.js';
      script.onload = () => resolve(globalThis.FluxRailPartFiles);
      script.onerror = reject;
      document.head.append(script);
    });
  }
  document.addEventListener('click', async event => {
    const link = event.target.closest?.('a[data-part-file]');
    if (!link || event.ctrlKey || event.metaKey || event.shiftKey || event.defaultPrevented) return;
    event.preventDefault();
    try {
      const path = decodeURIComponent(link.getAttribute('href'));
      const b64 = (await load())[path];
      if (!b64) throw new Error('Missing packaged part');
      const compressed = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
      const stream = new Blob([compressed]).stream().pipeThrough(new DecompressionStream('gzip'));
      const blob = await new Response(stream).blob();
      const saver = document.createElement('a');
      saver.download = path.split('/').at(-1);
      saver.href = URL.createObjectURL(blob);
      document.body.append(saver);
      saver.click();
      saver.remove();
      setTimeout(() => URL.revokeObjectURL(saver.href), 30000);
    } catch {
      // The file itself is still available when opened from a partial unpack.
      location.href = link.href;
    }
  });
})();
