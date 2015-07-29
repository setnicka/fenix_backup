#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "BackupCleaner.hpp"
#include "FileTree.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

class BackupCleaner::BackupCleanerData {
  public:
    time_t cleanup_time;

    BackupCleanerData() {
        std::time(&cleanup_time);
    }

    struct chunk_info {
        unsigned int baddness = -1;
        std::vector<std::pair<int, int>> file_records;
    };

    std::unordered_map<std::string, chunk_info> chunks;
    std::vector<std::vector<std::shared_ptr<FileInfo>>> files;
    std::unordered_set<std::shared_ptr<FileInfo>> files_used;

    void AddFile(int index, std::shared_ptr<FileTree> tree, std::shared_ptr<FileInfo> file);

    void CountBaddness(unsigned int index, unsigned int subindex);
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

void BackupCleaner::BackupCleanerData::CountBaddness(unsigned int index, unsigned int subindex) {
    auto file = files[index][subindex];
    auto rules = Config::GetRules(file->GetPath(), file->GetParams());

    // 1. Count min_distance
    unsigned int baddness = 0;
    // age (seconds) --> min_distance
    int age = file->GetTree()->GetConstructTime() - cleanup_time;
    int min_distance = age/10; // TODO

    // 2. Count baddness from neigtbours distance
    if (subindex != 0) {
            int distance1 = files[index][subindex-1]->GetTree()->GetConstructTime() - file->GetTree()->GetConstructTime();
            baddness += (100*min_distance)/(distance1*rules.history);
    }
    if (subindex != files[index].size() - 1) {
            int distance2 = file->GetTree()->GetConstructTime() - files[index][subindex+1]->GetTree()->GetConstructTime();
            baddness += (100*min_distance)/(distance2*rules.history);
    }

    // Update chunk min baddness
    if (baddness < chunks[file->GetHash()].baddness) chunks[file->GetHash()].baddness = baddness;
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

    // 2. Initialize chunks baddness and for each file list run baddness count function
    for (uint i = 0; i < data->files.size(); i++)
        for (uint j = 0; j < data->files[i].size(); j++) data->CountBaddness(i, j);

    // 3. Some sort -- TODO
}

size_t BackupCleaner::Clean() {
    // 1. Get chunk with greatest baddness

    // 2. Delete this chunk, count free size

    // 3. Update chunks baddness and (re)sort them?
}

}
