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

# Parallel determinism / thread-safety stress tester for the quicker core

quickerSnes9xParallelTester = executable('quickerSnes9xParallelTester',
  'source/parallelTester.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Cross-thread state-transfer tester: serialized states must be portable between
# threads (Save on A -> Load on B -> random inputs -> reload -> correct result).

quickerSnes9xTransferTester = executable('quickerSnes9xTransferTester',
  'source/transferTester.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Save-state size analysis tool (live WRAM extent, differential footprint)

quickerSnes9xStateMeasure = executable('quickerSnes9xStateMeasure',
  'source/stateMeasure.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Perturbation-confirmer: proves a WRAM region is gameplay-inert (e.g. sound-engine
# bytes) so it can be safely excluded from the dedup hash / desync validation.

quickerSnes9xPerturbConfirm = executable('quickerSnes9xPerturbConfirm',
  'source/perturbConfirm.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Auto-scan/subdivide driver: emits the minimal gameplay-relevant WRAM mask
# (recursively classifies regions via the perturbation oracle + union-confirms).

quickerSnes9xInertScan = executable('quickerSnes9xInertScan',
  'source/inertScan.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Lazy-timer A/B validator: null-input intro playthrough, rolling full-state hash.
# Build twice (lazy on, and with -DLAZY_SMP_TIMERS=0) and diff per ROM.

quickerSnes9xTimerABCheck = executable('quickerSnes9xTimerABCheck',
  'source/timerABCheck.cpp',
  cpp_args            : [ commonCompileArgs, '-DPREVENT_RENDERING' ],
  dependencies        : [ quickerSnes9xDependency, jaffarCommonDependency ],
)

# Building tests
subdir('tests')

endif # If not subproject
