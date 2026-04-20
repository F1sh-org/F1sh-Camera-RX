project('f1sh-camera-rx', 'cpp', 'c',
  version: '2.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=2'])

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Qml', 'Quick', 'QuickControls2', 'Network', 'SerialPort'])

# GStreamer dependencies
gstreamer_dep = dependency('gstreamer-1.0', required: true)
gst_video_dep = dependency('gstreamer-video-1.0', required: true)
gst_app_dep = dependency('gstreamer-app-1.0', required: true)

# gRPC and Protobuf dependencies
protobuf_dep = dependency('protobuf', required: true)
grpc_dep = dependency('grpc++', required: true)

# Find protoc and grpc_cpp_plugin
protoc = find_program('protoc', required: true)
grpc_cpp_plugin = find_program('grpc_cpp_plugin', required: true)

# Generate protobuf and gRPC sources
proto_file = files('proto/f1sh_camera.proto')

# Custom target for protobuf generation
proto_gen = custom_target('proto_gen',
  input: proto_file,
  output: ['f1sh_camera.pb.cc', 'f1sh_camera.pb.h'],
  command: [protoc,
    '--proto_path=@CURRENT_SOURCE_DIR@/proto',
    '--cpp_out=@OUTDIR@',
    '@INPUT@'],
  depend_files: proto_file
)

# Custom target for gRPC generation
grpc_gen = custom_target('grpc_gen',
  input: proto_file,
  output: ['f1sh_camera.grpc.pb.cc', 'f1sh_camera.grpc.pb.h'],
  command: [protoc,
    '--proto_path=@CURRENT_SOURCE_DIR@/proto',
    '--grpc_out=@OUTDIR@',
    '--plugin=protoc-gen-grpc=' + grpc_cpp_plugin.full_path(),
    '@INPUT@'],
  depend_files: proto_file
)

# Process Qt MOC files
processed = qt6.preprocess(
  moc_headers: ['src/serialportmanager.h', 'src/wifimanager.h', 'src/configmanager.h', 'src/logmanager.h', 'src/streammanager.h', 'src/grpcmanager.h', 'src/mdnsmanager.h'],
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
dnsapi = cpp.find_library('dnsapi', required: false)
ws2_32 = cpp.find_library('ws2_32', required: false)

sources = [
  'src/main_qml.cpp',
  'src/serialportmanager.cpp',
  'src/wifimanager.cpp',
  'src/configmanager.cpp',
  'src/logmanager.cpp',
  'src/streammanager.cpp',
  'src/grpcmanager.cpp',
  'src/mdnsmanager.cpp',
]

executable('f1sh-camera-rx',
  sources + processed + qml_resources + [proto_gen, grpc_gen],
  dependencies: [qt6_dep, gstreamer_dep, gst_video_dep, gst_app_dep, protobuf_dep, grpc_dep, wlanapi, iphlpapi, winhttp, dnsapi, ws2_32],
  win_subsystem: 'windows',
  include_directories: [include_directories('.'), include_directories('builddir')],
  install: true
)

if host_machine.system() == 'windows'
  install_data('run-portable.cmd', install_dir: '.')

  python = find_program('python3', 'python', required: true)
  meson.add_install_script(
    python,
    meson.project_source_root() / 'scripts/windows/meson-post-install.py'
  )
endif
