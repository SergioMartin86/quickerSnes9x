nomalloc = environment({'MALLOC_PERTURB_': '0'})

bash = find_program('bash')
testCommands = ['run_test.sh', quickerSnes9xTester.path(), snes9xTester.path() ]
testTimeout = 500

# Tests for copyrighted game roms (only for local testing or own CI runners)
protectedTestSet = [ 
  'princeOfPersia.anyPercent.test',
]

# Tests for open source free roms (for public cloud testing)
openSourceTestSet = [
  'theLastSuper.playaround.test',
  'christmasCraze.playaround.test',
]

# Creating test set based on whether copyrighted roms are to be used
testSet = openSourceTestSet
if get_option('onlyOpenSource') == false
 testSet += protectedTestSet
endif

# Adding tests to the suite
foreach testFile : testSet
  testSuite = testFile.split('.')[0]
  testName = testFile.split('.')[1]
  test(testName,
       bash,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ testCommands, testFile, '--cycleType', 'Full'],
       suite : [ testSuite ])
endforeach

# Thread-safety completeness gate: fails if the core defines any shared writable
# global that is neither thread-local nor on the reviewed const-after-init allowlist.
test('threadSafetyGate',
     bash,
     workdir : meson.current_source_dir(),
     timeout: testTimeout,
     args : [ 'check_thread_safety.sh', meson.project_build_root() ],
     suite : [ 'threadSafety' ])

# Parallel determinism stress: many concurrent workers replaying the same
# sequence must all reach the single-threaded reference hash (no shared-state leak).
# A modest thread count keeps cloud CI runners happy; bump --threads locally.
foreach testFile : testSet
  testSuite = testFile.split('.')[0]
  test(testSuite + '.parallelDeterminism',
       quickerSnes9xParallelTester,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ testFile, '--threads', '16', '--cycleType', 'Full' ],
       suite : [ 'threadSafety' ])
endforeach

# Cross-thread transfer: full states saved on one thread, loaded/advanced/reloaded
# on others, must reproduce the single-threaded reference (no per-thread affinity).
foreach testFile : testSet
  testSuite = testFile.split('.')[0]
  test(testSuite + '.transfer',
       quickerSnes9xTransferTester,
       workdir : meson.current_source_dir(),
       timeout: testTimeout,
       args : [ testFile, '--threads', '16', '--jobs', '1024' ],
       suite : [ 'threadSafety' ])
endforeach

