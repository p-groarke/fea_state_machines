//#include "transitions_and_states.gen.hpp"

#include <fea_benchmark/fea_benchmark.hpp>
#include <fea_state_machines/fsm.hpp>
#include <fea_state_machines/hfsm.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>


//#if defined(NDEBUG)

namespace {
// constexpr size_t num_trans_and_states = 10;
//
// void gen_header() {
//	std::ofstream ofs{ "transitions_and_states.gen.hpp" };
//
//	ofs << "#pragma once" << std::endl;
//	ofs << R"(
//#include <fea_state_machines/fsm.hpp>
//#include <memory>
//)";
//
//	ofs << "enum class transition : unsigned {" << std::endl;
//	for (size_t i = 0; i < num_trans_and_states; ++i) {
//		ofs << "\tt" << std::to_string(i) << "," << std::endl;
//	}
//	ofs << "count" << std::endl;
//	ofs << "};" << std::endl << std::endl;
//
//	ofs << "enum class state : unsigned {" << std::endl;
//	for (size_t i = 0; i < num_trans_and_states; ++i) {
//		ofs << "\ts" << std::to_string(i) << "," << std::endl;
//	}
//	ofs << "count" << std::endl;
//	ofs << "};" << std::endl << std::endl;
//
//	ofs << R"(
// template <class Machine>
// void add_states_and_transitions(Machine* m) {
//)";
//
//	for (size_t s = 0; s < num_trans_and_states; ++s) {
//		ofs << R"(
//	{
//		auto mstate = std::make_unique<
//				fea::fsm_state<transition, state, size_t&>>();
//
//		mstate->add_event<fea::fsm_event::on_enter>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//		mstate->add_event<fea::fsm_event::on_update>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//		mstate->add_event<fea::fsm_event::on_exit>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//)";
//
//		for (size_t i = 0; i < num_trans_and_states; ++i) {
//			ofs << "\t\tmstate->add_transition<transition::t"
//				<< std::to_string(i) << ", state::s" << std::to_string(i)
//				<< ">();" << std::endl;
//		}
//		ofs << "\t\tm->add_state<state::s" << std::to_string(s)
//			<< ">(std::move(*mstate));" << std::endl;
//
//		ofs << "\t}" << std::endl;
//	}
//	ofs << "}" << std::endl << std::endl;
//
//	ofs << R"(
// template <class Machine>
// void do_benchmark(Machine * m, size_t & event_counter) {
//)";
//
//	for (size_t i = 0; i < num_trans_and_states; ++i) {
//		ofs << "\tm->update(event_counter);" << std::endl;
//		ofs << "\tm->trigger<transition::t" << std::to_string(i)
//			<< ">(event_counter);" << std::endl;
//	}
//
//	ofs << "}" << std::endl;
//}

// template <class Machine>
// void add_states_and_transitions(Machine* m) {
//	{
//		auto mstate = std::make_unique<
//				fea::fsm_state<transition, state, size_t&>>();
//
//		mstate->add_event<fea::fsm_event::on_enter>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//		mstate->add_event<fea::fsm_event::on_update>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//		mstate->add_event<fea::fsm_event::on_exit>(
//				[](auto&, size_t& event_counter) { ++event_counter; });
//
//		mstate->add_transition<transition::t0, state::s0>();
//		m->add_state<state::s0>(std::move(*mstate));
//	}
//}

// template <class Machine>
// void do_benchmark(Machine* m, size_t& event_counter) {
//	for (size_t i = 0; i < size_t(transition::count); ++i) {
//		m->update(event_counter);
//		m->trigger<transition::t0>(event_counter);
//	}
//}

// TEST(simple_fsm, benchmarks) {
//	gen_header();
//
//	// bench::title("100 states, 100 transitions each");
//	// bench::start();
//	// std::unique_ptr<fea::fsm<transition, state, size_t&>> machine
//	//		= std::make_unique<fea::fsm<transition, state, size_t&>>();
//	// add_states_and_transitions(machine.get());
//	// bench::stop("build machine");
//
//	// size_t event_counter = 0;
//
//	// bench::start();
//	// do_benchmark(machine.get(), event_counter);
//	// bench::stop("update and transition all states once");
//
//	// printf("\nNum total events called : %zu\n", event_counter);
//}
} // namespace

//#endif // NDEBUG
