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

<<<<<<< HEAD
executable('f1sh-camera-rx',
  sources + processed,
  dependencies: qt6_dep,
  win_subsystem: 'windows',
  include_directories: include_directories('.')
)
=======
# Executable
if host_machine.system() == 'windows'
  executable(
    'f1sh-camera-rx',
    sources,
    dependencies : [
      gtk_dep,
      gstreamer_dep,
      gstreamer_video_dep,
      libcurl_dep,
      json_dep,
      microhttpd_dep,
      threads_dep
    ],
    win_subsystem : 'windows',
    link_args : [ '-lwlanapi', '-liphlpapi' ],
    install : true
  )

  # Install portable launcher script into prefix root
  install_data('run-portable.cmd', install_dir : get_option('prefix'))

  # Post-install script for dependency bundling
  python = find_program('python3', 'python', required : true)
  meson.add_install_script(
    python,
    meson.project_source_root() / 'scripts' / 'windows' / 'meson-post-install.py',
    meson.global_build_root() / '..' / get_option('prefix')
  )
else
  executable(
    'f1sh-camera-rx',
    sources,
    dependencies : [
      gtk_dep,
      gstreamer_dep,
      gstreamer_video_dep,
      libcurl_dep,
      json_dep,
      microhttpd_dep,
      threads_dep
    ],
    install : true
  )
endif
>>>>>>> fc0eb2a (fix: path registration)
