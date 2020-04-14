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
#include "tmp.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>

#if !defined(FEA_FSM_NOTHROW)
#include <stdexcept>
#endif

/*
A compile-time executable very simple fsm.
	https://philippegroarke.com/posts/2020/constexpr_fsm/

Features :
	- OnEnter, OnUpdate, OnExit.
	- OnEnterFrom, OnExitTo.
		Overrides event behavior when coming from/going to specified states.
	- Supports user arguments in the callbacks, as the fsm stores your lambda
		directly.
	- static_asserts wherever possible.
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
	- Uses tuple of Func types.
	- You *must* return the results of trigger.
	- You *must* capture the trigger results, this is your new state.

TODO :
	- If this turns out to be useful, adding the following seems reasonable.
	- Transition guards.
	- Yield transitions (aka history state).
*/


namespace fea {
namespace cexpr {
enum class fsm_event : uint8_t {
	on_enter_from,
	on_enter,
	on_update,
	on_exit,
	on_exit_to,
	count,
};

// Fsm keys are used to lookup your transitions and your states.

// Transition Key will query tuple map with Transition to get the target
// state.
template <class TransitionEnum, TransitionEnum FromTransition>
struct fsm_transition_key {};

// Event key will query the tuple map with Event and possibly a To/From
// state to execute your event.
template <fsm_event Event, class StateEnum, StateEnum FromToState>
struct fsm_event_key {};

// State key is used to find states in the state machine itself.
template <class StateEnum, StateEnum State>
struct fsm_state_key {};

template <class TransitionEnum, class StateEnum, class TransitionMap,
		class EventMap, StateEnum State>
struct fsm_state {
	static constexpr StateEnum state_e = State;

	template <TransitionEnum Transition>
	static constexpr StateEnum transition_target() {
		return _transitions.template find<
				fsm_transition_key<TransitionEnum, Transition>>();
	}

	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Machine, class... FuncArgs>
	static constexpr auto execute_event([[maybe_unused]] Machine& machine,
			[[maybe_unused]] FuncArgs&&... func_args) {
		static_assert(Event != fsm_event::on_enter_from,
				"state : do not execute on_enter_from, use on_enter instead "
				"and provide to_from_state");
		static_assert(Event != fsm_event::on_exit_to,
				"state : do not execute on_exit_to, use on_exit instead and "
				"provide to_from_state");

		static_assert(Event != fsm_event::count, "fsm_state : invalid event");

		// Check the event, call the appropriate user functions if it is
		// stored.

		// on_enter and on_enter_from
		if constexpr (Event == fsm_event::on_enter) {
			// Build our lookup key for the on_enter_from event.
			using enter_from_key_t = fsm_event_key<fsm_event::on_enter_from,
					StateEnum, FromToState>;
			// Build our lookup key for the on_enter event.
			using enter_key_t = fsm_event_key<fsm_event::on_enter, StateEnum,
					StateEnum::count>;

			// Encourage the compiler, he needs some positive reinforcement
			// after all this.
			constexpr bool has_enter_from
					= _events.template contains<enter_from_key_t>();
			[[maybe_unused]] constexpr bool has_enter
					= _events.template contains<enter_key_t>();

			if constexpr (FromToState != StateEnum::count && has_enter_from) {
				// Invoke with machine as last argument to support calling
				// member functions on object.
				constexpr auto& f = _events.template find<enter_from_key_t>();

				// std::invoke is not constexpr.
				return f(machine, std::forward<FuncArgs>(func_args)...);

			} else if constexpr (has_enter) {
				constexpr auto& f = _events.template find<enter_key_t>();
				return f(machine, std::forward<FuncArgs>(func_args)...);
			}

			// on_update
		} else if constexpr (Event == fsm_event::on_update) {
			// Lookup for on_update
			using update_key_t = fsm_event_key<fsm_event::on_update, StateEnum,
					StateEnum::count>;
			constexpr bool has_update
					= _events.template contains<update_key_t>();

			if constexpr (has_update) {
				constexpr auto& f = _events.template find<update_key_t>();
				return f(machine, std::forward<FuncArgs>(func_args)...);
			}

			// on_exit and on_exit_to
		} else if constexpr (Event == fsm_event::on_exit) {
			// Lookup for on_exit_to
			using exit_to_key_t = fsm_event_key<fsm_event::on_exit_to,
					StateEnum, FromToState>;

			// Lookup for on_exit
			using exit_key_t = fsm_event_key<fsm_event::on_exit, StateEnum,
					StateEnum::count>;

			constexpr bool has_exit_to
					= _events.template contains<exit_to_key_t>();
			[[maybe_unused]] constexpr bool has_exit
					= _events.template contains<exit_key_t>();

			if constexpr (FromToState != StateEnum::count && has_exit_to) {
				constexpr auto& f = _events.template find<exit_to_key_t>();
				return f(machine, std::forward<FuncArgs>(func_args)...);

			} else if constexpr (has_exit) {
				constexpr auto& f = _events.template find<exit_key_t>();
				return f(machine, std::forward<FuncArgs>(func_args)...);
			}
		}
	}

private:
	static constexpr auto _transitions = TransitionMap::unpack();
	static constexpr auto _events = EventMap::unpack();
};


template <class TransitionEnum, class StateEnum, StateEnum CurrentState,
		StateEnum StartState, bool InOnExit, class StateMap>
struct fsm {
	using fsm_t = fsm<TransitionEnum, StateEnum, CurrentState, StartState,
			InOnExit, StateMap>;

	using fsm_inexit_t = fsm<TransitionEnum, StateEnum, CurrentState,
			StartState, true, StateMap>;
	using fsm_notinexit_t = fsm<TransitionEnum, StateEnum, CurrentState,
			StartState, false, StateMap>;


	template <class... FuncArgs>
	[[nodiscard]] static constexpr auto init(FuncArgs&&... func_args) {
		static_assert(CurrentState == StateEnum::count,
				"CurrentState is valid, did you already call init?");

		// needs init
		constexpr auto& s
				= _states.template find<fsm_state_key<StateEnum, StartState>>();
		using new_fsm_t = fsm<TransitionEnum, StateEnum, StartState, StartState,
				InOnExit, StateMap>;
		constexpr auto passed_in = new_fsm_t{};

		using func_ret_t
				= decltype(s.template execute_event<fsm_event::on_enter>(
						passed_in, std::forward<FuncArgs>(func_args)...));

		// When triggering inside on_enter, the user must return the new fsm.
		// If the event returns void, no trigger happened.
		if constexpr (!std::is_same_v<func_ret_t, void>) {
			return s.template execute_event<fsm_event::on_enter>(
					passed_in, std::forward<FuncArgs>(func_args)...);
		} else {
			s.template execute_event<fsm_event::on_enter>(
					passed_in, std::forward<FuncArgs>(func_args)...);
			return passed_in;
		}
	}

	template <TransitionEnum Transition, class... FuncArgs>
	[[nodiscard]] static constexpr auto trigger(FuncArgs&&... func_args) {
		static_assert(CurrentState != StateEnum::count,
				"CurrentState is invalid, did you forget to call init?");

		static_assert(
				InOnExit == false, "cannot trigger transition in on_exit");

		// do normal trigger...
		// if anything can be considered normal at this point...
		// Here is when I started questioning my life decisions...
		constexpr StateEnum from_state = CurrentState;

		constexpr StateEnum to_state
				= _states.template find<fsm_state_key<StateEnum, from_state>>()
						  .template transition_target<Transition>();

		// Call on_exit
		constexpr auto& from_s
				= _states.template find<fsm_state_key<StateEnum, from_state>>();

		constexpr auto passed_in_exit = fsm_inexit_t{};
		from_s.template execute_event<fsm_event::on_exit, to_state>(
				passed_in_exit, std::forward<FuncArgs>(func_args)...);

		// Call on_enter and update current state.
		using new_fsm_t = fsm<TransitionEnum, StateEnum, to_state, StartState,
				false, StateMap>;

		constexpr auto passed_in_enter = new_fsm_t{};
		constexpr auto& to_s
				= _states.template find<fsm_state_key<StateEnum, to_state>>();

		using func_ret_t = decltype(
				to_s.template execute_event<fsm_event::on_enter, from_state>(
						passed_in_enter, std::forward<FuncArgs>(func_args)...));

		// When triggering inside on_enter, the user must return the new fsm.
		// If the event returns void, no trigger happened, state stays the same.
		if constexpr (!std::is_same_v<func_ret_t, void>) {
			// static_assert(detail::is_same_templated<func_ret_t, new_fsm_t>(),
			//		"You cannot return custom values in on_enter, on_exit. "
			//		"Only from update.");

			// Return new state.
			return to_s.template execute_event<fsm_event::on_enter, from_state>(
					passed_in_enter, std::forward<FuncArgs>(func_args)...);
		} else {
			to_s.template execute_event<fsm_event::on_enter, from_state>(
					passed_in_enter, std::forward<FuncArgs>(func_args)...);
			// Return 'current' state.
			return passed_in_enter;
		}
	}


	template <class... FuncArgs>
	static constexpr auto update(FuncArgs&&... func_args) {
		static_assert(CurrentState != StateEnum::count,
				"CurrentState is invalid, did you forget to call init?");

		constexpr auto passed_in = fsm_t{};
		constexpr auto& s = _states.template find<
				fsm_state_key<StateEnum, CurrentState>>();

		return s.template execute_event<fsm_event::on_update>(
				passed_in, std::forward<FuncArgs>(func_args)...);
	}

private:
	static constexpr auto _states = StateMap::unpack();
};


template <class TransitionEnum, class StateEnum, TransitionEnum FromT,
		StateEnum ToS, class Parent>
struct fsm_transition_builder {
	template <TransitionEnum NewFromT, StateEnum NewToS>
	static constexpr auto make_transition() {
		using parent_t = fsm_transition_builder<TransitionEnum, StateEnum,
				FromT, ToS, Parent>;

		return fsm_transition_builder<TransitionEnum, StateEnum, NewFromT,
				NewToS, parent_t>{};
	}

	static constexpr auto unpack() {
		if constexpr (!std::is_same_v<Parent, void>) {
			return std::tuple_cat(
					std::make_tuple(std::tuple{
							fsm_transition_key<TransitionEnum, FromT>{}, ToS }),
					Parent::unpack());
		} else {
			return std::make_tuple(std::tuple{
					fsm_transition_key<TransitionEnum, FromT>{}, ToS });
		}
	}
};


template <fsm_event Event, class StateEnum, StateEnum FromToState, class Parent,
		class Func>
struct fsm_event_builder {
	template <fsm_event NewEvent, StateEnum NewFromToState = StateEnum::count,
			class NewFunc>
	static constexpr auto make_event([[maybe_unused]] NewFunc newfunc) {

#if defined(_MSC_VER) && defined(FEA_FSM_NO_EVENT_WRAPPER)
		// Only works on MSVC.
		// There is apparently a proposal to remove the restrictions on static
		// constexpr variables in static constexpr functions.

		// Store the func here temporarily until we unpack this stuff.
		// Since the function is templated on NewFunc type, we shouldn't get
		// collisions with other make_event calls.
		// aka famous last words

		static constexpr auto f = newfunc;
		struct func_wrapper {
			static constexpr auto unpack() {
				return f;
			}
		};

		using parent_t = fsm_event_builder<Event, StateEnum, FromToState,
				Parent, Func>;
		return fsm_event_builder<NewEvent, StateEnum, NewFromToState, parent_t,
				func_wrapper>{};
#else
		// For now, use macro :(
		static_assert(detail::is_event_builder<Func>::value,
				"use 'fea_event' macro to create events. ex : "
				"fea_event(event_name, [](auto&){});");
		using parent_t = fsm_event_builder<Event, StateEnum, FromToState,
				Parent, Func>;
		return fsm_event_builder<NewEvent, StateEnum, NewFromToState, parent_t,
				NewFunc>{};
#endif
	}

	static constexpr auto unpack() {
		if constexpr (!std::is_same_v<Parent, void>) {
			return std::tuple_cat(
					std::make_tuple(std::tuple{
							fsm_event_key<Event, StateEnum, FromToState>{},
							Func::unpack() }),
					Parent::unpack());
		} else {
			return std::make_tuple(
					std::tuple{ fsm_event_key<Event, StateEnum, FromToState>{},
							Func::unpack() });
		}
	}
}; // namespace cexpr

template <class TransitionEnum, class StateEnum>
struct fsm_builder {
	static constexpr auto empty_t() {
		return fsm_transition_builder<TransitionEnum, StateEnum,
				TransitionEnum::count, StateEnum::count, void>{};
	}

	static constexpr auto empty_e() {
		struct func_wrapper {
			static constexpr auto unpack() {
				return [](auto&) {};
			}
		};
		return fsm_event_builder<fsm_event::count, StateEnum, StateEnum::count,
				void, func_wrapper>{};
	}

	template <TransitionEnum FromTransition, StateEnum ToState>
	static constexpr auto make_transition() {
		static_assert(
				FromTransition != TransitionEnum::count, "invalid transition");
		static_assert(ToState != StateEnum::count, "invalid state");

		return fsm_transition_builder<TransitionEnum, StateEnum, FromTransition,
				ToState, void>{};
	}

	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Func>
	static constexpr auto make_event([[maybe_unused]] Func func) {
		static_assert(Event != fsm_event::count, "invalid event");

#if defined(_MSC_VER) && defined(FEA_FSM_NO_EVENT_WRAPPER)
		// This only works on MSVC.
		// There is a proposal to remove the restrictions on static
		// cosntexpr variables in static constexpr functions.

		static constexpr auto f = func;
		struct func_wrapper {
			static constexpr auto unpack() {
				return f;
			}
		};

		return fsm_event_builder<Event, StateEnum, FromToState, void,
				func_wrapper>{};
#else
		// For now, use macro :(
		static_assert(detail::is_event_builder<Func>::value,
				"use 'fea_event' macro to create events. ex : "
				"fea_event(event_name, [](auto&){});");
		return fsm_event_builder<Event, StateEnum, FromToState, void, Func>{};
#endif
	}

	template <StateEnum State, class TransitionBuilder, class EventBuilder>
	static constexpr auto make_state(TransitionBuilder, EventBuilder) {
		struct t_map {
			static constexpr auto unpack() {
				return detail::make_constexpr_tuple_map<TransitionBuilder>();
			}
		};

		struct e_map {
			static constexpr auto unpack() {
				return detail::make_constexpr_tuple_map<EventBuilder>();
			}
		};

		return fsm_state<TransitionEnum, StateEnum, t_map, e_map, State>{};
	}

	template <class State1, class... States>
	static constexpr auto make_machine(State1, States...) {

		struct s_map {
			struct StateBuilder {
				static constexpr auto unpack() {
					return std::make_tuple(
							std::make_tuple(
									fsm_state_key<StateEnum, State1::state_e>{},
									State1{}),
							std::make_tuple(
									fsm_state_key<StateEnum, States::state_e>{},
									States{})...);
				}
			};

			static constexpr auto unpack() {
				return detail::make_constexpr_tuple_map<StateBuilder>();
			}
		};

		return fsm<TransitionEnum, StateEnum, StateEnum::count, State1::state_e,
				false, s_map>{};
	}
};

} // namespace cexpr
} // namespace fea
