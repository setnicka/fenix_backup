#include <unordered_map>
#include <string>

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

#include "FenixExceptions.hpp"
#include "FileInfo.hpp"
#include "FileChunk.hpp"
#include "Functions.hpp"

namespace FenixBackup {

// Hide data from .hpp file using PIMPL idiom
class FileInfo::FileInfoData {
  public:
	file_type type;
	version_file_status version_status = UNKNOWN;
	file_params params = {};

	// Versioning
    unsigned int file_index;
    unsigned int prev_version_id = 0;  // index of the file in the last version, 0 = no previous version

    std::shared_ptr<FileInfo> parent;
    std::string name;

	// For directory
	std::unordered_map<std::string, std::shared_ptr<FileInfo>> files;
	// For ordinal file
	std::string file_hash;

    // Cache (not serialize)
    std::string path;

	FileInfoData(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name):
		type{type}, parent{parent}, name{name} {}

    FileInfoData() : FileInfoData(DIR, nullptr, "") {}

    template <class Archive>
    void serialize(Archive & ar, std::uint32_t const version) {
        if (version == 1) ar(
            cereal::make_nvp("name", name),
            cereal::make_nvp("type", type),
            cereal::make_nvp("version_status", version_status),
            cereal::make_nvp("params", params),
            cereal::make_nvp("file_index", file_index),
            cereal::make_nvp("prev_version_id", prev_version_id),
            cereal::make_nvp("parent", parent),
            cereal::make_nvp("file_hash", file_hash),
            cereal::make_nvp("files", files)
        );
        else throw FileInfoException("Unknown version "+std::to_string(version)+" of FileInfo serialized data\n");
    }
};

// Constructors
FileInfo::FileInfo(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name):
	data{new FileInfoData(type, parent, name)} {}
FileInfo::FileInfo(): FileInfo(DIR, nullptr, "") {}

FileInfo::~FileInfo() {}

/*
FileInfo::FileInfo(const FileInfo& other) {
	//copy ctor
}

FileInfo& FileInfo::operator=(const FileInfo& rhs) {
	if (this == &rhs) return *this; // handle self assignment
	//assignment operator
	return *this;
}
*/

// Serialization -- must explicitly specify each archive type because of Cereal checks
void FileInfo::serialize_internal(cereal::JSONOutputArchive & archive) {
  archive(cereal::make_nvp("FileInfoData", data));
}
void FileInfo::serialize_internal(cereal::JSONInputArchive & archive) {
  archive(cereal::make_nvp("FileInfoData", data));
}
void FileInfo::serialize_internal(cereal::BinaryOutputArchive & archive) {
  archive(cereal::make_nvp("FileInfoData", data));
}
void FileInfo::serialize_internal(cereal::BinaryInputArchive & archive) {
  archive(cereal::make_nvp("FileInfoData", data));
}

void FileInfo::AddChild(std::string const& name, std::shared_ptr<FileInfo> child) {
	if (data->type != DIR) throw std::runtime_error("Cannot add child to not-dir FileInfo");
	data->files.insert(std::make_pair(name, child));
}
std::shared_ptr<FileInfo> FileInfo::GetChild(std::string const& name) {
	if (data->type != DIR) throw std::runtime_error("Cannot get child from not-dir FileInfo");
	auto i = data->files.find(name);
	if (i == data->files.end()) return nullptr;
	else return i->second;
}

const std::unordered_map<std::string, std::shared_ptr<FileInfo>>& FileInfo::GetChilds() { return data->files; }

std::ostream& FileInfo::GetFileContent(std::ostream& out) {
    if (data->type == DIR) throw FileInfoException("Cannot get content of directory\n");

    auto chunk = FileChunk::GetChunk(data->file_hash);
    out << chunk->LoadAndReturn();
    return out;
}

void FileInfo::ProcessFileContent(std::istream& file, std::shared_ptr<FileTree> tree) {
    if (data->type == DIR) throw FileInfoException("Cannot process content for directory\n");

    // 1. Count SHA256 hash of the given file
    data->file_hash = Functions::ComputeFileHash(file);

    //data->file_hashes.insert(std::make_pair(file_hash, file_node));

    // 2. If UNKNOWN ancestor try to localize it using file_hash
    if (data->version_status == UNKNOWN && tree != nullptr  && tree->GetPrevTree() != nullptr) {
        auto prev_version_node = tree->GetPrevTree()->GetFileByHash(data->file_hash);
        if (prev_version_node != nullptr) {
            data->prev_version_id = prev_version_node->GetId();
            // file_node->SetChunkName(prev_version_node->GetChunkName());
            data->version_status = (data->params == prev_version_node->GetParams() ? UNCHANGED : UPDATED_PARAMS);
            return;
        }
    }

    // If file has the same size and same hash as older file, there were only params updated
    if (tree != nullptr && tree->GetPrevTree() != nullptr && data->prev_version_id != 0) {
            auto prev_version_node = tree->GetPrevTree()->GetFileById(data->prev_version_id);
            if (prev_version_node != nullptr
                && data->params.file_size == prev_version_node->GetParams().file_size
                && data->file_hash == prev_version_node->GetHash()
            ) {
                data->version_status = UPDATED_PARAMS;
                return;
            }
    }

    // 3. Compute and save FileChunk
    //std::string chunk_name = GetTreeName() + "_" + std::to_string(file_node->GetId());

    // 3. Test if exists chunk for this file_hash and eventually save it
    if (FileChunk::GetChunk(data->file_hash) == nullptr) {
        FileChunk chunk(data->file_hash);
        std::string prev_chunk = (data->prev_version_id != 0 ? tree->GetPrevTree()->GetFileById(data->prev_version_id)->GetHash() : "" );
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        chunk.ProcessStringAndSave(prev_chunk, content);

    }

    // 4. Create file chunk

    // 4. Update info for this file
    data->version_status = (data->version_status == UNKNOWN ? NEW : UPDATED_FILE);

    //data->file_hashes.insert(std::make_pair(file_hash, file_node));
}

// Setters
void FileInfo::SetParams(file_params params) { data->params = params; }
void FileInfo::SetStatus(version_file_status status) { data->version_status = status; }
void FileInfo::SetId(unsigned int index) { data->file_index = index; }
void FileInfo::SetPrevVersionId(unsigned int index) { data->prev_version_id = index; }
void FileInfo::SetHash(std::string file_hash) { data->file_hash = file_hash; }
// Getters
const std::string& FileInfo::GetName() { return data->name; }
file_type FileInfo::GetType() { return data->type; }
version_file_status FileInfo::GetStatus() { return data->version_status; }
version_file_status FileInfo::GetVersionStatus() { return data->version_status; }
file_params FileInfo::GetParams() { return data->params; }
unsigned int FileInfo::GetId() { return data->file_index; }
const std::string& FileInfo::GetPath() {
    if (data->path.empty()) {
        auto parent = GetParent();
        if (parent != nullptr) data->path = parent->GetPath() + "/" + GetName();
    }
    return data->path;
}
std::shared_ptr<FileInfo> FileInfo::GetParent() { return data->parent; }
unsigned int FileInfo::GetPrevVersionId() { return data->prev_version_id; }
const std::string& FileInfo::GetHash() { return data->file_hash; }

}
CEREAL_CLASS_VERSION(FenixBackup::FileInfo::FileInfoData, 1);
