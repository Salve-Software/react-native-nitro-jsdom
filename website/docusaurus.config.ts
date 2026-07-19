import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

const config: Config = {
  title: 'react-native-nitro-jsdom',
  tagline:
    'A headless HTML/DOM environment for React Native, powered by Nitro Modules, Lexbor, and QuickJS.',
  favicon: 'img/favicon.png',

  // Future flags, see https://docusaurus.io/docs/api/docusaurus-config#future
  future: {
    v4: true, // Improve compatibility with the upcoming Docusaurus v4
  },

  // Set the production url of your site here
  url: 'https://salve-software.github.io',
  // Set the /<baseUrl>/ pathname under which your site is served
  // For GitHub pages deployment, it is often '/<projectName>/'
  baseUrl: '/react-native-nitro-jsdom/',

  // GitHub pages deployment config.
  organizationName: 'Salve-Software',
  projectName: 'react-native-nitro-jsdom',
  deploymentBranch: 'gh-pages',
  trailingSlash: false,

  onBrokenLinks: 'throw',

  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'pt-BR'],
    localeConfigs: {
      en: {label: 'English'},
      'pt-BR': {label: 'Português (Brasil)'},
    },
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          editUrl:
            'https://github.com/Salve-Software/react-native-nitro-jsdom/tree/main/website/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    image: 'img/logo.png',
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: 'react-native-nitro-jsdom',
      logo: {
        alt: 'Salve-Software logo',
        src: 'img/logo.png',
      },
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'docsSidebar',
          position: 'left',
          label: 'Docs',
        },
        {
          type: 'localeDropdown',
          position: 'right',
        },
        {
          href: 'https://github.com/Salve-Software/react-native-nitro-jsdom',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Docs',
          items: [
            {label: 'Introduction', to: '/docs/intro'},
            {label: 'Architecture', to: '/docs/architecture'},
            {label: 'Public API', to: '/docs/api'},
            {label: 'Compatibility', to: '/docs/compatibility'},
            {label: 'Roadmap', to: '/docs/roadmap'},
          ],
        },
        {
          title: 'Community',
          items: [
            {
              label: 'Salve-Software on GitHub',
              href: 'https://github.com/Salve-Software',
            },
            {
              label: 'Issues',
              href: 'https://github.com/Salve-Software/react-native-nitro-jsdom/issues',
            },
          ],
        },
        {
          title: 'More',
          items: [
            {
              label: 'npm',
              href: 'https://www.npmjs.com/package/@salve-software/react-native-nitro-jsdom',
            },
            {
              label: 'GitHub',
              href: 'https://github.com/Salve-Software/react-native-nitro-jsdom',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} Salve-Software. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
