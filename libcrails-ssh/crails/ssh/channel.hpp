#ifndef  SSH_CHANNEL_HPP
# define SSH_CHANNEL_HPP

# include <libssh/libssh.h>
# include <string>
# include <chrono>
# include <functional>
# include "exit_status.hpp"

namespace Crails
{
  namespace Ssh
  {
    class Session;

    // A Channel must be destroyed before the Session that created it.
    class Channel
    {
      friend class Session;
      enum InputType { Stdout, Stderr };
      typedef std::chrono::steady_clock clock;
      ssh_channel handle = nullptr;
      int timeout_ms = 0;   // max time without any output (0 = disabled)
      int deadline_ms = 0;  // max total time (0 = disabled)
      InputType currently_reading = Stdout;
    public:
      Channel() = default;
      Channel(const Channel&) = delete;
      Channel& operator=(const Channel&) = delete;
      ~Channel();

      template<typename STREAM>
      ExitStatus exec(const std::string& command, STREAM& stream)
      {
        return exec(command, std::bind(&STREAM::put, &stream, std::placeholders::_1));
      }

      template<typename STREAM_A, typename STREAM_B>
      ExitStatus exec(const std::string& command, STREAM_A& out, STREAM_B& err)
      {
        return exec(command, std::function<void(char)>([this, &out, &err](char c)
        {
          if (currently_reading == Stdout)
            out.put(c);
          else
            err.put(c);
        }));
      }

      void set_timeout_duration(int value) { timeout_ms = value; }
      void set_timeout_duration(std::chrono::milliseconds duration) { timeout_ms = static_cast<int>(duration.count()); }
      void set_deadline(std::chrono::milliseconds duration) { deadline_ms = static_cast<int>(duration.count()); }

    private:
      bool is_within_time_limits(const clock::time_point& started, const clock::time_point& last_output);
      ExitStatus exec(const std::string& command, std::function<void(char)> output);
      ExitStatus read(std::function<void(char)> output);
    };
  }
}

#endif
