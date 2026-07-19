import type {ReactNode} from 'react';
import Translate from '@docusaurus/Translate';
import Heading from '@theme/Heading';
import {
  IconLayers,
  IconBox,
  IconZap,
  IconCode,
  IconSync,
  IconTrash,
} from '@site/src/components/Icons';
import styles from './styles.module.css';
import sharedStyles from '../../pages/index.module.css';

type FeatureItem = {
  icon: React.ComponentType<React.ComponentProps<'svg'>>;
  title: ReactNode;
  description: ReactNode;
};

const FeatureList: FeatureItem[] = [
  {
    icon: IconLayers,
    title: <Translate id="homepage.features.offTree.title">Headless & off-tree</Translate>,
    description: (
      <Translate id="homepage.features.offTree.description">
        No React component, no screen, no UI tree. Parse, mutate, and evaluate
        HTML entirely off-screen.
      </Translate>
    ),
  },
  {
    icon: IconBox,
    title: <Translate id="homepage.features.isolated.title">Truly isolated runtime</Translate>,
    description: (
      <Translate id="homepage.features.isolated.description">
        Every JSDOM.create() spins up its own QuickJS runtime, not a shared
        Hermes instance. No leaking globals between sandboxes.
      </Translate>
    ),
  },
  {
    icon: IconZap,
    title: <Translate id="homepage.features.native.title">Native HTML parsing</Translate>,
    description: (
      <Translate id="homepage.features.native.description">
        Backed by Lexbor, the fastest WHATWG-compliant HTML parser in C99,
        with zero dependencies.
      </Translate>
    ),
  },
  {
    icon: IconCode,
    title: <Translate id="homepage.features.jsdom.title">jsdom-shaped DOM API</Translate>,
    description: (
      <Translate id="homepage.features.jsdom.description">
        querySelector, textContent, dataset, and more mirror jsdom's DOM
        shape, though usage differs: everything runs through the async
        evaluate(), not jsdom's synchronous object access.
      </Translate>
    ),
  },
  {
    icon: IconSync,
    title: <Translate id="homepage.features.jsi.title">Synchronous JSI bridge</Translate>,
    description: (
      <Translate id="homepage.features.jsi.description">
        Powered by Nitro Modules, with direct JSI calls, no bridge, no JSON
        serialization overhead.
      </Translate>
    ),
  },
  {
    icon: IconTrash,
    title: <Translate id="homepage.features.memory.title">Full memory control</Translate>,
    description: (
      <Translate id="homepage.features.memory.description">
        Call dispose() to deterministically free the Lexbor document and
        QuickJS runtime, no waiting on GC.
      </Translate>
    ),
  },
];

function Feature({icon: Icon, title, description}: FeatureItem) {
  return (
    <div className={styles.featureCard}>
      <div className={styles.featureIcon}>
        <Icon width={24} height={24} />
      </div>
      <Heading as="h3" className={styles.featureTitle}>
        {title}
      </Heading>
      <p className={styles.featureDescription}>{description}</p>
    </div>
  );
}

export default function HomepageFeatures(): ReactNode {
  return (
    <section className={sharedStyles.section}>
      <div className="container">
        <div className={sharedStyles.sectionHeader}>
          <span className={sharedStyles.sectionEyebrow}>
            <Translate id="homepage.features.eyebrow">Why this library</Translate>
          </span>
          <Heading as="h2" className={sharedStyles.sectionTitle}>
            <Translate id="homepage.features.title">
              Everything a WebView gives you, none of the baggage
            </Translate>
          </Heading>
        </div>
        <div className={styles.featureGrid}>
          {FeatureList.map((props, idx) => (
            <Feature key={idx} {...props} />
          ))}
        </div>
      </div>
    </section>
  );
}
