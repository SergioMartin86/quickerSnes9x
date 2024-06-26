project('quickerSnes9x','c','cpp',
  version: '1.0.0',
  default_options : ['cpp_std=c++20', 'default_library=shared', 'buildtype=release'],
  subproject_dir : 'extern'
)

# Loading dependencies
subdir('source')

# Do not build any targets if this is a subproject
if meson.is_subproject() == false

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
                            jaffarCommonDependency
                          ],
    include_directories : include_directories(['source']),
    link_args           : [ '-lncurses' ],
  )

  snes9xPlayer = executable('snes9xPlayer',
    'source/player.cpp',
    cpp_args            : [ commonCompileArgs, '-DNCURSES' ],
    dependencies        : [ snes9xDependency,
                            jaffarCommonDependency,
                          ],
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

endif # If not subproject
