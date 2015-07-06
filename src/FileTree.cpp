#include <fstream>
#include <ctime>
#include <boost/filesystem.hpp>

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>

#include "FenixExceptions.hpp"
#include "FileTree.hpp"
#include "FileChunk.hpp"
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

	// Versioning
	std::string prev_version_tree_name;
	std::string prev_version_hash;

	std::vector<std::shared_ptr<FileInfo>> files;
	std::unordered_map<std::string, std::shared_ptr<FileInfo>> file_hashes;

    std::vector<std::pair<std::shared_ptr<FileInfo>, int>> files_to_process;

	std::shared_ptr<FileInfo> AddNode(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name, file_params params);
	void CountScore(std::shared_ptr<FileInfo> file);

    template <class Archive>
    void serialize(Archive & ar, std::uint32_t const version) {
        if (version <= 1) ar(
            cereal::make_nvp("tree_name", tree_name),
            cereal::make_nvp("prev_version_tree_name", prev_version_tree_name),
            cereal::make_nvp("prev_version_hash", prev_version_hash),
            cereal::make_nvp("root", root),
            cereal::make_nvp("filesById", files),
            cereal::make_nvp("filesByHash", file_hashes)
        );
        else throw FileTreeException("Unknown version "+std::to_string(version)+" of FileTree serialized data\n");
    }
};

FileTree::FileTreeData::FileTreeData(bool initialize) {
    if (!initialize) return;
    root = std::make_shared<FileInfo>(DIR, nullptr, "");
    root->SetId(1); root->SetPrevVersionId(1);
    files.push_back(nullptr);
    files.push_back(root);

    // Construct tree name from current datetime
    std::time_t rawtime;
    std::tm* timeinfo;
    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);
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

void FileTree::FileTreeData::CountScore(std::shared_ptr<FileInfo> file) {
    // TODO: Counting score according to Config
    int score = 1;
    files_to_process.push_back(std::make_pair(file, score));
}

////////////////////////////////////////////////////////////////////////////////

FileTree::FileTree(): data{new FileTreeData(true)} {}

FileTree::FileTree(std::string name) {
	// Load data from given FileTree name
    std::ifstream is(Config::GetTreeFilename(name));
    if (!is.good()) throw FileTreeException("Couldn't load FileTree '"+name+"' (from filename '"+Config::GetTreeFilename(name)+"')\n");
    cereal::JSONInputArchive archive(is);

    archive(data);
}

FileTree::~FileTree() { }

/*FileTree::FileTree(const FileTree& other) {
	//copy ctor
}*/

std::shared_ptr<FileInfo> FileTree::GetRoot() { return data->root; }

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
	return history_trees[name];
}

std::shared_ptr<FileTree> FileTree::GetNewTree() {
    auto tree = std::make_shared<FileTree>();
    history_trees.insert(std::make_pair(tree->GetTreeName(), tree));
    return tree;
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
				if (type == DIR) {
					// Check only params
					status = (params == prev_version_params ? UNCHANGED : UPDATED_PARAMS);
				} else {
					if (prev_version_params.file_size != params.file_size || prev_version_params.modification_time != params.modification_time)
                        status = NOT_UPDATED;
                    else {
                            status = (params == prev_version_params ? UNCHANGED : UPDATED_PARAMS);
                            // No change to the file content -> same hash and chunk name as the previous
                            file->SetFileHash(prev_version_file->GetFileHash());
                            file->SetChunkName(prev_version_file->GetChunkName());
                            file_hashes.insert(std::make_pair(file->GetFileHash() ,file));
                    }
				}
			}
		}
	}
	if (type == DIR && status == UNKNOWN) status = NEW; // When DIR, there are no data to save -> we have done all work for this file
	file->SetStatus(status);

	// Count score and add file to score-heap
	if (type == FILE && status != UNCHANGED && status != UPDATED_PARAMS) CountScore(file);

	return file;
}


std::shared_ptr<FileInfo> FileTree::GetFileByPath(std::string const& path) {
	return nullptr;
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

void FileTree::SaveTree() {
    std::string temp_name = Config::GetTreeFilename(data->tree_name)+".tmp";
    std::ofstream os(temp_name, std::ios::binary);
    {
        cereal::JSONOutputArchive archive(os);
        archive(cereal::make_nvp("FileTreeData", data));
    }
    // Need to unallocate archive (to finish the data) before closing ofstream
    os.close();
    rename(temp_name.c_str(), Config::GetTreeFilename(data->tree_name).c_str());
}

std::shared_ptr<FileTree> FileTree::GetPrevVersion() { return GetHistoryTree(data->prev_version_tree_name); }
std::shared_ptr<FileTree> FileTree::GetNextVersion() {
    auto trees = GetHistoryTreeList();
    for (auto i = trees.rbegin(); i != trees.rend(); ++i) {
        if (GetTreeName().compare(*i) == 0) break;
        auto tree = GetHistoryTree(*i);
        if (tree->GetPrevVersion()->GetTreeName() == GetTreeName()) return(tree);
    }

    return nullptr;
}

std::ostream& FileTree::GetFileContent(std::shared_ptr<FileInfo> file_node, std::ostream& out) {
    if (file_node != GetFileById(file_node->GetId())) throw FileTreeException("Cannot call GetFileContent with FileInfo from different FileTree\n");
    if (file_node->GetType() == DIR) throw FileTreeException("Cannot get content of directory\n");

    auto chunk = FileChunk::GetChunk(file_node->GetChunkName());
    out << chunk->LoadAndReturn();
    return out;
}

void FileTree::ProcessFileContent(std::shared_ptr<FileInfo> file_node, std::istream& file) {
    if (file_node->GetId() >= data->files.size() || file_node != GetFileById(file_node->GetId()))
        throw FileTreeException("Cannot call ProcessFileContent with FileInfo from different FileTree\n");
    if (file_node->GetType() == DIR) throw FileTreeException("Cannot process content for directory\n");

    // 1. Count SHA256 hash of the given file
    std::string file_hash = Functions::ComputeFileHash(file);

    file_node->SetFileHash(file_hash);
    data->file_hashes.insert(std::make_pair(file_hash, file_node));

    // 2. If UNKNOWN ancestor try to localize it using file_hash
    if (file_node->GetVersionStatus() == UNKNOWN && GetPrevVersion() != nullptr) {
        auto prev_version_node = GetPrevVersion()->GetFileByHash(file_hash);
        if (prev_version_node != nullptr) {
            file_node->SetPrevVersionId(prev_version_node->GetId());
            file_node->SetChunkName(prev_version_node->GetChunkName());
            auto status = (file_node->GetParams() == prev_version_node->GetParams() ? UNCHANGED : UPDATED_PARAMS);
            file_node->SetStatus(status);
            return;
        }
    }

    // If file has the same size and same hash as older file, there were only params updated
    if (GetPrevVersion() != nullptr && file_node->GetPrevVersionId() != 0) {
            auto prev_version_node = GetPrevVersion()->GetFileById(file_node->GetPrevVersionId());
            if (prev_version_node != nullptr
                && file_node->GetParams().file_size == prev_version_node->GetParams().file_size
                && file_node->GetFileHash() == prev_version_node->GetFileHash()
            ) {
                file_node->SetStatus(UPDATED_PARAMS);
                return;
            }
    }

    // 3. Compute and save FileChunk
    std::string chunk_name = GetTreeName() + "_" + std::to_string(file_node->GetId());
    FileChunk chunk(chunk_name);

    std::string prev_chunk = (file_node->GetPrevVersionId() != 0 ? GetPrevVersion()->GetFileById(file_node->GetPrevVersionId())->GetChunkName() : "" );
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    chunk.ProcessStringAndSave(prev_chunk, content);

    // 4. Update FileInfo for this file and save FileTree
    file_node->SetFileHash(file_hash);
    file_node->SetChunkName(chunk_name);
    file_node->SetStatus(UPDATED_FILE);
    data->file_hashes.insert(std::make_pair(file_hash, file_node));

    // Save tree or not?
    //SaveTree();
}

}
CEREAL_CLASS_VERSION(FenixBackup::FileTree::FileTreeData, 1);
