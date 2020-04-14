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

namespace fea {
namespace inl {

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
struct fsm_transition_key;

// Event key will query the tuple map with Event and possibly a To/From
// state to execute your event.
template <fsm_event Event, class StateEnum, StateEnum FromToState>
struct fsm_event_key;

// State key is used to find states in the state machine itself.
template <class StateEnum, StateEnum State>
struct fsm_state_key;


template <class TransitionEnum, class StateEnum, StateEnum State,
		class TransitionMap, class EventMap, class FuncRet, class... FuncArgs>
struct fsm_state {
	fsm_state(const EventMap& e_map)
			: _events(e_map) {
	}

	template <TransitionEnum Transition>
	static constexpr StateEnum transition_target() {
		return _transitions.template find<
				fsm_transition_key<TransitionEnum, Transition>>();
	}

	// Used internally to call user events if they are set.
	// If a non 'void' return value is set, on_update must return it.
	// on_enter, on_enter_from, on_exit and on_exit_to ignore return values.
	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Machine>
	auto execute_event([[maybe_unused]] Machine& machine,
			[[maybe_unused]] FuncArgs... func_args) {

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
				auto& f = _events.template find<enter_from_key_t>();

				static_assert(
						std::is_invocable_v<decltype(f), FuncArgs..., Machine&>,
						"fsm_event : event signature uncallable. Do your "
						"function arguments match the declared types? Did you "
						"remember to add an auto& as a last argument for the "
						"state machine?");

				f(func_args..., machine);
				return;

			} else if constexpr (has_enter) {
				auto& f = _events.template find<enter_key_t>();

				static_assert(
						std::is_invocable_v<decltype(f), FuncArgs..., Machine&>,
						"fsm_event : event signature uncallable. Do your "
						"function arguments match the declared types? Did you "
						"remember to add an auto& as a last argument for the "
						"state machine?");

				f(func_args..., machine);
				return;
			}

			// on_update
		} else if constexpr (Event == fsm_event::on_update) {
			// Lookup for on_update
			using update_key_t = fsm_event_key<fsm_event::on_update, StateEnum,
					StateEnum::count>;
			constexpr bool has_update
					= _events.template contains<update_key_t>();

			if constexpr (has_update) {
				auto& f = _events.template find<update_key_t>();

				static_assert(
						std::is_invocable_v<decltype(f), FuncArgs..., Machine&>,
						"fsm_event : event signature uncallable. Do your "
						"function arguments match the declared types? Did you "
						"remember to add an auto& as a last argument for the "
						"state machine?");

				// On update is the only event that requires a perfect match of
				// the user provided return type.
				using ret_t = decltype(f(func_args..., machine));
				static_assert(std::is_same_v<ret_t, FuncRet>,
						"fsm_state : invalid 'on_update' return type. Did you "
						"forget to return a value, or declare a return value "
						"in the builder?");

				return f(func_args..., machine);
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
				auto& f = _events.template find<exit_to_key_t>();

				static_assert(
						std::is_invocable_v<decltype(f), FuncArgs..., Machine&>,
						"fsm_event : event signature uncallable. Do your "
						"function arguments match the declared types? Did you "
						"remember to add an auto& as a last argument for the "
						"state machine?");

				f(func_args..., machine);
				return;
			} else if constexpr (has_exit) {
				auto& f = _events.template find<exit_key_t>();

				static_assert(
						std::is_invocable_v<decltype(f), FuncArgs..., Machine&>,
						"fsm_event : event signature uncallable. Do your "
						"function arguments match the declared types? Did you "
						"remember to add an auto& as a last argument for the "
						"state machine?");
				f(func_args..., machine);
				return;
			}
		}
	}

	auto value() const {
		return *this;
	}

	using key_t = typename fsm_state_key<StateEnum, State>;
	static constexpr StateEnum state_t = State;

private:
	static constexpr TransitionMap _transitions{};
	EventMap _events;
};


template <class TransitionEnum, class StateEnum, class StateMap, class FuncRet,
		class... FuncArgs>
struct fsm {
	fsm(const StateMap& s_map, StateEnum start_state)
			: _states(s_map)
			, _start_state(start_state) {
	}

	template <TransitionEnum Transition>
	void trigger(FuncArgs... func_args) {
		maybe_init(func_args...);

		detail::to_constexpr_of_doom(
				[&, this](auto idx_t) {
					constexpr size_t i = std::decay_t<decltype(idx_t)>::value;

					if constexpr (i < size_t(StateEnum::count)) {
						constexpr StateEnum current_state = StateEnum(i);

						using from_key_t
								= fsm_state_key<StateEnum, current_state>;
						if constexpr (_states.template contains<from_key_t>()) {

							auto& from_state
									= _states.template find<from_key_t>();
							constexpr StateEnum to_state_e
									= from_state.template transition_target<
											Transition>();

							if (!_in_on_exit) {
								_in_on_exit = true;

								// Can recursively call trigger. We must handle
								// that.
								from_state.template execute_event<
										fsm_event::on_exit, to_state_e>(
										*this, func_args...);

								if (_in_on_exit == false) {
									// Exit has triggered transition. Abort.
									return;
								}
							}
							_in_on_exit = false;

							_current_state = to_state_e;

							// Always execute on_enter.
							using to_key_t
									= fsm_state_key<StateEnum, to_state_e>;
							auto& to_state = _states.template find<to_key_t>();
							to_state.template execute_event<fsm_event::on_enter,
									to_state_e>(*this, func_args...);

						} else {
							assert(false && "should never get here");
						}
					} else {
						assert(false && "should never get here");
					}
				},
				size_t(_current_state));
	}

	FuncRet update(FuncArgs... func_args) {
		maybe_init(func_args...);

		// to_constexpr to get the state and call call onupdate event
		return detail::to_constexpr_of_doom(
				[&, this](auto idx_t) {
					constexpr size_t i = std::decay_t<decltype(idx_t)>::value;

					if constexpr (i < size_t(StateEnum::count)) {
						constexpr StateEnum current_state = StateEnum(i);

						using key_t = fsm_state_key<StateEnum, current_state>;
						if constexpr (_states.template contains<key_t>()) {
							auto& state = _states.template find<key_t>();

							return state.template execute_event<
									fsm_event::on_update>(*this, func_args...);
						} else {
							assert(false && "should never get here");
							return FuncRet{};
						}
					} else {
						assert(false && "should never get here");
						return FuncRet{};
					}
				},
				size_t(_current_state));
	}

private:
	void maybe_init(FuncArgs... func_args) {
		if (_current_state != StateEnum::count) {
			return;
		}

		_current_state = _start_state;

		detail::to_constexpr_of_doom(
				[&, this](auto idx_t) {
					constexpr size_t i = std::decay_t<decltype(idx_t)>::value;

					if constexpr (i < size_t(StateEnum::count)) {
						constexpr StateEnum s = StateEnum(i);

						using key_t = fsm_state_key<StateEnum, s>;
						if constexpr (_states.template contains<key_t>()) {
							auto& state = _states.template find<key_t>();

							state.template execute_event<fsm_event::on_enter>(
									*this, func_args...);
						} else {
							assert(false && "should never get here");
						}
					} else {
						assert(false && "should never get here");
					}
				},
				size_t(_current_state));
	}

	StateMap _states;

	StateEnum _current_state = StateEnum::count;
	StateEnum _start_state = StateEnum::count;

	bool _in_on_exit = false;
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


template <class TransitionEnum, class StateEnum, TransitionEnum FromTransition,
		StateEnum ToState>
struct fsm_transition_builder {
	static constexpr bool is_event_builder = false;
	static constexpr bool is_transition_builder = true;

	using key_t = typename fsm_transition_key<TransitionEnum, FromTransition>;
	static constexpr StateEnum value_t = ToState;
};

template <fsm_event Event, class StateEnum, StateEnum FromToState, class Func>
struct fsm_event_builder {
	fsm_event_builder(Func func)
			: _func(func) {
	}

	Func value() const {
		return _func;
	}


	static constexpr bool is_event_builder = true;
	static constexpr bool is_transition_builder = false;

	using key_t = typename fsm_event_key<Event, StateEnum, FromToState>;

private:
	Func _func;
};


template <class T>
constexpr auto transition_filter() {
	if constexpr (T::is_transition_builder) {
		// Doubly nested for tuple_cat later.
		return std::make_tuple(
				std::make_tuple(typename T::key_t{}, T::value_t));
	} else {
		return std::tuple{};
	}
};

template <class T>
constexpr auto event_filter([[maybe_unused]] T t) {
	if constexpr (T::is_event_builder) {
		return std::tuple{ t };
	} else {
		return std::tuple{};
	}
};


template <class, class, class>
struct fsm_builder;

template <class TransitionEnum, class StateEnum, class FuncRet,
		class... FuncArgs>
struct fsm_builder<TransitionEnum, StateEnum, FuncRet(FuncArgs...)> {
	template <TransitionEnum FromTransition, StateEnum ToState>
	static constexpr auto make_transition() {
		return fsm_transition_builder<TransitionEnum, StateEnum, FromTransition,
				ToState>{};
	}

	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Func>
	static constexpr auto make_event(Func func) {
		if constexpr (Event == fsm_event::on_enter
				|| Event == fsm_event::on_exit
				|| Event == fsm_event::on_update) {
			static_assert(FromToState == StateEnum::count,
					"make_event : cannot provide a from/to state  when "
					"creating 'on_enter', 'on_update' or 'on_exit' events");
		}
		return fsm_event_builder<Event, StateEnum, FromToState, Func>{ func };
	}

	template <StateEnum State, class... TransitionsAndEvents>
	static constexpr auto make_state(TransitionsAndEvents&&... ts_and_es) {
		// tuple<typle<key, value>> for transition -> state pairs, as they are
		// completely constexpr
		struct transition_build {
			static constexpr auto unpack() {
				return std::tuple_cat(transition_filter<
						std::decay_t<TransitionsAndEvents>>()...);
			}
		};

		auto events_tup = std::tuple_cat(
				event_filter(std::forward<TransitionsAndEvents>(ts_and_es))...);

		auto t_map = detail::make_constexpr_tuple_map<transition_build>();
		auto e_map = detail::make_semi_constexpr_tuple_map(events_tup);
		return fsm_state<TransitionEnum, StateEnum, State,
				std::decay_t<decltype(t_map)>, std::decay_t<decltype(e_map)>,
				FuncRet, FuncArgs...>{ e_map };
	}

	template <class State1, class... States>
	static constexpr auto make_machine(State1& s1, States&&... states) {
		auto states_tup
				= std::tuple_cat(std::make_tuple(std::forward<State1>(s1)),
						std::make_tuple(std::forward<States>(states))...);

		auto s_map = detail::make_semi_constexpr_tuple_map(states_tup);
		return fsm<TransitionEnum, StateEnum, std::decay_t<decltype(s_map)>,
				FuncRet, FuncArgs...>{ s_map, State1::state_t };
	}
};

} // namespace inl
} // namespace fea
