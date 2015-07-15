#include <unordered_map>
#include <string>

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

#include "FenixExceptions.hpp"
#include "FileInfo.hpp"

namespace FenixBackup {

bool file_params::operator==(const file_params &second) const {
    return (permissions == second.permissions
            && uid == second.uid
            && gid == second.gid
            && file_size == second.file_size
            && modification_time == second.modification_time);
}
bool file_params::operator!=(const file_params &second) const { return !operator==(second); }

// Hide data from .hpp file using PIMP idiom
class FileInfo::FileInfoData {
  public:
	file_type type;
	version_file_status version_status = UNKNOWN;
	file_params params = {};

	// Versioning
	// std::string last_version;  // name of the last version
    unsigned int file_index;
    unsigned int prev_version_file_index = 0;  // index of the file in the last version, 0 = no previous version

    std::shared_ptr<FileInfo> parent;
    std::string name;

	// For directory
	std::unordered_map<std::string, std::shared_ptr<FileInfo>> files;
	// For ordinal file
	std::string file_hash;
	std::string file_chunk_name;

    // Cache (not serialize)
    std::string path;

	FileInfoData(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name):
		type{type}, parent{parent}, name{name} {}

    FileInfoData() : FileInfoData(DIR, nullptr, "") {}

    template <class Archive>
    void serialize(Archive & ar, std::uint32_t const version) {
        if (version <= 1) ar(
            cereal::make_nvp("name", name),
            cereal::make_nvp("type", type),
            cereal::make_nvp("version_status", version_status),
            cereal::make_nvp("params", params),
            cereal::make_nvp("file_index", file_index),
            cereal::make_nvp("prev_version_file_index", prev_version_file_index),
            cereal::make_nvp("parent", parent),
            cereal::make_nvp("files", files),
            cereal::make_nvp("file_hash", file_hash),
            cereal::make_nvp("file_chunk_name", file_chunk_name)
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

// Setters
void FileInfo::SetParams(file_params params) { data->params = params; }
void FileInfo::SetStatus(version_file_status status) { data->version_status = status; }
void FileInfo::SetId(unsigned int index) { data->file_index = index; }
void FileInfo::SetPrevVersionId(unsigned int index) { data->prev_version_file_index = index; }
void FileInfo::SetFileHash(std::string file_hash) { data->file_hash = file_hash; }
void FileInfo::SetChunkName(std::string file_chunk_name) { data->file_chunk_name = file_chunk_name; }
// Getters
const std::string& FileInfo::GetName() { return data->name; }
file_type FileInfo::GetType() { return data->type; }
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
unsigned int FileInfo::GetPrevVersionId() { return data->prev_version_file_index; }
const std::string& FileInfo::GetFileHash() { return data->file_hash; }
const std::string& FileInfo::GetChunkName() { return data->file_chunk_name; }

}
CEREAL_CLASS_VERSION(FenixBackup::FileInfo::FileInfoData, 1);
