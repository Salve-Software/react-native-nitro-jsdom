import type {ReactNode} from 'react';
import clsx from 'clsx';
import Link from '@docusaurus/Link';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import CodeBlock from '@theme/CodeBlock';
import Translate, {translate} from '@docusaurus/Translate';

import HomepageFeatures from '@site/src/components/HomepageFeatures';
import HomepageArchitecture from '@site/src/components/HomepageArchitecture';
import HomepageComparison from '@site/src/components/HomepageComparison';
import HomepageCTA from '@site/src/components/HomepageCTA';
import {IconGithub} from '@site/src/components/Icons';

import styles from './index.module.css';

const heroCode = `import { JSDOM } from 'react-native-nitro-jsdom'

const dom = JSDOM.create(\`
  <html>
    <body>
      <div id="result">0</div>
    </body>
  </html>
\`)

await dom.evaluate(\`
  document.getElementById('result').textContent = String(2 + 2)
\`)

const value = await dom.evaluate(
  \`document.getElementById('result').textContent\`
)
// → "4"

dom.dispose()`;

function HomepageHeader() {
  return (
    <header className={styles.heroBanner}>
      <span className={styles.heroStripe} />
      <div className="container">
        <div className={styles.heroInner}>
          <div>
            <span className={styles.badge}>
              <Translate id="homepage.hero.badge">
                Nitro Modules · Lexbor · QuickJS
              </Translate>
            </span>
            <Heading as="h1" className={styles.heroTitle}>
              <Translate id="homepage.hero.title.line1">
                A headless DOM sandbox
              </Translate>
              <br />
              <span className={styles.heroTitleAccent}>
                <Translate id="homepage.hero.title.line2">
                  for React Native
                </Translate>
              </span>
            </Heading>
            <p className={styles.heroSubtitle}>
              <Translate id="homepage.hero.subtitle">
                A headless HTML/DOM environment for React Native, powered by
                Nitro Modules, Lexbor, and QuickJS.
              </Translate>
            </p>
            <div className={styles.buttons}>
              <Link
                className={clsx(
                  'button button--lg',
                  styles.primaryButton
                )}
                to="/docs/intro">
                <Translate id="homepage.hero.cta.getStarted">
                  Get Started
                </Translate>
              </Link>
              <Link
                className={clsx(
                  'button button--lg',
                  styles.secondaryButton
                )}
                to="https://github.com/Salve-Software/react-native-nitro-jsdom">
                <IconGithub width={18} height={18} />
                <Translate id="homepage.hero.cta.github">GitHub</Translate>
              </Link>
            </div>
          </div>
          <div className={styles.heroCodePanel}>
            <CodeBlock language="ts" title="sandbox.ts">
              {heroCode}
            </CodeBlock>
          </div>
        </div>
      </div>
    </header>
  );
}

export default function Home(): ReactNode {
  const description = translate({
    id: 'homepage.metaDescription',
    message:
      'A headless HTML/DOM environment for React Native, powered by Nitro Modules, Lexbor, and QuickJS. Run and mutate a real DOM off the React tree, no WebView required.',
  });

  return (
    <Layout
      title={translate({
        id: 'homepage.metaTitle',
        message: 'react-native-nitro-jsdom: Headless DOM sandbox for React Native',
      })}
      description={description}>
      <HomepageHeader />
      <main>
        <HomepageFeatures />
        <HomepageArchitecture />
        <HomepageComparison />
        <HomepageCTA />
      </main>
    </Layout>
  );
}
