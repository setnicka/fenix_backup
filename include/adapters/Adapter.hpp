#ifndef ADAPTERS_ADAPTER_HPP
#define ADAPTERS_ADAPTER_HPP

namespace FenixBackup {
    class Adapter;
}

#include <memory.h>
#include "FileTree.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

enum restore_mode {ALL, ONLY_DATA, ONLY_PERMISSIONS};

enum restore_tactic {ONLY_THIS_VERSION, NEWEST_KNOWN_VERSION};

class Adapter {
  public:
    Adapter();
    Adapter(std::string tree_name);
    virtual ~Adapter();

    /// Scan and construct FileTree, return pointer to it
    virtual std::shared_ptr<FileTree> Scan() = 0;
    virtual std::shared_ptr<FileTree> GetTree() = 0;
    virtual void SetTree(std::shared_ptr<FileTree> tree) = 0;

    /// Get and process each given file
    virtual void GetAndProcess(std::shared_ptr<FileInfo> file) = 0;

    /// Restore file to the original machine
    virtual void RestoreFile(std::shared_ptr<FileInfo> file, restore_mode mode = ALL, restore_tactic tactic = NEWEST_KNOWN_VERSION) = 0;
    virtual void RestoreFileToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, restore_tactic tactic = NEWEST_KNOWN_VERSION, bool preserve_inbackup_path = true) = 0;
    /// Restore file local
    virtual void RestoreFileToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode = ALL, restore_tactic tactic = NEWEST_KNOWN_VERSION, bool preserve_inbackup_path = true) = 0;

    /// Restoring subtrees
    virtual void RestoreSubtree(std::shared_ptr<FileInfo> file, restore_mode mode = ALL, restore_tactic tactic = NEWEST_KNOWN_VERSION);
    virtual void RestoreSubtreeToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path,
                                            restore_mode mode = ALL, restore_tactic tactic = NEWEST_KNOWN_VERSION,
                                            bool preserve_inbackup_path = true);
    virtual void RestoreSubtreeToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path,
                                           restore_mode mode = ALL,restore_tactic tactic = NEWEST_KNOWN_VERSION,
                                           bool preserve_inbackup_path = true);
};

}

#endif // ADAPTERS_ADAPTER_HPP
