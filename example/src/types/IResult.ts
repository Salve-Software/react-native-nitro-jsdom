export interface IResult {
  label: string;
  value: string;
  onPress?: () => Promise<string>;
}