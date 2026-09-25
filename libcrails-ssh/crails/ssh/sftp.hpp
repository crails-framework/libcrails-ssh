#ifndef  SSH_SFTP_HPP
# define SSH_SFTP_HPP

# include <libssh/libssh.h>
# include <libssh/sftp.h>
# include <sys/stat.h>
# include <cstdint>
# include <iosfwd>
# include <string>
# include <vector>

namespace Crails
{
  namespace Ssh
  {
    struct SftpEntry
    {
      std::string   name;
      bool          is_directory;
      std::uint64_t size;
    };

    class Sftp
    {
    public:
      explicit Sftp(ssh_session session_handle);
      Sftp(const Sftp&) = delete;
      Sftp& operator=(const Sftp&) = delete;
      ~Sftp();

      void push_file(const std::string& local_source, const std::string& remote_target, int mode = S_IRUSR | S_IWUSR);
      void push_text(const std::string& text,         const std::string& remote_target, int mode = S_IRUSR | S_IWUSR);
      void push_stream(std::istream& input,           const std::string& remote_target, int mode = S_IRUSR | S_IWUSR);
      void push_directory(const std::string& remote_path, int mode = S_IRWXU); // no error if it already exists

      void        pull_file(const std::string& remote_source, std::ostream& output);
      void        pull_file(const std::string& remote_source, const std::string& local_target);
      std::string pull_text(const std::string& remote_source);

      bool                   exists(const std::string& remote_path);
      bool                   is_directory(const std::string& remote_path);
      std::vector<SftpEntry> list_directory(const std::string& remote_path);
      void                   remove_file(const std::string& remote_path);
      void                   remove_directory(const std::string& remote_path);
      void                   rename(const std::string& from, const std::string& to);

    private:
      std::string error_message(const std::string& message) const;
      [[noreturn]] void raise(const std::string& message) const;

      sftp_session handle = nullptr;
      ssh_session  session_handle;
    };
  }
}

#endif
