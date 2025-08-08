project('hyprcrosshair', 'c',
  version: '0.1.0',
  default_options: ['warning_level=2', 'c_std=c11']
)

gtk = dependency('gtk4', version: '>=4.10')
adw = dependency('libadwaita-1', version: '>=1.2')

layershell = dependency('gtk4-layer-shell-0', required: false)
if not layershell.found()
  layershell = dependency('gtk4-layer-shell', required: false)
endif
if not layershell.found()
  error('Dependency gtk4-layer-shell not found. Install gtk-layer-shell with GTK4 support (pkg-config: gtk4-layer-shell-0 or gtk4-layer-shell).')
endif

cc = meson.get_compiler('c')
libm = cc.find_library('m', required: false)

deps = [gtk, adw, layershell]
if libm.found()
  deps += libm
endif

executable('hyprcrosshair',
  sources: ['hyprcrosshair.c'],
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir')
)

install_data('data/hyprcrosshair.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications')
)