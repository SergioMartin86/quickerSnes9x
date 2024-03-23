project('quickerSnes9x','c','cpp',
  version: '1.0.0',
  license: 'GPL-3.0-only',
  default_options : ['cpp_std=c++20', 'default_library=shared', 'buildtype=release'],
  subproject_dir : 'extern'
)

# Loading dependencies
subdir('source')

# Common application flags
commonCompileArgs = [ '-Wall', '-Wfatal-errors' ]

# Grabbing jaffarCommon dependency

jaffarCommonSubproject = subproject('jaffarCommon')
jaffarCommonDependency = jaffarCommonSubproject.get_variable('jaffarCommonDependency')

# Building playback tool

if get_option('buildPlayer') == true

  quickerSnes9xPlayer = executable('quickerSnes9xPlayer',
    'source/player.cpp',
    cpp_args            : [ commonCompileArgs, '-DNCURSES' ],
    dependencies        : [ quickerSnes9xDependency,
                            jaffarCommonDependency,
                            dependency('x11', required : false),
                            dependency('xext', required : false),
                            dependency('xv', required : false), ],
    include_directories : include_directories(['source']),
    link_args           : [ '-lncurses' ],
  )

  snes9xPlayer = executable('snes9xPlayer',
    'source/player.cpp',
    cpp_args            : [ commonCompileArgs, '-DNCURSES' ],
    dependencies        : [ snes9xDependency,
                            jaffarCommonDependency,
                            dependency('x11', required : false),
                            dependency('xext', required : false),
                            dependency('xv', required : false), ],
    include_directories : include_directories(['source']),
    link_args           : [ '-lncurses' ],
  )

endif

# Building tester tool for QuickerSnes9x

quickerSnes9xTester = executable('quickerSnes9xTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ], 
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Building tester tool for the original snes9x

snes9xTester = executable('snes9xTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING'  ],
  dependencies        : [ snes9xDependency, jaffarCommonDependency ],
)

# Building tests
subdir('tests')