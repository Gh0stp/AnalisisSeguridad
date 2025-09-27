#include "user.hpp"

#include "impersonator/client.hpp"

namespace fz::authentication {

bool subscribe(shared_user &su, event_handler &eh)
{
	if (su) {
		if (auto d = std::get_deleter<shared_user_deleter>(su)) {
			auto lock = su->lock();

			return d->handlers_.emplace(&eh).second;
		}
	}

	return false;
}

bool unsubscribe(shared_user &su, event_handler &eh)
{
	if (su) {
		if (auto d = std::get_deleter<shared_user_deleter>(su)) {
			auto lock = su->lock();

			return d->handlers_.erase(&eh);
		}
	}

	return false;
}

bool notify(shared_user &su)
{
	if (su) {
		if (auto d = std::get_deleter<shared_user_deleter>(su)) {
			auto lock = su->lock();

			d->notifications_count_ += 1;

			for (auto eh: d->handlers_)
				eh->send_event<shared_user_changed_event>(su);


			return true;
		}
	}

	return false;
}

std::size_t notifications_count(shared_user &su)
{
	if (su) {
		if (auto d = std::get_deleter<shared_user_deleter>(su)) {
			auto lock = su->lock();

			return d->notifications_count_;
		}
	}

	return 0;
}


std::size_t number_of_subscribers(shared_user &su)
{
	if (su) {
		if (auto d = std::get_deleter<shared_user_deleter>(su)) {
			auto lock = su->lock();
			return d->handlers_.size();
		}
	}

	return 0;
}

}
