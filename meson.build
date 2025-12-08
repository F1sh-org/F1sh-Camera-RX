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
  'src/ui.c',
  'src/stream.c',
  'src/serial_service.c'
]

# Executable
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
