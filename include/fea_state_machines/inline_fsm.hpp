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


template <class TransitionEnum, class StateEnum, class TransitionMap,
		class EventMap>
struct fsm_state {
	fsm_state(const EventMap& e_map)
			: _events(e_map) {
	}

	template <TransitionEnum Transition>
	static constexpr StateEnum transition_target() {
		return _transitions.template find<
				fsm_transition_key<TransitionEnum, Transition>>();
	}

private:
	static constexpr TransitionMap _transitions{};
	EventMap _events;
};


template <class TransitionEnum, class StateEnum, class StateMap>
struct fsm {

	void update() {
		// to_constexpr to get the state and call call onupdate event
	}

private:
	StateMap _states;

	StateEnum _current_state = StateEnum::count;
	StateEnum _start_state = StateEnum::count;
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

template <class TransitionEnum, class StateEnum, TransitionEnum FromTransition,
		StateEnum ToState>
auto make_transition() {
	return fsm_transition_builder<TransitionEnum, StateEnum, FromTransition,
			ToState>{};
}

template <fsm_event Event, class StateEnum, StateEnum FromToState, class Func>
struct fsm_event_builder {
	static constexpr bool is_event_builder = true;
	static constexpr bool is_transition_builder = false;

	using key_t = typename fsm_event_key<Event, StateEnum, FromToState>;

	fsm_event_builder(Func func)
			: _func(func) {
	}

	Func value() const {
		return _func;
	}

private:
	Func _func;
};

template <fsm_event Event, class StateEnum,
		StateEnum FromToState = StateEnum::count, class Func>
auto make_event(Func func) {
	return fsm_event_builder<Event, StateEnum, FromToState, Func>{ func };
}


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

template <class TransitionEnum, class StateEnum, class... TransitionsAndEvents>
auto make_state(TransitionsAndEvents&&... ts_and_es) {
	// tuple<typle<key, value>> for transition -> state pairs, as they are
	// completely constexpr
	struct transition_build {
		static constexpr auto unpack() {
			return std::tuple_cat(
					transition_filter<std::decay_t<TransitionsAndEvents>>()...);
		}
	};

	auto events_tup = std::tuple_cat(
			event_filter(std::forward<TransitionsAndEvents>(ts_and_es))...);

	auto t_map = detail::make_constexpr_tuple_map<transition_build>();
	auto e_map = detail::make_semi_constexpr_tuple_map(events_tup);
	return fsm_state<TransitionEnum, StateEnum, std::decay_t<decltype(t_map)>,
			std::decay_t<decltype(e_map)>>{ e_map };
}


template <class TransitionEnum, class StateEnum>
struct fsm_builder {
	template <TransitionEnum FromTransition, StateEnum ToState>
	static constexpr auto make_transition() {
		return fea::inl::make_transition<TransitionEnum, StateEnum,
				FromTransition, ToState>();
	}

	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Func>
	static constexpr auto make_event(Func func) {
		return fea::inl::make_event<Event, StateEnum, FromToState, Func>(func);
	}

	template <class... TransitionsAndEvents>
	static constexpr auto make_state(TransitionsAndEvents&&... ts_and_es) {
		return fea::inl::make_state<TransitionEnum, StateEnum>(
				std::forward<TransitionsAndEvents>(ts_and_es)...);
	}
};


} // namespace inl
} // namespace fea
