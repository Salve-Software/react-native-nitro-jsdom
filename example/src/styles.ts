import { StyleSheet } from "react-native";

export const styles = StyleSheet.create({
  container: {
    padding: 24,
    paddingTop: 60,
    backgroundColor: '#0D0D0D',
    minHeight: '100%',
  },

  title: {
    fontSize: 20,
    fontWeight: '700',
    marginBottom: 24,
    color: '#E8E8E8',
    textAlign: 'center',
  },

  card: {
    backgroundColor: '#1A1A1A',
    borderRadius: 10,
    padding: 16,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOpacity: 0.4,
    shadowRadius: 6,
    elevation: 4,
  },

  label: {
    fontSize: 13,
    color: '#555',
    marginBottom: 4,
    fontFamily: 'monospace',
  },

  value: {
    fontSize: 16,
    color: '#4ADE80',
    fontWeight: '600',
    fontFamily: 'monospace',
  },
})
