#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "BackupCleaner.hpp"
#include "FileTree.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

class BackupCleaner::BackupCleanerData {
  public:
    BackupCleanerData() {}

    struct chunk_info {
        int baddness;
        std::vector<std::pair<int, int>> file_records;
    };

    std::unordered_map<std::string, chunk_info> chunks;
    std::vector<std::vector<std::shared_ptr<FileInfo>>> files;
    std::unordered_set<std::shared_ptr<FileInfo>> files_used;

    void AddFile(int index, std::shared_ptr<FileTree> tree, std::shared_ptr<FileInfo> file);
};

BackupCleaner::BackupCleaner(): data{new BackupCleaner::BackupCleanerData()} {}
BackupCleaner::~BackupCleaner() {}

void BackupCleaner::BackupCleanerData::AddFile(int index, std::shared_ptr<FileTree> tree, std::shared_ptr<FileInfo> file) {
    if (file->GetStatus() != DELETED && file->GetStatus() != NOT_UPDATED && file->GetStatus() != UNKNOWN) {
        int subindex = files[index].size();
        files[index].push_back(file);

        // Chunk
        if (chunks.find(file->GetHash()) == chunks.end()) {
            chunks.insert(std::make_pair(file->GetHash(), chunk_info()));
        }
        chunks[file->GetHash()].file_records.push_back(std::make_pair(index, subindex));
    }
    if (file->GetPrevVersionId() != 0 && tree != nullptr) {
        auto prev_tree = tree->GetPrevTree();
        if (prev_tree != nullptr) AddFile(index, prev_tree, prev_tree->GetFileById(file->GetPrevVersionId()));
    }

    files_used.insert(file);
}

void BackupCleaner::LoadData() {
    // 1. Scan all trees and construct file lists and global chunk list
    std::vector<std::string> trees = FileTree::GetHistoryTreeList();
    while (!trees.empty()) {
        // XXX: Newest tree has baddness always = 0
        auto tree = FileTree::GetHistoryTree(trees.back());
        trees.pop_back();
        auto& files = tree->GetAllFiles();
        for (auto& file: files) {
            if (file == nullptr) continue;
            if (file->GetType() != DIR && data->files_used.find(file) == data->files_used.end()) {
                int index = data->files.size();
                data->files.push_back(std::vector<std::shared_ptr<FileInfo>>());
                data->AddFile(index, tree, file);
            }
        }
    }

    // 2. TODO...
}

}
