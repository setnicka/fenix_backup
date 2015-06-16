#include <stdexcept>

#include "FileTree.hpp"
namespace FenixBackup {

// Hide data from .hpp file using PIMP idiom
class FileTree::FileTreeData {
  public:
	std::shared_ptr<FileInfo> root;

	// Versioning
	std::string prev_version_tree_name;

	std::vector<std::shared_ptr<FileInfo>> files;

	FileTreeData() {
		auto root = std::make_shared<FileInfo>(DIR, nullptr, "");
		files.push_back(root);
	}

	std::shared_ptr<FileInfo> AddNode(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name, file_params params);
};

FileTree::FileTree(): data{new FileTreeData()} {

}

FileTree::FileTree(std::string name): FileTree() {
	// TODO: Load from file
}

FileTree::~FileTree() { }

/*FileTree::FileTree(const FileTree& other) {
	//copy ctor
}*/

std::shared_ptr<FileInfo> FileTree::GetRoot() {
	return data->root;
}

std::unordered_map<std::string, std::shared_ptr<FileTree>> FileTree::history_trees;

std::shared_ptr<FileTree> FileTree::GetHistoryTree(std::string name) {
	if (history_trees.find(name) == history_trees.end())
		history_trees.insert(std::make_pair(name, std::make_shared<FileTree>(name)));

	return history_trees[name];
}

std::shared_ptr<FileInfo> FileTree::AddDirectory(std::shared_ptr<FileInfo> parent, std::string const& name, file_params params) {
	return data->AddNode(DIR, parent, name, params);
}

std::shared_ptr<FileInfo> FileTree::AddFile(std::shared_ptr<FileInfo> parent, std::string const& name, file_params params) {
	return data->AddNode(FILE, parent, name, params);
}

std::shared_ptr<FileInfo> FileTree::FileTreeData::AddNode(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name, file_params params) {
	if (parent->GetType() != DIR) throw std::invalid_argument("Parent must be dir");

	auto file = std::make_shared<FileInfo>(type, parent, name);
	file->SetParams(params);
	parent->AddChild(name, file);

	file->SetId(files.size());
	files.push_back(file);

	// Check previous known version of file and set status
	version_file_status status = UNKNOWN_FILE;
	if (!prev_version_tree_name.empty()) {
		auto prev_version_tree = FileTree::GetHistoryTree(prev_version_tree_name);
		std::shared_ptr<FileInfo> prev_version_parent;

		if (parent->GetId() == 0) prev_version_parent = prev_version_tree->GetRoot();
		else if (parent->GetPrevVersionId() != 0) prev_version_parent = prev_version_tree->GetFileById(parent->GetPrevVersionId());

		if (prev_version_parent != nullptr) {
			std::shared_ptr<FileInfo> prev_version_file = prev_version_parent->GetChild(name);
			if (prev_version_file != nullptr && prev_version_file->GetType() == type) {
				// Match only files to files and dirs to dirs (else UNKNOWN_FILE)
				file->SetPrevVersionId(prev_version_file->GetId());
				file_params prev_version_params = prev_version_file->GetParams();
				if (type == DIR) {
					// TODO: Maybe status = UNCHANGED_FILE; for some situation?
					status = UPDATED_FILE;
				} else {
					if (prev_version_params.file_size == params.file_size && prev_version_params.modification_time == params.modification_time) status = UNCHANGED_FILE;
					else status = NOT_UPDATED_FILE;
				}
			}
		}
	}
	if (type == DIR && status == UNKNOWN_FILE) status = NEW_FILE; // When DIR, there are no data to save -> we have done all work for this file
	file->SetStatus(status);

	// TODO: Count score and add file to score-heap
	if (type == FILE)

	return file;
}


std::shared_ptr<FileInfo> FileTree::GetFileByPath(std::string path) {
	return nullptr;
}

std::shared_ptr<FileInfo> FileTree::GetFileById(unsigned int file_id) {
	if (file_id < 0 || file_id >= data->files.size()) throw std::out_of_range("File index out of range");
	return data->files[file_id];
}


std::vector<FileInfo> FileTree::CloseTree() {

	std::vector<FileInfo> t;
	return t;
}

}
