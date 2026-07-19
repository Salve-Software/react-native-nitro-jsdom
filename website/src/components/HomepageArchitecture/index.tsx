import type {ReactNode} from 'react';
import Link from '@docusaurus/Link';
import Translate from '@docusaurus/Translate';
import Heading from '@theme/Heading';
import styles from './styles.module.css';
import sharedStyles from '../../pages/index.module.css';

export default function HomepageArchitecture(): ReactNode {
  return (
    <section className={`${sharedStyles.section} ${sharedStyles.sectionAlt}`}>
      <div className="container">
        <div className={sharedStyles.sectionHeader}>
          <span className={sharedStyles.sectionEyebrow}>
            <Translate id="homepage.architecture.eyebrow">Under the hood</Translate>
          </span>
          <Heading as="h2" className={sharedStyles.sectionTitle}>
            <Translate id="homepage.architecture.title">
              One synchronous call, two native engines
            </Translate>
          </Heading>
          <p className={sharedStyles.sectionSubtitle}>
            <Translate id="homepage.architecture.subtitle">
              evaluate() is the only door into the sandbox: everything else
              happens natively, off the JS bridge.
            </Translate>
          </p>
        </div>

        <div className={styles.diagram}>
          <div className={`${styles.node} ${styles.nodeTop}`}>
            <Translate id="homepage.architecture.node.ts">
              TypeScript (consumer)
            </Translate>
          </div>

          <div className={styles.arrow}>
            <span className={styles.arrowLabel}>
              <Translate id="homepage.architecture.arrow.jsi">
                synchronous JSI call, no bridge, no JSON
              </Translate>
            </span>
          </div>

          <div className={`${styles.node} ${styles.nodeMid}`}>
            <Translate id="homepage.architecture.node.nitro">
              Nitro Module (C++ binding layer)
            </Translate>
          </div>

          <div className={styles.branches}>
            <div className={styles.branch}>
              <div className={`${styles.node} ${styles.nodeLeaf}`}>Lexbor (C99)</div>
              <ul className={styles.leafList}>
                <li>
                  <Translate id="homepage.architecture.lexbor.html">
                    WHATWG-compliant HTML parsing
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.lexbor.dom">
                    In-memory DOM tree
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.lexbor.query">
                    querySelector / querySelectorAll
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.lexbor.serialize">
                    serialize() → HTML string
                  </Translate>
                </li>
              </ul>
            </div>
            <div className={styles.branch}>
              <div className={`${styles.node} ${styles.nodeLeaf}`}>QuickJS (C)</div>
              <ul className={styles.leafList}>
                <li>
                  <Translate id="homepage.architecture.quickjs.isolated">
                    Isolated JS runtime per instance
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.quickjs.bindings">
                    DOM bindings → delegates to Lexbor
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.quickjs.timers">
                    setTimeout / setInterval, own event loop
                  </Translate>
                </li>
                <li>
                  <Translate id="homepage.architecture.quickjs.exec">
                    Executes arbitrary user scripts
                  </Translate>
                </li>
              </ul>
            </div>
          </div>
        </div>

        <div className={styles.readMore}>
          <Link to="/docs/architecture">
            <Translate id="homepage.architecture.readMore">
              Read the full architecture breakdown →
            </Translate>
          </Link>
        </div>
      </div>
    </section>
  );
}
