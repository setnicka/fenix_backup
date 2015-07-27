#include <time.h>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

#ifndef GLOBAL_HPP
#define GLOBAL_HPP

namespace FenixBackup {

enum file_type { DIR, FILE, SYMLINK };

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
    dev_t   device;             // device number
    ino_t   inode;              // inode number
	mode_t	permissions;		// protection
	uid_t	uid;				// user ID of owner
	gid_t	gid;				// group ID of owner
	size_t	file_size;			// total size, in bytes
	// timespec access_time;    // time of last access (nanoseconds)
	// time_t	access_time;	// time of last access
	timespec modification_time; // time of last modification (nanoseconds)
	// time_t modification_time;	// time of last modification
	// TODO: ACL

	bool operator==(const file_params &second) const;
	bool operator!=(const file_params &second) const;

    template <class Archive>
    void serialize(Archive & ar) {
        ar(
            cereal::make_nvp("device", device),
            cereal::make_nvp("inode", inode),
            cereal::make_nvp("permissions", permissions),
            cereal::make_nvp("uid", uid),
            cereal::make_nvp("gid", gid),
            cereal::make_nvp("file_size", file_size),
            cereal::make_nvp("mtime_seconds", modification_time.tv_sec),
            cereal::make_nvp("mtime_nanoseconds", modification_time.tv_nsec)
        );
    }
};

inline bool operator==(const timespec& a, const timespec& b) {
    return (a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec);
}
inline bool operator!=(const timespec& a, const timespec& b) { return !(a==b); }

inline bool file_params::operator==(const file_params &second) const {
    return (permissions == second.permissions
            && uid == second.uid
            && gid == second.gid
            && file_size == second.file_size
            && modification_time == second.modification_time);
}
inline bool file_params::operator!=(const file_params &second) const { return !operator==(second); }

}

#endif // GLOBAL_HPP
