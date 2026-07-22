import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

const sidebars: SidebarsConfig = {
  docsSidebar: [
    'intro',
    'architecture',
    'api',
    'compatibility',
    'testing-webview',
    'roadmap',
  ],
};

export default sidebars;
