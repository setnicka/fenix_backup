#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <memory>
#include <ctime>
#include <sys/stat.h>

namespace FenixBackup {
    class FileInfo;
}

#include "Global.hpp"
#include "FileTree.hpp"

namespace FenixBackup {

class FileInfo {
  public:
	FileInfo();
	FileInfo(std::shared_ptr<FileTree> tree, file_type type, std::shared_ptr<FileInfo> parent, std::string const& name);
	virtual ~FileInfo();
	/*FileInfo(const FileInfo& other);
	FileInfo& operator=(const FileInfo& other);*/

    const std::string& GetName();
	const file_params& GetParams();
	version_file_status GetStatus();
	file_type GetType();
	unsigned int GetId();
	const std::string& GetPath();
	unsigned int GetPrevVersionId();
	const std::string& GetHash();
	std::shared_ptr<FileTree> GetTree();

	void SetParams(const file_params& params);
	void SetStatus(version_file_status status);
	void SetId(unsigned int index);
	void SetPrevVersionId(unsigned int index);
	void SetHash(const std::string& file_hash);
	void SetTree(std::shared_ptr<FileTree> tree);

	std::shared_ptr<FileInfo> GetPrevVersion();
	std::shared_ptr<FileInfo> GetParent();

	void AddChild(std::string const& name, std::shared_ptr<FileInfo> child);
	std::shared_ptr<FileInfo> GetChild(std::string const& name);
	const std::unordered_map<std::string, std::shared_ptr<FileInfo>>& GetChilds();

	// In the packing process
	void ProcessFileContent(std::istream& file, std::shared_ptr<FileTree> tree = nullptr);
	std::ostream& GetFileContent(std::ostream& out);

    class FileInfoData;
  private:
	std::unique_ptr<FileInfoData> data;

    void serialize_internal(cereal::JSONOutputArchive & archive);
    void serialize_internal(cereal::JSONInputArchive & archive);
    void serialize_internal(cereal::BinaryOutputArchive & archive);
    void serialize_internal(cereal::BinaryInputArchive & archive);

    template<class Archive>
    void serialize(Archive & archive) {
        serialize_internal(archive);
    }

    friend class cereal::access;
};



}
#endif // FILEINFO_HPP
