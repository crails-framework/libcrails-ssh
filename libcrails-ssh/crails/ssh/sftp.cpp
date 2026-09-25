#include "sftp.hpp"
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace Crails::Ssh;
using namespace std;

static const size_t chunk_size = 16384;

namespace
{
  class RemoteFile
  {
    sftp_file file;
  public:
    explicit RemoteFile(sftp_file file) : file(file) {}
    RemoteFile(const RemoteFile&) = delete;
    ~RemoteFile() { if (file) sftp_close(file); }
    operator sftp_file() const { return file; }
    explicit operator bool() const { return file != nullptr; }
    bool close()
    {
      int rc = sftp_close(file);
      file = nullptr;
      return rc == SSH_OK;
    }
  };

  struct AttributesDeleter { void operator()(sftp_attributes a) const { sftp_attributes_free(a); } };
  typedef unique_ptr<sftp_attributes_struct, AttributesDeleter> Attributes;
}

Sftp::Sftp(ssh_session session_handle) : session_handle(session_handle)
{
  handle = sftp_new(session_handle);
  if (handle == nullptr)
    throw runtime_error(string("Ssh::Sftp: failed to create SFTP session: ") + ssh_get_error(session_handle));
  if (sftp_init(handle) != SSH_OK)
  {
    string message = error_message("failed to initialize SFTP subsystem (is it enabled on the server?)");
    sftp_free(handle);
    throw runtime_error(message);
  }
}

Sftp::~Sftp()
{
  if (handle)
    sftp_free(handle);
}

string Sftp::error_message(const string& message) const
{
  stringstream stream;

  stream << "Ssh::Sftp: " << message
         << " (sftp code " << (handle ? sftp_get_error(handle) : -1)
         << ": " << ssh_get_error(session_handle) << ')';
  return stream.str();
}

void Sftp::raise(const string& message) const
{
  throw runtime_error(error_message(message));
}

void Sftp::push_stream(istream& input, const string& target, int mode)
{
  RemoteFile file(sftp_open(handle, target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode));
  char       buffer[chunk_size];

  if (!file)
    raise("cannot open remote file " + target);
  while (input)
  {
    input.read(buffer, sizeof(buffer));
    const size_t length = static_cast<size_t>(input.gcount());

    for (size_t done = 0 ; done < length ; )
    {
      auto written = sftp_write(file, buffer + done, length - done);

      if (written <= 0)
        raise("cannot write remote file " + target);
      done += static_cast<size_t>(written);
    }
  }
  if (input.bad())
    throw runtime_error("Ssh::Sftp: error while reading local data for " + target);
  if (!file.close())
    raise("cannot finalize remote file " + target);
}

void Sftp::push_file(const string& source, const string& target, int mode)
{
  ifstream input(source, ios::in | ios::binary);

  if (!input.is_open())
    throw runtime_error("Ssh::Sftp: cannot read local file " + source);
  push_stream(input, target, mode);
}

void Sftp::push_text(const string& content, const string& target, int mode)
{
  istringstream input(content, ios::in | ios::binary);

  push_stream(input, target, mode);
}

void Sftp::push_directory(const string& path, int mode)
{
  if (sftp_mkdir(handle, path.c_str(), mode) != SSH_OK)
  {
    const string message = error_message("cannot remotely create directory " + path);

    if (!is_directory(path))
      throw runtime_error(message);
  }
}

void Sftp::pull_file(const string& source, ostream& output)
{
  RemoteFile file(sftp_open(handle, source.c_str(), O_RDONLY, 0));
  char       buffer[chunk_size];

  if (!file)
    raise("cannot open remote file " + source);
  for (;;)
  {
    auto count = sftp_read(file, buffer, sizeof(buffer));

    if (count < 0)
      raise("cannot read remote file " + source);
    if (count == 0)
      break ;
    output.write(buffer, count);
    if (!output)
      throw runtime_error("Ssh::Sftp: cannot write local output while pulling " + source);
  }
}

void Sftp::pull_file(const string& source, const string& target)
{
  ofstream output(target, ios::out | ios::binary | ios::trunc);

  if (!output.is_open())
    throw runtime_error("Ssh::Sftp: cannot open local file " + target);
  pull_file(source, output);
  output.close();
  if (output.fail())
    throw runtime_error("Ssh::Sftp: cannot finalize local file " + target);
}

string Sftp::pull_text(const string& source)
{
  ostringstream output(ios::out | ios::binary);

  pull_file(source, output);
  return output.str();
}

bool Sftp::exists(const string& path)
{
  Attributes attributes(sftp_stat(handle, path.c_str()));

  if (attributes)
    return true;
  if (sftp_get_error(handle) == SSH_FX_NO_SUCH_FILE)
    return false;
  raise("cannot stat " + path);
}

bool Sftp::is_directory(const string& path)
{
  Attributes attributes(sftp_stat(handle, path.c_str()));

  return attributes && attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
}

vector<SftpEntry> Sftp::list_directory(const string& path)
{
  vector<SftpEntry> entries;
  sftp_dir          directory = sftp_opendir(handle, path.c_str());

  if (directory == nullptr)
    raise("cannot open remote directory " + path);
  while (Attributes attributes{sftp_readdir(handle, directory)})
  {
    const string name = attributes->name ? attributes->name : "";

    if (name == "." || name == "..")
      continue ;
    entries.push_back({
      name,
      attributes->type == SSH_FILEXFER_TYPE_DIRECTORY,
      static_cast<uint64_t>(attributes->size)
    });
  }
  const bool complete = sftp_dir_eof(directory);
  sftp_closedir(directory);
  if (!complete)
    raise("cannot read remote directory " + path);
  return entries;
}

void Sftp::remove_file(const string& path)
{
  if (sftp_unlink(handle, path.c_str()) != SSH_OK)
    raise("cannot remove remote file " + path);
}

void Sftp::remove_directory(const string& path)
{
  if (sftp_rmdir(handle, path.c_str()) != SSH_OK)
    raise("cannot remove remote directory " + path);
}

void Sftp::rename(const string& from, const string& to)
{
  if (sftp_rename(handle, from.c_str(), to.c_str()) != SSH_OK)
    raise("cannot rename " + from + " to " + to);
}
