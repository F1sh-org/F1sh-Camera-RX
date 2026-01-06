project(
  'f1sh-camera-rx',
  'c',
  version : '1.0.0',
  default_options : [
    'warning_level=3',
    'werror=false',
    'c_std=c11'
  ]
)

# Dependencies
gtk_dep = dependency('gtk+-3.0')
gstreamer_dep = dependency('gstreamer-1.0')
gstreamer_video_dep = dependency('gstreamer-video-1.0')
threads_dep = dependency('threads')
libcurl_dep = dependency('libcurl')
json_dep = dependency('jansson')
microhttpd_dep = dependency('libmicrohttpd')

# Source files
sources = [
  'src/main.c',
  'src/http_client.c',
  'src/http_server.c',
  'src/ui_main.c',
  'src/ui_login.c',
  'src/ui_configuration.c',
  'src/ui_log.c',
  'src/ui_rotate.c',
  'src/ui_wifi.c',
  'src/wifi_info.c',
  'src/serial_probe.c',
  'src/stream.c'
]

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

  # Install portable launcher script
  install_data('run-portable.cmd', install_dir : '.')

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
