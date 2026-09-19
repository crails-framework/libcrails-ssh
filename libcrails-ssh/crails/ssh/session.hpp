#ifndef  SSH_SESSION_HPP
# define SSH_SESSION_HPP

# include <libssh/libssh.h>
# include <memory>
# include <string>
# include <chrono>
# include "scp.hpp"
# include "channel.hpp"

#ifndef DEFAULT_SSH_READ_TIMEOUT
# define DEFAULT_SSH_READ_TIMEOUT 30000
#endif

namespace Crails
{
  namespace Ssh
  {
    struct Session
    {
      friend class Channel;
      ssh_session handle;
      int vbs = SSH_LOG_RARE;
      bool accepts_unknown_hosts = false;
      bool is_open = false;
      long connect_timeout_seconds = 0;
    public:
      Session();
      Session(const Session&) = delete;
      Session& operator=(const Session&) = delete;
      ~Session();

      void should_accept_unknown_hosts(bool val) { accepts_unknown_hosts = val; } // TODO: not implemented ?

      void                     set_verbosity(int value) { vbs = value; }
      void                     set_connect_timeout(std::chrono::seconds value) { connect_timeout_seconds = value.count(); }
      bool                     is_connected() const { return is_open; }
      std::string              get_host_fingerprint(); // SHA256 fingerprint, like `ssh-keygen -l`
      void                     connect(const std::string& user, const std::string& ip, const std::string& port = "22");
      void                     authentify_with_password(const std::string& password);
      void                     authentify_with_pubkey(const std::string& password = "");

      std::shared_ptr<Channel> make_channel(int read_timeout = DEFAULT_SSH_READ_TIMEOUT);
      std::shared_ptr<Scp>     make_scp_session(const std::string& path, ScpMode mode);

      std::string get_error();

      template<typename STREAM>
      int exec(const std::string& command, STREAM& output, int read_timeout = DEFAULT_SSH_READ_TIMEOUT)
      {
        return make_channel(read_timeout)->exec(command, output);
      }

      template<typename STREAM>
      int exec(const std::string& command, STREAM& output, std::chrono::milliseconds read_timeout)
      {
        return exec(command, output, read_timeout.count());
      }

    private:
      void raise(const std::string& message);
      void check_auth_result(int auth_result);
    };
  }
}

#endif
