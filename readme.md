# fea_state_machines
<!-- [![Build Status](https://travis-ci.org/p-groarke/fea_flat_recurse.svg?branch=master)](https://travis-ci.org/p-groarke/fea_flat_recurse)
[![Build status](https://ci.appveyor.com/api/projects/status/kw99c4io4f50n6en/branch/master?svg=true)](https://ci.appveyor.com/project/p-groarke/fea-flat-recurse/branch/master) -->

A buffet of state machines.

## Examples

```c++

```

## Build
`fea_state_machines` is a header only state machine lib with no dependencies other than the stl.

The unit tests depend on gtest. They are not built by default.

Install recent conan, cmake and compiler.

### Windows
```
mkdir build && cd build
cmake .. -DBUILD_TESTING=On && cmake --build . --config debug
bin\fea_state_machines_tests.exe

// Optionally
cmake --build . --target install
```

### Unixes
```
mkdir build && cd build
cmake .. -DBUILD_TESTING=On && cmake --build . --config debug
bin\fea_state_machines_tests.exe

// Optionally
cmake --build . --target install
```
