#ifndef ADAPTERS_ADAPTER_HPP
#define ADAPTERS_ADAPTER_HPP

#include <memory.h>
#include "FileTree.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

enum restore_mode {ALL, ONLY_DATA, ONLY_PERMISSIONS};

class Adapter {
  public:
    Adapter();
    virtual ~Adapter();

    /// Scan and construct FileTree, return pointer to it
    virtual std::shared_ptr<FileTree> Scan() = 0;

    /// Get and process each given file
    virtual void GetAndProcess(std::shared_ptr<FileInfo> file) = 0;

    /// Restore file to the original machine
    virtual void RestoreFile(std::shared_ptr<FileInfo> file, restore_mode mode = ALL) = 0;
    virtual void RestoreFileToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true) = 0;
    /// Restore file local
    virtual void RestoreFileToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true) = 0;

    /// Restoring subtrees
    virtual void RestoreSubtree(std::shared_ptr<FileInfo> file, restore_mode mode = ALL);
    virtual void RestoreSubtreeToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true);
    virtual void RestoreSubtreeToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, bool preserve_inbackup_path = true);
};

}

#endif // ADAPTERS_ADAPTER_HPP
