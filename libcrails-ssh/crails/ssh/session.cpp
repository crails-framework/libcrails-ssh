#include "session.hpp"
#include "channel.hpp"
#include "scp.hpp"
#include <crails/logger.hpp>

using namespace std;
using namespace Crails;
using namespace Crails::Ssh;

Session::Session()
{
  handle = ssh_new();
  ssh_set_blocking(handle, 1);
}

Session::~Session()
{
  logger << Logger::Debug << "Closing ssh session" << Logger::endl;
  if (handle != NULL)
    ssh_free(handle);
}

void Session::connect(const string& user, const string& ip, const string& port)
{
  ssh_options_set(handle, SSH_OPTIONS_HOST,          ip.c_str());
  ssh_options_set(handle, SSH_OPTIONS_PORT_STR,      port.c_str());
  ssh_options_set(handle, SSH_OPTIONS_USER,          user.c_str());
  ssh_options_set(handle, SSH_OPTIONS_LOG_VERBOSITY, &vbs);
  int con_result = ssh_connect(handle);
  if (con_result != SSH_OK)
  {
    logger << Logger::Error << "SSH connection failed. Error code is:  " << con_result << Logger::endl;
    raise("SSH connection failed");
  }
  else
    logger << Logger::Debug << "SSH connection opened to " << user << '@' << ip << ':' << port << Logger::endl;
}

static void check_auth_result(Session& session, int auth_result)
{
  if (auth_result != SSH_AUTH_SUCCESS)
  {
    logger << Logger::Error << "SSH authentication failed. Error code is:  " << auth_result << Logger::endl;
    session.raise("SSH authentication failed");
  }
  else
    logger << Logger::Debug << "SSH authentication success" << Logger::endl;
}

void Session::authentify_with_password(const string& password)
{
  check_auth_result(
    *this, ssh_userauth_password(handle, NULL, password.c_str())
  );
}

void Session::authentify_with_pubkey(const string& password)
{
  check_auth_result(
    *this, ssh_userauth_publickey_auto(handle, NULL, password.c_str())
  );
}

shared_ptr<Channel> Session::make_channel(int read_timeout)
{
  auto channel = make_shared<Channel>();

  channel->handle = ssh_channel_new(handle);
  channel->timeout_ms = read_timeout;
  if (channel == NULL)
    raise("Failed to create SSH channel");
  ssh_channel_open_session(channel->handle);
  return channel;
}

shared_ptr<Scp> Session::make_scp_session(const string& path, ScpMode mode)
{
  return make_shared<Scp>(handle, path, mode);
}

void Session::raise(const string& message)
{
  std::stringstream stream;

  stream << "Ssh::Session " << message << ": " << ssh_get_error(handle);
  throw std::runtime_error(stream.str().c_str());
}

std::string Session::get_error()
{
  return ssh_get_error(handle);
}
