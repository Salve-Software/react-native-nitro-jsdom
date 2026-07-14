import type { ISection } from '../types';
import { domSection } from './sections/dom';
import { timersSection } from './sections/timers';
import { consoleSection } from './sections/console';
import { mutationObserverSection } from './sections/mutationObserver';
import { dialogsSection } from './sections/dialogs';
import { fetchSection } from './sections/fetch';

export const runExamples = async (): Promise<ISection[]> => {
  return Promise.all([
    domSection(),
    timersSection(),
    consoleSection(),
    mutationObserverSection(),
    dialogsSection(),
    fetchSection(),
  ]);
};
