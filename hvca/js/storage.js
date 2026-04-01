import { STORAGE_KEY } from './utils.js';

export const Storage = {
  load(defaultState) {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return defaultState;
      const parsed = JSON.parse(raw);
      if (!parsed || !Array.isArray(parsed.equipamentos) || !Array.isArray(parsed.registros)) {
        return defaultState;
      }
      return parsed;
    } catch (_) {
      return defaultState;
    }
  },

  save(state) {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
      return true;
    } catch (_) {
      alert('Falha ao salvar dados locais. Verifique armazenamento do navegador.');
      return false;
    }
  },
};
