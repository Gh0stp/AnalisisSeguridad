#ifndef FZ_SERIALIZATION_TYPES_COMMON_HPP
#define FZ_SERIALIZATION_TYPES_COMMON_HPP

#include "../trait.hpp"
#include "../helpers.hpp"

namespace fz::serialization
{
	namespace detail
	{
		template <typename T, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
		struct force_enum
		{
			explicit force_enum(T v) noexcept
				: value_(v)
			{}

			template <typename U, std::enable_if_t<std::is_same_v<U, T>>* = nullptr>
			operator U() const noexcept
			{
				return value_;
			}

		private:
			T value_;
		};

	}

	namespace trait
	{

		template <typename T, typename SFINAEHelper = void>
		struct has_enum_to_string: std::false_type{};

		template <typename T>
		struct has_enum_to_string<T, std::void_t<decltype(enum_to_string(std::declval<detail::force_enum<T>>(), std::declval<std::string &>()) == bool())>>: std::true_type{};

		template <typename T>
		inline constexpr bool has_enum_to_string_v = has_enum_to_string<T>::value;

		template <typename T, typename SFINAEHelper = void>
		struct has_string_to_enum: std::false_type{};

		template <typename T>
		struct has_string_to_enum<T, std::void_t<decltype(string_to_enum(std::declval<std::string>(), std::declval<T&>()) == bool())>>: std::true_type{};

		template <typename T>
		inline constexpr bool has_string_to_enum_v = has_string_to_enum<T>::value;

	}

	//! Save enums
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E> && !(trait::is_textual_v<Archive> && trait::has_enum_to_string_v<E>)>* = nullptr>
	auto save_minimal(const Archive &, const E &e, typename Archive::error_t &) {
		return std::underlying_type_t<E>(e);
	}

	//! Load enums
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E> && !(trait::is_textual_v<Archive> && trait::has_enum_to_string_v<E>)>* = nullptr>
	void load_minimal(const Archive &, E &e, const std::underlying_type_t<E> &v, typename Archive::error_t &) {
		e = static_cast<E>(v);
	}

	//! Save enums on text archives
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E> && trait::is_textual_v<Archive> && trait::has_enum_to_string_v<E>>* = nullptr>
	std::string save_minimal(const Archive &ar, const E &e, typename Archive::error_t &err) {
		std::string ret;

		if (!enum_to_string(detail::force_enum<E>(e), ret)) {
			err = ar.error(ERANGE);
		}

		return ret;
	}

	//! Load enums from text archives
	template <typename Archive, typename E, std::enable_if_t<std::is_enum_v<E> && trait::is_textual_v<Archive> && trait::has_string_to_enum_v<E>>* = nullptr>
	void load_minimal(const Archive &ar, E &e, const std::string &v, typename Archive::error_t &err) {
		if (!string_to_enum(v, e)) {
			err = ar.error(ERANGE);
		}
	}

	//! Save types aliased to enums
	template <typename Archive, typename T, std::enable_if_t<std::is_enum_v<typename T::serialization_alias> && std::is_convertible_v<typename T::serialization_alias, T>>* = nullptr>
	auto save_minimal(const Archive &ar, const T &t, typename Archive::error_t &err) {
		return save_minimal(ar, static_cast<typename T::serialization_alias>(t), err);
	}

	//! Load types aliased to enums
	template <typename Archive, typename T, std::enable_if_t<std::is_enum_v<typename T::serialization_alias> && std::is_convertible_v<T, typename T::serialization_alias>>* = nullptr>
	void load_minimal(const Archive &ar, T &t, decltype(save_minimal(ar, t, std::declval<typename Archive::error_t&>())) v, typename Archive::error_t &err) {
		typename T::serialization_alias u{};
		load_minimal(ar, u, v, err);
		t = static_cast<T>(u);
	}

}

#endif // FZ_SERIALIZATION_TYPES_COMMON_HPP
