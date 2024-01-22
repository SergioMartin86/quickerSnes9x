project('quickerSnes9x','c','cpp',
  version: '1.0.0',
  license: 'GPL-3.0-only',
  default_options : ['cpp_std=c++20', 'default_library=shared', 'buildtype=release']
)

# Loading dependencies
subdir('source')

# Common application flags
commonCompileArgs = [ '-Wfatal-errors', '-Wall' ]

# Building playback tool

if get_option('buildPlayer') == true
  executable('player',
    'source/player.cpp',
    cpp_args            : [ commonCompileArgs, '-DNCURSES' ],
    dependencies        : [ quickerSnes9xDependency, toolDependency, dependency('sdl2'), dependency('SDL2_image') ],
    include_directories : include_directories(['source']),
    link_args           : [ '-lncurses' ],
  )
endif

# Building tester tool for QuickerSnes9x

quickerNESTester = executable('quickerSnes9xTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING', '-Werror' ],
  dependencies        : [ quickerSnes9xDependency, toolDependency ],
  include_directories : include_directories(['../extern/json'])
)

# Building tester tool for the original snes9x

quickNESTester = executable('snes9xTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs,  ],
  dependencies        : [ snes9xDependency, toolDependency ],
  include_directories : include_directories(['../extern/json'])
)

# Building tests
subdir('tests')