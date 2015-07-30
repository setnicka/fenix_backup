#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "BackupCleaner.hpp"
#include "FileTree.hpp"
#include "FileInfo.hpp"
#include "FileChunk.hpp"

namespace FenixBackup {

class BackupCleaner::BackupCleanerData {
  public:
    time_t cleanup_time;

    BackupCleanerData() {
        std::time(&cleanup_time);
    }

    struct chunk_info {
        unsigned int badness = -1;
        unsigned int normalized_badness;
        std::vector<std::pair<unsigned int, unsigned int>> file_records;
    };

    struct heap_item {
        unsigned int badness;
        std::string chunk;

        heap_item(unsigned int badness, const std::string& chunk): badness{badness}, chunk{chunk} {}
    };
    struct heap_comparator {
        bool operator() (heap_item a, heap_item b){
            return a.badness < b.badness;
        }
    };

    std::unordered_map<std::string, chunk_info> chunks;
    std::priority_queue<heap_item, std::vector<heap_item>, heap_comparator> chunk_heap;
    std::vector<std::vector<std::shared_ptr<FileInfo>>> files;
    std::unordered_set<std::shared_ptr<FileInfo>> files_used;

    void AddFile(int index, std::shared_ptr<FileTree> tree, std::shared_ptr<FileInfo> file);

    void CountBadness(unsigned int index, unsigned int subindex);
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

void BackupCleaner::BackupCleanerData::CountBadness(unsigned int index, unsigned int subindex) {
    auto file = files[index][subindex];
    auto rules = Config::GetRules(file->GetPath(), file->GetParams());

    // 1. Count min_distance
    unsigned int badness = 0;
    // age (seconds) --> min_distance
    int age = file->GetTree()->GetConstructTime() - cleanup_time;
    int min_distance = age; // TODO: better function?

    // 2. Count badness from neigtbours distance
    int newer = subindex - 1;
    while (newer >= 0 && files[index][newer]->GetStatus() == DELETED) newer--; // Skip DELETED files
    if (newer >= 0) {
            int distance1 = files[index][newer]->GetTree()->GetConstructTime() - file->GetTree()->GetConstructTime();
            badness += (100*min_distance)/(distance1*rules.history);
    }
    unsigned int older = subindex + 1;
    while (older < files[index].size() && files[index][older]->GetStatus() == DELETED) older++; // Skip DELETED files
    if (older < files[index].size()) {
            int distance2 = file->GetTree()->GetConstructTime() - files[index][subindex+1]->GetTree()->GetConstructTime();
            badness += (100*min_distance)/(distance2*rules.history);
    }

    // 3. Update chunk min badness
    if (badness < chunks[file->GetHash()].badness) chunks[file->GetHash()].badness = badness;
}

void BackupCleaner::LoadData() {
    // 1. Scan all trees and construct file lists and global chunk list
    std::vector<std::string> trees = FileTree::GetHistoryTreeList();
    while (!trees.empty()) {
        // XXX: Newest tree has badness always = 0
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

    // 2. Initialize chunks badness and for each file list run badness count function
    for (uint i = 0; i < data->files.size(); i++)
        for (uint j = 0; j < data->files[i].size(); j++) data->CountBadness(i, j);

    // 3. Compute normalized badness and push new chunk value into heap
    for (auto& chunk: data->chunks) {
            chunk.second.normalized_badness = chunk.second.badness / chunk.second.file_records.size();
            BackupCleanerData::heap_item item(chunk.second.normalized_badness, chunk.first);
            data->chunk_heap.push(item);
    }
}

int BackupCleaner::Clean() {
    // 1. Get chunk with greatest badness
    if (data->chunk_heap.empty()) return 0;

    // Reheap item if real badness drops
    while (data->chunk_heap.top().badness != data->chunks[data->chunk_heap.top().chunk].normalized_badness) {
        auto new_item = data->chunk_heap.top();
        new_item.badness = data->chunks[new_item.chunk].normalized_badness;
        data->chunk_heap.pop();
        data->chunk_heap.push(new_item);
    }

    auto item = data->chunk_heap.top();
    data->chunk_heap.pop();

    // 2. Delete this chunk, count free size
    int size_change = (FileChunk::GetChunk(item.chunk) == nullptr ? 0 : FileChunk::GetChunk(item.chunk)->DeleteChunk());
    // std::cout << "Deleted " << item.chunk << ", size change " << size_change << std::endl;

    std::unordered_set<std::shared_ptr<FileTree>> trees_to_save;

    // 3. Add DELETED status to files and recompute near files badness
    for(auto& record: data->chunks[item.chunk].file_records) {
        auto file = data->files[record.first][record.second];
        file->SetStatus(DELETED);
        if (record.second > 0) data->CountBadness(record.first, record.second - 1);
        if (record.second < data->files[record.first].size() - 1) data->CountBadness(record.first, record.second + 1);
        trees_to_save.insert(file->GetTree());
        // std::cout << "In tree " << file->GetTree()->GetTreeName() << " deleted file " << file->GetPath() << std::endl;
    }

    // 4. Save affected trees
    for(auto& tree: trees_to_save) tree->SaveTree();

    return size_change;
}

}
