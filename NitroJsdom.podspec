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
    # Third-party: QuickJS (uncomment once third_party/quickjs is added as a submodule)
    # "third_party/quickjs/quickjs.{c,h}",
    # "third_party/quickjs/libregexp.{c,h}",
    # "third_party/quickjs/libunicode.{c,h}",
    # "third_party/quickjs/cutils.{c,h}",
  ]

  # Third-party: Lexbor
  # Option A — CocoaPod (if available): s.dependency 'Lexbor'
  # Option B — vendored sources: add third_party/lexbor source files above and set:
  # s.preserve_paths = "third_party/lexbor/**"
  # s.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/../third_party/lexbor/source"' }

  load 'nitrogen/generated/ios/NitroJsdom+autolinking.rb'
  add_nitrogen_files(s)

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'
  install_modules_dependencies(s)
end
