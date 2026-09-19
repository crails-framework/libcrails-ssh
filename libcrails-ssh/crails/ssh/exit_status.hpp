#ifndef  SSH_EXIT_STATUS_HPP
# define SSH_EXIT_STATUS_HPP

# include <libssh/libssh.h>
# include <string_view>

namespace Crails
{
  namespace Ssh
  {
    class Channel;

    class ExitStatus
    {
      friend class Channel;
      ExitStatus(ssh_channel);
      ExitStatus() {}
      static ExitStatus on_time_out();
    public:
      ExitStatus(ExitStatus&&);
      ExitStatus(const ExitStatus&) = delete;
      ExitStatus& operator=(ExitStatus&& other);
      ExitStatus& operator=(const ExitStatus&) = delete;
      ~ExitStatus();

      bool             has_exit_status() const { return retrieved; }
      bool             has_signal() const { return signal != nullptr; }
      uint32_t         get_code() const { return code; }
      std::string_view get_signal() const;
      bool             was_dumped() const { return dumped; }
      bool             was_timed_out() const { return timed_out; }

      operator int() const
      {
        if (!retrieved || dumped || (signal != nullptr))
          return -1;
        return static_cast<int>(code);
      }

    private:
      bool     retrieved = false;
      uint32_t code = 0;
      char*    signal = nullptr;
      bool     dumped = false;
      bool     timed_out = false;
    };
  }
}

#endif
