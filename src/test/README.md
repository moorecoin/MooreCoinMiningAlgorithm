# notes
the sources in this directory are unit test cases.  boost includes a
unit testing framework, and since bitcoin already uses boost, it makes
sense to simply use this framework rather than require developers to
configure some other framework (we want as few impediments to creating
unit tests as possible).

the build system is setup to compile an executable called "test_bitcoin"
that runs all of the unit tests.  the main source file is called
test_bitcoin.cpp, which simply includes other files that contain the
actual unit tests (outside of a couple required preprocessor
directives).  the pattern is to create one test file for each class or
source file for which you want to create unit tests.  the file naming
convention is "<source_filename>_tests.cpp" and such files should wrap
their tests in a test suite called "<source_filename>_tests".  for an
examples of this pattern, examine uint160_tests.cpp and
uint256_tests.cpp.

for further reading, i found the following website to be helpful in
explaining how the boost unit test framework works:
[http://www.alittlemadness.com/2009/03/31/c-unit-testing-with-boosttest/](http://www.alittlemadness.com/2009/03/31/c-unit-testing-with-boosttest/).