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
  int dumped_as_int = 0;
  int result;

#ifndef USE_OLD_SSH_EXIT_STATUS
  result = ssh_channel_get_exit_state(handle, &code, &signal, &dumped_as_int);
  dumped = dumped_as_int != 0;
  if (result == SSH_OK)
    retrieved = true;
#else
  code = ssh_channel_get_exit_status(handle);
  retrieved = true;
#endif
}

ExitStatus::ExitStatus(ExitStatus&& other)
{
  retrieved = other.retrieved;
  code = other.code;
  std::swap(signal, other.signal);
  dumped = other.dumped;
}

ExitStatus& ExitStatus::operator=(ExitStatus&& other)
{
  retrieved = other.retrieved;
  code = other.code;
  std::swap(signal, other.signal);
  dumped = other.dumped;
  return *this;
}

ExitStatus::~ExitStatus()
{
#ifndef USE_OLD_SSH_EXIT_STATUS
  if (signal)
    ssh_string_free_char(signal);
#endif
}

string_view ExitStatus::get_signal() const
{
  if (signal)
    return string_view(signal, strlen(signal));
  return string_view();
}
