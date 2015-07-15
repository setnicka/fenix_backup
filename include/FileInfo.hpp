#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/access.hpp>

#include <memory>
#include <ctime>
#include <sys/stat.h>

namespace FenixBackup {

enum file_type { DIR, FILE };

enum version_file_status { UNKNOWN, NEW, UNCHANGED, UPDATED_PARAMS, UPDATED_FILE, NOT_UPDATED };
// UNKNOWN - new file, which is not yet saved (there isn't any older file)
// NEW - new file, which is saved
// UNCHANGED - same as older saved version
// UPDATED_PARAMS - updated only params
// UPDATED_FILE - updated content (and params) of the file (diff with older version or with empty file when new file)
// NOT_UPDATED - there are changes, but content wasn't yet saved (there is older version)
//
// Paths: UNKNOWN->NEW, NOT_UPDATED->UPDATED_FILE

struct file_params {
	mode_t	permissions;		// protection
	uid_t	uid;				// user ID of owner
	gid_t	gid;				// group ID of owner
	off_t	file_size;			// total size, in bytes
	// time_t	access_time;		// time of last access
	time_t	modification_time;	// time of last modification
	// TODO: ACL

	bool operator==(const file_params &second) const;
	bool operator!=(const file_params &second) const;

    template <class Archive>
    void serialize(Archive & ar) {
        ar(
            cereal::make_nvp("permissions", permissions),
            cereal::make_nvp("uid", uid),
            cereal::make_nvp("gid", gid),
            cereal::make_nvp("file_size", file_size),
            cereal::make_nvp("modification_time", modification_time)
        );
    }
};

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
