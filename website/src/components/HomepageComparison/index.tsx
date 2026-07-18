import type {ReactNode} from 'react';
import Translate, {translate} from '@docusaurus/Translate';
import Heading from '@theme/Heading';
import styles from './styles.module.css';
import sharedStyles from '../../pages/index.module.css';

type Row = {
  label: ReactNode;
  webview: boolean | string;
  nitro: boolean | string;
};

function Cell({value}: {value: boolean | string}) {
  if (typeof value === 'boolean') {
    return (
      <span className={value ? styles.yes : styles.no}>
        {value
          ? '✓'
          : '✕'}
      </span>
    );
  }
  return <span className={styles.textValue}>{value}</span>;
}

export default function HomepageComparison(): ReactNode {
  const rows: Row[] = [
    {
      label: <Translate id="homepage.comparison.row.component">Requires a React component</Translate>,
      webview: true,
      nitro: false,
    },
    {
      label: <Translate id="homepage.comparison.row.headless">Runs headless / off-tree</Translate>,
      webview: false,
      nitro: true,
    },
    {
      label: <Translate id="homepage.comparison.row.isolation">Isolated instances</Translate>,
      webview: translate({id: 'homepage.comparison.value.complex', message: 'Complex'}),
      nitro: translate({id: 'homepage.comparison.value.native', message: 'Native'}),
    },
    {
      label: <Translate id="homepage.comparison.row.arbitrary">Arbitrary JS execution</Translate>,
      webview: true,
      nitro: true,
    },
    {
      label: <Translate id="homepage.comparison.row.fullDom">Full DOM API</Translate>,
      webview: true,
      nitro: translate({id: 'homepage.comparison.value.progressive', message: 'Progressive'}),
    },
    {
      label: <Translate id="homepage.comparison.row.memory">Memory control</Translate>,
      webview: translate({id: 'homepage.comparison.value.limited', message: 'Limited'}),
      nitro: translate({id: 'homepage.comparison.value.full', message: 'Full (dispose())'}),
    },
    {
      label: <Translate id="homepage.comparison.row.overhead">Performance overhead</Translate>,
      webview: translate({id: 'homepage.comparison.value.high', message: 'High'}),
      nitro: translate({id: 'homepage.comparison.value.low', message: 'Low'}),
    },
  ];

  return (
    <section className={sharedStyles.section}>
      <div className="container">
        <div className={sharedStyles.sectionHeader}>
          <span className={sharedStyles.sectionEyebrow}>
            <Translate id="homepage.comparison.eyebrow">The alternative</Translate>
          </span>
          <Heading as="h2" className={sharedStyles.sectionTitle}>
            <Translate id="homepage.comparison.title">
              Why not just use a hidden WebView?
            </Translate>
          </Heading>
        </div>

        <div className={styles.tableWrapper}>
          <table className={styles.table}>
            <thead>
              <tr>
                <th></th>
                <th>
                  <Translate id="homepage.comparison.col.webview">
                    WebView (hidden)
                  </Translate>
                </th>
                <th className={styles.highlightCol}>react-native-nitro-jsdom</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((row, idx) => (
                <tr key={idx}>
                  <td className={styles.rowLabel}>{row.label}</td>
                  <td>
                    <Cell value={row.webview} />
                  </td>
                  <td className={styles.highlightCol}>
                    <Cell value={row.nitro} />
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </section>
  );
}
