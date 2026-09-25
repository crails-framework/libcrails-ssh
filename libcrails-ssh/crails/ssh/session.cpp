#include "session.hpp"
#include "channel.hpp"
#include "scp.hpp"
#include "sftp.hpp"
#include <crails/logger.hpp>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace Crails;
using namespace Crails::Ssh;

Session::Session()
{
  handle = ssh_new();
  if (handle == NULL)
    throw std::runtime_error("Ssh::Session: ssh_new failed");
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
  bool options_ok =
    ssh_options_set(handle, SSH_OPTIONS_HOST,          ip.c_str())   == SSH_OK
 && ssh_options_set(handle, SSH_OPTIONS_PORT_STR,      port.c_str()) == SSH_OK
 && ssh_options_set(handle, SSH_OPTIONS_USER,          user.c_str()) == SSH_OK
 && ssh_options_set(handle, SSH_OPTIONS_LOG_VERBOSITY, &vbs)         == SSH_OK;

  if (!options_ok)
    raise("invalid connection options");
  if (connect_timeout_seconds > 0
   && ssh_options_set(handle, SSH_OPTIONS_TIMEOUT, &connect_timeout_seconds) != SSH_OK)
    raise("invalid connection timeout");
  int con_result = ssh_connect(handle);
  if (con_result != SSH_OK)
  {
    logger << Logger::Error << std::bind(&log_connection_attempt, "connection failed", user, ip, port)
           << ". Error code is:  " << con_result << Logger::endl;
    raise("SSH connection failed");
  }
  is_open = true;
  try { verify_host_key(); }
  catch (...)
  {
    ssh_disconnect(handle);
    is_open = false;
    throw;
  }
  logger << Logger::Debug << std::bind(&log_connection_attempt, "connection opened", user, ip, port) << Logger::endl;
}

void Session::verify_host_key()
{
  switch (ssh_session_is_known_server(handle))
  {
  case SSH_KNOWN_HOSTS_OK:
    return ;
  case SSH_KNOWN_HOSTS_CHANGED:
    raise("host key has changed (possible man-in-the-middle attack)");
  case SSH_KNOWN_HOSTS_OTHER:
    raise("host key type differs from the known one (possible man-in-the-middle attack)");
  case SSH_KNOWN_HOSTS_NOT_FOUND:
  case SSH_KNOWN_HOSTS_UNKNOWN:
    if (!accepts_unknown_hosts)
      raise("unknown host, fingerprint " + get_host_fingerprint());
    if (ssh_session_update_known_hosts(handle) != SSH_OK)
      raise("could not save host to known_hosts");
    logger << Logger::Info << "[ssh] added unknown host to known_hosts, fingerprint "
           << get_host_fingerprint() << Logger::endl;
    return ;
  case SSH_KNOWN_HOSTS_ERROR:
  default:
    raise("host key verification failed");
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
  const char* password_ptr = password.empty()
    ? NULL
    : password.c_str();

  check_auth_result(
    ssh_userauth_publickey_auto(handle, NULL, password_ptr)
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

shared_ptr<Sftp> Session::make_sftp_session()
{
  return make_shared<Sftp>(handle);
}

void Session::raise(const string& message)
{
  std::stringstream stream;

  stream << "Ssh::Session " << message << ": " << ssh_get_error(handle);
  throw std::runtime_error(stream.str());
}

std::string Session::get_error()
{
  return ssh_get_error(handle);
}
