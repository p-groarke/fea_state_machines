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


namespace fea {
namespace inl {
namespace detail {
template <class T, class Tuple>
struct tuple_idx {
	static_assert(!std::is_same_v<Tuple, std::tuple<>>,
			"could not find T in given Tuple");
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

//// Used to identify an element by type.
// template <class... Keys>
// struct tuple_map_key {};

// A ordered compile time map container.
// You can search for elements using compile time types.

// Pass in your tuple_map_key type.
// Build with multiple tuple<tuple_map_key<Key>, T>...
template <class KeysTuple, class ValuesTuple>
struct tuple_map {
	tuple_map(KeysTuple keys, ValuesTuple values)
			: _keys(keys)
			, _values(values) {
	}

	// Search by non-type template.
	template <class Key>
	constexpr const auto& find() const {
		constexpr size_t idx = tuple_idx_v<Key, KeysTuple>;
		return std::get<idx>(_values);
	}
	template <class Key>
	constexpr auto& find() {
		constexpr size_t idx = tuple_idx_v<Key, KeysTuple>;
		return std::get<idx>(_values);
	}

private:
	// tuple<tuple_map_key<my, type, keys>>
	KeysTuple _keys;

	// tuple<T>
	ValuesTuple _values;
};

template <class... KeyValues>
constexpr auto make_tuple_map(std::tuple<KeyValues...> tup) {
	auto tup_of_tup = std::apply(
			[&](auto... key_vals) {
				auto keys = std::tuple{ std::get<0>(key_vals)... };
				auto vals = std::tuple{ (std::get<1>(key_vals), ...) };
				return std::tuple{ keys, vals };
			},
			tup);

	return tuple_map{ std::get<0>(tup_of_tup), std::get<1>(tup_of_tup) };
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

template <class, class, class...>
struct fsm;

// Fsm keys are used to lookup your transitions and your states.

// Transition Key will query tuple map with Transition to get the target
// state.
template <class TransitionEnum, TransitionEnum FromTransition>
struct fsm_transition_key {};

// Event key will query the tuple map with Event and possibly a To/From
// state to execute your event.
template <fsm_event Event, class StateEnum, StateEnum FromToStateIdx>
struct fsm_event_key {};

template <class TransitionEnum, class StateEnum, class TransitionMap,
		class EventMap>
struct fsm_state {
	// using builder_t = typename fsm_builder<TransitionEnum, StateEnum>;

	// template <TransitionEnum Trans>
	// using t_key = typename builder_t::transition_key<Trans>;

	// template <fsm_event E, StateEnum S>
	// using event_key = typename builder_t::event_key<E, S>;

	constexpr fsm_state(TransitionMap transition_map, EventMap event_map)
			: _transitions(transition_map)
			, _events(event_map) {
	}

	template <TransitionEnum Transition>
	constexpr StateEnum transition_target() const {
		StateEnum e = _transitions.find<
				fsm_transition_key<TransitionEnum, Transition>>();
		return e;
	}

private:
	// std::array<StateEnum, size_t(TransitionEnum::count)> _transitions;
	TransitionMap _transitions;
	EventMap _events;
};


template <class TransitionEnum, class StateEnum>
struct fsm_builder {

	template <TransitionEnum FromTransition, StateEnum ToState>
	constexpr auto make_transition() {
		static_assert(
				FromTransition != TransitionEnum::count, "invalid transition");
		static_assert(ToState != StateEnum::count, "invalid state");

		return std::tuple{ fsm_transition_key<TransitionEnum, FromTransition>{},
			ToState };
	}

	template <fsm_event Event, StateEnum FromToStateIdx = StateEnum::count,
			class Func>
	constexpr auto make_event(Func func) {
		static_assert(Event != fsm_event::count, "invalid event");

		return std::tuple{ fsm_event_key<Event, StateEnum, FromToStateIdx>{},
			func };
	}

	template <class... Transitions, class... Events>
	constexpr auto make_state(std::tuple<Transitions...> transitions,
			std::tuple<Events...> events) {
		auto tranny_map = detail::make_tuple_map(transitions);
		auto events_map = detail::make_tuple_map(events);

		return fsm_state<TransitionEnum, StateEnum, decltype(tranny_map),
				decltype(events_map)>{ tranny_map, events_map };
	}
};

} // namespace inl
} // namespace fea
