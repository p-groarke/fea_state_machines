#include <fea_state_machines/inlined_fsm.hpp>
#include <gtest/gtest.h>

namespace fea {
using namespace inl;
}

namespace {

TEST(simple_inlined_fsm, example) {
	struct test_data {
		bool walk_enter = false;
		bool walk_update = false;

		size_t num_onenterfrom_calls = 0;
		size_t num_onenter_calls = 0;
		size_t num_onupdate_calls = 0;
		size_t num_onexit_calls = 0;
		size_t num_onexitto_calls = 0;
	};
	test_data mtest_data;

	// Create your enums. They MUST end with 'count'.
	enum class state { walk, run, jump, count };
	enum class transition { do_walk, do_run, do_jump, count };

	// Used for callbacks

	// Create your state machine.
	// fea::fsm<transition, state, test_data&> machine;
	// using machine_t = decltype(machine);

	// Create your states.
	// Walk
	{
		constexpr fea::fsm_builder<transition, state> builder;

		auto walk_transitions
				= builder.make_transition<transition::do_run, state::run>()
						  .make_transition<transition::do_jump, state::jump>()
						  .make_transition<transition::do_walk, state::walk>();

		auto walk_events
				= builder.make_event<fea::fsm_event::on_enter>(
								 [](auto&, test_data& t) {
									 ++t.num_onenter_calls;
								 })
						  .make_event<fea::fsm_event::on_exit_to, state::jump>(
								  [](auto&, test_data& t) {
									  ++t.num_onexitto_calls;
								  });

		// constexpr auto walk_state = builder.make_state(
		//	{ walk_to_run }, { on_enter });

		// Must use std::make_tuple if you only have 1 element, or else the copy
		// ctor is called!

		auto walk_state = builder.make_state(walk_transitions, walk_events);
		// auto walk_state = builder.make_state(walk_transitions.unpack(),
		// events);
		// auto walk_state = builder.make_state(
		//		std::make_tuple(walk_to_run), std::make_tuple(on_enter));

		state s = walk_state.transition_target<transition::do_walk>();

		printf("%zu\n", size_t(s));

		//// Utiliser beaucoup dans le monde des states machines.
		// auto walk_state = fea::state_builder<transition, state>{}
		//						  .add_event<fea::fsm_event::on_enter>(
		//								  [](auto&, test_data& t) {
		//									  ++t.num_onenter_calls;
		//								  })
		//						  .add_event<fea::fsm_event::on_update>(
		//								  [](auto&, test_data& t) {
		//									  ++t.num_onupdate_calls;
		//								  });

		//// Ou

		//// Plus c++ standard p-e?
		// auto on_enter = fea::make_event<fea::fsm_event::on_enter>(
		//		[](auto&, test_data& t) { ++t.num_onenter_calls; });

		// auto on_update = fea::make_event<fea::fsm_event::on_update>(
		//		[](auto&, test_data& t) { ++t.num_onupdate_calls; });

		// auto walk_state
		//		= fea::make_state<transition, state>(on_enter, on_update);
	}

	// auto on_enter = fea::make_event<fea::fsm_event::on_enter>(
	//		[](auto&, test_data& t) { ++t.num_onenter_calls; });

	//// auto on_enter = [](auto&, test_data& t) { ++t.num_onenter_calls; };
	// auto on_update = [](auto&, test_data& t) { ++t.num_onupdate_calls; };

	// auto walk_state
	//		= fea::make_state<transition, state>(on_enter, on_update);


	//// Add allowed transitions.
	// walk_state.add_transition<transition::do_run, state::run>();

	//// Add state events.
	// walk_state.add_event<fea::fsm_event::on_enter>(
	//		[](machine_t& /*machine*/, test_data& t) {
	//			t.walk_enter = true;
	//			++t.num_onenter_calls;
	//		});
	// walk_state.add_event<fea::fsm_event::on_update>(
	//		[](machine_t& /*machine*/, test_data& t) {
	//			t.walk_update = true;
	//			++t.num_onupdate_calls;
	//		});

	// machine.add_state<state::walk>(std::move(walk_state));
}

//// Run
//{
//	fea::fsm_state<transition, state, test_data&> run_state;
//	run_state.add_transition<transition::do_walk, state::walk>();
//	run_state.add_transition<transition::do_jump, state::jump>();
//	run_state.add_event<fea::fsm_event::on_enter_from, state::walk>(
//			[](machine_t& machine, test_data& t) {
//				++t.num_onenterfrom_calls;
//				// This is OK.
//				machine.trigger<transition::do_walk>(t);
//			});
//	run_state.add_event<fea::fsm_event::on_update>(
//			[](machine_t&, test_data& t) { ++t.num_onupdate_calls; });
//	run_state.add_event<fea::fsm_event::on_exit>(
//			[](machine_t& machine, test_data& t) {
//				++t.num_onexit_calls;

//				// This is also OK, though probably not recommended from a
//				// "design" standpoint.
//				machine.trigger<transition::do_jump>(t);
//			});
//	machine.add_state<state::run>(std::move(run_state));
//}

//// Jump
//{
//	fea::fsm_state<transition, state, test_data&> jump_state;
//	jump_state.add_transition<transition::do_walk, state::walk>();
//	jump_state.add_transition<transition::do_run, state::run>();

//	jump_state.add_event<fea::fsm_event::on_enter_from, state::run>(
//			[](machine_t&, test_data& t) { ++t.num_onenterfrom_calls; });

//	jump_state.add_event<fea::fsm_event::on_exit_to, state::walk>(
//			[](machine_t&, test_data& t) { ++t.num_onexitto_calls; });

//	machine.add_state<state::jump>(std::move(jump_state));
//}


//// Init and update default state (walk).
// machine.update(mtest_data);
// EXPECT_TRUE(mtest_data.walk_enter);
// EXPECT_TRUE(mtest_data.walk_update);
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

//// Currently doesn't handle walk to jump transition.
// EXPECT_THROW(machine.trigger<transition::do_jump>(mtest_data),
//		std::invalid_argument);

//// Go to jump.
// machine.state<state::walk>()
//		.add_transition<transition::do_jump, state::jump>();
// EXPECT_NO_THROW(machine.trigger<transition::do_jump>(mtest_data));

//// Nothing should have changed.
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

//// Go back to walk.
// machine.trigger<transition::do_walk>(mtest_data);

//// Should get on exit to walk + on enter walk.
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

//// Update walk.
// machine.update(mtest_data);
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

//// Test retrigger in on_enter and in on_exit.
// machine.trigger<transition::do_run>(mtest_data);
//// run on_enter_from -> run on_exit -> jump on_enter_from
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

//// Does nothing, no jump update.
// machine.update(mtest_data);
// machine.update(mtest_data);
// machine.update(mtest_data);
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

//// And back to walk.
// machine.trigger<transition::do_walk>(mtest_data);
// EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
// EXPECT_EQ(mtest_data.num_onenter_calls, 3u);
// EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
// EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
// EXPECT_EQ(mtest_data.num_onexitto_calls, 2u);
//}
} // namespace
