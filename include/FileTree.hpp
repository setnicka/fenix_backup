#ifndef FILETREE_HPP
#define FILETREE_HPP

namespace FenixBackup {
    class FileTree;
}

#include "Config.hpp"
#include "FileInfo.hpp"

#include <unordered_map>
#include <vector>

namespace FenixBackup {

class FileTree {
  public:
	FileTree(); // Construct new one
	FileTree(std::string name); // Try to load from tree of given name
	// FileTree(const FileTree& other);
	virtual ~FileTree();

	// Tree nodes functions
	std::shared_ptr<FileInfo> GetRoot();
	std::shared_ptr<FileInfo> AddDirectory(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params);
	std::shared_ptr<FileInfo> AddFile(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params);
	std::shared_ptr<FileInfo> AddSymlink(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params);

	std::shared_ptr<FileInfo> GetFileByPath(std::string const& file_path);
	std::shared_ptr<FileInfo> GetFileByHash(std::string const& file_hash);
	std::shared_ptr<FileInfo> GetFileById(unsigned int file_id);

    const std::vector<std::shared_ptr<FileInfo>>& GetAllFiles();

	std::vector<std::shared_ptr<FileInfo>> FinishTree(); // End construction of the file tree and return the files which it wants to download them (TODO: ordered by priority)

	// Saving functions
	const std::string& GetTreeName();
	const time_t GetConstructTime();
	void SaveTree();
	std::shared_ptr<FileTree> GetNextTree();
	std::shared_ptr<FileTree> GetPrevTree();

    // Static functions
    static const std::vector<std::string>& GetHistoryTreeList();
    static const std::string GetNewestTreeName();
	static std::shared_ptr<FileTree> CreateNewTree();
	static std::shared_ptr<FileTree> GetHistoryTree(std::string name);

    class FileTreeData;
  private:
	std::unique_ptr<FileTreeData> data;

    static std::vector<std::string> history_trees_list;
	static std::unordered_map<std::string, std::shared_ptr<FileTree>> history_trees;
};

}
#endif // FILETREE_HPP
