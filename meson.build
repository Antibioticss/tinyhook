project('tinyhook', 'c',
  version : '1.0',
  default_options : ['warning_level=2', 'c_std=gnu11']
)

# Options
tinyhook_compact = get_option('tinyhook_compact')
tinyhook_no_export = get_option('tinyhook_no_export')

# Source files
th_sources = [
  'src/memory.c',
  'src/tinyhook.c',
  'src/interpose.c',
  'src/objcrt.c',
  'src/symsolve/symtable.c',
  'src/symsolve/symexport.c',
  'src/exhook.c'
]

# Compile arguments
compile_args = ['-fvisibility=hidden', '-Wall', '-Wshadow']

if tinyhook_compact
  compile_args += ['-DCOMPACT']
endif
if tinyhook_no_export
  compile_args += ['-DNO_EXPORT']
endif

# Include directories
inc = include_directories('src')
public_inc = include_directories('include')

# Static library
tinyhook_static = static_library(
  'tinyhook',
  th_sources,
  include_directories : [inc, public_inc],
  c_args : compile_args
)

# Shared library
tinyhook_shared = shared_library(
  'tinyhook',
  th_sources,
  include_directories : [inc, public_inc],
  c_args : compile_args,
  link_with : [],
  dependencies : dependency('objc', required : false)
)
