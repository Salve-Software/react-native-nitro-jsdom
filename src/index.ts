import { NitroModules } from 'react-native-nitro-modules'
import type { NitroJsdom as NitroJsdomSpec } from './specs/nitro-jsdom.nitro'

export const NitroJsdom =
  NitroModules.createHybridObject<NitroJsdomSpec>('NitroJsdom')