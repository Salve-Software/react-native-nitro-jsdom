## [1.0.1](https://github.com/Salve-Software/react-native-nitro-jsdom/compare/v1.0.0...v1.0.1) (2026-07-19)

### 🐛 Bug Fixes

* **pkg:** point homepage to docs site ([132e649](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/132e6495ea908e81a89a1d56f48f5e2ff3c29902))

### 📚 Documentation

* add repo banner image ([de01500](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/de01500dcac0fb9b600680772c9d2adce7e7d3d0))

## 1.0.0 (2026-07-19)

### ✨ Features

* **api:** reduce JSDOM class to create/evaluate/serialize/dispose ([b0a6650](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b0a6650d5b5f276a69181e07092b39e44d11d5ba))
* **bindings:** node identity cache — one JS wrapper per native node ([9a1ebbb](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/9a1ebbb27ecb21d15474675201bc0bbbd23cad44))
* **bindings:** register window.alert/confirm/prompt as QuickJS C functions ([e0449c0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/e0449c0c96470bb1c591d1dd1bd41e1bb86e38fa))
* **docs:** add docusaurus documentation site ([8d674f8](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/8d674f84037e35de94ce324ef802ca19d5c8d8ab))
* **dom:** add timers, events, console bindings ([3d6f6a0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/3d6f6a0ea64d060f6830e8c9b1780ae130be3b56))
* **dom:** implement element bindings for QuickJS ([32f7974](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/32f797462071113616bfe5e14e7c68b7c0983006))
* **dom:** implement runScripts support ([4487a13](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/4487a13a8d77c1b5dcfc972282496c9e4e7a1e05))
* **example:** add fetch playground section ([55e054e](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/55e054e04a5775695407dac77a435090a4244531))
* **example:** add interactive button to result cards ([b6536ee](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b6536ee8a71334172201f0ac11fde25778388ca3))
* **example:** add storage playground section ([502c5f3](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/502c5f3fa7f042d87562e8223a90f6fbcfb41177))
* **example:** collapsible sections in playground ([34786b9](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/34786b9fbde032f86d01da3094f60c1f6604c1a1))
* **example:** wire interactive alert/confirm to RN Alert ([5349afd](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5349afd8e033dd503082f9d28c784e19e085f9e0))
* **hybrid:** implement setDialogCallbacks in HybridHtmlSandbox ([8c8f468](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/8c8f468f9e611a8e325fc887308ed04b76b9bd53))
* implement HtmlSandbox foundation with Nitro spec and C++ structure ([5f9eb72](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5f9eb728b9a6ed616153bbcd087bd46b4c1efa27))
* **jsdom:** add onFetch option to JSDOM.create ([cb5dcf9](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/cb5dcf91873a5b2c792b7ebf71d580129c8eddc0))
* **lexbor:** implement real DOM parsing and queries ([f532333](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f532333a235403ee80113cde53680bc56f725f90))
* **mutation-observer:** add example demo + tick roadmap ([2c983c5](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/2c983c5dff11afbf9432740d09f10797966b748e))
* **mutation-observer:** add MutationObservers skeleton + lifecycle wiring ([1192aee](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/1192aee86d15c6432e05d40f0396653cd7cf7978))
* **mutation-observer:** implicit attribute/characterData enable + TypeError guard ([5457f81](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5457f81a0bd17c9db79b77107251b73945f7e9ad))
* **mutation-observer:** instrument DOM mutation hooks + detached-node safety ([c2471ad](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/c2471ad0504881ef523bd54a196fb47634e89146))
* **nitro:** add setConsoleCallback to Nitro spec + TS API ([5db1abd](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5db1abdec65e48d3aa1c3b4331c08d33b2b8be68))
* **quickjs:** implement DOM bindings wiring QuickJS to Lexbor ([a550b05](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/a550b05c8087ee69b7aad4c130b861bf20670016))
* **quickjs:** implement isolated JS runtime with evaluate ([c40e67c](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/c40e67c334b6d1b9db2bc4ba9b9a2574493cebc7))
* **runtime:** add dialog callback slots and setters to QuickJSRuntime ([949578a](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/949578a3476480ec603a6ecc875e3fc6a695910b))
* **sandbox:** add CustomEvent ([597f89a](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/597f89abd5fd30c47e39b89506cabd16f23d5dba))
* **sandbox:** add document.title ([4939b84](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/4939b84f35005d1ffa91db6795825abe2ad4cf1e))
* **sandbox:** add generic Node traversal ([6338ca5](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/6338ca559c0c1b8a6fe015341aece82136862dc3))
* **sandbox:** add getElementsByClassName/getElementsByTagName ([d651ac4](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d651ac4926f2ace8bfbaa3c511ebc4b3210e9dda))
* **sandbox:** add localStorage/sessionStorage stubs ([907575f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/907575f415e7c19ebccd4df21b7837ec506024ba))
* **sandbox:** add v0.7 Node identity & DOM ergonomics ([d0cdcbd](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d0cdcbda9190c986cf2e16e87afddfe078129e28))
* **sandbox:** add v0.8 attribute enumeration & node comparison ([516beb4](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/516beb467029606529dfd1530316ac7ec04523bf))
* **sandbox:** add window.location ([24ee9fd](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/24ee9fd405d8935b99aa5ee3103333c8b949ffca))
* **sandbox:** add XMLHttpRequest stub ([016e07a](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/016e07a04a3e538b63dc6f6a2fc9364bfff87028))
* **sandbox:** bridge fetch() through QuickJS to RN's network stack ([092f52e](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/092f52e93c9661ed6cafa75e379d33410e8721c5))
* **sandbox:** finish v0.6 Node & Style API roadmap ([49d4a6e](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/49d4a6e28b85fb45b8075f453d64473d1683df30))
* **spec:** add setDialogCallbacks to HtmlSandbox Nitro spec ([d761eff](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d761eff0f42e0bcb1edf992c94d53782beef8524))
* **spec:** add setFetchCallback to native HtmlSandbox spec ([b3687c7](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b3687c711fdb1efb78bef992e8ce693de503c638))
* **spec:** collapse Nitro spec to initialize/evaluate/serialize ([0019789](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/00197892d2444aa1cb68a2f73ba3f1e198ca92e9))
* **ts:** add onAlert/onConfirm/onPrompt options; wire setDialogCallbacks ([13a0a6a](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/13a0a6ac9356c2b52ef57f404aac87bb342c3c8e))

### 🐛 Bug Fixes

* **a11y:** add table semantics ([f512b76](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f512b76d5520d4c59c679e3e1ed9b7af4b8c491c))
* **android:** resolve QuickJS stack overflow on async thread ([31276b0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/31276b0e40c592beabccc37cef20d8d0d6231035))
* **bindings:** guard JS_NewClassID against multi-runtime overwrite ([276c246](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/276c2465b78b9b8e04c6fd5a6f1054d7845c1085))
* **bindings:** propagate JS_EXCEPTION from JS_ToString in dialog fns ([f4a7b20](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f4a7b20039229064e38b2e1c4de8f14557085f42))
* **classList:** implement full DOMTokenList token contract ([f43ca89](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f43ca89bb2ef1efa5345f5fcd66b883a26cd46ad))
* **custom-event:** consume bubbles/cancelable getter exceptions ([23cc760](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/23cc7601e0913942548b864e777a81781a37ecb7))
* **docs:** remove unimplemented option ([c02eb4b](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/c02eb4bb692207b43d0f9205d9eb790a07c1c286))
* **dom:** resolve JSValue leaks and listener key collision in DOMBindings ([2267aa5](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/2267aa5af1d62387b704fe2275eeb42aa0988ea9))
* **element:** use-after-free in removedNodes, comment textContent, insertBefore ownership ([0a9004f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/0a9004f8bf4759e7ff3e1b9600047cab0757bcf8))
* **events:** dedup listeners, honor removal during dispatch, document receiver ([59a82b0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/59a82b00e22f91506cef5680fccc1f42c00733b3))
* **example:** add jest as a devDependency for react-native-harness ([5267a51](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5267a5134797c32a419735a531eee051a637a8db)), closes [#15](https://github.com/Salve-Software/react-native-nitro-jsdom/issues/15)
* **fetch:** propagate exceptions, guard callbacks, fix headers, json() as promise ([59c30f1](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/59c30f12be5a92c0deccff08f71f3cba3391a1bb))
* **harness:** point iOS runner at an installed simulator ([54d6e4f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/54d6e4f926fca150a3dbfbdb7f2a2822c83d112d))
* **hybrid:** match generated Promise<bool>/Promise<variant> signatures ([f8228f1](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f8228f1710ec7f57bcef9c98306a7c0e7ea2a2d1))
* **hybrid:** use await().get() for confirm/prompt callbacks ([09441d9](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/09441d905425f8a97b5a9e73795a60b3d7a45e52))
* **ios:** resolve iOS build failures ([cf934e0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/cf934e011b3789286b9fd5cfbf66019bbbd78a75)), closes [#include](https://github.com/Salve-Software/react-native-nitro-jsdom/issues/include)
* **lexbor:** apply review fixes ([b82b482](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b82b4829cd740d53af2d810f5492772de7b29cd0))
* **location:** resolve relative URLs against current href ([87b3a9f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/87b3a9fd034107f056d8cf4f1c21a59b6756abbc))
* **mutation-observer:** capture siblings after insertion; null attributeName ([af9c2bd](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/af9c2bdcd63e431ae878708b2ad7d7ee83eb2522))
* **mutation-observer:** empty() fast-path with active_count tracking ([d4364c7](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d4364c71d6a3cb3d793dc0c92d946c70aca36d59))
* **mutation-observer:** generation counter prevents stale dispatch after takeRecords ([ba3db53](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/ba3db5321f2f97397768be730e82175d8797ef52))
* **mutation-observer:** prevent UAF from detached subtree and removedNodes ([a510f20](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/a510f20a6d533fdbb448d9ce8805f53e963223fb))
* **mutation-observer:** re-observe must not clear pending queue ([a7d5e7c](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/a7d5e7c3abe6c2ce72a97f8e032ca1aee83034ae))
* **mutation-observer:** skip oldValue capture when no observer needs it ([a213387](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/a213387ccc782442522d1cf9a6feb720c7dbd3a1))
* **quickjs:** apply review fixes ([0bb1ce8](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/0bb1ce818987abed81a1d2730dacfcbb47567906))
* **quickjs:** resolve memory safety issues in QuickJSRuntime ([38fe8ea](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/38fe8eaaeca7ec2a34cc205b4600b5c456f6d487))
* **quickjs:** unwrap Promise result after event loop drain ([f9c22e0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/f9c22e08a18a2f1bf8beb77fc875a670fda85277))
* resolve Android build and runtime crashes ([5c82fc4](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5c82fc41d3e5b1c2a7ed8778402ef6d178481d13))
* resolve bundle crash ([67b593c](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/67b593c65fa9032abb86e718ee1ec1f3dfdf44ab))
* **style:** skip semicolons inside parentheses when parsing declarations ([78900d9](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/78900d9a118000ebc7774f74b694899195c0b9c0))
* **test:** correct dispose-with-pending-timers test pattern ([ef85fe6](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/ef85fe6ac73f0f1a9ce80ffb415d7c9584f879ff))
* **ui:** fix mobile architecture overflow ([6e6afb7](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/6e6afb71d756dc002ee9ccf3cff0df2481a4dc26))

### 💨 Performance Improvements

* **ci:** reduce build times for iOS and Android ([b8a7eda](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b8a7edaa176acd31c42281778b82593e9d14799c))
* **jsdom:** call setDialogCallbacks only when a dialog option is set ([3f1a832](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/3f1a832672f701629ec56c1bede77bbb92846831))

### 🔄 Code Refactors

* **bindings:** separate Node and Element JS class IDs ([4d28567](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/4d28567f2c698414611fc10cca903c612ebd4672))
* **cpp:** reorganize into lexbor/ quickjs/ subdirs ([63960fe](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/63960fe44c8bcfc404de71961ac6d97138caca9f))
* **example:** extract components to folder ([6d936ff](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/6d936ff5d86ecf188d8ad00d8bc01915cb583eb6))
* move submodules to packages/ dir ([bb51971](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/bb51971fa9189a20bc46cb32354f448b03ad35ca))
* **quickjs:** introduce RuntimeContext, migrate get_doc ([d4a97b8](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d4a97b8a1f4279c3795c134a0a775b841faf2e3f))
* **sandbox:** split DOMBindings.cpp into per-concern modules ([d2fe06f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d2fe06f6b84c9abfaec6b682da560a4e710c10d1))

### 📚 Documentation

* add project overview with architecture, API reference and roadmap ([3107dd0](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/3107dd0bcf72e93d351aa618732657574a385ae4))
* add v0.6 roadmap from jsdom parity audit ([b083fe5](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/b083fe515dddf96f09929b3d0fa51a1e88ed8c3b))
* **example:** update API usage ([09cd3b8](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/09cd3b896936f3768e0377158a50f6560b21c8b8))
* fix inaccurate claims and example ([39bb43e](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/39bb43efe1c0dbbb5587f041725ab596c3656221))
* fix roadmap heading hierarchy ([c672b94](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/c672b9461e5b36e03f17f9ab48a14e064eda0115))
* mark alert/confirm/prompt as shipped; add options to create() example ([0e24fe4](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/0e24fe4b32d38e0bee9bb51749c48166a6f03dd4))
* mark fetch complete in v0.4 roadmap ([092877e](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/092877ee26114a3cb8607d59821eaf40599b8448))
* mark localStorage/sessionStorage complete in v0.4 roadmap ([e2be8ef](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/e2be8efd16ff94f3e3c786b3ed2c5827cbf0386f))
* narrow jsdom compatibility claim ([d810b93](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/d810b93bfcb352495918c840b54a330bc15f284a))
* rewrite readme, add contributing guide ([c879f31](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/c879f31e64e2fcfe8221e9adc2052a6f7ced8ffb))
* sync overview with evaluate-only API ([29b2ed6](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/29b2ed61097c88f66f3fc074d28fcddd5d65f2bf))
* tick v0.3 roadmap checkboxes ([44c173f](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/44c173f622a70346d8b5abeb50a52c128ffe793d))
* up CLAUDE.md ([3866201](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/3866201e9d825304322586f1bb2a828569d18312))
* update roadmap completion status ([a3f67b2](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/a3f67b246d17e28dee224fc151146a5bd03169be))

### 🛠️ Other changes

* add generated and config files from pod install and yarn ([7d7bb82](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/7d7bb82a6a6a6ab6daa53c528fdf6facd1c0e011))
* **example:** gitignore react-native-harness cache directory ([89a8caf](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/89a8caf59e385c52617d6615563e52483bdf6e4c))
* remove aiworkers from CLAUDE.md ([5d164f9](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/5d164f97b49f166e1211c49bc35101f209897547))
* remove dependabot config ([94d5ec2](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/94d5ec27a63c34962d5b547f0a9c97990487d8ee))
* **scripts:** add docs commands ([23e80af](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/23e80af4891470b1cdc8f7dbf3ebc4fc87526a4f))
* up lock ([94c2615](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/94c26157aed677584b28a88d889bdb490921e994))
* up lock file ([8caf8c1](https://github.com/Salve-Software/react-native-nitro-jsdom/commit/8caf8c1b15caf977dc16b77e2953138d68e6b89f))
