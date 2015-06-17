#include <stdexcept>
#include <fstream>
#include <ctime>
#include <boost/filesystem.hpp>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

#include "FileTree.hpp"
#include "FenixExceptions.hpp"

namespace FenixBackup {

std::unordered_map<std::string, std::shared_ptr<FileTree>> FileTree::history_trees;
std::vector<std::string> FileTree::history_trees_list;

// Hide data from .hpp file using PIMP idiom
class FileTree::FileTreeData {
  public:
    FileTreeData();

	std::shared_ptr<FileInfo> root;

	std::string tree_name;

	// Versioning
	std::string prev_version_tree_name;

	std::vector<std::shared_ptr<FileInfo>> files;

	std::shared_ptr<FileInfo> AddNode(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name, file_params params);

    template <class Archive>
    void serialize(Archive & ar) {
        ar(
            cereal::make_nvp("tree_name", tree_name),
            cereal::make_nvp("prev_version_tree_name", prev_version_tree_name),
            cereal::make_nvp("root", root)
            //cereal::make_nvp("files", files)
        );
    }
};

FileTree::FileTreeData::FileTreeData() {
    auto root = std::make_shared<FileInfo>(DIR, nullptr, "");
    files.push_back(root);

    // Construct tree name from current datetime
    std::time_t rawtime;
    std::tm* timeinfo;
    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);
    char buffer[80];
    std::strftime(buffer, 80 , Config::GetConfig().treeFilePattern.c_str(), timeinfo);
    tree_name = std::string(buffer);

    // Get last tree name
    if (FileTree::GetNewestTreeName() != "") {
        prev_version_tree_name = FileTree::GetNewestTreeName();
        while (prev_version_tree_name.find(tree_name) != std::string::npos) {
            // New name is substring of the previous name -> more FileTrees from the same datetime, add char to the end
            tree_name = tree_name+"x";
        }
    }
}


FileTree::FileTree(): data{new FileTreeData()} { }

FileTree::FileTree(std::string name) {
	// Load data from given FileTree name
    std::ifstream is(Config::GetTreeFilename(name));
}

FileTree::~FileTree() { }

/*FileTree::FileTree(const FileTree& other) {
	//copy ctor
}*/

std::shared_ptr<FileInfo> FileTree::GetRoot() { return data->root; }

const std::vector<std::string>& FileTree::GetHistoryTreeList() {
    if (history_trees_list.empty()) {
        boost::filesystem::path dir (Config::GetTreeDir());
        try {
            if (boost::filesystem::exists(dir) && boost::filesystem::is_directory(dir)) {
                for (boost::filesystem::directory_iterator file(dir); file != boost::filesystem::directory_iterator(); ++file) {
                    if (boost::filesystem::is_regular_file(file->path())
                    && boost::filesystem::extension(file->path()) == Config::GetConfig().treeFileExtension) {
                        boost::filesystem::path a = file->path();
                        history_trees_list.push_back(a.replace_extension("").string());
                    }
                }
            }
            // TODO: What else? Error, create directory itself or nothing?
            else throw FileTreeException("Directory '"+Config::GetTreeDir()+"' missing\n");
        } catch (const boost::filesystem::filesystem_error& ex) {
            throw FileTreeException("Problem when parsing data directory '"+Config::GetTreeDir()+"'\n");
        }
        sort(history_trees_list.begin(), history_trees_list.end());
    }

    return history_trees_list;
}

const std::string FileTree::GetNewestTreeName() {
    auto tree_list = GetHistoryTreeList();
    if (tree_list.empty()) return "";
    return tree_list.back();
}

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
		else if (parent->GetPrevVersionId() != -1) prev_version_parent = prev_version_tree->GetFileById(parent->GetPrevVersionId());

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
	// if (type == FILE)

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

const std::string& FileTree::GetTreeName() { return data->tree_name; }

void FileTree::SaveTree() {
    std::ofstream os(Config::GetTreeFilename(data->tree_name));
    std::cout << Config::GetTreeFilename(data->tree_name) << std::endl;
    cereal::JSONOutputArchive archive(os);

    //archive(data);
}

}
