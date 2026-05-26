import type { IResult, ISection } from "./types";
import { useEffect, useState } from "react";
import { ActivityIndicator, ScrollView, Text, TouchableOpacity, View } from "react-native";
import { runExamples } from "./library";
import { styles } from "./styles";

const ResultCard: React.FC<{
  result: IResult;
  pressing: boolean;
  onPress: () => void;
}> = ({ result, pressing, onPress }) => (
  <View style={styles.card}>
    <View style={styles.cardContent}>
      <Text style={styles.label}>{result.label}</Text>
      <Text style={styles.value}>{result.value}</Text>
    </View>
    {result.onPress && (
      <TouchableOpacity
        style={[styles.pressButton, pressing && styles.pressButtonDisabled]}
        onPress={onPress}
        disabled={pressing}
        activeOpacity={0.7}
      >
        <Text style={styles.pressButtonText}>{pressing ? '…' : '▶'}</Text>
      </TouchableOpacity>
    )}
  </View>
);

const SectionGroup: React.FC<{
  section: ISection;
  onResultUpdate: (label: string, value: string) => void;
}> = ({ section, onResultUpdate }) => {
  const [expanded, setExpanded] = useState(false);
  const [pressingLabel, setPressingLabel] = useState<string | null>(null);

  const handlePress = async (result: IResult) => {
    if (!result.onPress || pressingLabel) return;
    setPressingLabel(result.label);
    try {
      const newValue = await result.onPress();
      onResultUpdate(result.label, newValue);
    } finally {
      setPressingLabel(null);
    }
  };

  return (
    <View style={styles.section}>
      <TouchableOpacity
        style={styles.sectionHeader}
        onPress={() => setExpanded(v => !v)}
        activeOpacity={0.7}
      >
        <Text style={styles.sectionTitle}>{section.title}</Text>
        <View style={styles.sectionMeta}>
          <Text style={styles.sectionCount}>{section.results.length}</Text>
          <Text style={styles.sectionChevron}>{expanded ? '▲' : '▼'}</Text>
        </View>
      </TouchableOpacity>

      {expanded && section.results.map((r) => (
        <ResultCard
          key={r.label}
          result={r}
          pressing={pressingLabel === r.label}
          onPress={() => handlePress(r)}
        />
      ))}
    </View>
  );
};

export const Playground: React.FC = () => {
  const [sections, setSections] = useState<ISection[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    runExamples().then(setSections).finally(() => setLoading(false));
  }, []);

  const handleResultUpdate = (sectionTitle: string, label: string, value: string) => {
    setSections(prev => prev.map(s =>
      s.title !== sectionTitle ? s : {
        ...s,
        results: s.results.map(r => r.label === label ? { ...r, value } : r),
      }
    ));
  };

  return (
    <ScrollView style={styles.scrollView} contentContainerStyle={styles.container}>
      <Text style={styles.title}>react-native-nitro-jsdom</Text>

      {loading ? (
        <ActivityIndicator size="large" color="#4ADE80" style={{ marginTop: 40 }} />
      ) : (
        sections.map((s) => (
          <SectionGroup
            key={s.title}
            section={s}
            onResultUpdate={(label, value) => handleResultUpdate(s.title, label, value)}
          />
        ))
      )}
    </ScrollView>
  );
};
