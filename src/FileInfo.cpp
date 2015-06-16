#include <unordered_map>
#include <string>

#include "FileInfo.hpp"

namespace FenixBackup {

// Hide data from .hpp file using PIMP idiom
class FileInfo::FileInfoData {
  public:
	file_type type;
	version_file_status version_status;
	file_params params;

	// Versioning
	// std::string last_version;  // name of the last version
    unsigned int file_index;
    unsigned int prev_version_file_index;  // index of the file in the last version

    std::shared_ptr<FileInfo> parent;
    std::string const& name;

	// For directory
	std::unordered_map<std::string, std::shared_ptr<FileInfo>> files;
	// For ordinal file
	std::string data_block;  // hash of the corresponding data block

	FileInfoData(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name):
		type{type}, parent{parent}, name{name} {}

};
// Constructors
FileInfo::FileInfo(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name):
	data{new FileInfoData(type, parent, name)} {}
FileInfo::FileInfo(): FileInfo(DIR, nullptr, nullptr) {}

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
void FileInfo::SetId(int index) { data->file_index = index; }
void FileInfo::SetPrevVersionId(int index) { data->prev_version_file_index = index; }
// Getters
file_type FileInfo::GetType() { return data->type; }
version_file_status FileInfo::GetVersionStatus() { return data->version_status; }
file_params FileInfo::GetParams() { return data->params; }
int FileInfo::GetId() { return data->file_index; }
int FileInfo::GetPrevVersionId() { return data->prev_version_file_index; }

}
