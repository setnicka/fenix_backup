#include <sys/types.h>
#include <memory>

#ifndef BACKUPCLEANER_HPP
#define BACKUPCLEANER_HPP

namespace FenixBackup {

class BackupCleaner {
  public:
    BackupCleaner();
    virtual ~BackupCleaner();

    void LoadData();

    /// Delete one file chunk and return the ammount of
    size_t Clean();
  private:
    class BackupCleanerData;
    std::unique_ptr<BackupCleanerData> data;
};

}

#endif // BACKUPCLEANER_HPP
