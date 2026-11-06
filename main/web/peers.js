/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
/* Peer menu: fills <nav id="peers"> with a drop-down (<select id="peer-select">) built
 * from GET /api/peers. Peer data only goes through textContent and value. The entry whose
 * label equals this device's name (ignoring case) is the selected option, so one list
 * fits every device; without such an entry a disabled "Peers" placeholder is selected.
 * Choosing another entry opens its URL in this tab. The menu stays hidden while the
 * list holds no other device. */
let peersCurrent = null;            /* option that shows where we are */

function peersRender(d) {
  const nav = document.getElementById('peers');
  if (!nav) return;
  const self = String((d && d.self) || '').toLowerCase();
  const sel = document.createElement('select');
  sel.id = 'peer-select';
  sel.className = 'peer-select';
  sel.setAttribute('aria-label', 'Peers');
  let current = null, others = 0;
  ((d && Array.isArray(d.peers)) ? d.peers : []).forEach(p => {
    const name = String(p.name), url = String(p.url);
    const mine = name.toLowerCase() === self;
    if (mine && current) return;                                  /* one "you are here" */
    if (!mine && !/^https?:\/\/[^\s"'<>`]+$/.test(url)) return;   /* never a javascript: URL */
    const o = document.createElement('option');
    o.textContent = name.toUpperCase();
    o.value = url;
    sel.appendChild(o);
    if (mine) current = o; else others++;
  });
  if (!current) {
    current = document.createElement('option');
    current.textContent = 'Peers';
    current.value = '';
    current.disabled = true;
    sel.insertBefore(current, sel.firstChild);
  }
  current.selected = true;
  sel.addEventListener('change', () => {
    const o = sel.options[sel.selectedIndex];
    if (o && o !== current && o.value) location.href = o.value;
    current.selected = true;        /* Back must not show the other device selected */
  });
  peersCurrent = current;
  nav.textContent = '';
  nav.appendChild(sel);
  nav.hidden = others === 0;
}

window.addEventListener('pageshow', () => { if (peersCurrent) peersCurrent.selected = true; });

fetch('/api/peers', {cache: 'no-store'})
  .then(r => (r.ok ? r.json() : null))
  .then(peersRender)
  .catch(() => {});
