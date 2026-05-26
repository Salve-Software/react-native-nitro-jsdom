import React, { useState } from "react";
import { Text, TouchableOpacity, View } from "react-native";
import type { IResult, ISection } from "../../types";
import { styles } from "../../styles";
import { ResultCard } from "../ResultCard/ResultCard";

export const SectionGroup: React.FC<{
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
