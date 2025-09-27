#ifndef FZ_ENUM_NAMES_HPP
#define FZ_ENUM_NAMES_HPP

#include <optional>
#include <string>

#include <libfilezilla/string.hpp>

#include "util/parser.hpp"
#include "preprocessor/str.hpp"
#include "transformed_view.hpp"
#include "enum_bitops.hpp"

namespace fz::enum_names
{

template <typename T, typename IBegin, typename IEnd, typename SBegin, typename SEnd>
inline constexpr std::optional<std::string_view> int_to_string(T v, std::false_type /*no bitops*/, IBegin ibegin, IEnd iend, SBegin sbegin, SEnd send)
{
	static_assert(std::is_integral_v<T>, "T must be an integral type");

	while (ibegin != iend && sbegin != send) {
		if (v == *ibegin) {
			return *sbegin;
		}

		++ibegin;
		++sbegin;
	}

	return {};
}

template <typename T, typename IBegin, typename IEnd, typename SBegin, typename SEnd>
inline constexpr std::optional<T> string_to_int(std::string_view s, std::false_type /*no bitops*/, IBegin ibegin, IEnd iend, SBegin sbegin, SEnd send)
{
	static_assert(std::is_integral_v<T>, "T must be an integral type");

	// In case the string is actually a number...
	util::parseable_range r(s);
	if (T tmp = {}; parse_int(r, tmp, 10) && eol(r)) {
		for (auto it = ibegin; it != iend; ++it) {
			if (tmp == *it) {
				return tmp;
			}
		}

		return {};
	}

	// Not a number
	while (ibegin != iend && sbegin != send) {
		if (s == *sbegin) {
			return *ibegin;
		}

		++ibegin;
		++sbegin;
	}

	return {};
}

template <typename T, typename IBegin, typename IEnd, typename SBegin, typename SEnd>
inline std::optional<std::string> int_to_string(T v, std::true_type /*has bitops*/, IBegin ibegin, IEnd iend, SBegin sbegin, SEnd send)
{
	static_assert(std::is_integral_v<T>, "T must be an integral type");

	std::string ret;

	while (ibegin != iend && sbegin != send) {
		if ((v & *ibegin) == *ibegin) {
			if (!ret.empty()) {
				ret.append(1, '|');
			}
			v &= ~T{*ibegin};

			ret.append(*sbegin);
		}

		++ibegin;
		++sbegin;
	}

	if (ret.empty() && v == T{}) {
		// The enum has no entry for the 0 case, so return it a string.
		return "0";
	}

	if (v != T{}) {
		// We couldn't convert at least one of the enums
		return std::nullopt;
	}

	return ret;
}

template <typename T, typename IBegin, typename IEnd, typename SBegin, typename SEnd>
inline constexpr std::optional<T> string_to_int(std::string_view s, std::true_type /*has bitops*/, IBegin ibegin, IEnd iend, SBegin sbegin, SEnd send)
{
	static_assert(std::is_integral_v<T>, "T must be an integral type");

	// In case the string is actually a number...
	util::parseable_range r(s);
	if (T tmp = {}; parse_int(r, tmp, 10) && eol(r)) {
		T ret = tmp;

		for (auto it = ibegin; it != iend; ++it) {
			if (auto v = *it; (tmp & v) == v) {
				tmp &= ~v;
			}
		}

		if (tmp == T{}) {
			// We matched all bits
			return ret;
		}

		// Some bits didn't match
		return {};
	}

	// Not a number
	T ret{};

	for (auto t: strtokenizer(s, "|+ ", false)) {
		bool found = false;

		auto iit = ibegin;
		auto sit = sbegin;
		while (iit != iend && sit != send) {
			if (t == *sit) {
				ret |= *iit;
				found = true;
				break;
			}

			++iit;
			++sit;
		}

		if (!found) {
			return std::nullopt;
		}
	}

	return ret;
}

template <typename T, typename... Ts>
inline constexpr auto to_underlying(T v, Ts... vs)
{
	using U = std::underlying_type_t<T>;

	return std::array<U, 1+sizeof...(Ts)>{U(v), U(vs)...};
}

inline constexpr auto extract_names(std::string_view strings)
{
	return transformed_view(strtokenizer(std::move(strings), " ,", true), [](std::string_view v) constexpr {
		if (auto pos = v.find_last_of(":"); pos != std::string_view::npos) {
			return v.substr(pos+1);
		}

		return v;
	});
}

template <typename T, std::size_t N, typename HasBitops>
inline constexpr std::optional<std::conditional_t<HasBitops{}, std::string, std::string_view>> int_to_string(T v, HasBitops has_bitops, const std::array<T, N> &values, std::string_view strings)
{
	auto names = extract_names(strings);

	if (auto ret = int_to_string(v, has_bitops, std::begin(values), std::end(values), std::begin(names), std::end(names))) {
		return *ret;
	}

	return {};
}

template <typename T, std::size_t N, typename HasBitops>
inline constexpr std::optional<T> string_to_int(std::string_view s, HasBitops has_bitops, const std::array<T, N> &values, std::string_view strings)
{
	auto names = extract_names(strings);

	if (auto ret = string_to_int<T>(s, has_bitops, std::begin(values), std::end(values), std::begin(names), std::end(names))) {
		return *ret;
	}

	return {};
}

}

#define FZ_ENUM_NAMES_DEFINE_FOR(...) FZ_ENUM_NAMES_DEFINE_( , __VA_ARGS__)
#define FZ_ENUM_NAMES_FRIEND_DEFINE_FOR(...) FZ_ENUM_NAMES_DEFINE_(friend, __VA_ARGS__)

#define FZ_ENUM_NAMES_DEFINE_(F, ...)                                                                                                                                                                                                                    \
	inline F bool enum_to_string(decltype(__VA_ARGS__) fz_enum_value_, std::string &fz_enum_string_) {                                                                                                                                                   \
		if (auto fz_ret_ = ::fz::enum_names::int_to_string(std::underlying_type_t<decltype(__VA_ARGS__)>(fz_enum_value_), FZ_ENUM_BITOPS_IS_DEFINED_FOR(decltype(__VA_ARGS__)), ::fz::enum_names::to_underlying(__VA_ARGS__), FZ_PP_STR(__VA_ARGS__))) { \
			fz_enum_string_ = *fz_ret_;                                                                                                                                                                                                                  \
			return true;                                                                                                                                                                                                                                 \
		}                                                                                                                                                                                                                                                \
																																																														 \
		return false;                                                                                                                                                                                                                                    \
	}                                                                                                                                                                                                                                                    \
																																																														 \
	inline F bool string_to_enum(const std::string &fz_enum_string_, decltype(__VA_ARGS__) &fz_enum_value_) {                                                                                                                                            \
		if (auto fz_ret_ = ::fz::enum_names::string_to_int(fz_enum_string_, FZ_ENUM_BITOPS_IS_DEFINED_FOR(decltype(__VA_ARGS__)), ::fz::enum_names::to_underlying(__VA_ARGS__), FZ_PP_STR(__VA_ARGS__))) { \
			fz_enum_value_ = static_cast<decltype(__VA_ARGS__)>(*fz_ret_);                                                                                                                                 \
			return true;                                                                                                                                                                                   \
		}                                                                                                                                                                                                  \
																																																		   \
		return false;                                                                                                                                                                                      \
	}                                                                                                                                                                                                      \
/***/

#endif // FZ_ENUM_NAMES_HPP
