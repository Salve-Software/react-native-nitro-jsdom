require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "NitroJsdom"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :visionos => 1.0 }
  s.source       = { :git => "https://github.com/Salve-Software/react-native-nitro-jsdom.git", :tag => "#{s.version}" }

  s.source_files = [
    # Implementation (Swift)
    "ios/**/*.{swift}",
    # Autolinking/Registration (Objective-C++)
    "ios/**/*.{m,mm}",
    # Implementation (C++ objects)
    "cpp/**/*.{hpp,cpp}",
    # Third-party: QuickJS
    "third_party/quickjs/quickjs.{c,h}",
    "third_party/quickjs/libregexp.{c,h}",
    "third_party/quickjs/libunicode.{c,h}",
    "third_party/quickjs/cutils.{c,h}",
    "third_party/quickjs/*.h",
    # Third-party: Lexbor
    "third_party/lexbor/source/**/*.{c,h}",
  ]

  s.preserve_paths = ["third_party/lexbor/**", "third_party/quickjs/**"]

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/third_party/lexbor/source" "$(PODS_TARGET_SRCROOT)/third_party/quickjs"',
    'GCC_PREPROCESSOR_DEFINITIONS' => 'CONFIG_VERSION=\"2024-01-13\"'
  }

  s.compiler_flags = '-Wno-implicit-fallthrough -Wno-unused-variable -Wno-sign-compare -Wno-shorten-64-to-32'

  load 'nitrogen/generated/ios/NitroJsdom+autolinking.rb'
  add_nitrogen_files(s)

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'
  install_modules_dependencies(s)
end
