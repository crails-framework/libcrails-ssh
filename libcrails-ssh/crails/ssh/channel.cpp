#include "channel.hpp"
#include <crails/logger.hpp>

using namespace Crails;
using namespace Crails::Ssh;
using namespace std;

static const std::size_t buffer_size = 256;

int Channel::poll(char* buffer)
{
  int bytes_read = poll(buffer, Stdout);

  if (bytes_read == 0 && !ssh_channel_is_eof(handle))
    bytes_read = poll(buffer, Stderr);
  return bytes_read;
}

int Channel::poll(char* buffer, InputType type)
{
  int stream_id = type == Stdout ? 0 : 1;

  currently_reading = type;
  return ssh_channel_read_timeout(handle, buffer, buffer_size, stream_id, timeout_ms);
}

int Channel::exec(const string& command, function<void(char)> output)
{
  bool is_eof = false;
  char buffer[buffer_size];
  int  bytes_read;
  int rc = ssh_channel_request_exec(handle, command.c_str());

  logger << Logger::Debug << "Ssh::Channel: running command: `" << command << '`' << Logger::endl;
  if (rc == SSH_OK)
  {
    while (ssh_channel_is_open(handle))
    {
      bytes_read = poll(buffer);
      if (bytes_read < 0)
      {
        logger << Logger::Error << "Ssh::Channel: ssh_channel_read_timeout returned: " << bytes_read << Logger::endl;
        is_eof = ssh_channel_is_eof(handle);
      }
      if (bytes_read > 0)
      {
        for (int i = 0 ; i < bytes_read ; ++i)
          output(buffer[i]);
      }
      is_eof = ssh_channel_is_eof(handle);
      if (is_eof || bytes_read == 0)
      {
        if (!is_eof)
          logger << Logger::Error << "Ssh::Channel: ssh_channel_read_timeout timed out" << Logger::endl;
	else
          logger << Logger::Debug << "Ssh::Channel: ssh_channel_is_eof returns true" << Logger::endl;
        break ;
      }
    }
  }
  else
    logger << Logger::Error << "Ssh::Channel: ssh_channel_request_exec returned with status " << rc << Logger::endl;
  logger << Logger::Debug << "Ssh::Channel ssh_channel_request_exec ended" << Logger::endl;
  return is_eof ? ssh_channel_get_exit_status(handle) : -1;
}

Channel::~Channel()
{
  if (handle)
  {
    ssh_channel_close(handle);
    ssh_channel_free(handle);
  }
}
