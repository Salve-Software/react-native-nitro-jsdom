import type {ReactNode} from 'react';
import clsx from 'clsx';
import Link from '@docusaurus/Link';
import Translate from '@docusaurus/Translate';
import Heading from '@theme/Heading';
import styles from './styles.module.css';
import heroStyles from '../../pages/index.module.css';

export default function HomepageCTA(): ReactNode {
  return (
    <section className={styles.cta}>
      <span className={heroStyles.heroStripe} />
      <div className="container">
        <Heading as="h2" className={styles.ctaTitle}>
          <Translate id="homepage.cta.title">
            Ready to sandbox your first DOM?
          </Translate>
        </Heading>
        <p className={styles.ctaSubtitle}>
          <Translate id="homepage.cta.subtitle">
            Install the library and run your first evaluate() in minutes.
          </Translate>
        </p>
        <div className={styles.ctaButtons}>
          <Link
            className={clsx('button button--lg', heroStyles.primaryButton)}
            to="/docs/intro">
            <Translate id="homepage.cta.getStarted">Get Started</Translate>
          </Link>
          <Link
            className={clsx('button button--lg', styles.ctaSecondary)}
            to="https://github.com/Salve-Software/react-native-nitro-jsdom">
            <Translate id="homepage.cta.viewSource">View Source</Translate>
          </Link>
        </div>
      </div>
    </section>
  );
}
