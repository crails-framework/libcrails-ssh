#include "channel.hpp"
#include <crails/logger.hpp>

using namespace Crails;
using namespace Crails::Ssh;
using namespace std;

static const std::size_t buffer_size = 4096;
static const int poll_slice_ms = 100;

static inline ExitStatus log_and_return(ExitStatus status)
{
  logger << Logger::Debug << "Ssh::Channel command ended: ";
  if (status.was_timed_out())
    logger << "timed out";
  else if (!status.has_exit_status())
    logger << "could not retrieve exit status";
  else if (status.was_dumped())
    logger << "core dump";
  else if (status.has_signal())
    logger << "received signal " << status.get_signal();
  else
    logger << "code=" << status.get_code();
  logger << Logger::endl;
  return status;
}

inline bool Channel::is_within_time_limits(const clock::time_point& started, const clock::time_point& last_output)
{
  const auto now = clock::now();

  if ((deadline_ms > 0 && now - started     > chrono::milliseconds(deadline_ms))
   || (timeout_ms  > 0 && now - last_output > chrono::milliseconds(timeout_ms)))
  {
    ssh_channel_close(handle);
    return false;
  }
  return true;
}

ExitStatus Channel::read(function<void(char)> output)
{
  const clock::time_point started = clock::now();
  clock::time_point       last_output = started;
  char                    buffer[buffer_size];

  while (true)
  {
    bool received_data = false;

    if (!is_within_time_limits(started, last_output))
      return log_and_return(ExitStatus::on_time_out());
    for (InputType type : {Stdout, Stderr})
    {
      const int is_stderr = type == Stderr ? 1 : 0;
      int bytes_read = ssh_channel_read_timeout(handle, buffer, buffer_size, is_stderr, poll_slice_ms);

      if (bytes_read == SSH_ERROR)
      {
        logger << Logger::Error << "Ssh::Channel: connection error while reading command output" << Logger::endl;
        return log_and_return(ExitStatus());
      }
      if (bytes_read > 0)
      {
        currently_reading = type;
        for (int i = 0 ; i < bytes_read ; ++i)
          output(buffer[i]);
        received_data = true;
        last_output = clock::now();
      }
    }
    if (!received_data && (ssh_channel_is_eof(handle) || ssh_channel_is_closed(handle)))
      break ;
  }
  return log_and_return(ExitStatus(handle));
}

ExitStatus Channel::exec(const string& command, function<void(char)> output)
{
  int rc;

  logger << Logger::Debug << "Ssh::Channel: running command: `" << command << '`' << Logger::endl;
  rc = ssh_channel_request_exec(handle, command.c_str());
  if (rc == SSH_OK)
  {
    ssh_channel_send_eof(handle); // broadcast non-interactiveness, command reading stdin won't hang
    return read(output);
  }
  logger << Logger::Error << "Ssh::Channel: ssh_channel_request_exec returned with status " << rc << Logger::endl;
  return log_and_return(ExitStatus());
}

Channel::~Channel()
{
  if (handle)
    ssh_channel_free(handle);
}
