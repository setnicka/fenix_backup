#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <memory>
#include <ctime>
#include <sys/stat.h>

#include "Global.hpp"

namespace FenixBackup {

class FileInfo {
  public:
	FileInfo();
	FileInfo(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name);
	virtual ~FileInfo();
	/*FileInfo(const FileInfo& other);
	FileInfo& operator=(const FileInfo& other);*/

    const std::string& GetName();
	file_type GetType();
	version_file_status GetVersionStatus();
	file_params GetParams();
	unsigned int GetId();
	const std::string& GetPath();
	unsigned int GetPrevVersionId();
	const std::string& GetFileHash();
	const std::string& GetChunkName();

	void SetParams(file_params params);
	void SetStatus(version_file_status status);
	void SetId(unsigned int index);
	void SetPrevVersionId(unsigned int index);
	void SetFileHash(std::string file_hash);
	void SetChunkName(std::string file_chunk_name);

	std::shared_ptr<FileInfo> GetPrevVersion();
	std::shared_ptr<FileInfo> GetParent();

	void AddChild(std::string const& name, std::shared_ptr<FileInfo> child);
	std::shared_ptr<FileInfo> GetChild(std::string const& name);
	const std::unordered_map<std::string, std::shared_ptr<FileInfo>>& GetChilds();

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
