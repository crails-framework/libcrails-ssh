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
  {
    if (is_open)
      ssh_disconnect(handle);
    ssh_free(handle);
  }
}

static string log_connection_attempt(const char* state, const string& user, const string& ip, const string& port)
{
  return string("[ssh] ") + state + " with " + user + '@' + ip + ':' + port;
}

void Session::connect(const string& user, const string& ip, const string& port)
{
  ssh_options_set(handle, SSH_OPTIONS_HOST,          ip.c_str());
  ssh_options_set(handle, SSH_OPTIONS_PORT_STR,      port.c_str());
  ssh_options_set(handle, SSH_OPTIONS_USER,          user.c_str());
  ssh_options_set(handle, SSH_OPTIONS_LOG_VERBOSITY, &vbs);
  if (connect_timeout_seconds > 0)
    ssh_options_set(handle, SSH_OPTIONS_TIMEOUT, &connect_timeout_seconds);
  int con_result = ssh_connect(handle);
  if (con_result != SSH_OK)
  {
    logger << Logger::Error << std::bind(&log_connection_attempt, "connection failed", user, ip, port)
           << ". Error code is:  " << con_result << Logger::endl;
    raise("SSH connection failed");
  }
  else
  {
    is_open = true;
    logger << Logger::Debug << std::bind(&log_connection_attempt, "connection opened", user, ip, port) << Logger::endl;
  }
}

string Session::get_host_fingerprint()
{
  ssh_key        key = nullptr;
  unsigned char* hash = nullptr;
  size_t         hash_length = 0;
  string         result;

  if (is_open && ssh_get_server_publickey(handle, &key) == SSH_OK)
  {
    if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &hash_length) == SSH_OK)
    {
      char* text = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hash_length);

      if (text)
      {
        result = text;
        ssh_string_free_char(text);
      }
      ssh_clean_pubkey_hash(&hash);
    }
    ssh_key_free(key);
  }
  return result;
}

inline void Session::check_auth_result(int auth_result)
{
  if (auth_result != SSH_AUTH_SUCCESS)
  {
    logger << Logger::Error << "[ssh] authentication failed. Error code is:  " << auth_result << Logger::endl;
    raise("SSH authentication failed");
  }
  else
    logger << Logger::Debug << "[ssh] authentication success" << Logger::endl;
}

void Session::authentify_with_password(const string& password)
{
  check_auth_result(
    ssh_userauth_password(handle, NULL, password.c_str())
  );
}

void Session::authentify_with_pubkey(const string& password)
{
  check_auth_result(
    ssh_userauth_publickey_auto(handle, NULL, password.c_str())
  );
}

shared_ptr<Channel> Session::make_channel(int read_timeout)
{
  auto channel = make_shared<Channel>();

  channel->handle = ssh_channel_new(handle);
  channel->timeout_ms = read_timeout;
  if (channel->handle == NULL)
    raise("Failed to create SSH channel");
  if (ssh_channel_open_session(channel->handle) != SSH_OK)
    raise("Failed to open SSH channel");
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
