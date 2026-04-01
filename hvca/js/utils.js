export const STORAGE_KEY = 'cooltrack_v3';

export const TIPO_ICON = {
  'Split Hi-Wall': '❄️',
  'Split Cassete': '🌀',
  'Split Piso Teto': '📐',
  'Fan Coil': '💨',
  'Chiller': '🧊',
  'Câmara Fria': '🏔️',
  'VRF / VRV': '🔁',
};

export const STATUS_LABEL = { ok: 'Normal', warn: 'Atenção', danger: 'Crítico' };

export const Utils = {
  uid() {
    return Date.now().toString(36) + Math.random().toString(36).slice(2);
  },
  dateOffset(days) {
    const d = new Date();
    d.setDate(d.getDate() + days);
    return d.toISOString().slice(0, 10);
  },
  datetimeOffset(days) {
    const d = new Date();
    d.setDate(d.getDate() + days);
    return d.toISOString().slice(0, 16);
  },
  nowDatetime() {
    const d = new Date();
    d.setMinutes(d.getMinutes() - d.getTimezoneOffset());
    return d.toISOString().slice(0, 16);
  },
  formatDatetime(iso) {
    if (!iso) return '—';
    const d = new Date(iso);
    if (Number.isNaN(d.getTime())) return '—';
    return `${d.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit', year: 'numeric' })} ${d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' })}`;
  },
  formatDate(iso) {
    if (!iso || !iso.includes('-')) return '—';
    const [y, m, d] = iso.split('-');
    return `${d}/${m}/${y}`;
  },
  daysDiff(isoDate) {
    if (!isoDate) return 0;
    const today = new Date();
    today.setHours(0, 0, 0, 0);
    return Math.ceil((new Date(`${isoDate}T00:00:00`) - today) / 86400000);
  },
  truncate(str = '', len = 80) {
    return str.length > len ? `${str.slice(0, len)}...` : str;
  },
  escapeHtml(value = '') {
    return String(value).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  },
  getEl(id) { return document.getElementById(id); },
  getVal(id) { return Utils.getEl(id)?.value ?? ''; },
  setVal(id, value) { const el = Utils.getEl(id); if (el) el.value = value; },
  clearVals(...ids) { ids.forEach(id => Utils.setVal(id, '')); },
};
