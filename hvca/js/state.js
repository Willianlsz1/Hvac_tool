import { Utils } from './utils.js';
import { Storage } from './storage.js';

const INITIAL_STATE = { equipamentos: [], registros: [] };

const listeners = new Set();
let state = Storage.load(INITIAL_STATE);

function emit() { listeners.forEach(fn => fn(getState())); }

export function getState() {
  return {
    equipamentos: [...state.equipamentos],
    registros: [...state.registros],
  };
}

export function subscribe(listener) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function persist() { Storage.save(state); }

export function setState(updater, options = { persist: true, emit: true }) {
  const nextState = updater(state);
  if (nextState) state = nextState;
  if (options.persist) persist();
  if (options.emit) emit();
}

export function findEquip(id) {
  return state.equipamentos.find(e => e.id === id);
}

export function regsForEquip(id) {
  return state.registros.filter(r => r.equipId === id);
}

export function lastRegForEquip(id) {
  return regsForEquip(id).sort((a, b) => b.data.localeCompare(a.data))[0];
}

export function seedIfEmpty() {
  if (state.equipamentos.length) return;

  const eq = [
    { id: Utils.uid(), nome: 'Split UTI Adulto', tag: 'AC-UTI-01', local: 'UTI Adulto – 2º Andar', tipo: 'Split Hi-Wall', modelo: 'Carrier 18000 BTU', fluido: 'R-410A', status: 'ok' },
    { id: Utils.uid(), nome: 'Split Centro Cirúrgico', tag: 'AC-CC-01', local: 'Centro Cirúrgico – 3º Andar', tipo: 'Split Piso Teto', modelo: 'Trane 24000 BTU', fluido: 'R-410A', status: 'warn' },
    { id: Utils.uid(), nome: 'Fan Coil Recepção', tag: 'FC-REC-01', local: 'Recepção – Térreo', tipo: 'Fan Coil', modelo: 'Hitachi 36000 BTU', fluido: 'R-22', status: 'ok' },
    { id: Utils.uid(), nome: 'Câmara Fria Farmácia', tag: 'CF-FAR-01', local: 'Farmácia – Subsolo', tipo: 'Câmara Fria', modelo: 'Tecumseh', fluido: 'R-134A', status: 'danger' },
  ];

  state = {
    equipamentos: eq,
    registros: [
      { id: Utils.uid(), equipId: eq[0].id, data: Utils.datetimeOffset(-2), tipo: 'Manutenção Preventiva', pecas: 'Filtro G4, lubrificante', obs: 'Limpeza geral, verificação de correntes e tensões. Tudo OK.', proxima: Utils.dateOffset(28), status: 'ok', fotos: [] },
      { id: Utils.uid(), equipId: eq[1].id, data: Utils.datetimeOffset(-5), tipo: 'Carga de Gás Refrigerante', pecas: 'R-410A 1,5 kg', obs: 'Baixa eficiência detectada. Vazamento na linha de líquido, reparo e recarga realizada. Monitorar.', proxima: Utils.dateOffset(10), status: 'warn', fotos: [] },
      { id: Utils.uid(), equipId: eq[3].id, data: Utils.datetimeOffset(-1), tipo: 'Manutenção Corretiva', pecas: 'Capacitor 35µF', obs: 'Compressor não ligava. Capacitor queimado substituído. Aguardando verificação de temperatura.', proxima: Utils.dateOffset(3), status: 'danger', fotos: [] },
    ],
  };

  persist();
}
