#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <memory>

#include <ctime>
#include <sys/stat.h>

namespace FenixBackup {

enum file_type { DIR, FILE };

enum version_file_status { UNKNOWN_FILE, NEW_FILE, UNCHANGED_FILE, UPDATED_FILE, NOT_UPDATED_FILE };
// UNKNOWN_FILE - new file, which is not yet saved (there isn't any older file)
// NEW_FILE - new file, which is saved
// UNCHANGED_FILE - same as older saved version
// UPDATED_FILE - updated content (and params) of the file (diff with older version or with empty file when new file)
// NOT_UPDATED_FILE - there are changes, but content wasn't yet saved (there is older version)
//
// Paths: UNKNOWN_FILE->NEW_FILE, NOT_UPDATED_FILE->UPDATED_FILE

struct file_params {
	mode_t	permissions;		// protection
	uid_t	st_uid;				// user ID of owner
	gid_t	st_gid;				// group ID of owner
	off_t	file_size;			// total size, in bytes
	// time_t	access_time;		// time of last access
	time_t	modification_time;	// time of last modification
	// TODO: ACL

	bool operator==(const file_params &second) const;
	bool operator!=(const file_params &second) const;
};

class FileInfo {
  public:
	FileInfo();
	FileInfo(file_type type, std::shared_ptr<FileInfo> parent, std::string const& name);
	virtual ~FileInfo();
	/*FileInfo(const FileInfo& other);
	FileInfo& operator=(const FileInfo& other);*/

	file_type GetType();
	version_file_status GetVersionStatus();
	file_params GetParams();
	int GetId();
	int GetPrevVersionId();

	void SetParams(file_params params);
	void SetStatus(version_file_status status);
	void SetId(int index);
	void SetPrevVersionId(int index);

	void AddChild(std::string const& name, std::shared_ptr<FileInfo> child);
	std::shared_ptr<FileInfo> GetChild(std::string const& name);

  private:
	class FileInfoData;
	std::unique_ptr<FileInfoData> data;
};

}
#endif // FILEINFO_HPP
