import { StyleSheet } from "react-native";

export const styles = StyleSheet.create({
  scrollView: {
    backgroundColor: '#0D0D0D',
  },
  
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
    flexDirection: 'row',
    alignItems: 'center',
  },

  cardContent: {
    flex: 1,
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

  pressButton: {
    width: 36,
    height: 36,
    borderRadius: 8,
    backgroundColor: '#222',
    borderWidth: 1,
    borderColor: '#4ADE80',
    alignItems: 'center',
    justifyContent: 'center',
    marginLeft: 12,
  },

  pressButtonDisabled: {
    borderColor: '#333',
  },

  pressButtonText: {
    color: '#4ADE80',
    fontSize: 14,
  },

  section: {
    marginBottom: 16,
  },

  sectionHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: 10,
    paddingHorizontal: 4,
    marginBottom: 4,
    borderBottomWidth: 1,
    borderBottomColor: '#2A2A2A',
  },

  sectionTitle: {
    fontSize: 13,
    fontWeight: '700',
    color: '#E8E8E8',
    fontFamily: 'monospace',
    letterSpacing: 0.5,
    textTransform: 'uppercase',
  },

  sectionMeta: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },

  sectionCount: {
    fontSize: 11,
    color: '#555',
    fontFamily: 'monospace',
  },

  sectionChevron: {
    fontSize: 10,
    color: '#555',
  },
})
