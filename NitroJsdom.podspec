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
    "packages/quickjs/quickjs.{c,h}",
    "packages/quickjs/libregexp.{c,h}",
    "packages/quickjs/libunicode.{c,h}",
    "packages/quickjs/cutils.{c,h}",
    "packages/quickjs/dtoa.{c,h}",
    "packages/quickjs/*.h",
    # Third-party: Lexbor (explicit per-directory — omits ports/windows_nt)
    "packages/lexbor/source/lexbor/core/*.{c,h}",
    "packages/lexbor/source/lexbor/css/**/*.{c,h}",
    "packages/lexbor/source/lexbor/dom/**/*.{c,h}",
    "packages/lexbor/source/lexbor/encoding/*.{c,h}",
    "packages/lexbor/source/lexbor/html/**/*.{c,h}",
    "packages/lexbor/source/lexbor/ns/*.{c,h}",
    "packages/lexbor/source/lexbor/ports/posix/**/*.{c,h}",
    "packages/lexbor/source/lexbor/punycode/*.{c,h}",
    "packages/lexbor/source/lexbor/selectors/*.{c,h}",
    "packages/lexbor/source/lexbor/tag/*.{c,h}",
    "packages/lexbor/source/lexbor/unicode/*.{c,h}",
    "packages/lexbor/source/lexbor/url/*.{c,h}",
    "packages/lexbor/source/lexbor/utils/*.{c,h}",
  ]

  s.preserve_paths = ["packages/lexbor/**", "packages/quickjs/**"]

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/packages/lexbor/source"',
    # QuickJS headers use -iquote so packages/quickjs/VERSION never shadows <version> (C++20)
    'USER_HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/packages/quickjs"',
    'GCC_PREPROCESSOR_DEFINITIONS' => 'CONFIG_VERSION=\"2025-09-13\"',
  }

  s.compiler_flags = '-Wno-implicit-fallthrough -Wno-unused-variable -Wno-sign-compare -Wno-shorten-64-to-32'

  load 'nitrogen/generated/ios/NitroJsdom+autolinking.rb'
  add_nitrogen_files(s)

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'
  install_modules_dependencies(s)
end
