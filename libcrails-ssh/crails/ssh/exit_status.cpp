#include "channel.hpp"
#include <utility>
#include <string.h>
#if LIBSSH_VERSION_MAJOR == 0 && LIBSSH_VERSION_MINOR < 11
# define USE_OLD_SSH_EXIT_STATUS
#endif

using namespace Crails::Ssh;
using namespace std;

ExitStatus::ExitStatus(ssh_channel handle)
{
#ifndef USE_OLD_SSH_EXIT_STATUS
  int dumped_as_int = 0;
  int result = ssh_channel_get_exit_state(handle, &code, &signal, &dumped_as_int);

  if (result == SSH_OK)
  {
    dumped = dumped_as_int != 0;
    retrieved = true;
  }
  else
  {
    release();
    code = 0;
  }
#else
  int status = ssh_channel_get_exit_status(handle);

  if (status != SSH_ERROR)
  {
    code = static_cast<uint32_t>(status);
    retrieved = true;
  }
#endif
}

ExitStatus ExitStatus::on_time_out()
{
  ExitStatus status;
  status.timed_out = true;
  return status;
}

ExitStatus::ExitStatus(ExitStatus&& other) noexcept
{
  *this = std::move(other);
}

ExitStatus& ExitStatus::operator=(ExitStatus&& other) noexcept
{
  if (this != &other)
  {
    release();
    retrieved = other.retrieved;
    code      = other.code;
    dumped    = other.dumped;
    timed_out = other.timed_out;
    signal    = other.signal;
    other.signal = nullptr;
  }
  return *this;
}

ExitStatus::~ExitStatus()
{
  release();
}

void ExitStatus::release()
{
#ifndef USE_OLD_SSH_EXIT_STATUS
  if (signal)
    ssh_string_free_char(signal);
#endif
  signal = nullptr;
}

string_view ExitStatus::get_signal() const
{
  if (signal)
    return string_view(signal, strlen(signal));
  return string_view();
}
