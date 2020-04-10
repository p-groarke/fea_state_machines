#if defined(_MSC_VER)

#include <cstdio>
#define FEA_FSM_NO_EVENT_WRAPPER
#include <fea_state_machines/constexpr_fsm.hpp>
#include <gtest/gtest.h>

namespace fea {
using namespace cexpr;
}

namespace {

TEST(constexpr_fsm, example_windows) {
	struct test_data {
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


	// Create your states.
	fea::fsm_builder<transition, state> builder;

	// Walk
	auto walk_transitions
			= builder.make_transition<transition::do_run, state::run>()
					  .make_transition<transition::do_jump, state::jump>()
					  .make_transition<transition::do_walk, state::walk>();

	// fea_event(walk_onenter, [](auto&, test_data& t) { ++t.num_onenter_calls;
	// });

	auto walk_events
			= builder.make_event<fea::fsm_event::on_enter>(
							 [](auto&, test_data& t) { ++t.num_onenter_calls; })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&, test_data& t) {
								  ++t.num_onupdate_calls;
							  })
					  .make_event<fea::fsm_event::on_exit_to, state::jump>(
							  [](auto&, test_data& t) {
								  ++t.num_onexitto_calls;
							  });

	auto walk_state
			= builder.make_state<state::walk>(walk_transitions, walk_events);


	// Jump
	auto jump_transitions
			= builder.make_transition<transition::do_run, state::run>()
					  .make_transition<transition::do_walk, state::walk>();

	auto jump_events
			= builder.make_event<fea::fsm_event::on_enter>(
							 [](auto&, test_data& t) {
								 // format
								 ++t.num_onenter_calls;
							 })
					  .make_event<fea::fsm_event::on_exit_to, state::run>(
							  [](auto&, test_data& t) {
								  ++t.num_onexitto_calls;
							  });

	auto jump_state
			= builder.make_state<state::jump>(jump_transitions, jump_events);


	// Run
	auto run_transitions
			= builder.make_transition<transition::do_jump, state::jump>()
					  .make_transition<transition::do_walk, state::walk>();

	auto run_events
			= builder.make_event<fea::fsm_event::on_enter_from, state::jump>(
							 [](auto&, test_data& t) {
								 ++t.num_onenterfrom_calls;
							 })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&, test_data& t) {
								  ++t.num_onupdate_calls;
							  })
					  .make_event<fea::fsm_event::on_exit>(
							  [](auto&, test_data& t) {
								  ++t.num_onexit_calls;
							  });

	auto run_state
			= builder.make_state<state::run>(run_transitions, run_events);


	// Create your state machine.
	constexpr auto to_init
			= builder.make_machine(walk_state, jump_state, run_state);

	auto machine = to_init.init(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

	machine.update(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 1u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 0u);

	// Capture the trigger output.
	auto m1 = machine.template trigger<transition::do_jump>(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	m1.update(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 0u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 1u);

	auto m2 = m1.template trigger<transition::do_run>(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 1u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 1u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 2u);

	m2.update(mtest_data);
	EXPECT_EQ(mtest_data.num_onenterfrom_calls, 1u);
	EXPECT_EQ(mtest_data.num_onenter_calls, 2u);
	EXPECT_EQ(mtest_data.num_onupdate_calls, 2u);
	EXPECT_EQ(mtest_data.num_onexit_calls, 0u);
	EXPECT_EQ(mtest_data.num_onexitto_calls, 2u);
}


TEST(constexpr_fsm, compiler_letter_windows) {
#if defined(NDEBUG)
	static constexpr bool debug_build = false;
#else
	static constexpr bool debug_build = true;
#endif
	static constexpr bool assert_val = true;

	enum class transition {
		do_intro,
		do_debug,
		do_release,
		do_paragraph,
		do_outro,
		count,
	};
	enum class state {
		intro,
		debug,
		release,
		paragraph,
		outro,
		count,
	};

	fea::fsm_builder<transition, state> builder;


	// intro
	auto intro_transitions
			= builder.make_transition<transition::do_debug, state::debug>()
					  .make_transition<transition::do_release,
							  state::release>();

	auto intro_events
			= builder.make_event<fea::fsm_event::on_enter>([](auto& machine) {
						 static_assert(assert_val, "Dear");

						 if constexpr (debug_build) {
							 return machine
									 .template trigger<transition::do_debug>();
						 } else {
							 return machine.template trigger<
									 transition::do_release>();
						 }
					 })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&) { return 0; })
					  .make_event<fea::fsm_event::on_exit_to, state::debug>(
							  [](auto&) {
								  static_assert(assert_val,
										  "slow Visual Studio Compiler,");
							  })
					  .make_event<fea::fsm_event::on_exit_to,
							  state::release>([](auto&) {
						  static_assert(assert_val,
								  "relatively fast Visual Studio Compiler ;)");
					  });

	auto intro_state
			= builder.make_state<state::intro>(intro_transitions, intro_events);


	// debug
	auto debug_transitions = builder.make_transition<transition::do_paragraph,
			state::paragraph>();

	auto debug_events
			= builder.make_event<fea::fsm_event::on_enter>([](auto& m) {
						 static_assert(assert_val, "In debug mode,");
						 return m.template trigger<transition::do_paragraph>();
					 })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&) { return 1; });

	auto debug_state
			= builder.make_state<state::debug>(debug_transitions, debug_events);


	// release
	auto release_transitions = builder.make_transition<transition::do_paragraph,
			state::paragraph>();

	auto release_events
			= builder.make_event<fea::fsm_event::on_enter>([](auto& m) {
						 static_assert(assert_val, "In release mode,");
						 return m.template trigger<transition::do_paragraph>();
					 })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&) { return 2; });

	auto release_state = builder.make_state<state::release>(
			release_transitions, release_events);


	// paragraph
	auto par_transitions
			= builder.make_transition<transition::do_outro, state::outro>();

	auto par_events
			= builder.make_event<fea::fsm_event::on_enter>([](auto& m) {
						 static_assert(assert_val,
								 "We've been very critical of you in the "
								 "past.");
						 return m.template trigger<transition::do_outro>();
					 })
					  .make_event<fea::fsm_event::on_update>(
							  [](auto&) { return 3; })
					  .make_event<fea::fsm_event::on_exit>([](auto&) {
						  static_assert(assert_val, "And we still are.");
					  });

	auto par_state
			= builder.make_state<state::paragraph>(par_transitions, par_events);


	// outro
	auto outro_events = builder.make_event<fea::fsm_event::on_enter>([](auto&) {
								   static_assert(assert_val,
										   "But look how you've grown!");
							   })
								.make_event<fea::fsm_event::on_update>(
										[](auto&) { return 42; });

	auto outro_state
			= builder.make_state<state::outro>(builder.empty_t(), outro_events);

	// Tada!
	auto machine = builder.make_machine(intro_state, debug_state, release_state,
								  par_state, outro_state)
						   .init();

	// And we can get constexpr values from update, generated conditionally at
	// compile time.
	constexpr auto result = machine.update();
	static_assert(result == 42, "Wrong answer to life.");
}

template <class Machine>
struct my_test1 {

	static constexpr auto my_val = Machine::update();
};

template <class Machine>
struct my_test2 {

	static constexpr auto my_val = Machine::update();
};

TEST(constexpr_fsm, generate_tuple_windows) {
	enum class transition { flip_flop, count };
	enum class state { gen_string, gen_int, count };

	fea::fsm_builder<transition, state> builder;

	auto string_transitions
			= builder.make_transition<transition::flip_flop, state::gen_int>();

	auto string_events = builder.make_event<fea::fsm_event::on_enter>(
			[](auto& m, auto val) {
				using tup_t = std::decay_t<decltype(val())>;
				if constexpr (std::tuple_size_v<tup_t> >= 10) {
					return val();
				} else {

					return m.template trigger<transition::flip_flop>([=]() {
						return std::tuple_cat(
								val(), std::make_tuple("a string"));
					});
				}
			});

	auto string_state = builder.make_state<state::gen_string>(
			string_transitions, string_events);


	auto int_transitions = builder.make_transition<transition::flip_flop,
			state::gen_string>();

	auto int_events = builder.make_event<fea::fsm_event::on_enter>(
			[](auto& m, auto val) {
				return m.template trigger<transition::flip_flop>([=]() {
					return std::tuple_cat(val(), std::make_tuple(42));
				});
			});

	auto int_state
			= builder.make_state<state::gen_int>(int_transitions, int_events);

	constexpr auto tup
			= builder.make_machine(string_state, int_state).init([]() {
				  return std::make_tuple("prime");
			  });

	std::apply([](auto&... vals) { ((std::cout << vals << std::endl), ...); },
			tup);

	constexpr auto test_tup = std::make_tuple("prime", "a string", 42,
			"a string", 42, "a string", 42, "a string", 42, "a string", 42);

	using machine_tup_t = std::decay_t<decltype(tup)>;
	using expected_tup_t = std::decay_t<decltype(test_tup)>;

	static_assert(std::tuple_size_v<
						  machine_tup_t> == std::tuple_size_v<expected_tup_t>,
			"unit test failed : tuples are unequal");
}

} // namespace
#endif
