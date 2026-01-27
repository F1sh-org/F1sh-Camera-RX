project('f1sh-camera-rx', 'cpp',
  version: '2.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'])

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Qml', 'Quick', 'QuickControls2', 'Network'])

# Process Qt MOC files
processed = qt6.preprocess(
  moc_headers: ['src/serialportmanager.h', 'src/wifimanager.h', 'src/configmanager.h', 'src/logmanager.h'],
  dependencies: qt6_dep
)

# Compile QML resources
qml_resources = qt6.compile_resources(
  sources: 'qml.qrc'
)

# Windows libraries for WiFi and HTTP
cpp = meson.get_compiler('cpp')
wlanapi = cpp.find_library('wlanapi', required: false)
iphlpapi = cpp.find_library('iphlpapi', required: false)
winhttp = cpp.find_library('winhttp', required: false)

sources = [
  'src/main_qml.cpp',
  'src/serialportmanager.cpp',
  'src/wifimanager.cpp',
  'src/configmanager.cpp',
  'src/logmanager.cpp',
]

executable('f1sh-camera-rx',
  sources + processed + qml_resources,
  dependencies: [qt6_dep, wlanapi, iphlpapi, winhttp],
  win_subsystem: 'windows',
  include_directories: include_directories('.')
)
