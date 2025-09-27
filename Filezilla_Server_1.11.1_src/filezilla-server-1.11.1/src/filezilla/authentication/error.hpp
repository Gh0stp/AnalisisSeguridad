#ifndef FZ_AUTHENTICATION_ERROR_HPP
#define FZ_AUTHENTICATION_ERROR_HPP

#include <libfilezilla/string.hpp>

#include <cstdint>

namespace fz::authentication
{

struct error
{
	enum category: std::uint16_t {
		permanent_error = 0x0100,
		caused_by_user  = 0x0200,
		internal_error  = 0x0400,
	};

	enum type: std::uint16_t {
		none,

		user_disabled    = 1 | permanent_error | caused_by_user,
		user_nonexisting = 2 | permanent_error | caused_by_user,
		ip_disallowed    = 3 | permanent_error | caused_by_user,
		realm_disabled   = 4 | permanent_error | caused_by_user,

		invalid_credentials       = 5 | caused_by_user,
		auth_method_not_supported = 6 | caused_by_user,

		user_quota_reached = 7,

		internal = 8 | permanent_error | internal_error
	};

	constexpr error(type v = none) noexcept
		: v_(v)
	{}

	constexpr explicit operator bool() const noexcept
	{
		return v_ != none;
	}

	constexpr operator type() const noexcept
	{
		return v_;
	}

	constexpr bool is_internal() const noexcept
	{
		return v_ & std::uint16_t(internal_error);
	}

	constexpr bool is_user_fault() const noexcept
	{
		return v_ & std::uint16_t(caused_by_user);
	}

	constexpr bool is_permament() const noexcept
	{
		return v_ & std::uint16_t(permanent_error);
	}

	template <typename String>
	friend String toString(const error &e)
	{
		using C = typename String::value_type;

		switch (e.v_) {
			case none: return fzS(C, "No error");
			case user_quota_reached: return fzS(C, "User quota reached");
			case user_disabled: return fzS(C, "User is disabled");
			case user_nonexisting: return fzS(C, "User does not exist");
			case ip_disallowed: return fzS(C, "IP is not allowed");
			case auth_method_not_supported: return fzS(C, "Auth method is not supported");
			case realm_disabled: return fzS(C, "Auth realm is disabled");
			case invalid_credentials: return fzS(C, "Invalid credentials");
			case internal: return fzS(C, "Internal error");
		}

		return fzS(C, "Unknown error");
	}

private:
	type v_;
};

}


#endif // FZ_AUTHENTICATION_ERROR_HPP
