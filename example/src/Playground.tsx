import type { ISection } from "./types";
import { useCallback, useEffect, useState } from "react";
import { ActivityIndicator, ScrollView, Text } from "react-native";
import { runExamples } from "./library";
import { styles } from "./styles";
import { SectionGroup } from "./components";

export const Playground: React.FC = () => {
  const [sections, setSections] = useState<ISection[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    runExamples().then(setSections).finally(() => setLoading(false));
  }, []);

  const handleResultUpdate = useCallback((sectionTitle: string, label: string, value: string) => {
    setSections(prev => prev.map(s =>
      s.title !== sectionTitle ? s : {
        ...s,
        results: s.results.map(r => r.label === label ? { ...r, value } : r),
      }
    ));
  }, []);

  return (
    <ScrollView style={styles.scrollView} contentContainerStyle={styles.container}>
      <Text style={styles.title}>react-native-nitro-jsdom</Text>

      {loading
        ?
        <ActivityIndicator size="large" color="#4ADE80" style={{ marginTop: 40 }} />
        :
        sections.map((s) => (
          <SectionGroup
            key={s.title}
            section={s}
            onResultUpdate={(label, value) => handleResultUpdate(s.title, label, value)}
          />
        ))
      }
    </ScrollView>
  );
};
