#include "archives.hpp"

namespace fz::impersonator {

thread_local output_archive *output_archive::instance_{};
thread_local input_archive *input_archive::instance_{};

}

namespace fz::serialization {

void load(impersonator::input_archive &ar, tvfs::fd_owner &fd)
{
	fd.reset();

	if (bool has_fd = false; ar(has_fd) && has_fd) {
		if (ar.fds_buf_.empty()) {
			ar.error(EINVAL);
			return;
		}

		fd = std::move(ar.fds_buf_.front());
		ar.fds_buf_.pop_front();
	}
}

void save(impersonator::output_archive &ar, tvfs::fd_owner &fd)
{
	if (ar.out_fd_) {
		// We only allow one fd to be present in each serialization;
		ar.error(EEXIST);
		return;
	}

	ar.out_fd_ = std::move(fd);
	ar(ar.out_fd_.is_valid());
}

void load(serialization::binary_input_archive &ar, tvfs::fd_owner &fd)
{
	if (&ar == impersonator::input_archive::instance_) {
		load(*impersonator::input_archive::instance_, fd);
	}
	else {
		ar.error(EINVAL);
	}
}

void save(serialization::binary_output_archive &ar, tvfs::fd_owner &fd)
{
	if (&ar == impersonator::output_archive::instance_) {
		save(*impersonator::output_archive::instance_, fd);
	}
	else {
		ar.error(EINVAL);
	}
}

}
