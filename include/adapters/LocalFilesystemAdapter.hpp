#ifndef ADAPTERS_LOCALFILESYSTEMADAPTER_HPP
#define ADAPTERS_LOCALFILESYSTEMADAPTER_HPP

#include "adapters/Adapter.hpp"

namespace FenixBackup {

class LocalFilesystemAdapter: public Adapter {;
  public:
    LocalFilesystemAdapter();
    virtual ~LocalFilesystemAdapter();

    void SetPath(std::string path);

    virtual std::shared_ptr<FileTree> Scan();
    virtual void GetAndProcess(std::shared_ptr<FileInfo> file);
  private:
    class LocalFilesystemAdapterData;
    std::unique_ptr<LocalFilesystemAdapterData> data;
};

}
#endif // ADAPTERS_LOCALFILESYSTEMADAPTER_HPP
