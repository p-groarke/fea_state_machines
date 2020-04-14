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

struct test_obj {

	void test() {
		printf("test\n");
	}

	// void on_enter(int t, auto&) {
	//	printf("on enter from run : %d\n", t);
	//}
};

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


	// Create your state and transition enums. They MUST end with 'count'.
	enum class state { walk, run, jump, count };
	enum class transition { do_walk, do_run, do_jump, count };


	// Create a builder.
	// A builder is a zero cost helper so you don't have to type
	// your transition enum, state enum and function signature everywhere.
	fea::fsm_builder<transition, state, double(int)> builder;


	// Create some transitions.
	// Provide transition value and target state.
	auto t1 = builder.make_transition<transition::do_run, state::run>();
	auto t2 = builder.make_transition<transition::do_jump, state::jump>();


	// Create some events.
	// Provide the fea::fsm_event enum value. If it is on_enter_from or
	// on_exit_to, also provide a from/to state.

	// The event function signature must match the builder parameter,
	// plus, *one must add an 'auto&' argument at the end*.
	// The last argument is always a reference to the machine itself.
	// For ex : 'void(int)' becomes 'void(int, auto&)'

	// on_update events support return values, if you specified one in the
	// builder parameters. All other events ignore their return value.

	auto e1 = builder.make_event<fea::fsm_event::on_enter_from, state::run>(
			[](int t, auto&) { printf("on enter from run : %d\n", t); });

	auto e3 = builder.make_event<fea::fsm_event::on_enter>(
			[](int t, auto&) { printf("on enter : %d\n", t); });

	auto e2 = builder.make_event<fea::fsm_event::on_update>([](int t, auto&) {
		printf("on update : %d\n", t);
		return -42.0;
	});

	// Create your state.
	// Provide the state's enum value as a template parameter.
	// Pass in its transitions and events. The order doesn't matter.
	auto s = builder.make_state<state::walk>(t1, t2, e1, e2, e3);


	// Finally, create your state machine.
	// Pass in your states. The first state is the starting state.
	auto machine = builder.make_machine(s);


	// Run machine
	double test_ret = machine.update(42);
	printf("returned : %f\n", test_ret);
}
