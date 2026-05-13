import type { IResult } from "./types";
import { useEffect, useState } from "react";
import { ActivityIndicator, ScrollView, Text, View } from "react-native";
import { runExamples } from "./library";
import { styles } from "./styles";

export const Playground: React.FC = () => {
  const [results, setResults] = useState<IResult[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    runExamples().then(setResults).finally(() => setLoading(false))
  }, []);

  return (
    <ScrollView contentContainerStyle={styles.container}>
      <Text style={styles.title}>react-native-nitro-jsdom</Text>

      {loading ? (
        <ActivityIndicator size="large" color="#4ADE80" style={{ marginTop: 40 }} />
      ) : (
        results.map((r) => (
          <View key={r.label} style={styles.card}>
            <Text style={styles.label}>{r.label}</Text>
            <Text style={styles.value}>{r.value}</Text>
          </View>
        ))
      )}
    </ScrollView>
  )
}
