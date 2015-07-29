#include <iostream>
#include <fstream>
#include "adapters/LocalFilesystemAdapter.hpp"
#include "FileTree.hpp"
#include "FileChunk.hpp"
#include "FenixExceptions.hpp"
#include "BackupCleaner.hpp"

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>


using namespace std;

int main() {
	//auto storage = new FenixBackup::Storage();
	//storage->Test();

	try {
		FenixBackup::Config::Load("test_backup/config");
		// FenixBackup::FileTree::GetHistoryTreeList();

        // 1. Get adapter and run it -> non-saved FileTree

        FenixBackup::LocalFilesystemAdapter* adapter;
        if (FenixBackup::Config::GetConfig().adapter == "local_filesystem") {
            adapter = new FenixBackup::LocalFilesystemAdapter();
            adapter->SetPath("./");
        }
        // adapter->SetPath("/home/jirka/bc_FenixBackup/");
        auto tree = adapter->Scan();

        // 2. Get files list
        auto files = tree->FinishTree();

        // 3. Foreach file in the file list, get file content and process it
        for (auto& file: files) {
                std::cout << "Processing file " << file->GetPath() << std::endl;
                adapter->GetAndProcess(file);
        }
        std::cout << "Saving tree" << std::endl;
        tree->SaveTree();

        FenixBackup::BackupCleaner cleaner;
        cleaner.LoadData();

        //std::cout << "Restoring data" << std::endl;
        //adapter->RestoreSubtreeToLocalPath(tree->GetRoot(), "/tmp/pokus");

        //FenixBackup::LocalFilesystemAdapter adapter2("2015-07-29_072741");
        //adapter2.RestoreSubtreeToLocalPath(adapter2.GetTree()->GetRoot(), "/tmp/pokus2");

	} catch(FenixBackup::FenixException &ex) {
		cerr << ex.what();
		return(EXIT_FAILURE);
	}

	return(EXIT_SUCCESS);
}
