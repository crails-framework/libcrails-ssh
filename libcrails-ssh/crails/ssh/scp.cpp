#include "scp.hpp"
#include <stdexcept>
#include <crails/read_file.hpp>
#include <fstream>

using namespace Crails::Ssh;
using namespace std;

static int scp_mode_to_int(ScpMode mode)
{
  return mode == ReadMode ? SSH_SCP_READ : SSH_SCP_WRITE;
}

Scp::Scp(ssh_session session_handle, const string& path, ScpMode mode) : path(path), session_handle(session_handle)
{
  handle = ssh_scp_new(session_handle, scp_mode_to_int(mode), path.c_str());
  if (handle == NULL)
    throw std::runtime_error("failed to create SCP session for path " + path);
}

Scp::~Scp()
{
  if (is_open)
    close();
  ssh_scp_free(handle);
}

void Scp::open()
{
  int rc = ssh_scp_init(handle);

  if (rc != SSH_OK)
  {
    string strerr = string("Ssh::Scp: failed to open SCP session: ") + ssh_get_error(session_handle);
    throw std::runtime_error(strerr);
  }
  is_open = true;
}

void Scp::close()
{
  ssh_scp_close(handle);
}

void Scp::push_directory(const string& path, int mode)
{
  int rc;

  require_opened_session();
  rc = ssh_scp_push_directory(handle, path.c_str(), mode);
  if (rc != SSH_OK)
    throw std::runtime_error("cannot remotely create directory " + path);
}

void Scp::push_file(const std::string& source, const std::string& target, int mode)
{
  string content;

  if (Crails::read_file(source, content))
    push_text(content, target, mode);
  else
    throw std::runtime_error("Ssh::Scp: cannot read local file " + source);
}

void Scp::push_text(const std::string& content, const std::string& target, int mode)
{
  int rc;

  require_opened_session();
  rc = ssh_scp_push_file(handle, target.c_str(), content.length(), mode);
  if (rc != SSH_OK)
    throw std::runtime_error("Ssh::Scp: cannot open remote file " + target);
  rc = ssh_scp_write(handle, content.c_str(), content.length());
  if (rc != SSH_OK)
    throw std::runtime_error("Ssh::Scp: cannot write remote file " + target);
}

void Scp::pull_file(const std::string& target)
{
  ofstream target_file;

  target_file.open(target.c_str());
  if (target_file.is_open())
  {
    pull_file(target_file);
    target_file.close();
  }
  else
    throw runtime_error("Ssh::Scp: cannot open file " + target);
}

void Scp::pull_file(std::ostream& output)
{
  bool done = false;

  require_opened_session();
  do
  {
    switch (ssh_scp_pull_request(handle))
    {
    default:
      throw runtime_error("Ssh::Scp: ssh_scp_pull_request returned with an unepexcted status");
    case SSH_SCP_REQUEST_NEWDIR:
    case SSH_SCP_REQUEST_ENDDIR:
      throw runtime_error("Ssh::Scp: cannot pull file " + path + ": it's a directory");
    case SSH_ERROR:
      throw runtime_error("Ssh::Scp: cannot pull file " + path);
    case SSH_SCP_REQUEST_NEWFILE:
      {
        int  to_read  = ssh_scp_request_get_size(handle);
        int  read     = 0;
        char buffer[16384];

        while (read < to_read)
        {
          int  r = ssh_scp_read(handle, buffer, sizeof(buffer));

          output.write(buffer, r);
          read += r;
        }
      }
      break ;
    case SSH_SCP_REQUEST_EOF:
      done = true;
      break ;
    }
  } while (!done);
}
