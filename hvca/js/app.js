import { seedIfEmpty } from './state.js';
import { Modal, updateHeader, renderInicio, populateSelects } from './ui.js';
import { bindEvents } from './events.js';

function bootstrap() {
  seedIfEmpty();
  Modal.init();
  bindEvents();
  populateSelects();
  updateHeader();
  renderInicio();
}

bootstrap();
