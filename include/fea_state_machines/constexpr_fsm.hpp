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

#define FEA_TOKENPASTE(x, y) x##y
#define FEA_TOKENPASTE2(x, y) FEA_TOKENPASTE(x, y)
#define fea_event(name, f) \
	struct FEA_TOKENPASTE2( \
			FEA_TOKENPASTE2(fea_event_builder_, name), __LINE__) { \
		using is_event_builder = int; \
		static constexpr auto unpack() { \
			return f; \
		} \
	} name


namespace fea {
namespace cexpr {
namespace detail {
template <class T>
struct event_ {
	using type = void;
};

template <class, class = void>
struct is_event_builder : public std::false_type {};

template <class T>
struct is_event_builder<T, typename event_<typename T::is_event_builder>::type>
		: public std::true_type {};


template <class T, class Tuple>
struct tuple_idx {
	static_assert(!std::is_same_v<Tuple, std::tuple<>>,
			"could not find T in given Tuple");

	// static constexpr size_t value = std::numeric_limits<size_t>::max();
};
template <class T, class... Types>
struct tuple_idx<T, std::tuple<T, Types...>> {
	static constexpr size_t value = 0;
};
template <class T, class U, class... Types>
struct tuple_idx<T, std::tuple<U, Types...>> {
	static constexpr size_t value
			= 1 + tuple_idx<T, std::tuple<Types...>>::value;
};

template <class T, class Tuple>
inline constexpr size_t tuple_idx_v = tuple_idx<T, Tuple>::value;


template <class T, class Tuple>
struct tuple_contains;
template <class T>
struct tuple_contains<T, std::tuple<>> : std::false_type {};
template <class T, class U, class... Ts>
struct tuple_contains<T, std::tuple<U, Ts...>>
		: tuple_contains<T, std::tuple<Ts...>> {};
template <class T, class... Ts>
struct tuple_contains<T, std::tuple<T, Ts...>> : std::true_type {};

template <class T, class Tuple>
inline constexpr bool tuple_contains_v = tuple_contains<T, Tuple>::value;


template <class Func, size_t... I>
constexpr auto tuple_expander5000_impl(Func func, std::index_sequence<I...>) {
	return func(std::integral_constant<size_t, I>{}...);
}
template <size_t N, class Func>
constexpr auto tuple_expander5000(Func func) {
	return tuple_expander5000_impl(func, std::make_index_sequence<N>());
}


template <class Func, class Tuple, size_t N = 0>
inline auto runtime_get(Func func, Tuple& tup, size_t idx) {
	if (N == idx) {
		return func(std::get<N>(tup));
	}

	if constexpr (N + 1 < std::tuple_size_v<Tuple>) {
		return runtime_get<Func, Tuple, N + 1>(func, tup, idx);
	}
}


template <class Func, size_t... I>
constexpr auto static_for(Func func, std::index_sequence<I...>) {
	return (func(std::integral_constant<size_t, I>{}), ...);
}

template <size_t N, class Func>
constexpr void static_for(Func func) {
	return static_for(func, std::make_index_sequence<N>());
}


// Pass in your tuple_map_key type.
// Build with multiple tuple<tuple_map_key<Key>, T>...
template <class KeysBuilder, class ValuesBuilder>
struct tuple_map {
	// Search by non-type template.
	template <class Key>
	static constexpr const auto& find() {
		constexpr size_t idx = tuple_idx_v<Key, keys_tup_t>;
		return std::get<idx>(_values);
	}
	// template <class Key>
	// static constexpr auto& find() {
	//	constexpr size_t idx = tuple_idx_v<Key, KeysTuple>;
	//	return std::get<idx>(_values);
	//}

	template <class Key>
	static constexpr bool contains() {
		// Just to make sure we are in constexpr land.
		constexpr bool ret = tuple_contains_v<Key, keys_tup_t>;
		return ret;
	}

private:
	// tuple<tuple_map_key<my, type, keys>>
	// Used to find the index of the value inside the other tuple.
	static constexpr auto _keys = KeysBuilder::unpack();

	// tuple<T>
	// Your values.
	static constexpr auto _values = ValuesBuilder::unpack();

	using keys_tup_t = std::decay_t<decltype(_keys)>;
	using values_tup_t = std::decay_t<decltype(_values)>;
};

template <class Builder>
constexpr auto make_tuple_map() {
	// At compile time, take the tuple of tuple coming from the
	// builder, iterate through it, grab the first elements of
	// nested tuples (the key) and put that in a new tuple, grab
	// the second elements and put that in another tuple.

	// Basically, go from tuple<tuple<key, value>...> to
	// tuple<tuple<key...>, tuple<values...>>
	// which is our map basically.

	struct keys_tup {
		static constexpr auto unpack() {
			constexpr size_t tup_size
					= std::tuple_size_v<decltype(Builder::unpack())>;

			return detail::tuple_expander5000<tup_size>([](
					auto... Idxes) constexpr {
				constexpr auto tup_of_tups = Builder::unpack();

				// Gets all the keys.
				return std::make_tuple(std::get<0>(
						std::get<decltype(Idxes)::value>(tup_of_tups))...);
			});
		}
	};

	struct vals_tup {
		static constexpr auto unpack() {
			constexpr size_t tup_size
					= std::tuple_size_v<decltype(Builder::unpack())>;

			return detail::tuple_expander5000<tup_size>([](
					auto... Idxes) constexpr {
				constexpr auto tup_of_tups = Builder::unpack();

				// Gets all the values.
				return std::make_tuple(std::get<1>(
						std::get<decltype(Idxes)::value>(tup_of_tups))...);
			});
		}
	};

	return tuple_map<keys_tup, vals_tup>{};
}

} // namespace detail


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
			constexpr bool has_enter = _events.template contains<enter_key_t>();

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
			constexpr bool has_exit = _events.template contains<exit_key_t>();

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
				return detail::make_tuple_map<TransitionBuilder>();
			}
		};

		struct e_map {
			static constexpr auto unpack() {
				return detail::make_tuple_map<EventBuilder>();
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
				return detail::make_tuple_map<StateBuilder>();
			}
		};

		return fsm<TransitionEnum, StateEnum, StateEnum::count, State1::state_e,
				false, s_map>{};
	}
};

} // namespace cexpr
} // namespace fea
