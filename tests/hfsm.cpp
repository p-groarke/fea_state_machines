#include <cstdio>
#include <fea_state_machines/fea_hfsm.hpp>
#include <gtest/gtest.h>

namespace {
TEST(hfsm, basics) {

	enum class transition : size_t {
		do_walk,
		do_walk_crouch,
		do_walk_normal,
		do_run,
		do_run_sub,
		do_jump,
		count,
	};
	std::array<const char*, size_t(transition::count)> transition_names{
		"do_walk",
		"do_walk_crouch",
		"do_walk_normal",
		"do_run",
		"do_run_sub",
		"do_jump",
	};

	enum class state : size_t {
		walk,
		walk_crouch,
		walk_normal,
		run,
		run_sub,
		run_sub_sub,
		run_sub_sub_sub,
		run_sub_sub_sub_sub,
		jump,
		count,
	};

	struct test_me {
		size_t enter_num = 0;
		size_t update_num = 0;
		size_t exit_num = 0;
		size_t enter_from_num = 0;
		size_t exit_to_num = 0;
	};
	test_me state_test;


	bool state_machine_ready = false;
	bool do_something = false;
	fea::hfsm<transition, state> smachine{};


	fea::hfsm_state<transition, state> walk_normal_state{ state::walk_normal,
		"walk_normal" };
	walk_normal_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	walk_normal_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (do_something) {
			smachine.trigger<transition::do_jump>();
			return;
		}
		if (state_machine_ready) {
			smachine.trigger<transition::do_walk_crouch>();
			return;
		}
	});
	walk_normal_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });
	walk_normal_state.add_event<fea::hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { state_test.enter_from_num++; });
	walk_normal_state
			.add_event<fea::hfsm_event::on_enter_from, state::walk_crouch>(
					[&](auto&) { state_test.enter_from_num++; }, true);
	walk_normal_state
			.add_event<fea::hfsm_event::on_exit_to, state::walk_crouch>(
					[&](auto&) { state_test.exit_to_num++; });

	walk_normal_state
			.add_transition<transition::do_walk_crouch, state::walk_crouch>();

	walk_normal_state.enable_parent_update();

	walk_normal_state.init();

	walk_normal_state.execute_event<fea::hfsm_event::on_enter>(
			state::run, smachine);
	walk_normal_state.execute_event<fea::hfsm_event::on_enter>(
			state::jump, smachine);
	walk_normal_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk, smachine);
	walk_normal_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_normal, smachine);
	walk_normal_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_crouch, smachine);

	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 0);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);


	fea::hfsm_state<transition, state> walk_crouch_state{ state::walk_crouch,
		"walk_crouch" };
	walk_crouch_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	walk_crouch_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});
	walk_crouch_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });
	walk_crouch_state.add_event<fea::hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { state_test.enter_from_num++; });
	walk_crouch_state
			.add_event<fea::hfsm_event::on_enter_from, state::walk_normal>(
					[&](auto&) { state_test.enter_from_num++; }, true);
	walk_crouch_state
			.add_event<fea::hfsm_event::on_exit_to, state::walk_normal>(
					[&](auto&) { state_test.exit_to_num++; }, true);
	walk_crouch_state.add_event<fea::hfsm_event::on_exit_to, state::walk>(
			[&, v = false](auto&) mutable {
				state_test.exit_to_num++;
				if (state_machine_ready && !v) {
					v = true;
					smachine.trigger<transition::do_run>();
				}
			},
			true);

	walk_crouch_state
			.add_transition<transition::do_walk_normal, state::walk_normal>();
	walk_crouch_state.add_transition<transition::do_walk, state::walk>();
	walk_crouch_state.add_transition<transition::do_run, state::run>();
	walk_crouch_state.enable_parent_update();


	fea::hfsm_state<transition, state> walk_state{ state::walk, "walk" };
	walk_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });

	walk_state.add_event<fea::hfsm_event::on_enter_from, state::run>(
			[&](auto&) { state_test.enter_from_num++; });
	walk_state.add_event<fea::hfsm_event::on_enter_from, state::walk_crouch>(
			[&](auto&) { state_test.enter_from_num++; });

	walk_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (do_something) {
			do_something = false;
			smachine.trigger<transition::do_jump>();
		}
	});

	walk_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	walk_state.add_event<fea::hfsm_event::on_exit_to, state::jump>(
			[&](auto&) { state_test.exit_to_num++; });

	// walk_state.add_transition<transition::do_walk, state::walk>();
	walk_state.add_transition<transition::do_run, state::run>();
	walk_state.add_transition<transition::do_jump, state::jump>();

	walk_state.add_substate<state::walk_normal>(std::move(walk_normal_state));
	walk_state.add_substate<state::walk_crouch>(std::move(walk_crouch_state));

	walk_state.init();

	state_test = {};
	walk_state.execute_event<fea::hfsm_event::on_enter>(state::run, smachine);
	walk_state.execute_event<fea::hfsm_event::on_enter>(state::jump, smachine);
	walk_state.execute_event<fea::hfsm_event::on_enter>(state::walk, smachine);
	walk_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_normal, smachine);
	walk_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 0);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	walk_state.execute_event<fea::hfsm_event::on_update>(
			state::count, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 1);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	walk_state.execute_event<fea::hfsm_event::on_exit>(state::run, smachine);
	walk_state.execute_event<fea::hfsm_event::on_exit>(state::jump, smachine);
	walk_state.execute_event<fea::hfsm_event::on_exit>(state::walk, smachine);
	walk_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_normal, smachine);
	walk_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 1);
	EXPECT_EQ(state_test.exit_num, 4);
	EXPECT_EQ(state_test.exit_to_num, 1);

	fea::hfsm_state<transition, state>::tranny_info tg;
	walk_state.transition<transition::do_walk>(tg);
	EXPECT_EQ(tg.to, state::count);
	tg = {};
	walk_state.transition<transition::do_run>(tg);
	EXPECT_EQ(tg.to, state::run);
	tg = {};
	walk_state.transition<transition::do_jump>(tg);
	EXPECT_EQ(tg.to, state::jump);

	smachine.add_state<state::walk>(std::move(walk_state));

	fea::hfsm_state<transition, state> run_subsubsubsub_state{
		state::run_sub_sub_sub_sub, "run_subsubsubsubstate"
	};
	run_subsubsubsub_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	run_subsubsubsub_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});
	run_subsubsubsub_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	fea::hfsm_state<transition, state> run_subsubsub_state{
		state::run_sub_sub_sub, "run_subsubsubstate"
	};
	run_subsubsub_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	run_subsubsub_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});
	run_subsubsub_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	run_subsubsub_state.add_substate<state::run_sub_sub_sub_sub>(
			std::move(run_subsubsubsub_state));

	fea::hfsm_state<transition, state> run_subsub_state{ state::run_sub_sub,
		"run_subsubstate" };
	run_subsub_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	run_subsub_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});
	run_subsub_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	run_subsub_state.add_substate<state::run_sub_sub_sub>(
			std::move(run_subsubsub_state));

	fea::hfsm_state<transition, state> run_sub_state{ state::run_sub,
		"run_substate" };
	run_sub_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	run_sub_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});
	run_sub_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	run_sub_state.add_substate<state::run_sub_sub>(std::move(run_subsub_state));

	fea::hfsm_state<transition, state> run_state{ state::run, "run" };
	run_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });

	run_state.add_event<fea::hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { state_test.enter_from_num++; });

	run_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_walk>();
	});

	run_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });

	run_state.add_event<fea::hfsm_event::on_exit_to, state::jump>(
			[&](auto&) { state_test.exit_to_num++; });

	run_state.add_transition<transition::do_walk, state::walk>();
	run_state.add_transition<transition::do_jump, state::jump>();

	run_state.add_substate<state::run_sub>(std::move(run_sub_state));

	run_state.init();

	state_test = {};
	run_state.execute_event<fea::hfsm_event::on_enter>(state::run, smachine);
	run_state.execute_event<fea::hfsm_event::on_enter>(state::jump, smachine);
	run_state.execute_event<fea::hfsm_event::on_enter>(state::walk, smachine);
	run_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_normal, smachine);
	run_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 4);
	EXPECT_EQ(state_test.enter_from_num, 1);
	EXPECT_EQ(state_test.update_num, 0);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	run_state.execute_event<fea::hfsm_event::on_update>(state::run, smachine);
	run_state.execute_event<fea::hfsm_event::on_update>(state::jump, smachine);
	run_state.execute_event<fea::hfsm_event::on_update>(state::walk, smachine);
	run_state.execute_event<fea::hfsm_event::on_update>(
			state::walk_normal, smachine);
	run_state.execute_event<fea::hfsm_event::on_update>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 4);
	EXPECT_EQ(state_test.enter_from_num, 1);
	EXPECT_EQ(state_test.update_num, 5); // parent update not enabled
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	run_state.execute_event<fea::hfsm_event::on_exit>(state::run, smachine);
	run_state.execute_event<fea::hfsm_event::on_exit>(state::jump, smachine);
	run_state.execute_event<fea::hfsm_event::on_exit>(state::walk, smachine);
	run_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_normal, smachine);
	run_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 4);
	EXPECT_EQ(state_test.enter_from_num, 1);
	EXPECT_EQ(state_test.update_num, 5);
	EXPECT_EQ(state_test.exit_num, 4);
	EXPECT_EQ(state_test.exit_to_num, 1);

	tg = {};
	run_state.transition<transition::do_walk>(tg);
	EXPECT_EQ(tg.to, state::walk);
	tg = {};
	run_state.transition<transition::do_run>(tg);
	EXPECT_EQ(tg.to, state::count);
	tg = {};
	run_state.transition<transition::do_jump>(tg);
	EXPECT_EQ(tg.to, state::jump);

	smachine.add_state<state::run>(std::move(run_state));


	fea::hfsm_state<transition, state> jump_state{ state::jump, "jump" };
	jump_state.add_event<fea::hfsm_event::on_enter>(
			[&](auto&) { state_test.enter_num++; });
	jump_state.add_event<fea::hfsm_event::on_update>([&](auto&) {
		state_test.update_num++;
		if (state_machine_ready)
			smachine.trigger<transition::do_run>();
	});
	jump_state.add_event<fea::hfsm_event::on_exit>(
			[&](auto&) { state_test.exit_num++; });
	jump_state.add_event<fea::hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { state_test.enter_from_num++; });
	jump_state.add_event<fea::hfsm_event::on_enter_from, state::run>(
			[&](auto&) { state_test.enter_from_num++; });

	jump_state.add_transition<transition::do_walk, state::walk>();
	jump_state.add_transition<transition::do_run, state::run>();
	jump_state.add_guard_transition<transition::do_walk, state::walk_crouch>(
			[&]() { return false; });
	jump_state.add_guard_transition<transition::do_walk, state::walk_crouch>(
			[&]() { return true; });
	jump_state.add_guard_transition<transition::do_walk, state::walk_normal>(
			[&]() { return true; });

	state_test = {};
	jump_state.execute_event<fea::hfsm_event::on_enter>(state::run, smachine);
	jump_state.execute_event<fea::hfsm_event::on_enter>(state::jump, smachine);
	jump_state.execute_event<fea::hfsm_event::on_enter>(state::walk, smachine);
	jump_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_normal, smachine);
	jump_state.execute_event<fea::hfsm_event::on_enter>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 0);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	jump_state.execute_event<fea::hfsm_event::on_update>(state::run, smachine);
	jump_state.execute_event<fea::hfsm_event::on_update>(state::jump, smachine);
	jump_state.execute_event<fea::hfsm_event::on_update>(state::walk, smachine);
	jump_state.execute_event<fea::hfsm_event::on_update>(
			state::walk_normal, smachine);
	jump_state.execute_event<fea::hfsm_event::on_update>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 5);
	EXPECT_EQ(state_test.exit_num, 0);
	EXPECT_EQ(state_test.exit_to_num, 0);

	jump_state.execute_event<fea::hfsm_event::on_exit>(state::run, smachine);
	jump_state.execute_event<fea::hfsm_event::on_exit>(state::jump, smachine);
	jump_state.execute_event<fea::hfsm_event::on_exit>(state::walk, smachine);
	jump_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_normal, smachine);
	jump_state.execute_event<fea::hfsm_event::on_exit>(
			state::walk_crouch, smachine);
	EXPECT_EQ(state_test.enter_num, 3);
	EXPECT_EQ(state_test.enter_from_num, 2);
	EXPECT_EQ(state_test.update_num, 5);
	EXPECT_EQ(state_test.exit_num, 5);
	EXPECT_EQ(state_test.exit_to_num, 0);

	tg = {};
	jump_state.transition<transition::do_walk>(tg);
	EXPECT_EQ(tg.to, state::walk_crouch);
	tg = {};
	jump_state.transition<transition::do_run>(tg);
	EXPECT_EQ(tg.to, state::run);
	tg = {};
	jump_state.transition<transition::do_jump>(tg);
	EXPECT_EQ(tg.to, state::count);

	smachine.add_state<state::jump>(std::move(jump_state));

	state_machine_ready = true; // for tests
	smachine.add_transition_names(transition_names);
	smachine.enable_print();
	smachine.update();
	smachine.update();
	do_something = true;
	smachine.update();
	smachine.update();
	smachine.update();
	smachine.update();
	smachine.update();
	smachine.update();
	smachine.update();
}

TEST(hfsm, parallel) {
	using namespace fea;
	enum class transition : size_t {
		do_walk,
		do_run,
		do_head_idle,
		do_head_look,
		count,
	};

	enum class state : size_t {
		movement,
		walk,
		run,
		head,
		head_idle,
		head_look,
		count,
	};

	size_t enters = 0;
	size_t updates = 0;

	hfsm_state<transition, state> walk_state{ state::walk, "walk" };
	walk_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	walk_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	walk_state.add_transition<transition::do_run, state::run>();

	hfsm_state<transition, state> run_state{ state::run, "run" };
	run_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	run_state.add_transition<transition::do_walk, state::walk>();

	hfsm_state<transition, state> movement_state{ state::movement, "movement" };
	movement_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	movement_state.add_substate<state::walk>(std::move(walk_state));
	movement_state.add_substate<state::run>(std::move(run_state));

	hfsm_state<transition, state> head_idle_state{ state::head_idle,
		"head_idle" };
	head_idle_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	head_idle_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	head_idle_state
			.add_transition<transition::do_head_look, state::head_look>();
	head_idle_state.add_auto_transition_guard<transition::do_head_look>(
			[]() { return true; });

	hfsm_state<transition, state> head_look_state{ state::head_look,
		"head_look" };
	head_look_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	head_look_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	head_idle_state
			.add_transition<transition::do_head_idle, state::head_idle>();

	hfsm_state<transition, state> head_state{ state::head, "head" };
	head_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	head_state.add_substate<state::head_idle>(std::move(head_idle_state));
	head_state.add_substate<state::head_look>(std::move(head_look_state));

	hfsm<transition, state> smachine;
	smachine.add_state<state::movement>(std::move(movement_state));

	hfsm<transition, state> smachine2;
	smachine2.add_state<state::head>(std::move(head_state));

	smachine.add_parallel_hfsm(std::move(smachine2));

	smachine.enable_print();
	smachine.update();
	smachine.update();
	smachine.update();

	EXPECT_EQ(enters, 3);
	EXPECT_EQ(updates, 5);
}

TEST(hfsm, auto_transition_guards) {
	using namespace fea;
	enum class transition : size_t {
		do_walk,
		do_run,
		count,
	};

	enum class state : size_t {
		walk,
		run,
		count,
	};

	bool auto_guard = false;
	size_t enters = 0;
	size_t updates = 0;
	size_t exits = 0;

	hfsm_state<transition, state> walk_state{ state::walk, "walk" };
	walk_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	walk_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	walk_state.add_event<hfsm_event::on_exit>([&](auto&) { ++exits; });

	walk_state.add_transition<transition::do_run, state::run>();
	walk_state.add_auto_transition_guard<transition::do_run>(
			[&]() { return auto_guard; });

	hfsm_state<transition, state> run_state{ state::run, "run" };
	run_state.add_event<hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { ++enters; });
	run_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	run_state.add_event<hfsm_event::on_exit>([&](auto&) { ++exits; });

	run_state.add_transition<transition::do_walk, state::walk>();
	run_state.add_auto_transition_guard<transition::do_walk>(
			[&]() { return auto_guard; });

	hfsm<transition, state> smachine{};
	smachine.add_state<state::walk>(std::move(walk_state));
	smachine.add_state<state::run>(std::move(run_state));

	// smachine.enable_print();
	smachine.update();

	EXPECT_EQ(enters, 1);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 0);

	auto_guard = true;
	smachine.update();

	EXPECT_EQ(enters, 2);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 1);

	auto_guard = false;
	smachine.update();

	EXPECT_EQ(enters, 2);
	EXPECT_EQ(updates, 2);
	EXPECT_EQ(exits, 1);

	auto_guard = true;
	smachine.update();

	EXPECT_EQ(enters, 3);
	EXPECT_EQ(updates, 2);
	EXPECT_EQ(exits, 2);
}

TEST(hfsm, history_transition_guards) {
	using namespace fea;
	enum class transition : size_t {
		do_walk,
		do_run,
		do_jump,
		yield,
		count,
	};

	enum class state : size_t {
		walk,
		run,
		jump,
		count,
	};

	size_t enters = 0;
	size_t enters_from_run = 0;
	size_t enters_from_walk = 0;
	size_t updates = 0;
	size_t exits = 0;
	size_t exits_to_run = 0;
	size_t exits_to_walk = 0;

	hfsm<transition, state> smachine{};

	hfsm_state<transition, state> walk_state{ state::walk, "walk" };
	walk_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	walk_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	walk_state.add_event<hfsm_event::on_exit>([&](auto&) { ++exits; });

	walk_state.add_transition<transition::do_run, state::run>();
	walk_state.add_transition<transition::do_jump, state::jump>();

	hfsm_state<transition, state> run_state{ state::run, "run" };
	run_state.add_event<hfsm_event::on_enter>([&](auto&) { ++enters; });
	run_state.add_event<hfsm_event::on_update>([&](auto&) { ++updates; });
	run_state.add_event<hfsm_event::on_exit>([&](auto&) { ++exits; });

	run_state.add_transition<transition::do_walk, state::walk>();
	run_state.add_transition<transition::do_jump, state::jump>();

	hfsm_state<transition, state> jump_state{ state::jump, "jump" };
	jump_state.add_event<hfsm_event::on_enter_from, state::walk>(
			[&](auto&) { ++enters_from_walk; });
	jump_state.add_event<hfsm_event::on_enter_from, state::run>(
			[&](auto&) { ++enters_from_run; });
	jump_state.add_event<hfsm_event::on_update>([&](auto&) {
		++updates;
		smachine.trigger<transition::yield>();
	});
	jump_state.add_event<hfsm_event::on_exit_to, state::walk>(
			[&](auto&) { ++exits_to_walk; });
	jump_state.add_event<hfsm_event::on_exit_to, state::run>(
			[&](auto&) { ++exits_to_run; });

	jump_state.add_yield_transition<transition::yield>();

	smachine.add_state<state::walk>(std::move(walk_state));
	smachine.add_state<state::run>(std::move(run_state));
	smachine.add_state<state::jump>(std::move(jump_state));

	smachine.enable_print();
	smachine.update();

	EXPECT_EQ(enters, 1);
	EXPECT_EQ(enters_from_walk, 0);
	EXPECT_EQ(enters_from_run, 0);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 0);
	EXPECT_EQ(exits_to_walk, 0);
	EXPECT_EQ(exits_to_run, 0);

	smachine.trigger<transition::do_jump>();
	smachine.update();

	EXPECT_EQ(enters, 1);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 0);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 1);
	EXPECT_EQ(exits_to_walk, 0);
	EXPECT_EQ(exits_to_run, 0);

	smachine.update();

	EXPECT_EQ(enters, 2);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 0);
	EXPECT_EQ(updates, 2);
	EXPECT_EQ(exits, 1);
	EXPECT_EQ(exits_to_walk, 1);
	EXPECT_EQ(exits_to_run, 0);

	smachine.trigger<transition::do_run>();
	smachine.update();

	EXPECT_EQ(enters, 3);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 0);
	EXPECT_EQ(updates, 2);
	EXPECT_EQ(exits, 2);
	EXPECT_EQ(exits_to_walk, 1);
	EXPECT_EQ(exits_to_run, 0);

	smachine.update();
	EXPECT_EQ(enters, 3);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 0);
	EXPECT_EQ(updates, 3);
	EXPECT_EQ(exits, 2);
	EXPECT_EQ(exits_to_walk, 1);
	EXPECT_EQ(exits_to_run, 0);

	smachine.trigger<transition::do_jump>();
	smachine.update();

	EXPECT_EQ(enters, 3);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 1);
	EXPECT_EQ(updates, 3);
	EXPECT_EQ(exits, 3);
	EXPECT_EQ(exits_to_walk, 1);
	EXPECT_EQ(exits_to_run, 0);

	smachine.update();
	EXPECT_EQ(enters, 4);
	EXPECT_EQ(enters_from_walk, 1);
	EXPECT_EQ(enters_from_run, 1);
	EXPECT_EQ(updates, 4);
	EXPECT_EQ(exits, 3);
	EXPECT_EQ(exits_to_walk, 1);
	EXPECT_EQ(exits_to_run, 1);
}

TEST(hfsm, func_arguments) {
	using namespace fea;
	enum class transition : size_t {
		do_walk,
		do_run,
		count,
	};

	enum class state : size_t {
		walk,
		run,
		count,
	};

	size_t enters = 0;
	size_t updates = 0;
	size_t exits = 0;

	hfsm<transition, state, int&> smachine{};
	hfsm_state<transition, state, int&> walk_state{ state::walk, "walk" };
	walk_state.add_event<hfsm_event::on_enter>([&](auto&, int& v) {
		++v;
		++enters;
	});
	walk_state.add_event<hfsm_event::on_update>([&](auto&, int& v) {
		++v;
		++updates;
	});
	walk_state.add_event<hfsm_event::on_exit>([&](auto&, int& v) {
		++v;
		++exits;
	});

	walk_state.add_transition<transition::do_run, state::run>();

	hfsm_state<transition, state, int&> run_state{ state::run, "run" };
	run_state.add_event<hfsm_event::on_enter_from, state::walk>(
			[&](auto&, int& v) {
				++v;
				++enters;
			});
	run_state.add_event<hfsm_event::on_update>([&](auto&, int& v) {
		++v;
		++updates;
	});
	run_state.add_event<hfsm_event::on_exit>([&](auto&, int& v) {
		++v;
		++exits;
	});

	run_state.add_transition<transition::do_walk, state::walk>();

	smachine.add_state<state::walk>(std::move(walk_state));
	smachine.add_state<state::run>(std::move(run_state));

	// smachine.enable_print();

	int test = 0;
	smachine.update(test);

	EXPECT_EQ(test, 2);
	EXPECT_EQ(enters, 1);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 0);

	smachine.trigger<transition::do_run>(test);
	EXPECT_EQ(test, 2);

	smachine.update(test);

	EXPECT_EQ(test, 4);
	EXPECT_EQ(enters, 2);
	EXPECT_EQ(updates, 1);
	EXPECT_EQ(exits, 1);
}
} // namespace
