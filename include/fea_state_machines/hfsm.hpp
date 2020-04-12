/*
BSD 3-Clause License

Copyright (c) 2020, Philippe Groarke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// TODO :
// - sanity check

namespace fea {
/*
A large and feature-full heap based hfsm.
	https://statecharts.github.io/

Features :
	- OnEnter, OnUpdate, OnExit.
	- OnEnterFrom, OnExitTo.
		You decide if you override event behavior when coming from/going to
		specified states.
	- Supports user arguments in the callbacks (explained below).
	- State hierarchies : The "main" feature of a state chart.
	- Transition guards : These only transition if their predicate
		evaluates to true.
	- Auto transition guards : These automatically transition when you
		call update (before it) if their predicate evaluates to true.
	- Parallel states : Different state hierarchies running in parallel.
	- Yield transitions (aka history state) : This transition will return to the
		previous state.
	- Does NOT provide a "get_current_state" function.
		Checking the current state of an fsm is a major smell and usually points
		to either a misuse, misunderstanding or incomplete implementation of the
		fsm. Do not do that, rethink your states and transitions instead.

Callbacks :
	- The first argument of your callback is always a ref to the fsm itself.
		This is useful for retriggering and when you store fsms in containers.
		You can use auto& to simplify your callback signature.
		[](auto& mymachine){}

	- Pass your own types at the end of the fsm and fsm_state template.
		These will be passed on to your callbacks when you call update or
		trigger.
		For example : fsm<mytransitions, mystates, int, bool&, const void*>;
		Callback signature is:
			[](auto& machine, int, bool&, const void*){}


Notes :
	- Uses std::function and heap.
	- Throws on unhandled transition.
		You must explicitly add re-entrant transitions or ignored transitions
		(by providing empty callbacks). IMHO this is one of the bigest source of
		bugs and broken behavior when working with FSMs. Throwing makes
		debugging much faster and easier.

*/


namespace detail {
#if !defined(NDEBUG)
inline constexpr bool debug_build = true;
#else
inline constexpr bool debug_build = false;
#endif

template <class Func, size_t... I>
constexpr void static_for(Func func, std::index_sequence<I...>) {
	(std::invoke(func, std::integral_constant<size_t, I>{}), ...);
}

template <size_t N, class Func>
constexpr void static_for(Func func) {
	static_for(func, std::make_index_sequence<N>());
}

template <class Func>
struct on_exit {
	on_exit(Func func)
			: _func(func) {
	}

	~on_exit() {
		std::invoke(_func);
	}

private:
	Func _func;
};
} // namespace detail


template <class, class, class...>
struct hfsm;

enum class hfsm_event : size_t {
	on_enter,
	on_update,
	on_exit,
	simple_events_count,
	on_enter_from,
	on_exit_to,
	total_count,
};

template <class TransitionEnum, class StateEnum, class... FuncArgs>
struct hfsm_state {
	using hfsm_t = hfsm<TransitionEnum, StateEnum, FuncArgs...>;
	using hfsm_func_t = std::function<void(hfsm_t&, FuncArgs...)>;
	using hfsm_guard_func_t = std::function<bool(FuncArgs...)>;

	struct tranny_info {
		std::vector<hfsm_state*> exit_hierarchy;
		StateEnum from{ StateEnum::count };
		StateEnum to{ StateEnum::count };
		bool yield{ false };
		bool internal_transition{ false };
	};

	hfsm_state(StateEnum s, const char* name)
			: _state(s)
			, _name(name) {
		_substate_indexes.fill(std::numeric_limits<size_t>::max());
	}

	// Called internally, no need to call this.
	void init() {
		_current_substate = _default_substate;
		if (_current_substate != StateEnum::count) {
			current_substate().init();
		}
	}

	template <StateEnum State>
	void add_substate(hfsm_state&& state) {
		assert(std::get<size_t(State)>(_substate_indexes)
						== std::numeric_limits<size_t>::max()
				&& "state : substate already exists");

		std::get<size_t(State)>(_substate_indexes) = _substates.size();
		_substates.push_back(std::move(state));

		if (_default_substate == StateEnum::count) {
			_default_substate = State;
		}
	}

	// By default, uses the first added substate as the default substate.
	template <StateEnum State>
	void add_default_substate() {
		_default_substate = State;
	}

	// call_general_event enables enter_from and exit_to to call their
	// respective generalized versions. For ex :
	// on_enter_from would call on_enter before itself.
	// on_exit_to would call on_exit before itself.
	// The order ensures the generalized version doesn't override the
	// specialized one.
	template <hfsm_event Event, StateEnum State = StateEnum::count>
	void add_event(hfsm_func_t&& func,
			[[maybe_unused]] bool call_general_event = false) {
		if constexpr (Event == hfsm_event::on_enter_from) {
			static_assert(State != StateEnum::count,
					"state : must provide enter_from state when adding "
					"on_enter_from event");

			// Want this?
			if (std::get<size_t(State)>(_enter_from_exists)) {
				throw std::invalid_argument{
					"state : on_enter_from already exists for selected state"
				};
			}

			std::get<size_t(State)>(_enter_from_events) = std::move(func);
			std::get<size_t(State)>(_enter_from_exists) = true;
			std::get<size_t(State)>(_enter_from_calls_on_enter)
					= call_general_event;

		} else if constexpr (Event == hfsm_event::on_exit_to) {
			static_assert(State != StateEnum::count,
					"state : must provide exit_to state when adding on_exit_to "
					"event");

			if (std::get<size_t(State)>(_exit_to_exists)) {
				throw std::invalid_argument{
					"state : on_exit_to already exists for selected state"
				};
			}

			std::get<size_t(State)>(_exit_to_events) = std::move(func);
			std::get<size_t(State)>(_exit_to_exists) = true;
			std::get<size_t(State)>(_exit_to_calls_on_exit)
					= call_general_event;

		} else {
			assert(!call_general_event
					&& "state : call_general_event is only valid for "
					   "on_enter_from and on_exit_to events");

			static_assert(State == StateEnum::count,
					"state : no need to provide state for on_enter, on_update "
					"and on_exit events");
			if (std::get<size_t(Event)>(_simple_event_exists)) {
				throw std::invalid_argument{ "state : event already exists" };
			}

			std::get<size_t(Event)>(_simple_events) = std::move(func);
			std::get<size_t(Event)>(_simple_event_exists) = true;
		}
	}

	template <TransitionEnum Transition, StateEnum State>
	void add_transition() {
		static_assert(Transition != TransitionEnum::count,
				"state : invalid transition");

		// Want this?
		if (std::get<size_t(Transition)>(_transition_exists)) {
			throw std::invalid_argument{
				"state : transition already exists for selected state"
			};
		}

		if (std::get<size_t(Transition)>(_is_yield_transition)) {
			throw std::invalid_argument{
				"state : transition predefined as yield transition"
			};
		}

		std::get<size_t(Transition)>(_transitions) = State;
		std::get<size_t(Transition)>(_transition_exists) = true;
	}

	// Only takes transition if predicate evaluates to true.
	// Prioritized over normal transition, executed in order of addition.
	// You can still add a normal transition as a fallback mechanism.
	template <TransitionEnum Transition, StateEnum State>
	void add_guard_transition(hfsm_guard_func_t&& func) {
		static_assert(Transition != TransitionEnum::count,
				"state : invalid transition");

		std::get<size_t(Transition)>(_guard_transitions)
				.push_back({ std::move(func), std::move(State) });
		std::get<size_t(Transition)>(_guard_transition_exists) = true;
	}

	// Checked before on_update and will call the transition automatically.
	// May skip on_update. Is checked on all states in the hierarchy (parents
	// first).
	// Must provide a valid transition.
	template <TransitionEnum Transition>
	void add_auto_transition_guard(hfsm_guard_func_t&& func) {
		static_assert(Transition != TransitionEnum::count,
				"state : invalid transition");

		if (!std::get<size_t(Transition)>(_transition_exists)) {
			throw std::invalid_argument{ "state : transition doesn't exist" };
		}

		std::get<size_t(Transition)>(_auto_transition_guards)
				.push_back(std::move(func));
	}

	// A history transition returns to the previous state, whichever one it
	// is.
	template <TransitionEnum Transition>
	void add_yield_transition() {
		static_assert(Transition != TransitionEnum::count,
				"state : invalid transition");

		if (std::get<size_t(Transition)>(_is_yield_transition)) {
			throw std::invalid_argument{
				"state : transition is already set to yield"
			};
		}

		if (std::get<size_t(Transition)>(_transition_exists)) {
			throw std::invalid_argument{
				"state : transition already exists as non yield transition"
			};
		}

		std::get<size_t(Transition)>(_is_yield_transition) = true;
	}

	// Depth-first
	template <TransitionEnum Transition>
	void transition(tranny_info& tg, FuncArgs... func_args) {
		static_assert(Transition != TransitionEnum::count,
				"state : invalid transition");

		if (_current_substate != StateEnum::count) {
			current_substate().template transition<Transition>(
					tg, func_args...);
		}

		assert(!(tg.yield && tg.internal_transition)
				&& "state : can't yield and internally transition at the same "
				   "time");

		if (tg.yield) {
			tg.exit_hierarchy.push_back(this);
			return;
		}

		if (tg.internal_transition) {
			return;
		}

		// Return if a child handled transition.
		if (tg.to != StateEnum::count) {
			if (_substate_indexes[size_t(tg.to)]
					!= std::numeric_limits<size_t>::max()) {
				tg.internal_transition = true;
				tg.exit_hierarchy.push_back(this);
			}
			return;
		}

		if (std::get<size_t(Transition)>(_guard_transition_exists)) {
			for (const auto& [func, to_state] :
					_guard_transitions[size_t(Transition)]) {
				if (std::invoke(func, func_args...)) {
					tg.from = _state;
					tg.to = to_state;
					tg.exit_hierarchy.push_back(this);
					return;
				}
			}
		}

		if (std::get<size_t(Transition)>(_transition_exists)) {
			tg.from = _state;
			tg.to = std::get<size_t(Transition)>(_transitions);
			tg.exit_hierarchy.push_back(this);
			return;
		}

		if (std::get<size_t(Transition)>(_is_yield_transition)) {
			tg.from = _state;
			tg.exit_hierarchy.push_back(this);
			tg.yield = true;
			return;
		}
	}

	// State is ignored for unrelated events.
	template <hfsm_event Event>
	void execute_event([[maybe_unused]] StateEnum to_from_state,
			hfsm_t& machine, FuncArgs... func_args) {
		static_assert(Event != hfsm_event::on_enter_from,
				"state : do not execute on_enter_from, use on_enter instead");
		static_assert(Event != hfsm_event::on_exit_to,
				"state : do not execute on_exit_to, use on_exit instead");

		if constexpr (Event == hfsm_event::on_enter) {
			if (to_from_state != StateEnum::count
					&& _enter_from_exists[size_t(to_from_state)]) {
				std::invoke(_enter_from_events[size_t(to_from_state)], machine,
						func_args...);
			} else if (std::get<size_t(Event)>(_simple_event_exists)) {
				std::invoke(std::get<size_t(Event)>(_simple_events), machine,
						func_args...);
			}
		} else if constexpr (Event == hfsm_event::on_exit) {
			if (to_from_state != StateEnum::count
					&& _exit_to_exists[size_t(to_from_state)]) {
				std::invoke(_exit_to_events[size_t(to_from_state)], machine,
						func_args...);
			} else if (std::get<size_t(Event)>(_simple_event_exists)) {
				std::invoke(std::get<size_t(Event)>(_simple_events), machine,
						func_args...);
			}

		} else if constexpr (Event == hfsm_event::on_update) {
			if (std::get<size_t(Event)>(_simple_event_exists)) {
				std::invoke(std::get<size_t(Event)>(_simple_events), machine,
						func_args...);
			}
		}
	}

	const auto& auto_transition_guards() const {
		return _auto_transition_guards;
	}

	void enable_parent_update() {
		_parent_update = true;
	}
	bool parent_update_enabled() const {
		return _parent_update;
	}

	template <hfsm_event Event>
	bool handles_event(
			[[maybe_unused]] StateEnum to_from_state = StateEnum::count) const {
		if constexpr (Event == hfsm_event::on_enter_from) {
			if (to_from_state == StateEnum::count)
				return false;

			return _enter_from_exists[size_t(to_from_state)];
		} else if constexpr (Event == hfsm_event::on_exit_to) {
			if (to_from_state == StateEnum::count) {
				return false;
			}
			return _exit_to_exists[size_t(to_from_state)];
		} else {
			return std::get<size_t(Event)>(_simple_event_exists);
		}
	}

	bool enter_from_calls_on_enter(StateEnum from) const {
		if (from == StateEnum::count)
			return false;

		return _enter_from_calls_on_enter[size_t(from)]
				&& std::get<size_t(hfsm_event::on_enter)>(_simple_event_exists);
	}
	bool exit_to_calls_on_exit(StateEnum to) const {
		if (to == StateEnum::count)
			return false;

		return _exit_to_calls_on_exit[size_t(to)]
				&& std::get<size_t(hfsm_event::on_exit)>(_simple_event_exists);
	}

	StateEnum state() const {
		return _state;
	}

	std::string_view name() const {
		return { _name };
	}

	void current_states(std::vector<const hfsm_state*>& states,
			bool depth_first = false) const {
		if (depth_first) {
			if (_current_substate != StateEnum::count) {
				current_substate().current_states(states, depth_first);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			if (_current_substate != StateEnum::count) {
				current_substate().current_states(states, depth_first);
			}
		}
	}

	void current_states(
			std::vector<hfsm_state*>& states, bool depth_first = false) {
		if (depth_first) {
			if (_current_substate != StateEnum::count) {
				current_substate().current_states(states);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			if (_current_substate != StateEnum::count) {
				current_substate().current_states(states);
			}
		}
	}

	// void parent_state(StateEnum s) {
	//	assert(s != StateEnum::count);
	//}

	void all_states(std::vector<const hfsm_state*>& states,
			bool depth_first = false) const {
		if (depth_first) {
			for (const hfsm_state& s : _substates) {
				s.all_states(states, depth_first);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			for (const hfsm_state& s : _substates) {
				s.all_states(states, depth_first);
			}
		}
	}

	void all_states(
			std::vector<hfsm_state*>& states, bool depth_first = false) {
		if (depth_first) {
			for (hfsm_state& s : _substates) {
				s.all_states(states, depth_first);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			for (hfsm_state& s : _substates) {
				s.all_states(states, depth_first);
			}
		}
	}

	void default_states(std::vector<const hfsm_state*>& states,
			bool depth_first = false) const {
		if (depth_first) {
			if (_default_substate != StateEnum::count) {
				_substates[_substate_indexes[size_t(_default_substate)]]
						.default_states(states, depth_first);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			if (_default_substate != StateEnum::count) {
				_substates[_substate_indexes[size_t(_default_substate)]]
						.default_states(states, depth_first);
			}
		}
	}

	void default_states(
			std::vector<hfsm_state*>& states, bool depth_first = false) {
		if (depth_first) {
			if (_default_substate != StateEnum::count) {
				_substates[_substate_indexes[size_t(_default_substate)]]
						.default_states(states, depth_first);
			}
			states.push_back(this);
		} else {
			states.push_back(this);
			if (_default_substate != StateEnum::count) {
				_substates[_substate_indexes[size_t(_default_substate)]]
						.default_states(states, depth_first);
			}
		}
	}

	const hfsm_state* substate(StateEnum s) const {
		if (_substate_indexes[size_t(s)]
				== std::numeric_limits<size_t>::max()) {
			throw std::invalid_argument{
				"state : trying to access invalid state"
			};
		}
		return &_substates[_substate_indexes[size_t(s)]];
	}
	hfsm_state* substate(StateEnum s) {
		return const_cast<hfsm_state*>(
				static_cast<const hfsm_state*>(this)->substate(s));
	}

	void current_substate(StateEnum s) {
		if (_substate_indexes[size_t(s)]
				== std::numeric_limits<size_t>::max()) {
			throw std::invalid_argument{
				"state : trying to access invalid state"
			};
		}
		_current_substate = s;
	}

private:
	const hfsm_state& current_substate() const {
		return _substates[_substate_indexes[size_t(_current_substate)]];
	}
	hfsm_state& current_substate() {
		return _substates[_substate_indexes[size_t(_current_substate)]];
	}

	StateEnum _state;
	StateEnum _current_substate{ StateEnum::count };
	StateEnum _default_substate{ StateEnum::count };
	const char* _name;

	std::array<hfsm_func_t, size_t(hfsm_event::simple_events_count)>
			_simple_events{};
	std::array<bool, size_t(hfsm_event::simple_events_count)>
			_simple_event_exists{};

	std::array<hfsm_func_t, size_t(StateEnum::count)> _enter_from_events{};
	std::array<bool, size_t(StateEnum::count)> _enter_from_exists{};
	std::array<bool, size_t(StateEnum::count)> _enter_from_calls_on_enter{};

	std::array<hfsm_func_t, size_t(StateEnum::count)> _exit_to_events{};
	std::array<bool, size_t(StateEnum::count)> _exit_to_exists{};
	std::array<bool, size_t(StateEnum::count)> _exit_to_calls_on_exit{};

	std::array<StateEnum, size_t(TransitionEnum::count)> _transitions{};
	std::array<bool, size_t(TransitionEnum::count)> _transition_exists{};

	std::array<std::vector<std::pair<hfsm_guard_func_t, StateEnum>>,
			size_t(TransitionEnum::count)>
			_guard_transitions{};
	std::array<bool, size_t(TransitionEnum::count)> _guard_transition_exists{};

	std::array<std::vector<hfsm_guard_func_t>, size_t(TransitionEnum::count)>
			_auto_transition_guards{};

	std::array<bool, size_t(TransitionEnum::count)> _is_yield_transition{};

	std::vector<hfsm_state> _substates;
	std::array<size_t, size_t(StateEnum::count)> _substate_indexes;

	bool _parent_update{ false };

	using transition_underlying_t =
			typename std::underlying_type_t<TransitionEnum>;
	using state_underlying_t = typename std::underlying_type_t<StateEnum>;

	static_assert(std::is_enum_v<TransitionEnum>,
			"state : template parameter TransitionEnum must be enum class");
	static_assert(std::is_enum_v<StateEnum>,
			"state : template parameter StateEnum must be enum class");

	static_assert(std::is_unsigned_v<transition_underlying_t>,
			"state : TransitionEnum underlying type must be unsigned");
	static_assert(std::is_unsigned_v<state_underlying_t>,
			"state : StateEnum underlying type must be unsigned");

	static_assert(size_t(TransitionEnum::count) != 0,
			"state : TransitionEnum must declare count and count must not "
			"be "
			"0");
	static_assert(size_t(StateEnum::count) != 0,
			"state : StateEnum must declare count and count must not be 0");
};

template <class TransitionEnum, class StateEnum, class... FuncArgs>
struct hfsm {
	using state_t = hfsm_state<TransitionEnum, StateEnum, FuncArgs...>;
	using hfsm_func_t = typename state_t::hfsm_func_t;
	using auto_t_guard_arr
			= std::array<std::vector<typename state_t::hfsm_guard_func_t>,
					size_t(TransitionEnum::count)>;

	hfsm() {
		_state_indexes.fill(std::numeric_limits<size_t>::max());
		_state_names.resize(size_t(StateEnum::count), "");
		_transition_names.fill(nullptr);
		_state_topmost_parents.fill(StateEnum::count);
	}

	template <StateEnum State>
	void add_state(state_t&& state) {
		if (std::get<size_t(State)>(_state_indexes)
				!= std::numeric_limits<size_t>::max()) {
			throw std::invalid_argument{ "hfsm : state already exists" };
		}

		if (State != state.state()) {
			throw std::invalid_argument{
				"hfsm : misconfigured state, state enum mismatch"
			};
		}

		std::get<size_t(State)>(_state_indexes) = _states.size();
		_states.push_back(std::move(state));

		std::vector<const state_t*> states;
		_states.back().all_states(states);
		for (const state_t* s : states) {
			_state_names[size_t(s->state())] = s->name();
			// if (s->state() != State) {
			_state_topmost_parents[size_t(s->state())] = State;
			//}
		}

		if (_default_state == StateEnum::count) {
			_default_state = State;
		}
	}

	void add_parallel_hfsm(hfsm&& machine) {
		_parallel_machines.push_back(std::move(machine));
	}

	// By default, uses the first added state as the default substate.
	template <StateEnum State>
	void add_default_state() {
		_default_state = State;
	}

	// Optional, will use provided transition names when printing.
	void add_transition_names(
			std::array<const char*, size_t(TransitionEnum::count)> names) {
		_transition_names = std::move(names);
	}

	template <TransitionEnum Transition>
	void trigger(FuncArgs... func_args) {
		static_assert(Transition != TransitionEnum::count,
				"hfsm : invalid transition");
		auto g = detail::on_exit{ [this]() { _in_transition_guard = false; } };

		maybe_init(func_args...);

		_current_tranny_info = {};
		current_state().template transition<Transition>(
				_current_tranny_info, func_args...);

		if (_current_tranny_info.to == StateEnum::count
				&& !_current_tranny_info.yield) {
			throw std::invalid_argument{
				"hfsm : current state doesn't handle transition"
			};
		}

		if (_print) {
			if (_transition_names[size_t(Transition)] != nullptr) {
				if (_in_transition_guard) {
					printf("--- %s%s ---\n", "transition guard triggered : ",
							_transition_names[size_t(Transition)]);
				} else {
					printf("\n--- %s%s ---\n", "triggered : ",
							_transition_names[size_t(Transition)]);
				}
			} else {
				if (_in_transition_guard) {
					printf("--- %s%zu ---\n", "transition guard triggered : ",
							size_t(Transition));
				} else {
					printf("\n--- %s%zu ---\n",
							"triggered : ", size_t(Transition));
				}
			}
		}
		_transition_to_handle = Transition;

		for (auto& sub : _parallel_machines) {
			sub._in_transition_guard = _in_transition_guard;
			// auto subg = sg::make_scope_guard(
			//		[&]() { sub._in_transition_guard = false; });

			sub.template trigger<Transition>(func_args...);
		}
	}

	void update(FuncArgs... func_args) {
		maybe_init(func_args...);

		if (_print) {
			if (_in_parallel)
				printf("\n--- parallel update ---\n");
			else
				printf("\n--- update ---\n");
		}

		std::vector<state_t*> states;
		current_state().current_states(states, true);

		std::vector<state_t*> update_states;

		// gather valid states
		for (size_t i = 0; i < states.size(); ++i) {
			update_states.push_back(states[i]);
			if (!states[i]->parent_update_enabled()) {
				break;
			}
		}
		std::reverse(update_states.begin(), update_states.end());

		std::vector<hfsm_func_t> update_events;
		enqueue_update(update_events, update_states);
		execute_events(update_events, func_args...);

		//_in_parallel = true;
		// auto g = sg::make_scope_guard([this]() { _in_parallel = false; });

		for (auto& sub : _parallel_machines) {
			sub._in_parallel = true;
			auto g = detail::on_exit{ [&]() { sub._in_parallel = false; } };
			sub.update(func_args...);
		}
	}

	std::string_view state_name(StateEnum s) const {
		return _state_names[size_t(s)];
	}

	const std::vector<std::string_view>& state_names() const {
		return _state_names;
	}

	StateEnum current_state() const {
		return _current_state;
	}

	// Also enables print on parallel machines.
	void enable_print() {
		_print = true;
		for (auto& sub : _parallel_machines) {
			sub.enable_print();
		}
	}
	void disable_print() {
		_print = false;
		for (auto& sub : _parallel_machines) {
			sub.enable_print();
		}
	}

private:
	void enqueue_enter(std::vector<hfsm_func_t>& events,
			const std::vector<state_t*>& states, StateEnum to_from_state) {

		for (state_t* s : states) {
			bool call_generalized = s->enter_from_calls_on_enter(to_from_state);
			if (call_generalized) {
				events.push_back([s, this](hfsm&, FuncArgs... func_args) {
					_indentation += indentation_size;
					maybe_print(hfsm_event::on_enter, s);

					s->template execute_event<hfsm_event::on_enter>(
							StateEnum::count, *this, func_args...);
				});
			}

			events.push_back([s, to_from_state, indent = !call_generalized,
									 this](hfsm&, FuncArgs... func_args) {
				if (indent) {
					_indentation += indentation_size;
				}
				maybe_print(hfsm_event::on_enter, s, to_from_state);
				s->template execute_event<hfsm_event::on_enter>(
						to_from_state, *this, func_args...);
			});
		}
	}

	void enqueue_update(std::vector<hfsm_func_t>& events,
			const std::vector<state_t*>& states) {
		for (state_t* s : states) {
			events.push_back([s, this](hfsm&, FuncArgs... func_args) {
				execute_auto_transition_guards(
						s->auto_transition_guards(), func_args...);

				if (_transition_to_handle != TransitionEnum::count) {
					return;
				}

				maybe_print(hfsm_event::on_update, s);
				s->template execute_event<hfsm_event::on_update>(
						StateEnum::count, *this, func_args...);
			});
		}
	}

	void enqueue_exit(std::vector<hfsm_func_t>& events,
			const std::vector<state_t*>& states, StateEnum to_from_state) {

		for (state_t* s : states) {
			bool call_generalized = s->exit_to_calls_on_exit(to_from_state);
			if (call_generalized) {
				events.push_back([s, this](hfsm&, FuncArgs... func_args) {
					maybe_print(hfsm_event::on_exit, s);

					s->template execute_event<hfsm_event::on_exit>(
							StateEnum::count, *this, func_args...);
				});
			}

			events.push_back(
					[s, to_from_state, this](hfsm&, FuncArgs... func_args) {
						maybe_print(hfsm_event::on_exit, s, to_from_state);

						s->template execute_event<hfsm_event::on_exit>(
								to_from_state, *this, func_args...);

						if (_transition_to_handle == TransitionEnum::count) {
							_indentation -= indentation_size;
						}
					});
		}
	}

	void execute_auto_transition_guards(
			const auto_t_guard_arr& t_guards, FuncArgs... func_args) {
		bool found = false;

		detail::static_for<size_t(TransitionEnum::count)>([&](auto idx) {
			if (found)
				return;

			constexpr size_t c_idx = decltype(idx)::value;
			for (const auto& func : std::get<c_idx>(t_guards)) {
				if (std::invoke(func, func_args...)) {
					found = true;
					// constexpr TransitionEnum t
					//		= static_cast<TransitionEnum>(decltype(idx)::value);
					_in_transition_guard = true;
					trigger<TransitionEnum(c_idx)>(func_args...);
					break;
				}
			}
		});
	}

	void execute_events(
			std::vector<hfsm_func_t>& update_events, FuncArgs... func_args) {

		for (size_t i = 0; i < update_events.size(); ++i) {
			std::invoke(update_events[i], *this, func_args...);

			if (_transition_to_handle == TransitionEnum::count)
				continue;

			_transition_to_handle = TransitionEnum::count;
			update_events.erase(
					update_events.begin() + i + 1, update_events.end());

			if (_current_tranny_info.internal_transition) {
				state_t* parent = _current_tranny_info.exit_hierarchy.back();
				_current_tranny_info.exit_hierarchy.pop_back();

				enqueue_exit(update_events, _current_tranny_info.exit_hierarchy,
						_current_tranny_info.to);

				state_t* child = parent->substate(_current_tranny_info.to);

				update_events.push_back(
						[parent, to_state = _current_tranny_info.to](
								hfsm&, FuncArgs...) {
							parent->current_substate(to_state);
						});

				std::vector<state_t*> enter_states{ child };
				enqueue_enter(
						update_events, enter_states, _current_tranny_info.from);

				continue;
			}

			if (_current_tranny_info.yield) {
				_current_tranny_info.to = _history_state;
			}

			std::vector<state_t*> exit_states;
			current_state().current_states(exit_states, true);
			enqueue_exit(update_events, exit_states, _current_tranny_info.to);

			update_events.push_back([to_state = _current_tranny_info.to, this](
											hfsm&, FuncArgs...) {
				current_state(_state_topmost_parents[size_t(to_state)]);
			});


			std::vector<state_t*> enter_states;
			topmost_state(_current_tranny_info.to).default_states(enter_states);
			enqueue_enter(
					update_events, enter_states, _current_tranny_info.from);
		}
	}

	void maybe_init(FuncArgs... func_args) {
		assert(_states.size() != 0 && "hfsm : did you forget to add states?");

		if (_current_state != StateEnum::count)
			return;

		// Pretty heave, only run in debug
		if constexpr (detail::debug_build) {
			// just check the first init
			if (!_in_parallel) {
				std::vector<std::string_view> names = _state_names;

				if (_parallel_machines.size() != 0) {
					for (const auto& machine : _parallel_machines) {
						names.insert(names.end(), machine.state_names().begin(),
								machine.state_names().end());
					}

					names.erase(std::remove_if(names.begin(), names.end(),
										[](const std::string_view& str) {
											return str == "";
										}),
							names.end());
					assert(names.size() == size_t(StateEnum::count));
				}

				for (const auto& name : names) {
					if (name == "") {
						throw std::invalid_argument{ "hfsm : missing states" };
					}

					size_t num_name = 0;
					std::for_each(
							names.begin(), names.end(), [&](const auto& n) {
								if (n == name)
									++num_name;
							});
					assert(num_name == 1
							&& "hfsm : states have duplicate names");
				}
			}
		}

		if (_print) {
			if (_in_parallel)
				printf("\n--- parallel init ---\n");
			else
				printf("\n--- init ---\n");
		}

		current_state(_default_state);
		current_state().init();
		std::vector<state_t*> enter_states;
		current_state().current_states(enter_states);

		std::vector<hfsm_func_t> init_events;
		enqueue_enter(init_events, enter_states, StateEnum::count);
		execute_events(init_events, func_args...);
	}

	void maybe_print(hfsm_event ev, const state_t* from,
			StateEnum to = StateEnum::count) {
		if (!_print)
			return;

		assert(_indentation >= 0);

		const char* ev_name = nullptr;

		if (ev == hfsm_event::on_update) {
			if (!from->template handles_event<hfsm_event::on_update>())
				return;

			ev_name = "on_update";
			printf("%*s%s : %s\n", _indentation, "", from->name().data(),
					ev_name);
		} else if (ev == hfsm_event::on_enter
				|| ev == hfsm_event::on_enter_from) {
			if (from->template handles_event<hfsm_event::on_enter_from>(to)) {
				ev_name = "on_enter_from";
				printf("%*s%s : %s : %s\n", _indentation, "",
						from->name().data(), ev_name, state_name(to).data());
			} else if (from->template handles_event<hfsm_event::on_enter>()) {
				ev_name = "on_enter";
				printf("%*s%s : %s\n", _indentation, "", from->name().data(),
						ev_name);
			}
		} else if (ev == hfsm_event::on_exit || ev == hfsm_event::on_exit_to) {
			if (from->template handles_event<hfsm_event::on_exit_to>(to)) {
				ev_name = "on_exit_to";
				printf("%*s%s : %s : %s\n", _indentation, "",
						from->name().data(), ev_name, state_name(to).data());
			} else if (from->template handles_event<hfsm_event::on_exit>()) {
				ev_name = "on_exit";
				printf("%*s%s : %s\n", _indentation, "", from->name().data(),
						ev_name);
			}
		}
	}

	state_t& current_state() {
		size_t idx = _state_indexes[size_t(_current_state)];
		assert(idx != std::numeric_limits<size_t>::max());

		return _states[idx];
	}
	void current_state(StateEnum state) {
		_history_state = _current_state;
		_current_state = state;
		current_state().init();
	}

	state_t& topmost_state(StateEnum state) {
		StateEnum topmost = _state_topmost_parents[size_t(state)];
		assert(topmost != StateEnum::count);

		size_t idx = _state_indexes[size_t(topmost)];
		assert(idx != std::numeric_limits<size_t>::max());

		return _states[idx];
	}

	StateEnum _current_state{ StateEnum::count };
	StateEnum _history_state{ StateEnum::count };
	StateEnum _default_state{ StateEnum::count };
	TransitionEnum _transition_to_handle{ TransitionEnum::count };
	typename state_t::tranny_info _current_tranny_info;

	std::vector<state_t> _states{};
	std::vector<std::string_view> _state_names{};
	std::array<size_t, size_t(StateEnum::count)> _state_indexes;
	std::array<StateEnum, size_t(StateEnum::count)> _state_topmost_parents;
	std::array<const char*, size_t(TransitionEnum::count)> _transition_names;

	bool _print{ false };

	int indentation_size = 4;
	int _indentation{ -indentation_size };

	// required for msvc fuckup
	bool _in_parallel{ false };
	bool _in_transition_guard{ false };

	std::vector<hfsm> _parallel_machines;
};

} // namespace fea
