#include "file_based_authenticator.hpp"

namespace fz::authentication
{

impersonation_token file_based_authenticator::users::impersonator::msw::get_token() const
{
#if defined(FZ_WINDOWS)
	if (enabled)
		return impersonation_token(name, password);
#endif

	return {};
}

impersonation_token file_based_authenticator::users::impersonator::nix::get_token() const
{
#if !defined(FZ_WINDOWS)
	if (enabled) {
		impersonation_options opts;
		opts.group = group;
		return impersonation_token(name, impersonation_options::pwless, get_null_logger(), opts);
	}
#endif

	return {};
}

file_based_authenticator::users::impersonator::nix *file_based_authenticator::users::impersonator::any::nix()
{
	return std::get_if<impersonator::nix>(this);
}

file_based_authenticator::users::impersonator::msw *file_based_authenticator::users::impersonator::any::msw()
{
	return std::get_if<impersonator::msw>(this);
}

file_based_authenticator::users::impersonator::native *file_based_authenticator::users::impersonator::any::native()
{
	return std::get_if<impersonator::native>(this);
}

impersonation_token file_based_authenticator::users::impersonator::any::get_token() const
{
	return std::visit([](const auto &i) { return i.get_token(); }, static_cast<const variant &>(*this));
}

}
