#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

#include <unordered_map>
#include <boost/filesystem.hpp>

#include "FenixExceptions.hpp"
#include "adapters/LocalFilesystemAdapter.hpp"

namespace FenixBackup {

class LocalFilesystemAdapter::LocalFilesystemAdapterData {
public:
    void ScanFile(std::shared_ptr<FileInfo> parent, boost::filesystem::path& path);
    file_params GetParams(boost::filesystem::path& path);
    void ScanFilesInDirectory(std::shared_ptr<FileInfo> directory, boost::filesystem::path& directory_path);

    std::string path;
    std::shared_ptr<FileTree> tree;

    std::unordered_map<std::shared_ptr<FileInfo>, boost::filesystem::path> path_cache;
};

file_params LocalFilesystemAdapter::LocalFilesystemAdapterData
::GetParams(boost::filesystem::path& path) {
    file_params params;

    struct stat info;
    lstat(path.c_str(), &info);

    params.device = info.st_dev;
    params.inode = info.st_ino;
    params.permissions = info.st_mode;
    params.uid = info.st_uid;
    params.gid = info.st_gid;
    params.modification_time = info.st_mtim;
    params.file_size = info.st_size;

    return params;
}

void LocalFilesystemAdapter::LocalFilesystemAdapterData
::ScanFile(std::shared_ptr<FileInfo> parent, boost::filesystem::path& path) {
    auto params = GetParams(path);

    if (boost::filesystem::is_directory(path)) {
        auto dir = tree->AddDirectory(parent, path.filename().string(), params);
        if (dir != nullptr) ScanFilesInDirectory(dir, path);
    } else if (boost::filesystem::is_regular_file(path)) {
        auto file = tree->AddFile(parent, path.filename().string(), params);
        if (file != nullptr) path_cache.insert(std::make_pair(file, path));
    } else if (boost::filesystem::is_symlink(path)) {
        auto file = tree->AddSymlink(parent, path.filename().string(), params);
        if (file != nullptr) path_cache.insert(std::make_pair(file, path));
    } else {
        throw AdapterException("Unknown type of the file '"+path.string()+"'\n");
    }
}

void LocalFilesystemAdapter::LocalFilesystemAdapterData
::ScanFilesInDirectory(std::shared_ptr<FileInfo> directory, boost::filesystem::path& directory_path) {
    try {
        if (boost::filesystem::exists(directory_path) && boost::filesystem::is_directory(directory_path)) {
            for (boost::filesystem::directory_iterator file(directory_path); file != boost::filesystem::directory_iterator(); ++file) {
                auto path = file->path();
                ScanFile(directory, path);
            }
        }
    } catch (const boost::filesystem::filesystem_error& ex) {
        // TODO: What to do when reading from filesystem failed? Exit or log to logfile and continue?
    }
}

////////////////////////////////////////////////////////////////////////////////


LocalFilesystemAdapter::LocalFilesystemAdapter(): data{new LocalFilesystemAdapterData()} {}
LocalFilesystemAdapter::LocalFilesystemAdapter(std::string tree_name): LocalFilesystemAdapter() {
    data->tree = FileTree::GetHistoryTree(tree_name);
}
LocalFilesystemAdapter::~LocalFilesystemAdapter() {}

void LocalFilesystemAdapter::SetPath(std::string path) { data->path = path; }
std::shared_ptr<FileTree> LocalFilesystemAdapter::GetTree() { return data->tree; }
void LocalFilesystemAdapter::SetTree(std::shared_ptr<FileTree> tree) { data->tree = tree; }

std::shared_ptr<FileTree> LocalFilesystemAdapter::Scan() {
    data->tree = FileTree::CreateNewTree();

    // Scan all files in given path in filesystem and save them into tree
    auto path = boost::filesystem::path(data->path);
    data->tree->GetRoot()->SetParams(data->GetParams(path));
    data->ScanFilesInDirectory(data->tree->GetRoot(), path);

    data->tree->SaveTree();
    return data->tree;
}

void LocalFilesystemAdapter::GetAndProcess(std::shared_ptr<FileInfo> file) {
    // 1. Get filename
	std::string filename;
	auto i = data->path_cache.find(file);
	if (i == data->path_cache.end()) filename = data->path + file->GetPath();
	else filename = (i->second).string();

    // 2. Get content
    if (file->GetType() == FILE) {
        std::ifstream is(filename, std::ifstream::binary);
        if (!is.good()) throw AdapterException("Cannot read from file '"+filename+"'");
        file->ProcessFileContent(is, data->tree);
    } else if (file->GetType() == SYMLINK) {
        char buf[1024];
        int count = readlink(filename.c_str(), buf, sizeof(buf));
        if (count >= 0) {
            buf[count] = '\0';
            std::istringstream ss(buf);
            file->ProcessFileContent(ss, data->tree);
        }
    }
}

// In this case remote is equal to local
void LocalFilesystemAdapter::RestoreFile(std::shared_ptr<FileInfo> file, restore_mode mode, restore_tactic tactic) {
    RestoreFileToLocalPath(file, data->path, mode, tactic);
}
void LocalFilesystemAdapter::RestoreFileToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path,
                                                     restore_mode mode, restore_tactic tactic, bool preserve_inbackup_path)
{
    RestoreFileToLocalPath(file, path, mode, tactic, preserve_inbackup_path);
}

void LocalFilesystemAdapter::RestoreFileToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path,
                                                    restore_mode mode, restore_tactic tactic, bool preserve_inbackup_path)
{
    // 1. Restore newest known version
    if (tactic == NEWEST_KNOWN_VERSION && data->tree != nullptr && file->GetStatus() == NOT_UPDATED) {
        auto last_tree = data->tree->GetPrevTree();
        while (last_tree != nullptr && file->GetStatus() == NOT_UPDATED) {
            file = last_tree->GetFileById(file->GetPrevVersionId());
            last_tree = last_tree->GetPrevTree();
        }
    }

    boost::filesystem::path final_path(path);
    // 2. Make path if needed
    if (preserve_inbackup_path) {
        final_path /= file->GetPath();
        boost::filesystem::create_directories(final_path.parent_path());
    }

    // 3. Restore original content
    if (mode != ONLY_PERMISSIONS) {
        // version status NOT_UPDATED, UNKNOWN or DELETED
        if (file->GetType() != DIR && file->GetHash().empty()) return;
        if (file->GetStatus() == DELETED) return;

        if (file->GetType() == DIR) {
            if (!boost::filesystem::exists(final_path)) {
                if (!boost::filesystem::create_directory(final_path)) throw AdapterException("Cannot create directory '"+final_path.string()+"'");
            }
        } else if (file->GetType() == SYMLINK) {
            // First remove, beware of outer hardlinks
            boost::filesystem::remove(final_path);
            std::stringstream ss;
            file->GetFileContent(ss);
            boost::filesystem::path to(ss.str());
            boost::filesystem::create_symlink(to, final_path);
        } else {
            // First remove, beware of outer hardlinks
            boost::filesystem::remove(final_path);
            std::ofstream os(final_path.string());
            file->GetFileContent(os);
            os.close();
        }
    }

    // 4. Restore permissions
    if (mode != ONLY_DATA) {
        file_params params = file->GetParams();
        auto cstring_path = final_path.c_str();
        // XXX: lchmod is not implemented and will always fail -> use normal chmod on everything except symlinks
        if (file->GetType() != SYMLINK) chmod(cstring_path, params.permissions);
        lchown(cstring_path, params.uid, params.gid);

        struct timeval new_times[2];
        // No changes to acces time, no nanoseconds
        new_times[1].tv_sec = params.modification_time.tv_sec;
        new_times[1].tv_usec = params.modification_time.tv_nsec/1000;
        lutimes(cstring_path, new_times);
    }
}

}
