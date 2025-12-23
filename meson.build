project('f1sh-camera-rx', 'cpp',
  version: '2.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'])

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])

# Process Qt files (MOC and UI)
processed = qt6.preprocess(
  moc_headers: ['src/mainwindow.h'],
  ui_files: ['ui/mainwindow.ui'],
  dependencies: qt6_dep
)

sources = [
  'src/main.cpp',
  'src/mainwindow.cpp',
]

executable('f1sh-camera-rx',
  sources + processed,
  dependencies: qt6_dep,
  win_subsystem: 'windows',
  include_directories: include_directories('.')
)
