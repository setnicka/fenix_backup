#ifndef ADAPTERS_LOCALFILESYSTEMADAPTER_HPP
#define ADAPTERS_LOCALFILESYSTEMADAPTER_HPP

#include "adapters/Adapter.hpp"

namespace FenixBackup {

class LocalFilesystemAdapter: public Adapter {
  public:
    LocalFilesystemAdapter();
    virtual ~LocalFilesystemAdapter();

    void SetPath(std::string path);

    virtual std::shared_ptr<FileTree> Scan();
    virtual void GetAndProcess(std::shared_ptr<FileInfo> file);

    virtual void RestoreFile(std::shared_ptr<FileInfo> file, restore_mode mode = ALL);
    virtual void RestoreFileToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true);
    virtual void RestoreFileToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true);
  private:
    class LocalFilesystemAdapterData;
    std::unique_ptr<LocalFilesystemAdapterData> data;
};

}
#endif // ADAPTERS_LOCALFILESYSTEMADAPTER_HPP
