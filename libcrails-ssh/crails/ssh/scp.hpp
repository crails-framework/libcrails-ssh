#ifndef  SSH_SCP_HPP
# define SSH_SCP_HPP

# include <libssh/libssh.h>
# include <sys/stat.h>
# include <string>

namespace Crails
{
  namespace Ssh
  {
    enum ScpMode
    {
      WriteMode,
      ReadMode
    };

    class Scp
    {
      const std::string path;
    public:
      Scp(ssh_session session_handle, const std::string& path, ScpMode mode);
      ~Scp();

      void open();
      void close();

      void push_directory(const std::string& path, int mode = S_IRWXU);
      void push_file(const std::string& source, const std::string& target, int mode = S_IRUSR | S_IWUSR);
      void push_text(const std::string& text,   const std::string& target, int mode = S_IRUSR | S_IWUSR);

      void pull_file(std::ostream&);
      void pull_file(const std::string& target);

    private:
      inline void require_opened_session() { if (!is_open) { open(); } }

      ssh_scp handle;
      ssh_session session_handle;
      bool is_open = false;
    };
  }
}

#endif
