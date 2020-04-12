# fea_state_machines
[![Build status](https://ci.appveyor.com/api/projects/status/vf630kekui0oanfn/branch/master?svg=true)](https://ci.appveyor.com/project/p-groarke/fea-state-machines/branch/master)
[![Build Status](https://travis-ci.org/p-groarke/fea_state_machines.svg?branch=master)](https://travis-ci.org/p-groarke/fea_state_machines)

A buffet of state machines.

## Available Machines

Each header contains a more detailed explanation of the state machine's features, design and limitations. Here is a quick overview.

### fsm.hpp

This is a stack based state machine. It is very fast and supports the most essential fsm features. Under the hood, it uses `std::array` and `std::function`. This is a great fsm if you need many machines running concurrently, or you need a very fast runtime machine. The design is very simple, making it robust and easy to debug.

### hfsm.hpp

This is the "big boi". A full implementation of a hierarchical finite state machine (aka [state chart](https://statecharts.github.io/)). It is a heap fsm, evaluation uses a queue to process your events. It has all the bells and whistles : transition guards, auto transition guards, history state, parallel states, parent/children states, etc. Use this for very complex behavior you need to manage.

### constexpr_fsm.hpp

This is a compile-time executable state machine. It is mostly a novel design to experiment with the possibilities opened up when creating a truly `constexpr` state machine. The features are quite limited, though you have the added benefit of compile-time validation. For experimentation only, a thorough deep-dive is available in this [blog post](https://philippegroarke.com/posts/2020/constexpr_fsm/).

### inlined_fsm

TODO: This will be a mirror state machine to fsm.hpp. It should have a reasonable amount of features, with less overhead using `std::tuple` and storing your functions directly without the need of `std::function` or C pointers. If this gains feature parity with fsm.hpp, it will replace it.


## Build
`fea_state_machines` is a header only state machine lib with no dependencies other than the stl.

The unit tests depend on gtest. They are not built by default.

Install recent conan, cmake and compiler.

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=On && cmake --build . --config debug
bin/fea_state_machines_tests.exe

// Optionally
cmake --build . --target install
```

