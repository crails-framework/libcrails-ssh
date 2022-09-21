#ifndef  SSH_CHANNEL_HPP
# define SSH_CHANNEL_HPP

# include <libssh/libssh.h>
# include <string>
# include <chrono>
# include <functional>

namespace Crails
{
  namespace Ssh
  {
    class Session;

    class Channel
    {
      friend class Session;
      enum InputType { Stdout, Stderr };
      ssh_channel handle;
      int timeout_ms;
      InputType currently_reading;
    public:
      ~Channel();

      template<typename STREAM>
      int exec(const std::string& command, STREAM& stream)
      {
        return exec(command, std::bind(&STREAM::put, &stream, std::placeholders::_1));
      }

      template<typename STREAM_A, typename STREAM_B>
      int exec(const std::string& command, STREAM_A& stdout, STREAM_B& stderr)
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

    private:
      int exec(const std::string& command, std::function<void(char)> output);
      int poll(char* buffer);
      int poll(char* buffer, InputType type);
    };
  }
}

#endif
