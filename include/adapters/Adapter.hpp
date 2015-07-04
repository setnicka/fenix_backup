#ifndef ADAPTERS_ADAPTER_HPP
#define ADAPTERS_ADAPTER_HPP

#include <memory.h>
#include "FileTree.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

class Adapter {
  public:
    Adapter();
    virtual ~Adapter();

    /// Scan and construct FileTree, return pointer to it
    virtual std::shared_ptr<FileTree> Scan() = 0;

    /// Get and process each given file
    virtual void GetAndProcess(std::shared_ptr<FileInfo> file) = 0;
};

}

#endif // ADAPTERS_ADAPTER_HPP
