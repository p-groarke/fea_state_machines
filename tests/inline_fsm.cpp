#include <array>
#include <cstdio>
#include <fea_state_machines/inline_fsm.hpp>
#include <fea_state_machines/tmp.hpp>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <type_traits>


namespace fea {
using namespace inl;
}

TEST(inline_fsm, example) {

	//// Ou

	//// Plus c++ standard p-e?

	// fea::fsm_builder<transition, state> builder;
	// auto t1 = builder.make_transition<transtion::do_run, state::run>();

	// auto on_enter = builder.make_event<fea::fsm_event::on_enter_from,
	// state::run>([](auto&, test_data& t) { ++t.num_onenter_calls; });

	// auto on_update = builder.make_event<fea::fsm_event::on_update>(
	//		[](auto&, test_data& t) { ++t.num_onupdate_calls; });

	// auto walk_state
	//		= fea::make_state(on_enter, t1, on_update);

	// Create your enums. They MUST end with 'count'.
	enum class state { walk, run, jump, count };
	enum class transition { do_walk, do_run, do_jump, count };

	fea::fsm_builder<transition, state> builder;

	auto t1 = builder.make_transition<transition::do_run, state::run>();
	auto t2 = builder.make_transition<transition::do_jump, state::jump>();

	auto e1 = builder.make_event<fea::fsm_event::on_enter_from, state::run>(
			[](auto&) { printf("on enter from run\n"); });

	auto s = builder.make_state(t1, t2, e1);

	// size_t run_t = 1;

	state to_state = s.template transition_target<transition::do_run>();
	std::cout << "to state : " << size_t(to_state) << std::endl;

	// fea::detail::to_constexpr_of_doom(
	//		[&](auto idx) {
	//			constexpr size_t val = decltype(idx)::value;
	//			// static_assert(val < size_t(transition::count), "AAAAAAA");

	//			if constexpr (val < size_t(transition::count)) {
	//				constexpr transition t = transition(val);
	//				using key_t = fea::fsm_transition_key<transition, t>;

	//				if constexpr (s.template contains<key_t>()) {
	//					state to_state = s.template find<key_t>();
	//					std::cout << "to state : " << size_t(to_state)
	//							  << std::endl;
	//				}
	//			}
	//		},
	//		run_t);

	// std::apply(
	//		[](auto&... vals) {
	//			((std::cout << typeid(vals).name() << std::endl), ...);
	//		},
	//		s);
}
