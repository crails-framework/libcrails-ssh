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
      int timeout_ms = 0;
      int deadline_ms = 0;
      InputType currently_reading = Stdout;
    public:
      ~Channel();

      template<typename STREAM>
      ExitStatus exec(const std::string& command, STREAM& stream)
      {
        return exec(command, std::bind(&STREAM::put, &stream, std::placeholders::_1));
      }

      template<typename STREAM_A, typename STREAM_B>
      ExitStatus exec(const std::string& command, STREAM_A& stdout, STREAM_B& stderr)
      {
        return exec(command, [this, &stdout, &stderr](char c)
        {
          if (currently_reading == Stdout)
            stdout.put(c);
          else
            stderr.put(c);
        });
      }

      void set_timeout_duration(int value) { timeout_ms = value; }
      void set_timeout_duration(std::chrono::milliseconds duration) { timeout_ms = duration.count(); }
      void set_deadline(std::chrono::milliseconds duration) { deadline_ms = duration.count(); }

    private:
      bool timeout_check(const clock::time_point& started, const clock::time_point& last_receive);
      ExitStatus exec(const std::string& command, std::function<void(char)> output);
      ExitStatus read(std::function<void(char)> output);
    };
  }
}

#endif
