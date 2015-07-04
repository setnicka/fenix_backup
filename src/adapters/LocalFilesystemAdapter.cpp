#include <boost/filesystem.hpp>

#include "FenixExceptions.hpp"
#include "adapters/LocalFilesystemAdapter.hpp"

#include <sys/stat.h>

namespace FenixBackup {

class LocalFilesystemAdapter::LocalFilesystemAdapterData {
public:
    void ScanFile(std::shared_ptr<FileInfo> parent, boost::filesystem::path& path);
    void ScanFilesInDirectory(std::shared_ptr<FileInfo> directory, boost::filesystem::path& directory_path);

    std::string path;
    std::shared_ptr<FileTree> tree;
};

void LocalFilesystemAdapter::LocalFilesystemAdapterData
::ScanFile(std::shared_ptr<FileInfo> parent, boost::filesystem::path& path) {
    file_params params;

    struct stat info;
    stat(path.c_str(), &info);
    params.permissions = info.st_mode;
    params.uid = info.st_uid;
    params.gid = info.st_gid;
    params.modification_time = info.st_mtim.tv_sec;
    params.file_size = info.st_size;

    if (boost::filesystem::is_directory(path)) {
        auto dir = tree->AddDirectory(parent, path.filename().string(), params);
        ScanFilesInDirectory(dir, path);
    } else if (boost::filesystem::is_regular_file(path)) {
        tree->AddFile(parent, path.filename().string(), params);
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
LocalFilesystemAdapter::~LocalFilesystemAdapter() {}

void LocalFilesystemAdapter::SetPath(std::string path) { data->path = path; }

std::shared_ptr<FileTree> LocalFilesystemAdapter::Scan() {
    data->tree = FileTree::GetNewTree();

    // Scan all files in given path in filesystem and save them into tree
    auto path = boost::filesystem::path(data->path);
    data->ScanFilesInDirectory(data->tree->GetRoot(), path);

    return data->tree;
}

void LocalFilesystemAdapter::GetAndProcess(std::shared_ptr<FileInfo> file) {

}

}
