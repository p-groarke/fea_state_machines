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
#include <cstdint>
#include <functional>

namespace fea {
/*
A small, fast and simple stack based fsm.
	Not as small as inlined simple fsm, but easier to work with and debug.

Features :
	- OnEnter, OnUpdate, OnExit.
	- OnEnterFrom, OnExitTo.
		Overrides event behavior when coming from/going to specified states or
		transitions.
	- Supports user arguments in the callbacks (explained below).
	//- DelayedTrigger. // Removed till bug fix
	//	Trigger will happen next time you call fsm::update.
	- Define FEA_FSM_NOTHROW to assert instead of throw.
	- Does NOT provide a "get_current_state" function.
		Checking the current state of an fsm is a major smell and usually points
		to either a misuse, misunderstanding or incomplete implementation of the
		fsm. Do not do that, rethink your states and transitions instead.

Callbacks :
	- The last argument of your callback is always a ref to the fsm itself.
		This is useful for retriggering and when you store fsms in containers.
		You can use auto& to simplify your callback signature.
		[](your_args..., auto& mymachine){}

	- Pass your event signature at the end of the fsm and fsm_state template.
		These will be passed on to your callbacks when you call update or
		trigger.
		For example :
			fsm<myts, mystates, void(int, bool&, const void*)>;
		Callback signature is:
			[](int, bool&, const void*, auto& machine){}
	- The state machine is passed at the end to allow you to call member
		functions directly.
		For example :
			fsm<my_tses, my_states, bool(my_obj*, int)>
		Call your triggers passing the object pointer as the first argument, and
		the member functions will be called.
			machine.trigger<some::transition>(&obj, 42);


Notes :
	- Uses std::function. If you can't have that, use inlined fsms instead.
	- Throws on unhandled transition.
		You must explicitly add re-entrant transitions or ignored transitions
		(by providing empty callbacks). IMHO this is one of the bigest source of
		bugs and broken behavior when working with FSMs. Throwing makes
		debugging much faster and easier.

TODO :
	- Yield/History state
	- Anonymous transition (1 runtime transition per state).
	- Auto transition guards?
	- Guard transitions maybe?
*/

enum class fsm_event : uint8_t {
	on_enter_from,
	on_enter,
	on_update,
	on_exit,
	on_exit_to,
	count,
};

template <class, class, class>
struct fsm;

template <class, class, class>
struct fsm_state;

template <class TransitionEnum, class StateEnum, class FuncRet,
		class... FuncArgs>
struct fsm_state<TransitionEnum, StateEnum, FuncRet(FuncArgs...)> {
	using fsm_t = fsm<TransitionEnum, StateEnum, FuncRet(FuncArgs...)>;
	using fsm_func_t = std::function<FuncRet(FuncArgs..., fsm_t&)>;

	fsm_state() {
		std::fill(_transitions.begin(), _transitions.end(), StateEnum::count);
	}

	// Add your event implementation.
	template <fsm_event Event>
	void add_event(fsm_func_t&& func) {
		static_assert(Event == fsm_event::on_enter
						|| Event == fsm_event::on_exit
						|| Event == fsm_event::on_update,
				"add_event : wrong template resolution called");

		if constexpr (Event == fsm_event::on_enter) {
			_on_enter_func = std::move(func);
		} else if constexpr (Event == fsm_event::on_update) {
			_on_update_func = std::move(func);
		} else if constexpr (Event == fsm_event::on_exit) {
			_on_exit_func = std::move(func);
		}
	}

	template <fsm_event Event, StateEnum State>
	void add_event(fsm_func_t&& func) {
		static_assert(Event == fsm_event::on_enter_from
						|| Event == fsm_event::on_exit_to,
				"add_event : must use on_enter_from or on_exit_to when "
				"custumizing on transition");

		if constexpr (Event == fsm_event::on_enter_from) {
			std::get<size_t(State)>(_on_enter_from_state_funcs)
					= std::move(func);
		} else if constexpr (Event == fsm_event::on_exit_to) {
			std::get<size_t(State)>(_on_exit_to_state_funcs) = std::move(func);
		}
	}

	template <fsm_event Event, TransitionEnum Transition>
	void add_event(fsm_func_t&& func) {
		static_assert(Event == fsm_event::on_enter_from
						|| Event == fsm_event::on_exit_to,
				"add_event : must use on_enter_from or on_exit_to when "
				"custumizing on transition");

		if constexpr (Event == fsm_event::on_enter_from) {
			std::get<size_t(Transition)>(_on_enter_from_transition_funcs)
					= std::move(func);
		} else if constexpr (Event == fsm_event::on_exit_to) {
			std::get<size_t(Transition)>(_on_exit_to_transition_funcs)
					= std::move(func);
		}
	}

	// Handle transition to a specified state.
	template <TransitionEnum Transition, StateEnum State>
	void add_transition() {
		static_assert(Transition != TransitionEnum::count,
				"fsm_state : bad transition");
		static_assert(State != StateEnum::count, "fsm_state : bad state");
		std::get<size_t(Transition)>(_transitions) = State;
	}

	// Used internally to get which state is associated to the provided
	// transition.
	template <TransitionEnum Transition>
	StateEnum transition_target() const {
#if defined(FEA_FSM_NOTHROW)
		assert(std::get<size_t(Transition)>(_transitions) == StateEnum::count);
#else
		if (std::get<size_t(Transition)>(_transitions) == StateEnum::count) {
			throw std::invalid_argument{ "fsm_state : unhandled transition" };
		}
#endif

		return std::get<size_t(Transition)>(_transitions);
	}

	// Used internally, executes a specific event.
	template <fsm_event Event>
	auto execute_event([[maybe_unused]] StateEnum to_from_state,
			[[maybe_unused]] TransitionEnum to_from_transition, fsm_t& machine,
			FuncArgs... func_args) {
		static_assert(Event != fsm_event::on_enter_from,
				"state : do not execute on_enter_from, use on_enter instead "
				"and provide to_from_state");
		static_assert(Event != fsm_event::on_exit_to,
				"state : do not execute on_exit_to, use on_exit instead and "
				"provide to_from_state");

		static_assert(Event != fsm_event::count, "fsm_state : invalid event");


		// Check the event, call the appropriate user functions if it is stored.
		if constexpr (Event == fsm_event::on_enter) {
			if (to_from_state != StateEnum::count
					&& _on_enter_from_state_funcs[size_t(to_from_state)]) {
				// has enter_from state
				std::invoke(_on_enter_from_state_funcs[size_t(to_from_state)],
						func_args..., machine);

			} else if (to_from_transition != TransitionEnum::count
					&& _on_enter_from_transition_funcs[size_t(
							to_from_transition)]) {
				// has enter_from transition
				std::invoke(_on_enter_from_transition_funcs[size_t(
									to_from_transition)],
						func_args..., machine);

			} else if (_on_enter_func) {
				std::invoke(_on_enter_func, func_args..., machine);
			}

		} else if constexpr (Event == fsm_event::on_update) {
			if (_on_update_func) {
				return std::invoke(_on_update_func, func_args..., machine);
			}
		} else if constexpr (Event == fsm_event::on_exit) {
			if (to_from_state != StateEnum::count
					&& _on_exit_to_state_funcs[size_t(to_from_state)]) {
				// has exit_to
				std::invoke(_on_exit_to_state_funcs[size_t(to_from_state)],
						func_args..., machine);

			} else if (to_from_transition != TransitionEnum::count
					&& _on_exit_to_transition_funcs[size_t(
							to_from_transition)]) {
				// has exit_to
				std::invoke(_on_exit_to_transition_funcs[size_t(
									to_from_transition)],
						func_args..., machine);

			} else if (_on_exit_func) {
				std::invoke(_on_exit_func, func_args..., machine);
			}
		}
	}

private:
	std::array<StateEnum, size_t(TransitionEnum::count)> _transitions;
	fsm_func_t _on_enter_func;
	fsm_func_t _on_update_func;
	fsm_func_t _on_exit_func;

	std::array<fsm_func_t, size_t(StateEnum::count)> _on_enter_from_state_funcs;
	std::array<fsm_func_t, size_t(StateEnum::count)> _on_exit_to_state_funcs;

	std::array<fsm_func_t, size_t(TransitionEnum::count)>
			_on_enter_from_transition_funcs;
	std::array<fsm_func_t, size_t(TransitionEnum::count)>
			_on_exit_to_transition_funcs;

	// TBD, makes it heavy but helps debuggability
	// const char* _name;
};

template <class TransitionEnum, class StateEnum, class FuncRet,
		class... FuncArgs>
struct fsm<TransitionEnum, StateEnum, FuncRet(FuncArgs...)> {
	// using fsm_t = fsm<TransitionEnum, StateEnum, FuncArgs...>;
	using state_t = fsm_state<TransitionEnum, StateEnum, FuncRet(FuncArgs...)>;
	using fsm_func_t = typename state_t::fsm_func_t;

	// Here, we use move semantics not for performance (it doesn't do anything).
	// It is to make it clear to the user he cannot modify the state anymore.
	// The fsm gobbles the state.
	template <StateEnum State>
	void add_state(state_t&& state) {
		static_assert(State != StateEnum::count, "fsm : bad state");

		std::get<size_t(State)>(_states) = std::move(state);

		if (_default_state == StateEnum::count) {
			_default_state = State;
		}
	}

	// Set starting state.
	// By default, the first added state is used.
	template <StateEnum State>
	void set_start_state() {
		static_assert(State != StateEnum::count, "fsm : bad state");
		_default_state = State;
	}

	template <StateEnum State>
	void set_finish_state() {
		static_assert(State != StateEnum::count, "fsm : bad state");
		_finish_state = State;
	}

	bool finished() const {
		if (_finish_state != StateEnum::count) {
			return _finish_state == _current_state;
		}
		return false;
	}

	// TODO : Fix retrigger on_exit.
	//// First come first served.
	//// Trigger will be called next update(...).
	//// Calling this prevents subsequent triggers to be executed.
	//// Allows more relaxed trigger argument requirements.
	// template <TransitionEnum Transition>
	// void delayed_trigger() {
	//	if (_has_delayed_trigger)
	//		return;

	//	_has_delayed_trigger = true;
	//	_delayed_trigger_func = [](fsm& f, FuncArgs... func_args) {
	//		f._has_delayed_trigger = false;
	//		f.trigger<Transition>(func_args...);
	//	};
	//}

	// Trigger a transition.
	// Throws on bad transition (or asserts, if you defined FEA_FSM_NOTHROW).
	// If you had previously called delayed_trigger, this
	// won't do anything.
	template <TransitionEnum Transition>
	void trigger(FuncArgs... func_args) {
		if (_has_delayed_trigger)
			return;

		maybe_init(func_args...);

		StateEnum from_state = _current_state;
		StateEnum to_state = _states[size_t(_current_state)]
									 .template transition_target<Transition>();

		// Only execute on_exit if we aren't in a trigger from on_exit.
		if (!_in_on_exit) {
			_in_on_exit = true;

			// Can recursively call trigger. We must handle that.
			_states[size_t(from_state)]
					.template execute_event<fsm_event::on_exit>(
							to_state, Transition, *this, func_args...);

			if (_in_on_exit == false) {
				// Exit has triggered transition. Abort.
				return;
			}
		}
		_in_on_exit = false;

		_current_state = to_state;

		// Always execute on_enter.
		_states[size_t(to_state)].template execute_event<fsm_event::on_enter>(
				from_state, Transition, *this, func_args...);
	}

	// Update the fsm.
	// Calls on_update on the current state.
	// Processes delay_trigger if that was called.
	FuncRet update(FuncArgs... func_args) {
		while (_has_delayed_trigger) {
			std::invoke(_delayed_trigger_func, func_args..., *this);
		}

		maybe_init(func_args...);

		return _states[size_t(_current_state)]
				.template execute_event<fsm_event::on_update>(StateEnum::count,
						TransitionEnum::count, *this, func_args...);
	}

	// Get the specified state.
	template <StateEnum State>
	const state_t& state() const {
		return std::get<size_t(State)>(_states);
	}
	template <StateEnum State>
	state_t& state() {
		return std::get<size_t(State)>(_states);
	}

private:
	void maybe_init(FuncArgs... func_args) {
		if (_current_state != StateEnum::count)
			return;

		_current_state = _default_state;
		_states[size_t(_current_state)]
				.template execute_event<fsm_event::on_enter>(StateEnum::count,
						TransitionEnum::count, *this, func_args...);
	}

	std::array<state_t, size_t(StateEnum::count)> _states;
	StateEnum _current_state = StateEnum::count;
	StateEnum _default_state = StateEnum::count;
	StateEnum _finish_state = StateEnum::count;

	bool _in_on_exit = false;

	fsm_func_t _delayed_trigger_func = {};
	bool _has_delayed_trigger = false;
};

template <class, class, class>
struct fsm_builder;

template <class TransitionEnum, class StateEnum, class FuncRet,
		class... FuncArgs>
struct fsm_builder<TransitionEnum, StateEnum, FuncRet(FuncArgs...)> {
	static constexpr auto make_state() {
		return fsm_state<TransitionEnum, StateEnum, FuncRet(FuncArgs...)>{};
	}

	static constexpr fsm<TransitionEnum, StateEnum, FuncRet(FuncArgs...)>
	make_machine() {
		return fsm<TransitionEnum, StateEnum, FuncRet(FuncArgs...)>{};
	}
};
} // namespace fea
