#include "adapters/Adapter.hpp"

namespace FenixBackup {

Adapter::Adapter() {}
Adapter::~Adapter() {}

void Adapter::RestoreSubtree(std::shared_ptr<FileInfo> file, restore_mode mode) {
    if (mode != ONLY_PERMISSIONS) RestoreFile(file, ONLY_DATA);
    if (file->GetType() == DIR)
        for(auto& item: file->GetChilds()) RestoreSubtree(item.second, mode);
    if (mode != ONLY_DATA) RestoreFile(file, ONLY_PERMISSIONS);
}
void Adapter::RestoreSubtreeToRemotePath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode, bool preserve_inbackup_path) {
    if (mode != ONLY_PERMISSIONS) RestoreFileToRemotePath(file, path, ONLY_DATA, preserve_inbackup_path);
    if (file->GetType() == DIR)
        for(auto& item: file->GetChilds()) {
            if (preserve_inbackup_path) RestoreSubtreeToRemotePath(item.second, path, mode, true);
            else RestoreSubtreeToRemotePath(item.second, path+"/"+item.second->GetName(), mode, false);
        }
    if (mode != ONLY_DATA) RestoreFileToRemotePath(file, path, ONLY_PERMISSIONS, preserve_inbackup_path);
}
void Adapter::RestoreSubtreeToLocalPath(std::shared_ptr<FileInfo> file, const std::string& path, restore_mode mode, bool preserve_inbackup_path) {
    if (mode != ONLY_PERMISSIONS) RestoreFileToLocalPath(file, path, ONLY_DATA, preserve_inbackup_path);
    if (file->GetType() == DIR)
        for(auto& item: file->GetChilds()) {
            if (preserve_inbackup_path) RestoreSubtreeToLocalPath(item.second, path, mode, true);
            else RestoreSubtreeToLocalPath(item.second, path+"/"+item.second->GetName(), mode, true);
        }
    if (mode != ONLY_DATA) RestoreFileToLocalPath(file, path, ONLY_PERMISSIONS, preserve_inbackup_path);
}

}
