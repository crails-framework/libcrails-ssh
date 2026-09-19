#ifndef  SSH_EXIT_STATUS_HPP
# define SSH_EXIT_STATUS_HPP

# include <libssh/libssh.h>
# include <cstdint>
# include <string_view>

namespace Crails
{
  namespace Ssh
  {
    class Channel;

    class ExitStatus
    {
      friend class Channel;
      explicit ExitStatus(ssh_channel);
      ExitStatus() {}
      static ExitStatus on_time_out();
    public:
      ExitStatus(ExitStatus&&) noexcept;
      ExitStatus(const ExitStatus&) = delete;
      ExitStatus& operator=(ExitStatus&& other) noexcept;
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
        if (!retrieved || timed_out || dumped || (signal != nullptr))
          return -1;
        return static_cast<int>(code);
      }

    private:
      void release();

      bool     retrieved = false;
      uint32_t code = 0;
      char*    signal = nullptr;
      bool     dumped = false;
      bool     timed_out = false;
    };
  }
}

#endif
