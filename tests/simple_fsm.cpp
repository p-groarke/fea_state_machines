﻿#include <fea_state_machines/fea_simple_fsm.hpp>
#include <gtest/gtest.h>


namespace {

TEST(simple_fsm, example) {
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
	using machine_t = fea::fsm<transition, state, test_data&>;

	// Create your state machine.
	fea::fsm<transition, state, test_data&> machine;

	// Create your states.
	// Walk
	{
		fea::fsm_state<transition, state, test_data&> walk_state;

		// Add allowed transitions.
		walk_state.add_transition<transition::do_run, state::run>();

		// Add state events.
		walk_state.add_event<fea::fsm_event::on_enter>(
				[](machine_t& /*machine*/, test_data& t) {
					t.walk_enter = true;
					++t.num_onenter_calls;
				});
		walk_state.add_event<fea::fsm_event::on_update>(
				[](machine_t& /*machine*/, test_data& t) {
					t.walk_update = true;
					++t.num_onupdate_calls;
				});

		machine.add_state<state::walk>(std::move(walk_state));
	}

	// Run
	{
		fea::fsm_state<transition, state, test_data&> run_state;
		run_state.add_transition<transition::do_walk, state::walk>();
		run_state.add_transition<transition::do_jump, state::jump>();
		run_state.add_event<fea::fsm_event::on_enter_from, state::walk>(
				[](machine_t& machine, test_data& t) {
					++t.num_onenterfrom_calls;
					// This is OK.
					machine.trigger<transition::do_walk>(t);
				});
		run_state.add_event<fea::fsm_event::on_update>(
				[](machine_t&, test_data& t) { ++t.num_onupdate_calls; });
		run_state.add_event<fea::fsm_event::on_exit>(
				[](machine_t& machine, test_data& t) {
					++t.num_onexit_calls;

					// This is also OK, though probably not recommended from a
					// "design" standpoint.
					machine.trigger<transition::do_jump>(t);
				});
		machine.add_state<state::run>(std::move(run_state));
	}

	// Jump
	{
		fea::fsm_state<transition, state, test_data&> jump_state;
		jump_state.add_transition<transition::do_walk, state::walk>();
		jump_state.add_transition<transition::do_run, state::run>();

		jump_state.add_event<fea::fsm_event::on_enter_from, state::run>(
				[](machine_t&, test_data& t) { ++t.num_onenterfrom_calls; });

		jump_state.add_event<fea::fsm_event::on_exit_to, state::walk>(
				[](machine_t&, test_data& t) { ++t.num_onexitto_calls; });

		machine.add_state<state::jump>(std::move(jump_state));
	}


	// Init and update default state (walk).
	machine.update(mtest_data);
	EXPECT_TRUE(mtest_data.walk_enter);
	EXPECT_TRUE(mtest_data.walk_update);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

	// Currently doesn't handle walk to jump transition.
	EXPECT_THROW(machine.trigger<transition::do_jump>(mtest_data),
			std::invalid_argument);

	// Go to jump.
	machine.state<state::walk>()
			.add_transition<transition::do_jump, state::jump>();
	EXPECT_NO_THROW(machine.trigger<transition::do_jump>(mtest_data));

	// Nothing should have changed.
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

	// Go back to walk.
	machine.trigger<transition::do_walk>(mtest_data);

	// Should get on exit to walk + on enter walk.
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	// Update walk.
	machine.update(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	// Test retrigger in on_enter and in on_exit.
	machine.trigger<transition::do_run>(mtest_data);
	// run on_enter_from -> run on_exit -> jump on_enter_from
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	// Does nothing, no jump update.
	machine.update(mtest_data);
	machine.update(mtest_data);
	machine.update(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	// And back to walk.
	machine.trigger<transition::do_walk>(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 2u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 3u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 2u);
}
} // namespace
