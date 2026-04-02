import { STORAGE_KEY } from './utils.js';

function normalizeEquip(e) {
  if (!e || typeof e !== 'object') return null;
  if (!e.id || !e.nome || !e.local) return null;
  return {
    id: String(e.id),
    nome: String(e.nome),
    local: String(e.local),
    status: ['ok', 'warn', 'danger'].includes(e.status) ? e.status : 'ok',
    tag: String(e.tag || ''),
    tipo: String(e.tipo || 'Outro'),
    modelo: String(e.modelo || ''),
    fluido: String(e.fluido || ''),
  };
}

function normalizeRegistro(r, equipamentoIds) {
  if (!r || typeof r !== 'object') return null;
  if (!r.id || !r.equipId || !equipamentoIds.has(String(r.equipId))) return null;
  if (!r.data || !r.tipo) return null;
  return {
    id: String(r.id),
    equipId: String(r.equipId),
    data: String(r.data),
    tipo: String(r.tipo),
    obs: String(r.obs || ''),
    status: ['ok', 'warn', 'danger'].includes(r.status) ? r.status : 'ok',
    pecas: String(r.pecas || ''),
    proxima: String(r.proxima || ''),
    fotos: Array.isArray(r.fotos) ? r.fotos.filter(f => typeof f === 'string') : [],
  };
}

export const Storage = {
  load(defaultState) {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return defaultState;
      const parsed = JSON.parse(raw);
      if (!parsed || !Array.isArray(parsed.equipamentos) || !Array.isArray(parsed.registros)) {
        return defaultState;
      }
      const equipamentos = parsed.equipamentos.map(normalizeEquip).filter(Boolean);
      const equipamentoIds = new Set(equipamentos.map(e => e.id));
      const registros = parsed.registros.map(r => normalizeRegistro(r, equipamentoIds)).filter(Boolean);
      return { equipamentos, registros };
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
