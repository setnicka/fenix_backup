#include <iostream>

#include "CLI.hpp"
#include "Config.hpp"
#include "FenixExceptions.hpp"
#include "adapters/LocalFilesystemAdapter.hpp"
#include "BackupCleaner.hpp"

namespace FenixBackup {

const char * file_type_names[] = { "DIR", "FILE", "SYMLINK" };
const char * version_file_status_names[] = { "UNKNOWN ", "NEW     ", "UNCHANGED", "UPDATED_PARAMS", "UPDATED_FILE", "NOT_UPDATED", "DELETED" };

int usage(char* argv[]) {
    std::cout << "Usage: " << argv[0] << " <config_file>" << std::endl << "And one of these commands:" << std::endl;
    std::cout << "  show backups\t\t\t(displays list of all backups)" << std::endl;
    std::cout << "  show files [<backup>]\t\t(displays list of all files in given backup)" << std::endl;
    std::cout << "  show history <backup> <path>\t(displays known history of given file)" << std::endl;
    std::cout << "  backup\t\t\t(run backup)" << std::endl;
    std::cout << "  restore full <backup>\t\t(run full restore to original path)" << std::endl;
    std::cout << "  restore full <backup> <path>\t(run full restore to given path)" << std::endl;
    std::cout << "  restore subtree <backup> <subtree_path>" << std::endl << "\t\t\t\t(restore subtree to original path)" << std::endl;
    std::cout << "  restore subtree <backup> <subtree_path> <path>" << std::endl << "\t\t\t\t(restore subtree to given path)" << std::endl;
    std::cout << "  restore file <backup> <file_path>" << std::endl << "\t\t\t\t(restore one file to original path)" << std::endl;
    std::cout << "  restore file <backup> <file_path> <path>" << std::endl << "\t\t\t\t(restore one file to given path)" << std::endl;
    std::cout << "  cleanup [<x>]\t\t\t(run <x> rounds of cleanup, default 1)" << std::endl;
    return(EXIT_FAILURE);
}

int CLI::Run(int argc, char* argv[]) {
    // Get params and actions
    if (argc < 3) return usage(argv);
    try {
        FenixBackup::Config::Load(argv[1]);
        std::string command = argv[2];
        std::string subcommand = argc > 3 ? argv[3] : "";
        ////////////////////////
        std::cout << argc << std::endl;
        if (command == "show") {
            if (subcommand == "backups") {
                auto backups = FileTree::GetHistoryTreeList();
                for(auto& backup: backups) std::cout << backup << std::endl;
            } else if (subcommand == "files" && argc <= 5) {
                std::string backup_name = (argc == 5 ? argv[4] : FileTree::GetNewestTreeName());
                auto backup = FileTree::GetHistoryTree(backup_name);
                if (backup == nullptr) {
                    std::cerr << "No backup name " << backup_name << std::endl;
                    return(EXIT_FAILURE);
                }
                for (auto& file: backup->GetAllFiles())
                    if (file != nullptr)
                        std::cout << version_file_status_names[file->GetStatus()] << "\t" << file->GetPath() << std::endl;
            } else if (subcommand == "history" && argc == 6) {
                std::string backup_name = argv[4];
                std::string backup_path = argv[5];
                auto backup = FileTree::GetHistoryTree(backup_name);
                if (backup == nullptr) {
                    std::cerr << "No backup name " << backup_name << std::endl;
                    return(EXIT_FAILURE);
                }
                auto file = backup->GetFileByPath(backup_path);
                if (file == nullptr) {
                    std::cerr << "No file '" << backup_path << "' backup name " << backup_name << std::endl;
                    return(EXIT_FAILURE);
                }
                bool first = true;
                while (file != nullptr && backup != nullptr) {
                    if (first || file->GetStatus() != UNCHANGED)
                        std::cout << backup->GetTreeName() << ": " << version_file_status_names[file->GetStatus()]
                        << "\tpath: " << file->GetPath()
                        << ", filesize " << file->GetParams().file_size
                        << std::endl;
                    first = false;
                    backup = backup->GetPrevTree();
                    if (file->GetPrevVersionId() == 0) break;
                    file = backup->GetFileById(file->GetPrevVersionId());
                }
            } else return usage(argv);
        //////////////////////////////////////////////
        } else if (command == "backup" && argc == 3) {
            auto adapter = FenixBackup::Config::GetAdapter();
            auto tree = adapter->Scan();
            // Get files list
            auto files = tree->FinishTree();
            // 3. Foreach file in the file list, get file content and process it
            for (auto& file: files) {
                std::cout << "Processing file " << file->GetPath() << std::endl;
                adapter->GetAndProcess(file);
            }
            std::cout << "Saving new backup '" << tree->GetTreeName() << "'" << std::endl;
            tree->SaveTree();
        ///////////////////////////////////////////////
        } else if (command == "restore" && argc >= 5) {
            auto adapter = FenixBackup::Config::GetAdapter();
            std::string backup_name = argv[4];
            auto tree = FileTree::GetHistoryTree(backup_name);
            if (tree == nullptr) {
                std::cerr << "No backup name " << backup_name << std::endl;
                return(EXIT_FAILURE);
            }
            adapter->SetTree(tree);
            if (subcommand == "full" && argc <= 6) {
                if (argc == 6) {
                    std::string path = argv[5];
                    adapter->RestoreSubtreeToLocalPath(tree->GetRoot(), path);
                } else adapter->RestoreSubtree(tree->GetRoot());
            } else if (subcommand == "subtree" && argc >= 6 && argc <= 7) {
                std::string subtree_path = argv[5];
                auto file = tree->GetFileByPath(subtree_path);
                if (file == nullptr) {
                    std::cerr << "No file '" << subtree_path << std::endl;
                    return(EXIT_FAILURE);
                }
                if (argc == 7) {
                    std::string path = argv[6];
                    adapter->RestoreSubtreeToLocalPath(file, path, ALL, NEWEST_KNOWN_VERSION, false);
                } else adapter->RestoreSubtree(file);
            } else if (subcommand == "file" && argc >= 6 && argc <= 7) {
                std::string file_path = argv[5];
                auto file = tree->GetFileByPath(file_path);
                if (file == nullptr) {
                    std::cerr << "No file '" << file_path << std::endl;
                    return(EXIT_FAILURE);
                }
                if (argc == 7) {
                    std::string path = argv[6];
                    adapter->RestoreFileToLocalPath(file, path, ALL, NEWEST_KNOWN_VERSION, false);
                } else adapter->RestoreFile(file);
            } else return usage(argv);
        ///////////////////////////////////////////////
        } else if (command == "cleanup" && argc <= 4) {
            int runs = (argc == 4 ? atoi(argv[3]) : 1);
            FenixBackup::BackupCleaner cleaner;
            std::cout << "Loading data for BackupCleaner" << std::endl;
            cleaner.LoadData();
            std::cout << "Cleaning (" << runs << " runs)..." << std::endl;
            int cleaned = 0;
            for (int i = 0; i < runs; i++) cleaned += cleaner.Clean();
            std::cout << "Cleaned " << -cleaned << " bytes of data" << std::endl;
        } else return usage(argv);
	} catch(FenixBackup::FenixException &ex) {
		std::cerr << ex.what();
		return(EXIT_FAILURE);
	}

    return(EXIT_SUCCESS);
}

}
