#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <tuple>
#if !defined(FEA_FSM_NOTHROW)
#include <stdexcept>
#endif

namespace fea {
namespace inl {

/*
[Almost] the smallest and simplest fea fsm.
	Not as small as inlined simple fsm, but easier to work with and debug.

Features :
	- OnEnter, OnUpdate, OnExit.
	- OnEnterFrom, OnExitTo.
		Overrides event behavior when coming from/going to specified states.
	- Supports user arguments in the callbacks (explained below).
	- DelayedTrigger.
		Trigger will happen next time you call fsm::update.
	- Define FEA_FSM_NOTHROW to assert instead of throw.
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
	- Uses std::function. If you can't have that, use inlined fsms instead.
	- Throws on unhandled transition.
		You must explicitly add re-entrant transitions or ignored transitions
		(by providing empty callbacks). IMHO this is one of the bigest source of
		bugs and broken behavior when working with FSMs. Throwing makes
		debugging much faster and easier.
*/


enum class fsm_event : uint8_t {
	on_enter_from,
	on_enter,
	on_update,
	on_exit,
	on_exit_to,
	count,
};

namespace detail {} // namespace detail

template <class, class, class...>
struct fsm;

template <fsm_event Event, size_t FromToStateIdx, class Func>
struct fsm_event_func {
	Func func;
};

template <fsm_event Event,
		size_t FromToStateIdx = std::numeric_limits<size_t>::max(), class Func>
constexpr auto make_event(Func func) {
	return fsm_event_func<Event, FromToStateIdx, Func>{ func };
}

template <class TransitionEnum, class StateEnum, class... EventFuncs>
struct fsm_state {
	// static_assert(std::tuple_size_v<OnEnterFromTup> ==
	// size_t(StateEnum::count), 		"fsm_state : on_enter_from function
	// tuple size must match " 		"StateEnum::count");
	// static_assert(std::tuple_size_v<OnExitToTup> == size_t(StateEnum::count),
	//		"fsm_state : on_exit_to function tuple size must match "
	//		"StateEnum::count");

	// using fsm_t = fsm<TransitionEnum, StateEnum>;
	// using fsm_func_t = decltype(void(fsm_t&, FuncArgs...));

	fsm_state(EventFuncs... event_funcs) {

		std::fill(_transitions.begin(), _transitions.end(), StateEnum::count);
	}

	// Add your event implementation.
	// template <fsm_event Event, StateEnum State = StateEnum::count>
	// void add_event(fsm_func_t&& func) {
	//	if constexpr (Event == fsm_event::on_enter_from) {
	//		std::get<size_t(State)>(_on_enter_from_funcs) = std::move(func);
	//	} else if constexpr (Event == fsm_event::on_exit_to) {
	//		std::get<size_t(State)>(_on_exit_to_funcs) = std::move(func);
	//	} else if constexpr (Event == fsm_event::on_enter) {
	//		_on_enter_func = std::move(func);
	//	} else if constexpr (Event == fsm_event::on_update) {
	//		_on_update_func = std::move(func);
	//	} else if constexpr (Event == fsm_event::on_exit) {
	//		_on_exit_func = std::move(func);
	//	}
	//}

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

	//// Used internally, executes a specific event.
	// template <fsm_event Event, class FSMT, class... FuncArgs>
	// void execute_event([[maybe_unused]] StateEnum to_from_state, FSMT&
	// machine, 		FuncArgs... func_args) { 	static_assert(Event !=
	// fsm_event::on_enter_from, 			"state : do not execute
	// on_enter_from, use on_enter instead " 			"and provide
	// to_from_state"); static_assert(Event != fsm_event::on_exit_to,
	// "state : do not execute on_exit_to, use on_exit instead and "
	// "provide to_from_state");

	//	static_assert(Event != fsm_event::count, "fsm_state : invalid event");


	//	// Check the event, call the appropriate user functions if it is stored.
	//	if constexpr (Event == fsm_event::on_enter) {
	//		if (to_from_state != StateEnum::count
	//				&& _on_enter_from_funcs[size_t(to_from_state)]) {
	//			// has enter_from
	//			std::invoke(_on_enter_from_funcs[size_t(to_from_state)],
	//					machine, func_args...);

	//		} else if (_on_enter_func) {
	//			std::invoke(_on_enter_func, machine, func_args...);
	//		}

	//	} else if constexpr (Event == fsm_event::on_update) {
	//		if (_on_update_func) {
	//			std::invoke(_on_update_func, machine, func_args...);
	//		}
	//	} else if constexpr (Event == fsm_event::on_exit) {
	//		if (to_from_state != StateEnum::count
	//				&& _on_exit_to_funcs[size_t(to_from_state)]) {
	//			// has exit_to
	//			std::invoke(_on_exit_to_funcs[size_t(to_from_state)], machine,
	//					func_args...);

	//		} else if (_on_exit_func) {
	//			std::invoke(_on_exit_func, machine, func_args...);
	//		}
	//	}
	//}

private:
	template <fsm_event Event, StateEnum FromToState = StateEnum::count>
	struct func_key {
		static constexpr size_t func_idx;
	};

	std::array<StateEnum, size_t(TransitionEnum::count)> _transitions;
	std::tuple<> _event_funcs;

	OnEnterFunc _on_enter_func;
	OnUpdateFunc _on_update_func;
	OnExitFunc _on_exit_func;

	// OnEnterFromTup _on_enter_from_funcs;
	// OnExitToTup _on_exit_to_funcs;

	// TBD, makes it heavy but helps debuggability
	// const char* _name;
};

// template <class StateEnum, template <class, class> class... FuncPairs>
// constexpr auto make_on_enter_from_funcs(FuncPairs... func_pairs) {
//}

template <class TransitionEnum, class StateEnum, class OnEnterFunc,
		class OnUpdateFunc = decltype(detail::empty_lambda),
		class OnExitFunc = decltype(detail::empty_lambda)>
constexpr auto make_state(OnEnterFunc on_enter_func,
		OnUpdateFunc on_update_func = detail::empty_lambda,
		OnExitFunc on_exit_func = detail::empty_lambda) {

	return fsm_state<TransitionEnum, StateEnum, OnEnterFunc, OnUpdateFunc,
			OnExitFunc>(on_enter_func, on_update_func, on_exit_func);
}


// template <class TransitionEnum, class StateEnum, class... FuncArgs>
// struct fsm {
//	// using fsm_t = fsm<TransitionEnum, StateEnum, FuncArgs...>;
//	using state_t = fsm_state<TransitionEnum, StateEnum, FuncArgs...>;
//	using fsm_func_t = typename state_t::fsm_func_t;
//
//	// Here, we use move semantics not for performance (it doesn't do anything).
//	// It is to make it clear to the user he cannot modify the state anymore.
//	// The fsm gobbles the state.
//	template <StateEnum State>
//	void add_state(state_t&& state) {
//		static_assert(State != StateEnum::count, "fsm : bad state");
//
//		std::get<size_t(State)>(_states) = std::move(state);
//
//		if (_default_state == StateEnum::count) {
//			_default_state = State;
//		}
//	}
//
//	// Set starting state.
//	// By default, the first added state is used.
//	template <StateEnum State>
//	void set_default_state() {
//		static_assert(State != StateEnum::count, "fsm : bad state");
//		_default_state = State;
//	}
//
//	// First come first served.
//	// Trigger will be called next update(...).
//	// Calling this prevents subsequent triggers to be executed.
//	// Allows more relaxed trigger argument requirements.
//	template <TransitionEnum Transition>
//	void delayed_trigger() {
//		if (_has_delayed_trigger)
//			return;
//
//		_has_delayed_trigger = true;
//		_delayed_trigger_func = [](fsm& f, FuncArgs... func_args) {
//			f._has_delayed_trigger = false;
//			f.trigger<Transition>(func_args...);
//		};
//	}
//
//	// Trigger a transition.
//	// Throws on bad transition (or asserts, if you defined FEA_FSM_NOTHROW).
//	// If you had previously called delayed_trigger, this
//	// won't do anything.
//	template <TransitionEnum Transition>
//	void trigger(FuncArgs... func_args) {
//		if (_has_delayed_trigger)
//			return;
//
//		maybe_init(func_args...);
//
//		StateEnum from_state = _current_state;
//		StateEnum to_state = _states[size_t(_current_state)]
//									 .transition_target<Transition>();
//
//		// Only execute on_exit if we aren't in a trigger from on_exit.
//		if (!_in_on_exit) {
//			_in_on_exit = true;
//
//			// Can recursively call trigger. We must handle that.
//			_states[size_t(from_state)].execute_event<fsm_event::on_exit>(
//					to_state, *this, func_args...);
//
//			if (_in_on_exit == false) {
//				// Exit has triggered transition. Abort.
//				return;
//			}
//		}
//		_in_on_exit = false;
//
//		_current_state = to_state;
//
//		// Always execute on_enter.
//		_states[size_t(to_state)].execute_event<fsm_event::on_enter>(
//				from_state, *this, func_args...);
//	}
//
//	// Update the fsm.
//	// Calls on_update on the current state.
//	// Processes delay_trigger if that was called.
//	void update(FuncArgs... func_args) {
//		while (_has_delayed_trigger) {
//			std::invoke(_delayed_trigger_func, *this, func_args...);
//		}
//
//		maybe_init(func_args...);
//
//		_states[size_t(_current_state)].execute_event<fsm_event::on_update>(
//				StateEnum::count, *this, func_args...);
//	}
//
//	// Get the specified state.
//	template <StateEnum State>
//	const state_t& state() const {
//		return std::get<size_t(State)>(_states);
//	}
//	template <StateEnum State>
//	state_t& state() {
//		return std::get<size_t(State)>(_states);
//	}
//
// private:
//	void maybe_init(FuncArgs... func_args) {
//		if (_current_state != StateEnum::count)
//			return;
//
//		_current_state = _default_state;
//		_states[size_t(_current_state)].execute_event<fsm_event::on_enter>(
//				StateEnum::count, *this, func_args...);
//	}
//
//	std::array<state_t, size_t(StateEnum::count)> _states;
//	StateEnum _current_state = StateEnum::count;
//	StateEnum _default_state = StateEnum::count;
//
//	bool _in_on_exit = false;
//
//	fsm_func_t _delayed_trigger_func = {};
//	bool _has_delayed_trigger = false;
//};

} // namespace inl
} // namespace fea
