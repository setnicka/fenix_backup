#include <iostream>
#include <fstream>
#include "FileTree.hpp"
#include "FenixExceptions.hpp"

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>

using namespace std;

int main() {
	//auto storage = new FenixBackup::Storage();
	//storage->Test();

	try {
		FenixBackup::Config::Load("/home/jirka/bc_FenixBackup/test/config");
		FenixBackup::FileTree::GetHistoryTreeList();
        auto tree = new FenixBackup::FileTree();
        //tree->SaveTree();
	} catch(FenixBackup::FenixException &ex) {
		cerr << ex.what();
		return(EXIT_FAILURE);
	}

    std::ofstream os("test.test");
    cereal::JSONOutputArchive archive(os);

    auto FI = make_shared<FenixBackup::FileInfo>();
    FenixBackup::FileInfo FI2;

    archive(FI2);

	return(EXIT_SUCCESS);
}
