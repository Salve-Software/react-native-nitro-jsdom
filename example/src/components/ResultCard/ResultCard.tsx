import React from "react";
import { Text, TouchableOpacity, View } from "react-native";
import type { IResult } from "../../types";
import { styles } from "../../styles";

export const ResultCard: React.FC<{
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
