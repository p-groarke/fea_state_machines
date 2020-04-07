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

// template <size_t N, class = std::make_index_sequence<N>>
// struct tuple_expander5000;
//
// template <size_t N, std::size_t... Is>
// struct tuple_expander5000<N, std::index_sequence<Is...>> {
//	template <class Lambda>
//	constexpr auto operator()(Lambda lambda) {
//		return lambda(std::integral_constant<std::size_t, Is>{}...);
//	}
//};

// Pass in your tuple_map_key type.
// Build with multiple tuple<tuple_map_key<Key>, T>...
template <class KeysTuple, class ValuesTuple>
struct tuple_map {
	constexpr tuple_map(KeysTuple keys, ValuesTuple values)
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
	// Used to find the index of the value inside the other tuple.
	KeysTuple _keys{};

	// tuple<T>
	// Your values.
	ValuesTuple _values{};
};

template <class Builder>
constexpr auto make_tuple_map() {
	// At compile time, take the tuple of tuple coming from the
	// builder, iterate through it, grab the first elements of
	// nested tuples (the key) and put that in a new tuple, grab
	// the second element and put that in another tuple.

	// Basically, go from tuple<tuple<key, value>...> to
	// tuple<tuple<key...>, tuple<values...>>
	// which is our map basically.

	constexpr size_t tup_size = std::tuple_size_v<decltype(Builder::unpack())>;

	constexpr auto tup_of_tup
			= detail::tuple_expander5000<tup_size>([](auto... Idxes) constexpr {
				  constexpr auto tup_of_tups = Builder::unpack();

				  constexpr auto keys = std::tuple{ std::get<0>(
						  std::get<decltype(Idxes)::value>(tup_of_tups))... };

				  constexpr auto vals = std::tuple{ std::get<1>(
						  std::get<decltype(Idxes)::value>(tup_of_tups))... };

				  return std::tuple{ keys, vals };
			  });

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
		return _transitions
				.find<fsm_transition_key<TransitionEnum, Transition>>();
	}

private:
	// std::array<StateEnum, size_t(TransitionEnum::count)> _transitions;
	TransitionMap _transitions;
	EventMap _events;
};


template <class TransitionEnum, class StateEnum, TransitionEnum FromT,
		StateEnum ToS, class Parent>
struct fsm_transition_builder {
	template <TransitionEnum NewFromT, StateEnum NewToS>
	constexpr auto make_transition() const {
		return fsm_transition_builder<TransitionEnum, StateEnum, NewFromT,
				NewToS, std::decay_t<decltype(*this)>>{};
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

// template <class Func>
// struct fsm_func_wrapper {
//	static constexpr auto unpack() {
//		return func;
//	}
//	static constexpr NewFunc func;
//};

template <fsm_event Event, class StateEnum, StateEnum FromToState, class Parent,
		class Func>
struct fsm_event_builder {
	template <fsm_event NewEvent, StateEnum NewFromToState = StateEnum::count,
			class NewFunc>
	constexpr auto make_event(NewFunc newfunc) const {

		// yes yes, lambdas can access static vars no problem ;)
		static constexpr auto f = newfunc;

		struct func_wrapper {
			static constexpr auto unpack() {
				// inline static constexpr Func mfunc = func;
				return []() constexpr {
					return f;
				}
				();
			}
		};

		return fsm_event_builder<NewEvent, StateEnum, NewFromToState,
				std::decay_t<decltype(*this)>, func_wrapper>{};
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
};

template <class TransitionEnum, class StateEnum>
struct fsm_builder {
	template <TransitionEnum FromTransition, StateEnum ToState>
	constexpr auto make_transition() const {
		static_assert(
				FromTransition != TransitionEnum::count, "invalid transition");
		static_assert(ToState != StateEnum::count, "invalid state");

		return fsm_transition_builder<TransitionEnum, StateEnum, FromTransition,
				ToState, void>{};
	}

	template <fsm_event Event, StateEnum FromToState = StateEnum::count,
			class Func>
	constexpr auto make_event(Func func) const {
		static_assert(Event != fsm_event::count, "invalid event");

		static constexpr auto f = func;
		struct func_wrapper {
			static constexpr auto unpack() {
				return []() constexpr {
					return f;
				}
				();
			}
		};

		return fsm_event_builder<Event, StateEnum, FromToState, void,
				func_wrapper>{};
		// return std::tuple{ fsm_event_key<Event, StateEnum, FromToStateIdx>{},
		//	func };
	}

	template <class TransitionBuilder, class EventBuilder>
	constexpr auto make_state(TransitionBuilder, EventBuilder) const {
		constexpr auto tranny_map = detail::make_tuple_map<TransitionBuilder>();
		constexpr auto event_map = detail::make_tuple_map<EventBuilder>();

		return fsm_state<TransitionEnum, StateEnum, decltype(tranny_map),
				decltype(event_map)>{ tranny_map, event_map };
	}
}; // namespace inl

} // namespace inl
} // namespace fea
