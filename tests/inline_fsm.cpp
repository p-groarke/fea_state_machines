#include <cstdio>
#include <fea_state_machines/constexpr_fsm.hpp>
#include <gtest/gtest.h>

template <class Func, class Tuple, size_t N = 0>
inline auto runtime_get(Func func, Tuple& tup, size_t idx) {
	if (N == idx) {
		return func(std::get<N>(tup));
	}

	if constexpr (N + 1 < std::tuple_size_v<Tuple>) {
		return runtime_get<Func, Tuple, N + 1>(func, tup, idx);
	}
}

TEST(inline_fsm, basics) {

	// std::tuple t{ 42, 3 };
	// size_t runtime = 0;

	// runtime_get([](auto& myval) { printf("\n%d\n\n", myval); }, t, 0);
}
