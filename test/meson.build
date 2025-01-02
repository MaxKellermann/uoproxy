gtest = dependency('gtest', main: true,
                   include_type: 'system',
                   required: get_option('test'),
                   fallback: ['gtest', 'gtest_main_dep'])
if not gtest.found()
  subdir_done()
endif

test(
  'TestUoProxy',
  executable(
    'TestUoProxy',
    'TestCompression.cxx',
    '../src/Compression.cxx',
    include_directories: inc,
    dependencies: [
      gtest,
    ],
  )
)
