project(
    'liquidd', 'c',
    default_options : ['c_std=gnu11'],
    meson_version : '>=0.56.0',
)

cc = meson.get_compiler('c')

if cc.has_argument('-fno-strict-aliasing')
    add_project_arguments('-fno-strict-aliasing', language : 'c')
endif

common_deps = [
    dependency('glib-2.0'),
    dependency('gio-2.0'),
    dependency('gio-unix-2.0'),
]

server_deps = common_deps + [
    dependency('gudev-1.0'),
]

sources = files(
    'liquidd.c',
    'hid_device.c',
    'hid_device_info.c',
    'hid_manager.c',
    'driver.c',
    'driver_hid.c',
    'driver_nzxt_smart2.c',
)

gnome = import('gnome')

gdbus_sources = gnome.gdbus_codegen(
    'dbus_interfaces',
    sources: files(
        'org.liquidctl.FanSpeedRPM.xml',
        'org.liquidctl.InitDevice.xml',
        'org.liquidctl.HidDevice.xml',
    ),
    interface_prefix : 'org.liquidctl.',
    namespace : 'Liquid_DBus',
    object_manager: true,
    autocleanup: 'all'
)

executable('liquidd', sources, gdbus_sources, dependencies : server_deps)
executable('liquidctl', 'liquidctl.c', gdbus_sources, dependencies : common_deps)

configure_file(
    input : 'aux' / 'liquidd.sublime-project.in',
    output : 'liquidd.sublime-project',
    configuration : {
        'source_dir' : meson.project_source_root(),
        'build_dir' : meson.project_build_root(),
    }
)
