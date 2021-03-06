#include <vector>
#include <fstream>
#include <ctime>
#include <boost/filesystem.hpp>

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>

#include "FenixExceptions.hpp"
#include "FileTree.hpp"
#include "Functions.hpp"

namespace FenixBackup {

std::unordered_map<std::string, std::shared_ptr<FileTree>> FileTree::history_trees;
std::vector<std::string> FileTree::history_trees_list;

// Hide data from .hpp file using PIMP idiom
class FileTree::FileTreeData {
  public:
    FileTreeData(bool not_initialize = false);

	std::shared_ptr<FileInfo> root;
	std::string tree_name;
	time_t construct_time;

	// Versioning
	std::string prev_version_tree_name;
	std::string prev_version_hash;

    // Cache - not serialized
	std::vector<std::shared_ptr<FileInfo>> files;
	std::unordered_map<std::string, std::shared_ptr<FileInfo>> file_hashes;
	bool loaded_arrays = false;
	std::shared_ptr<FileTree> this_tree;
	bool in_tree_list = true;

    std::vector<std::pair<std::shared_ptr<FileInfo>, int>> files_to_process;

	std::shared_ptr<FileInfo> AddNode(file_type type, std::shared_ptr<FileInfo> parent, const std::string& name, const file_params& params);
	void CountScore(std::shared_ptr<FileInfo> file, const Config::Rules& rules);

    template <class Archive>
    void save(Archive & ar, std::uint32_t const version) const {
        if (version == 1) ar(
            cereal::make_nvp("tree_name", tree_name),
            cereal::make_nvp("construct_time", construct_time),
            cereal::make_nvp("prev_version_tree_name", prev_version_tree_name),
            cereal::make_nvp("prev_version_hash", prev_version_hash),
            cereal::make_nvp("root", root)
        );
        else throw FileTreeException("Unknown version "+std::to_string(version)+" of FileTree serialized data\n");
    }

    template <class Archive>
    void load(Archive & ar, std::uint32_t const version) {
        if (version == 1) ar(
            cereal::make_nvp("tree_name", tree_name),
            cereal::make_nvp("construct_time", construct_time),
            cereal::make_nvp("prev_version_tree_name", prev_version_tree_name),
            cereal::make_nvp("prev_version_hash", prev_version_hash),
            cereal::make_nvp("root", root)
        );
        else throw FileTreeException("Unknown version "+std::to_string(version)+" of FileTree serialized data\n");

        // Construct arrays files and file_hashes
        files.push_back(nullptr);
        files.push_back(root);
        LoadArrays(root);
        loaded_arrays = true;
    }

    void LoadArrays(std::shared_ptr<FileInfo> node) {
        if(node->GetId() >= files.size()) files.resize(node->GetId() + 1);
        files[node->GetId()] = node;

        if (node->GetType() == DIR) {
            for(auto file: node->GetChilds()) LoadArrays(file.second);
        } else {
            file_hashes.insert(std::make_pair(node->GetHash(), node));
        }
    }

    void UpdateTreeLinks() {
         // Register this_tree as tree in each FileInfo
        for(auto& file: files) if (file != nullptr) file->SetTree(this_tree);
    }
};

FileTree::FileTreeData::FileTreeData(bool initialize) {
    if (!initialize) return;
    in_tree_list = false; // It is new tree
    root = std::make_shared<FileInfo>(this_tree, DIR, nullptr, "");
    root->SetId(1); root->SetPrevVersionId(1);
    files.push_back(nullptr);
    files.push_back(root);

    // Construct tree name from current datetime
    std::tm* timeinfo;
    std::time(&construct_time);
    timeinfo = std::localtime(&construct_time);
    char buffer[80];
    std::strftime(buffer, 80 , Config::GetConfig().treeFilePattern.c_str(), timeinfo);
    tree_name = std::string(buffer);

    // Get last tree name anc construct name for this tree
    prev_version_tree_name = FileTree::GetNewestTreeName();
    if (prev_version_tree_name != "") {
        while (prev_version_tree_name.find(tree_name) != std::string::npos) {
            // New name is substring of the previous name -> more FileTrees from the same datetime, add char to the end
            tree_name = tree_name+"x";
        }
        // Count SHA256 hash of the previous tree and save it
        std::ifstream file(Config::GetTreeFilename(prev_version_tree_name));
        prev_version_hash = Functions::ComputeFileHash(file);
    }
}

void FileTree::FileTreeData::CountScore(std::shared_ptr<FileInfo> file, const Config::Rules& rules) {
    // Score = time_from_last_backup * priority;
    int age;
    if (prev_version_tree_name.empty()) age = 1;
    else if (file->GetPrevVersionId() == 0) age = construct_time - this_tree->GetPrevTree()->GetConstructTime();
    else {
        auto tree = this_tree->GetPrevTree();
        auto prev_file = tree->GetFileById(file->GetPrevVersionId());
        while (tree->GetPrevTree() != nullptr && prev_file->GetPrevVersionId() != 0
            && (prev_file->GetStatus() == UNKNOWN || prev_file->GetStatus() == NOT_UPDATED)
        ) {
            tree = tree->GetPrevTree();
            prev_file = tree->GetFileById(prev_file->GetPrevVersionId());
        }
        age = construct_time - tree->GetConstructTime();
    }
    files_to_process.push_back(std::make_pair(file, age * rules.priority));
}

////////////////////////////////////////////////////////////////////////////////

FileTree::FileTree(): data{new FileTreeData(true)} {}

FileTree::FileTree(std::string name) {
	// Load data from given FileTree name
    std::ifstream is(Config::GetTreeFilename(name));
    if (!is.good()) throw FileTreeException("Couldn't load FileTree '"+name+"' (from filename '"+Config::GetTreeFilename(name)+"')\n");
    //cereal::JSONInputArchive archive(is);
    cereal::BinaryInputArchive archive(is);

    archive(data);
}

FileTree::~FileTree() { }

/*FileTree::FileTree(const FileTree& other) {
	//copy ctor
}*/

std::shared_ptr<FileInfo> FileTree::GetRoot() { return data->root; }

const std::vector<std::shared_ptr<FileInfo>>& FileTree::GetAllFiles() {
    if (!data->loaded_arrays) {
        data->LoadArrays(GetRoot());
        data->loaded_arrays = true;
    }
    return data->files;
}

const std::vector<std::string>& FileTree::GetHistoryTreeList() {
    if (history_trees_list.empty()) {
        boost::filesystem::path dir(Config::GetTreeDir());
        try {
            if (boost::filesystem::exists(dir) && boost::filesystem::is_directory(dir)) {
                for (boost::filesystem::directory_iterator file(dir); file != boost::filesystem::directory_iterator(); ++file) {
                    if (boost::filesystem::is_regular_file(file->path())
                    && boost::filesystem::extension(file->path()) == Config::GetConfig().treeFileExtension) {
                        history_trees_list.push_back(boost::filesystem::basename(file->path()));
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
	if (history_trees.find(name) == history_trees.end()) {
        if(!std::ifstream(Config::GetTreeFilename(name)).good()) return nullptr;
		history_trees.insert(std::make_pair(name, std::make_shared<FileTree>(name)));
    }
    history_trees[name]->data->this_tree = history_trees[name];
    history_trees[name]->data->UpdateTreeLinks();
	return history_trees[name];
}

std::shared_ptr<FileTree> FileTree::CreateNewTree() {
    auto tree = std::make_shared<FileTree>();
    history_trees.insert(std::make_pair(tree->GetTreeName(), tree));
    tree->data->this_tree = tree;
    tree->data->UpdateTreeLinks();
    return tree;
}

std::shared_ptr<FileInfo> FileTree::AddDirectory(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params) {
	return data->AddNode(DIR, parent, name, params);
}

std::shared_ptr<FileInfo> FileTree::AddFile(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params) {
	return data->AddNode(FILE, parent, name, params);
}

std::shared_ptr<FileInfo> FileTree::AddSymlink(std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params) {
	return data->AddNode(SYMLINK, parent, name, params);
}

std::shared_ptr<FileInfo> FileTree::FileTreeData::AddNode(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name, const file_params& params) {
	if (parent->GetType() != DIR) throw std::invalid_argument("Parent must be dir");

    // Test if we want to backup this file
    auto rules = Config::GetRules(parent->GetPath() + "/" + name, params);
    if ((type == DIR && !rules.scan) || (type == FILE && !rules.backup) ) return nullptr;

	auto file = std::make_shared<FileInfo>(this_tree, type, parent, name);
	file->SetParams(params);
	parent->AddChild(name, file);

	file->SetId(files.size());
	files.push_back(file);

	// Check previous known version of file and set status
	version_file_status status = UNKNOWN;
	if (!prev_version_tree_name.empty()) {
		auto prev_version_tree = FileTree::GetHistoryTree(prev_version_tree_name);
		std::shared_ptr<FileInfo> prev_version_parent;

		if (parent->GetPrevVersionId() != 0) prev_version_parent = prev_version_tree->GetFileById(parent->GetPrevVersionId());

		if (prev_version_parent != nullptr) {
			std::shared_ptr<FileInfo> prev_version_file = prev_version_parent->GetChild(name);
			if (prev_version_file != nullptr && prev_version_file->GetType() == type) {
				// Match only files to files and dirs to dirs (else UNKNOWN)
				file->SetPrevVersionId(prev_version_file->GetId());
				auto prev_version_params = prev_version_file->GetParams();
				auto prev_version_status = prev_version_file->GetStatus();
				if (type == DIR) {
					// Check only params
					status = (params == prev_version_params ? UNCHANGED : UPDATED_PARAMS);
				} else { // FILE or SYMLINK
					if (prev_version_params.file_size != params.file_size || prev_version_params.modification_time != params.modification_time
                    || prev_version_status == UNKNOWN || prev_version_status == NOT_UPDATED)
                        // If previous file is UNKNOWN (no known previous version and not processed) -> UNKNOWN
                        // else (if there is at least one known and proccessed previous version) -> NOT_UPDATED
                        // Newest version of each file can't be DELETED
                        status = prev_version_status == UNKNOWN ? UNKNOWN : NOT_UPDATED;
                    else {
                            status = (params == prev_version_params ? UNCHANGED : UPDATED_PARAMS);
                            // No change to the file content -> same hash and chunk name as the previous
                            file->SetHash(prev_version_file->GetHash());
                            file_hashes.insert(std::make_pair(file->GetHash() ,file));
                    }
				}
			}
		}
	}
	if (type == DIR && status == UNKNOWN) status = NEW; // When DIR, there are no data to save -> we have done all work for this file
	file->SetStatus(status);

	// Count score and add file to score-heap
	if ((type == FILE || type == SYMLINK) && status != UNCHANGED && status != UPDATED_PARAMS) CountScore(file, rules);

	return file;
}


std::shared_ptr<FileInfo> FileTree::GetFileByPath(std::string const& path) {
    size_t start = 0; size_t len = path.length();
    auto node = data->root;
    auto pos = path.find('/', start);

    // Recursive search
    while (pos != std::string::npos) {
        // If directory name is "/" (in cases like "dir//subdir") or "./", move start and ask again
        if (pos - start > 0 && !(pos - start == 1 && path[start] == '.')) {
            node = node->GetChild(path.substr(start, pos - start));
            if (node == nullptr) return nullptr;
        }
        start = pos + 1;
        pos = path.find('/', start);
    }
    // If there is rest of the name, try to use it as subdir name, else return this dir
    if (len - start > 0) return node->GetChild(path.substr(start, len - start));
    else return node;
}

std::shared_ptr<FileInfo> FileTree::GetFileById(unsigned int file_id) {
	if (file_id < 0 || file_id >= data->files.size())
        throw std::out_of_range("File index "+std::to_string(file_id)+" out of range (range "+std::to_string(data->files.size())+") in the tree "+data->tree_name);
	return data->files[file_id];
}

std::shared_ptr<FileInfo> FileTree::GetFileByHash(std::string const& file_hash) {
	auto i = data->file_hashes.find(file_hash);
	if (i == data->file_hashes.end()) return nullptr;
	else return i->second;
}


std::vector<std::shared_ptr<FileInfo>> FileTree::FinishTree() {
    SaveTree();

    sort(data->files_to_process.begin(), data->files_to_process.end(),
        [](const std::pair<std::shared_ptr<FileInfo>, int> & a, const std::pair<std::shared_ptr<FileInfo>, int> & b) -> bool
        { return a.second > b.second; }
    );

	std::vector<std::shared_ptr<FileInfo>> out;
	for (auto& item: data->files_to_process) out.push_back(item.first);
	return out;
}

const std::string& FileTree::GetTreeName() { return data->tree_name; }
const time_t FileTree::GetConstructTime() { return data->construct_time; }

void FileTree::SaveTree() {
    std::string temp_name = Config::GetTreeFilename(data->tree_name)+".tmp";
    std::ofstream os(temp_name, std::ios::binary);
    {
        //cereal::JSONOutputArchive archive(os);
        cereal::BinaryOutputArchive archive(os);
        archive(cereal::make_nvp("FileTreeData", data));
    }
    // Need to unallocate archive (to finish the data) before closing ofstream
    os.close();
    rename(temp_name.c_str(), Config::GetTreeFilename(data->tree_name).c_str());

    if (!data->in_tree_list) FileTree::history_trees_list.clear(); // To recache this
}

std::shared_ptr<FileTree> FileTree::GetPrevTree() { return GetHistoryTree(data->prev_version_tree_name); }
std::shared_ptr<FileTree> FileTree::GetNextTree() {
    auto trees = GetHistoryTreeList();
    for (auto i = trees.rbegin(); i != trees.rend(); ++i) {
        if (GetTreeName().compare(*i) == 0) break;
        auto tree = GetHistoryTree(*i);
        if (tree->GetPrevTree()->GetTreeName() == GetTreeName()) return(tree);
    }

    return nullptr;
}

}
CEREAL_CLASS_VERSION(FenixBackup::FileTree::FileTreeData, 1);
