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
#include <tuple>
#include <type_traits>

/*
yolo...
*/

#define FEA_TOKENPASTE(x, y) x##y
#define FEA_TOKENPASTE2(x, y) FEA_TOKENPASTE(x, y)
#define fea_event(name, f) \
	struct FEA_TOKENPASTE2( \
			FEA_TOKENPASTE2(fea_event_builder_, name), __LINE__) { \
		using is_event_builder [[maybe_unused]] = int; \
		static constexpr auto unpack() { \
			return f; \
		} \
	} name

namespace fea {
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
struct constexpr_tuple_map {
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
constexpr auto make_constexpr_tuple_map() {
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

	return constexpr_tuple_map<keys_tup, vals_tup>{};
}


// Pass in your tuple_map_key type.
// Build with multiple tuple<tuple_map_key<Key>, T>...
template <class KeysBuilder, class ValuesTuple>
struct semi_constexpr_tuple_map {
	semi_constexpr_tuple_map(const ValuesTuple& values)
			: _values(values) {
	}

	// Search by non-type template.
	template <class Key>
	const auto& find() const {
		constexpr size_t idx = tuple_idx_v<Key, keys_tup_t>;
		return std::get<idx>(_values);
	}
	template <class Key>
	auto& find() {
		constexpr size_t idx = tuple_idx_v<Key, keys_tup_t>;
		return std::get<idx>(_values);
	}

	template <class Key>
	static constexpr bool contains() {
		constexpr bool ret = tuple_contains_v<Key, keys_tup_t>;
		return ret;
	}

	static constexpr size_t size() {
		return std::tuple_size_v<keys_tup_t>;
	}

private:
	// tuple<tuple_map_key<my, type, keys>>
	// Used to find the index of the value inside the other tuple.
	static constexpr auto _keys = KeysBuilder::unpack();

	// tuple<T>
	// Your values.
	ValuesTuple _values;

	using keys_tup_t = std::decay_t<decltype(_keys)>;
	using values_tup_t = std::decay_t<decltype(_values)>;
};

// The keys are static constexpr, but the values are not.
template <class BuilderTuple>
constexpr auto make_semi_constexpr_tuple_map(const BuilderTuple& builder_tup) {
	// At compile time, take the tuple of tuple coming from the
	// builder, iterate through it, grab the first elements of
	// nested tuples (the key) and put that in a new tuple, grab
	// the second elements and put that in another tuple.

	// Basically, go from tuple<tuple<key, value>...> to
	// tuple<tuple<key...>, tuple<values...>>
	// which is our map basically.

	struct keys_tup {
		static constexpr auto unpack() {
			constexpr size_t tup_size = std::tuple_size_v<BuilderTuple>;

			return detail::tuple_expander5000<tup_size>([](
					auto... Idxes) constexpr {
				// Gets all the keys.
				constexpr auto tup = std::make_tuple(
						typename std::tuple_element_t<decltype(Idxes)::value,
								BuilderTuple>::key_t{}...);
				return tup;
			});
		}
	};

	auto val_tup = std::apply(
			[](const auto&... vals) {
				return std::make_tuple(vals.value()...);
			},
			builder_tup);

	return semi_constexpr_tuple_map<keys_tup, std::decay_t<decltype(val_tup)>>{
		val_tup
	};
}


template <class Func>
auto to_constexpr_of_doom(Func func, size_t i) {
	using ret_t = decltype(func(std::integral_constant<size_t, 0>{}));

	// TODO : benchmark with if constexpr (generates o(n) assembly?)
	//#define FEA_CASE(n) \
//	case (n): { \
//		if constexpr (n < MaxN) { \
//			std::integral_constant<size_t, n> v; \
//			return func(v); \
//		} \
//	} \
//		[[fallthrough]]

	switch (i) {
#define FEA_CASE(n) \
	case (n): { \
		std::integral_constant<size_t, n> v; \
		return func(v); \
	} \
		[[fallthrough]]

		FEA_CASE(0);
		FEA_CASE(1);
		FEA_CASE(2);
		FEA_CASE(3);
		FEA_CASE(4);
		FEA_CASE(5);
		FEA_CASE(6);
		FEA_CASE(7);
		FEA_CASE(8);
		FEA_CASE(9);
		FEA_CASE(10);
		FEA_CASE(11);
		FEA_CASE(12);
		FEA_CASE(13);
		FEA_CASE(14);
		FEA_CASE(15);
		FEA_CASE(16);
		FEA_CASE(17);
		FEA_CASE(18);
		FEA_CASE(19);
		FEA_CASE(20);
		FEA_CASE(21);
		FEA_CASE(22);
		FEA_CASE(23);
		FEA_CASE(24);
		FEA_CASE(25);
		FEA_CASE(26);
		FEA_CASE(27);
		FEA_CASE(28);
		FEA_CASE(29);
		FEA_CASE(30);
		FEA_CASE(31);
		FEA_CASE(32);
		FEA_CASE(33);
		FEA_CASE(34);
		FEA_CASE(35);
		FEA_CASE(36);
		FEA_CASE(37);
		FEA_CASE(38);
		FEA_CASE(39);
		FEA_CASE(40);
		FEA_CASE(41);
		FEA_CASE(42);
		FEA_CASE(43);
		FEA_CASE(44);
		FEA_CASE(45);
		FEA_CASE(46);
		FEA_CASE(47);
		FEA_CASE(48);
		FEA_CASE(49);
		FEA_CASE(50);
		FEA_CASE(51);
		FEA_CASE(52);
		FEA_CASE(53);
		FEA_CASE(54);
		FEA_CASE(55);
		FEA_CASE(56);
		FEA_CASE(57);
		FEA_CASE(58);
		FEA_CASE(59);
		FEA_CASE(60);
		FEA_CASE(61);
		FEA_CASE(62);
		FEA_CASE(63);
		FEA_CASE(64);
		FEA_CASE(65);
		FEA_CASE(66);
		FEA_CASE(67);
		FEA_CASE(68);
		FEA_CASE(69);
		FEA_CASE(70);
		FEA_CASE(71);
		FEA_CASE(72);
		FEA_CASE(73);
		FEA_CASE(74);
		FEA_CASE(75);
		FEA_CASE(76);
		FEA_CASE(77);
		FEA_CASE(78);
		FEA_CASE(79);
		FEA_CASE(80);
		FEA_CASE(81);
		FEA_CASE(82);
		FEA_CASE(83);
		FEA_CASE(84);
		FEA_CASE(85);
		FEA_CASE(86);
		FEA_CASE(87);
		FEA_CASE(88);
		FEA_CASE(89);
		FEA_CASE(90);
		FEA_CASE(91);
		FEA_CASE(92);
		FEA_CASE(93);
		FEA_CASE(94);
		FEA_CASE(95);
		FEA_CASE(96);
		FEA_CASE(97);
		FEA_CASE(98);
		FEA_CASE(99);
#if defined(FEA_FSM_500_STATES) || defined(FEA_FSM_1000_STATES)
		FEA_CASE(100);
		FEA_CASE(101);
		FEA_CASE(102);
		FEA_CASE(103);
		FEA_CASE(104);
		FEA_CASE(105);
		FEA_CASE(106);
		FEA_CASE(107);
		FEA_CASE(108);
		FEA_CASE(109);
		FEA_CASE(110);
		FEA_CASE(111);
		FEA_CASE(112);
		FEA_CASE(113);
		FEA_CASE(114);
		FEA_CASE(115);
		FEA_CASE(116);
		FEA_CASE(117);
		FEA_CASE(118);
		FEA_CASE(119);
		FEA_CASE(120);
		FEA_CASE(121);
		FEA_CASE(122);
		FEA_CASE(123);
		FEA_CASE(124);
		FEA_CASE(125);
		FEA_CASE(126);
		FEA_CASE(127);
		FEA_CASE(128);
		FEA_CASE(129);
		FEA_CASE(130);
		FEA_CASE(131);
		FEA_CASE(132);
		FEA_CASE(133);
		FEA_CASE(134);
		FEA_CASE(135);
		FEA_CASE(136);
		FEA_CASE(137);
		FEA_CASE(138);
		FEA_CASE(139);
		FEA_CASE(140);
		FEA_CASE(141);
		FEA_CASE(142);
		FEA_CASE(143);
		FEA_CASE(144);
		FEA_CASE(145);
		FEA_CASE(146);
		FEA_CASE(147);
		FEA_CASE(148);
		FEA_CASE(149);
		FEA_CASE(150);
		FEA_CASE(151);
		FEA_CASE(152);
		FEA_CASE(153);
		FEA_CASE(154);
		FEA_CASE(155);
		FEA_CASE(156);
		FEA_CASE(157);
		FEA_CASE(158);
		FEA_CASE(159);
		FEA_CASE(160);
		FEA_CASE(161);
		FEA_CASE(162);
		FEA_CASE(163);
		FEA_CASE(164);
		FEA_CASE(165);
		FEA_CASE(166);
		FEA_CASE(167);
		FEA_CASE(168);
		FEA_CASE(169);
		FEA_CASE(170);
		FEA_CASE(171);
		FEA_CASE(172);
		FEA_CASE(173);
		FEA_CASE(174);
		FEA_CASE(175);
		FEA_CASE(176);
		FEA_CASE(177);
		FEA_CASE(178);
		FEA_CASE(179);
		FEA_CASE(180);
		FEA_CASE(181);
		FEA_CASE(182);
		FEA_CASE(183);
		FEA_CASE(184);
		FEA_CASE(185);
		FEA_CASE(186);
		FEA_CASE(187);
		FEA_CASE(188);
		FEA_CASE(189);
		FEA_CASE(190);
		FEA_CASE(191);
		FEA_CASE(192);
		FEA_CASE(193);
		FEA_CASE(194);
		FEA_CASE(195);
		FEA_CASE(196);
		FEA_CASE(197);
		FEA_CASE(198);
		FEA_CASE(199);
		FEA_CASE(200);
		FEA_CASE(201);
		FEA_CASE(202);
		FEA_CASE(203);
		FEA_CASE(204);
		FEA_CASE(205);
		FEA_CASE(206);
		FEA_CASE(207);
		FEA_CASE(208);
		FEA_CASE(209);
		FEA_CASE(210);
		FEA_CASE(211);
		FEA_CASE(212);
		FEA_CASE(213);
		FEA_CASE(214);
		FEA_CASE(215);
		FEA_CASE(216);
		FEA_CASE(217);
		FEA_CASE(218);
		FEA_CASE(219);
		FEA_CASE(220);
		FEA_CASE(221);
		FEA_CASE(222);
		FEA_CASE(223);
		FEA_CASE(224);
		FEA_CASE(225);
		FEA_CASE(226);
		FEA_CASE(227);
		FEA_CASE(228);
		FEA_CASE(229);
		FEA_CASE(230);
		FEA_CASE(231);
		FEA_CASE(232);
		FEA_CASE(233);
		FEA_CASE(234);
		FEA_CASE(235);
		FEA_CASE(236);
		FEA_CASE(237);
		FEA_CASE(238);
		FEA_CASE(239);
		FEA_CASE(240);
		FEA_CASE(241);
		FEA_CASE(242);
		FEA_CASE(243);
		FEA_CASE(244);
		FEA_CASE(245);
		FEA_CASE(246);
		FEA_CASE(247);
		FEA_CASE(248);
		FEA_CASE(249);
		FEA_CASE(250);
		FEA_CASE(251);
		FEA_CASE(252);
		FEA_CASE(253);
		FEA_CASE(254);
		FEA_CASE(255);
		FEA_CASE(256);
		FEA_CASE(257);
		FEA_CASE(258);
		FEA_CASE(259);
		FEA_CASE(260);
		FEA_CASE(261);
		FEA_CASE(262);
		FEA_CASE(263);
		FEA_CASE(264);
		FEA_CASE(265);
		FEA_CASE(266);
		FEA_CASE(267);
		FEA_CASE(268);
		FEA_CASE(269);
		FEA_CASE(270);
		FEA_CASE(271);
		FEA_CASE(272);
		FEA_CASE(273);
		FEA_CASE(274);
		FEA_CASE(275);
		FEA_CASE(276);
		FEA_CASE(277);
		FEA_CASE(278);
		FEA_CASE(279);
		FEA_CASE(280);
		FEA_CASE(281);
		FEA_CASE(282);
		FEA_CASE(283);
		FEA_CASE(284);
		FEA_CASE(285);
		FEA_CASE(286);
		FEA_CASE(287);
		FEA_CASE(288);
		FEA_CASE(289);
		FEA_CASE(290);
		FEA_CASE(291);
		FEA_CASE(292);
		FEA_CASE(293);
		FEA_CASE(294);
		FEA_CASE(295);
		FEA_CASE(296);
		FEA_CASE(297);
		FEA_CASE(298);
		FEA_CASE(299);
		FEA_CASE(300);
		FEA_CASE(301);
		FEA_CASE(302);
		FEA_CASE(303);
		FEA_CASE(304);
		FEA_CASE(305);
		FEA_CASE(306);
		FEA_CASE(307);
		FEA_CASE(308);
		FEA_CASE(309);
		FEA_CASE(310);
		FEA_CASE(311);
		FEA_CASE(312);
		FEA_CASE(313);
		FEA_CASE(314);
		FEA_CASE(315);
		FEA_CASE(316);
		FEA_CASE(317);
		FEA_CASE(318);
		FEA_CASE(319);
		FEA_CASE(320);
		FEA_CASE(321);
		FEA_CASE(322);
		FEA_CASE(323);
		FEA_CASE(324);
		FEA_CASE(325);
		FEA_CASE(326);
		FEA_CASE(327);
		FEA_CASE(328);
		FEA_CASE(329);
		FEA_CASE(330);
		FEA_CASE(331);
		FEA_CASE(332);
		FEA_CASE(333);
		FEA_CASE(334);
		FEA_CASE(335);
		FEA_CASE(336);
		FEA_CASE(337);
		FEA_CASE(338);
		FEA_CASE(339);
		FEA_CASE(340);
		FEA_CASE(341);
		FEA_CASE(342);
		FEA_CASE(343);
		FEA_CASE(344);
		FEA_CASE(345);
		FEA_CASE(346);
		FEA_CASE(347);
		FEA_CASE(348);
		FEA_CASE(349);
		FEA_CASE(350);
		FEA_CASE(351);
		FEA_CASE(352);
		FEA_CASE(353);
		FEA_CASE(354);
		FEA_CASE(355);
		FEA_CASE(356);
		FEA_CASE(357);
		FEA_CASE(358);
		FEA_CASE(359);
		FEA_CASE(360);
		FEA_CASE(361);
		FEA_CASE(362);
		FEA_CASE(363);
		FEA_CASE(364);
		FEA_CASE(365);
		FEA_CASE(366);
		FEA_CASE(367);
		FEA_CASE(368);
		FEA_CASE(369);
		FEA_CASE(370);
		FEA_CASE(371);
		FEA_CASE(372);
		FEA_CASE(373);
		FEA_CASE(374);
		FEA_CASE(375);
		FEA_CASE(376);
		FEA_CASE(377);
		FEA_CASE(378);
		FEA_CASE(379);
		FEA_CASE(380);
		FEA_CASE(381);
		FEA_CASE(382);
		FEA_CASE(383);
		FEA_CASE(384);
		FEA_CASE(385);
		FEA_CASE(386);
		FEA_CASE(387);
		FEA_CASE(388);
		FEA_CASE(389);
		FEA_CASE(390);
		FEA_CASE(391);
		FEA_CASE(392);
		FEA_CASE(393);
		FEA_CASE(394);
		FEA_CASE(395);
		FEA_CASE(396);
		FEA_CASE(397);
		FEA_CASE(398);
		FEA_CASE(399);
		FEA_CASE(400);
		FEA_CASE(401);
		FEA_CASE(402);
		FEA_CASE(403);
		FEA_CASE(404);
		FEA_CASE(405);
		FEA_CASE(406);
		FEA_CASE(407);
		FEA_CASE(408);
		FEA_CASE(409);
		FEA_CASE(410);
		FEA_CASE(411);
		FEA_CASE(412);
		FEA_CASE(413);
		FEA_CASE(414);
		FEA_CASE(415);
		FEA_CASE(416);
		FEA_CASE(417);
		FEA_CASE(418);
		FEA_CASE(419);
		FEA_CASE(420);
		FEA_CASE(421);
		FEA_CASE(422);
		FEA_CASE(423);
		FEA_CASE(424);
		FEA_CASE(425);
		FEA_CASE(426);
		FEA_CASE(427);
		FEA_CASE(428);
		FEA_CASE(429);
		FEA_CASE(430);
		FEA_CASE(431);
		FEA_CASE(432);
		FEA_CASE(433);
		FEA_CASE(434);
		FEA_CASE(435);
		FEA_CASE(436);
		FEA_CASE(437);
		FEA_CASE(438);
		FEA_CASE(439);
		FEA_CASE(440);
		FEA_CASE(441);
		FEA_CASE(442);
		FEA_CASE(443);
		FEA_CASE(444);
		FEA_CASE(445);
		FEA_CASE(446);
		FEA_CASE(447);
		FEA_CASE(448);
		FEA_CASE(449);
		FEA_CASE(450);
		FEA_CASE(451);
		FEA_CASE(452);
		FEA_CASE(453);
		FEA_CASE(454);
		FEA_CASE(455);
		FEA_CASE(456);
		FEA_CASE(457);
		FEA_CASE(458);
		FEA_CASE(459);
		FEA_CASE(460);
		FEA_CASE(461);
		FEA_CASE(462);
		FEA_CASE(463);
		FEA_CASE(464);
		FEA_CASE(465);
		FEA_CASE(466);
		FEA_CASE(467);
		FEA_CASE(468);
		FEA_CASE(469);
		FEA_CASE(470);
		FEA_CASE(471);
		FEA_CASE(472);
		FEA_CASE(473);
		FEA_CASE(474);
		FEA_CASE(475);
		FEA_CASE(476);
		FEA_CASE(477);
		FEA_CASE(478);
		FEA_CASE(479);
		FEA_CASE(480);
		FEA_CASE(481);
		FEA_CASE(482);
		FEA_CASE(483);
		FEA_CASE(484);
		FEA_CASE(485);
		FEA_CASE(486);
		FEA_CASE(487);
		FEA_CASE(488);
		FEA_CASE(489);
		FEA_CASE(490);
		FEA_CASE(491);
		FEA_CASE(492);
		FEA_CASE(493);
		FEA_CASE(494);
		FEA_CASE(495);
		FEA_CASE(496);
		FEA_CASE(497);
		FEA_CASE(498);
		FEA_CASE(499);
#endif
#if defined(FEA_FSM_1000_STATES)
		FEA_CASE(500);
		FEA_CASE(501);
		FEA_CASE(502);
		FEA_CASE(503);
		FEA_CASE(504);
		FEA_CASE(505);
		FEA_CASE(506);
		FEA_CASE(507);
		FEA_CASE(508);
		FEA_CASE(509);
		FEA_CASE(510);
		FEA_CASE(511);
		FEA_CASE(512);
		FEA_CASE(513);
		FEA_CASE(514);
		FEA_CASE(515);
		FEA_CASE(516);
		FEA_CASE(517);
		FEA_CASE(518);
		FEA_CASE(519);
		FEA_CASE(520);
		FEA_CASE(521);
		FEA_CASE(522);
		FEA_CASE(523);
		FEA_CASE(524);
		FEA_CASE(525);
		FEA_CASE(526);
		FEA_CASE(527);
		FEA_CASE(528);
		FEA_CASE(529);
		FEA_CASE(530);
		FEA_CASE(531);
		FEA_CASE(532);
		FEA_CASE(533);
		FEA_CASE(534);
		FEA_CASE(535);
		FEA_CASE(536);
		FEA_CASE(537);
		FEA_CASE(538);
		FEA_CASE(539);
		FEA_CASE(540);
		FEA_CASE(541);
		FEA_CASE(542);
		FEA_CASE(543);
		FEA_CASE(544);
		FEA_CASE(545);
		FEA_CASE(546);
		FEA_CASE(547);
		FEA_CASE(548);
		FEA_CASE(549);
		FEA_CASE(550);
		FEA_CASE(551);
		FEA_CASE(552);
		FEA_CASE(553);
		FEA_CASE(554);
		FEA_CASE(555);
		FEA_CASE(556);
		FEA_CASE(557);
		FEA_CASE(558);
		FEA_CASE(559);
		FEA_CASE(560);
		FEA_CASE(561);
		FEA_CASE(562);
		FEA_CASE(563);
		FEA_CASE(564);
		FEA_CASE(565);
		FEA_CASE(566);
		FEA_CASE(567);
		FEA_CASE(568);
		FEA_CASE(569);
		FEA_CASE(570);
		FEA_CASE(571);
		FEA_CASE(572);
		FEA_CASE(573);
		FEA_CASE(574);
		FEA_CASE(575);
		FEA_CASE(576);
		FEA_CASE(577);
		FEA_CASE(578);
		FEA_CASE(579);
		FEA_CASE(580);
		FEA_CASE(581);
		FEA_CASE(582);
		FEA_CASE(583);
		FEA_CASE(584);
		FEA_CASE(585);
		FEA_CASE(586);
		FEA_CASE(587);
		FEA_CASE(588);
		FEA_CASE(589);
		FEA_CASE(590);
		FEA_CASE(591);
		FEA_CASE(592);
		FEA_CASE(593);
		FEA_CASE(594);
		FEA_CASE(595);
		FEA_CASE(596);
		FEA_CASE(597);
		FEA_CASE(598);
		FEA_CASE(599);
		FEA_CASE(600);
		FEA_CASE(601);
		FEA_CASE(602);
		FEA_CASE(603);
		FEA_CASE(604);
		FEA_CASE(605);
		FEA_CASE(606);
		FEA_CASE(607);
		FEA_CASE(608);
		FEA_CASE(609);
		FEA_CASE(610);
		FEA_CASE(611);
		FEA_CASE(612);
		FEA_CASE(613);
		FEA_CASE(614);
		FEA_CASE(615);
		FEA_CASE(616);
		FEA_CASE(617);
		FEA_CASE(618);
		FEA_CASE(619);
		FEA_CASE(620);
		FEA_CASE(621);
		FEA_CASE(622);
		FEA_CASE(623);
		FEA_CASE(624);
		FEA_CASE(625);
		FEA_CASE(626);
		FEA_CASE(627);
		FEA_CASE(628);
		FEA_CASE(629);
		FEA_CASE(630);
		FEA_CASE(631);
		FEA_CASE(632);
		FEA_CASE(633);
		FEA_CASE(634);
		FEA_CASE(635);
		FEA_CASE(636);
		FEA_CASE(637);
		FEA_CASE(638);
		FEA_CASE(639);
		FEA_CASE(640);
		FEA_CASE(641);
		FEA_CASE(642);
		FEA_CASE(643);
		FEA_CASE(644);
		FEA_CASE(645);
		FEA_CASE(646);
		FEA_CASE(647);
		FEA_CASE(648);
		FEA_CASE(649);
		FEA_CASE(650);
		FEA_CASE(651);
		FEA_CASE(652);
		FEA_CASE(653);
		FEA_CASE(654);
		FEA_CASE(655);
		FEA_CASE(656);
		FEA_CASE(657);
		FEA_CASE(658);
		FEA_CASE(659);
		FEA_CASE(660);
		FEA_CASE(661);
		FEA_CASE(662);
		FEA_CASE(663);
		FEA_CASE(664);
		FEA_CASE(665);
		FEA_CASE(666);
		FEA_CASE(667);
		FEA_CASE(668);
		FEA_CASE(669);
		FEA_CASE(670);
		FEA_CASE(671);
		FEA_CASE(672);
		FEA_CASE(673);
		FEA_CASE(674);
		FEA_CASE(675);
		FEA_CASE(676);
		FEA_CASE(677);
		FEA_CASE(678);
		FEA_CASE(679);
		FEA_CASE(680);
		FEA_CASE(681);
		FEA_CASE(682);
		FEA_CASE(683);
		FEA_CASE(684);
		FEA_CASE(685);
		FEA_CASE(686);
		FEA_CASE(687);
		FEA_CASE(688);
		FEA_CASE(689);
		FEA_CASE(690);
		FEA_CASE(691);
		FEA_CASE(692);
		FEA_CASE(693);
		FEA_CASE(694);
		FEA_CASE(695);
		FEA_CASE(696);
		FEA_CASE(697);
		FEA_CASE(698);
		FEA_CASE(699);
		FEA_CASE(700);
		FEA_CASE(701);
		FEA_CASE(702);
		FEA_CASE(703);
		FEA_CASE(704);
		FEA_CASE(705);
		FEA_CASE(706);
		FEA_CASE(707);
		FEA_CASE(708);
		FEA_CASE(709);
		FEA_CASE(710);
		FEA_CASE(711);
		FEA_CASE(712);
		FEA_CASE(713);
		FEA_CASE(714);
		FEA_CASE(715);
		FEA_CASE(716);
		FEA_CASE(717);
		FEA_CASE(718);
		FEA_CASE(719);
		FEA_CASE(720);
		FEA_CASE(721);
		FEA_CASE(722);
		FEA_CASE(723);
		FEA_CASE(724);
		FEA_CASE(725);
		FEA_CASE(726);
		FEA_CASE(727);
		FEA_CASE(728);
		FEA_CASE(729);
		FEA_CASE(730);
		FEA_CASE(731);
		FEA_CASE(732);
		FEA_CASE(733);
		FEA_CASE(734);
		FEA_CASE(735);
		FEA_CASE(736);
		FEA_CASE(737);
		FEA_CASE(738);
		FEA_CASE(739);
		FEA_CASE(740);
		FEA_CASE(741);
		FEA_CASE(742);
		FEA_CASE(743);
		FEA_CASE(744);
		FEA_CASE(745);
		FEA_CASE(746);
		FEA_CASE(747);
		FEA_CASE(748);
		FEA_CASE(749);
		FEA_CASE(750);
		FEA_CASE(751);
		FEA_CASE(752);
		FEA_CASE(753);
		FEA_CASE(754);
		FEA_CASE(755);
		FEA_CASE(756);
		FEA_CASE(757);
		FEA_CASE(758);
		FEA_CASE(759);
		FEA_CASE(760);
		FEA_CASE(761);
		FEA_CASE(762);
		FEA_CASE(763);
		FEA_CASE(764);
		FEA_CASE(765);
		FEA_CASE(766);
		FEA_CASE(767);
		FEA_CASE(768);
		FEA_CASE(769);
		FEA_CASE(770);
		FEA_CASE(771);
		FEA_CASE(772);
		FEA_CASE(773);
		FEA_CASE(774);
		FEA_CASE(775);
		FEA_CASE(776);
		FEA_CASE(777);
		FEA_CASE(778);
		FEA_CASE(779);
		FEA_CASE(780);
		FEA_CASE(781);
		FEA_CASE(782);
		FEA_CASE(783);
		FEA_CASE(784);
		FEA_CASE(785);
		FEA_CASE(786);
		FEA_CASE(787);
		FEA_CASE(788);
		FEA_CASE(789);
		FEA_CASE(790);
		FEA_CASE(791);
		FEA_CASE(792);
		FEA_CASE(793);
		FEA_CASE(794);
		FEA_CASE(795);
		FEA_CASE(796);
		FEA_CASE(797);
		FEA_CASE(798);
		FEA_CASE(799);
		FEA_CASE(800);
		FEA_CASE(801);
		FEA_CASE(802);
		FEA_CASE(803);
		FEA_CASE(804);
		FEA_CASE(805);
		FEA_CASE(806);
		FEA_CASE(807);
		FEA_CASE(808);
		FEA_CASE(809);
		FEA_CASE(810);
		FEA_CASE(811);
		FEA_CASE(812);
		FEA_CASE(813);
		FEA_CASE(814);
		FEA_CASE(815);
		FEA_CASE(816);
		FEA_CASE(817);
		FEA_CASE(818);
		FEA_CASE(819);
		FEA_CASE(820);
		FEA_CASE(821);
		FEA_CASE(822);
		FEA_CASE(823);
		FEA_CASE(824);
		FEA_CASE(825);
		FEA_CASE(826);
		FEA_CASE(827);
		FEA_CASE(828);
		FEA_CASE(829);
		FEA_CASE(830);
		FEA_CASE(831);
		FEA_CASE(832);
		FEA_CASE(833);
		FEA_CASE(834);
		FEA_CASE(835);
		FEA_CASE(836);
		FEA_CASE(837);
		FEA_CASE(838);
		FEA_CASE(839);
		FEA_CASE(840);
		FEA_CASE(841);
		FEA_CASE(842);
		FEA_CASE(843);
		FEA_CASE(844);
		FEA_CASE(845);
		FEA_CASE(846);
		FEA_CASE(847);
		FEA_CASE(848);
		FEA_CASE(849);
		FEA_CASE(850);
		FEA_CASE(851);
		FEA_CASE(852);
		FEA_CASE(853);
		FEA_CASE(854);
		FEA_CASE(855);
		FEA_CASE(856);
		FEA_CASE(857);
		FEA_CASE(858);
		FEA_CASE(859);
		FEA_CASE(860);
		FEA_CASE(861);
		FEA_CASE(862);
		FEA_CASE(863);
		FEA_CASE(864);
		FEA_CASE(865);
		FEA_CASE(866);
		FEA_CASE(867);
		FEA_CASE(868);
		FEA_CASE(869);
		FEA_CASE(870);
		FEA_CASE(871);
		FEA_CASE(872);
		FEA_CASE(873);
		FEA_CASE(874);
		FEA_CASE(875);
		FEA_CASE(876);
		FEA_CASE(877);
		FEA_CASE(878);
		FEA_CASE(879);
		FEA_CASE(880);
		FEA_CASE(881);
		FEA_CASE(882);
		FEA_CASE(883);
		FEA_CASE(884);
		FEA_CASE(885);
		FEA_CASE(886);
		FEA_CASE(887);
		FEA_CASE(888);
		FEA_CASE(889);
		FEA_CASE(890);
		FEA_CASE(891);
		FEA_CASE(892);
		FEA_CASE(893);
		FEA_CASE(894);
		FEA_CASE(895);
		FEA_CASE(896);
		FEA_CASE(897);
		FEA_CASE(898);
		FEA_CASE(899);
		FEA_CASE(900);
		FEA_CASE(901);
		FEA_CASE(902);
		FEA_CASE(903);
		FEA_CASE(904);
		FEA_CASE(905);
		FEA_CASE(906);
		FEA_CASE(907);
		FEA_CASE(908);
		FEA_CASE(909);
		FEA_CASE(910);
		FEA_CASE(911);
		FEA_CASE(912);
		FEA_CASE(913);
		FEA_CASE(914);
		FEA_CASE(915);
		FEA_CASE(916);
		FEA_CASE(917);
		FEA_CASE(918);
		FEA_CASE(919);
		FEA_CASE(920);
		FEA_CASE(921);
		FEA_CASE(922);
		FEA_CASE(923);
		FEA_CASE(924);
		FEA_CASE(925);
		FEA_CASE(926);
		FEA_CASE(927);
		FEA_CASE(928);
		FEA_CASE(929);
		FEA_CASE(930);
		FEA_CASE(931);
		FEA_CASE(932);
		FEA_CASE(933);
		FEA_CASE(934);
		FEA_CASE(935);
		FEA_CASE(936);
		FEA_CASE(937);
		FEA_CASE(938);
		FEA_CASE(939);
		FEA_CASE(940);
		FEA_CASE(941);
		FEA_CASE(942);
		FEA_CASE(943);
		FEA_CASE(944);
		FEA_CASE(945);
		FEA_CASE(946);
		FEA_CASE(947);
		FEA_CASE(948);
		FEA_CASE(949);
		FEA_CASE(950);
		FEA_CASE(951);
		FEA_CASE(952);
		FEA_CASE(953);
		FEA_CASE(954);
		FEA_CASE(955);
		FEA_CASE(956);
		FEA_CASE(957);
		FEA_CASE(958);
		FEA_CASE(959);
		FEA_CASE(960);
		FEA_CASE(961);
		FEA_CASE(962);
		FEA_CASE(963);
		FEA_CASE(964);
		FEA_CASE(965);
		FEA_CASE(966);
		FEA_CASE(967);
		FEA_CASE(968);
		FEA_CASE(969);
		FEA_CASE(970);
		FEA_CASE(971);
		FEA_CASE(972);
		FEA_CASE(973);
		FEA_CASE(974);
		FEA_CASE(975);
		FEA_CASE(976);
		FEA_CASE(977);
		FEA_CASE(978);
		FEA_CASE(979);
		FEA_CASE(980);
		FEA_CASE(981);
		FEA_CASE(982);
		FEA_CASE(983);
		FEA_CASE(984);
		FEA_CASE(985);
		FEA_CASE(986);
		FEA_CASE(987);
		FEA_CASE(988);
		FEA_CASE(989);
		FEA_CASE(990);
		FEA_CASE(991);
		FEA_CASE(992);
		FEA_CASE(993);
		FEA_CASE(994);
		FEA_CASE(995);
		FEA_CASE(996);
		FEA_CASE(997);
		FEA_CASE(998);
		FEA_CASE(999);
#endif
	default: {
	} break;
	}

	return ret_t();
}

} // namespace detail
} // namespace fea
