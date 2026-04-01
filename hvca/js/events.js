import { Utils } from './utils.js';
import { renderEquip, renderHist, renderRelatorio, Photos } from './ui.js';

export function bindEvents() {
  Utils.getEl('hist-busca')?.addEventListener('input', () => renderHist());
  Utils.getEl('hist-equip')?.addEventListener('change', () => renderHist());
  Utils.getEl('rel-equip')?.addEventListener('change', () => renderRelatorio());
  Utils.getEl('rel-de')?.addEventListener('change', () => renderRelatorio());
  Utils.getEl('rel-ate')?.addEventListener('change', () => renderRelatorio());

  const equipSearch = document.querySelector('#view-equipamentos .search-bar__input');
  equipSearch?.addEventListener('input', (e) => renderEquip(e.target.value));

  const upload = document.querySelector('.photo-drop input[type="file"]');
  upload?.addEventListener('change', e => Photos.add(e.target));

  Utils.getEl('lightbox')?.addEventListener('click', () => Photos.closeLightbox());
}
