import type { IResult } from "./types";
import { useEffect, useState } from "react";
import { ActivityIndicator, ScrollView, Text, TouchableOpacity, View } from "react-native";
import { runExamples } from "./library";
import { styles } from "./styles";

export const Playground: React.FC = () => {
  const [results, setResults] = useState<IResult[]>([]);
  const [loading, setLoading] = useState(true);
  const [pressing, setPressing] = useState<string | null>(null);

  useEffect(() => {
    runExamples().then(setResults).finally(() => setLoading(false))
  }, []);

  const handlePress = async (r: IResult) => {
    if (!r.onPress || pressing) return;
    setPressing(r.label);
    try {
      const newValue = await r.onPress();
      setResults(prev => prev.map(item =>
        item.label === r.label ? { ...item, value: newValue } : item
      ));
    } finally {
      setPressing(null);
    }
  };

  return (
    <ScrollView style={styles.scrollView} contentContainerStyle={styles.container}>
      <Text style={styles.title}>react-native-nitro-jsdom</Text>

      {loading ? (
        <ActivityIndicator size="large" color="#4ADE80" style={{ marginTop: 40 }} />
      ) : (
        results.map((r) => (
          <View key={r.label} style={styles.card}>
            <View style={styles.cardContent}>
              <Text style={styles.label}>{r.label}</Text>
              <Text style={styles.value}>{r.value}</Text>
            </View>
            {r.onPress && (
              <TouchableOpacity
                style={[styles.pressButton, pressing === r.label && styles.pressButtonDisabled]}
                onPress={() => handlePress(r)}
                disabled={!!pressing}
                activeOpacity={0.7}
              >
                <Text style={styles.pressButtonText}>
                  {pressing === r.label ? '…' : '▶'}
                </Text>
              </TouchableOpacity>
            )}
          </View>
        ))
      )}
    </ScrollView>
  )
}
