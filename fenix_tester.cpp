#include <iostream>
#include <fstream>
#include "FileTree.hpp"
#include "FenixExceptions.hpp"

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>


using namespace std;

int main() {
	//auto storage = new FenixBackup::Storage();
	//storage->Test();

	try {
		FenixBackup::Config::Load("/home/jirka/bc_FenixBackup/test/config");
		FenixBackup::FileTree::GetHistoryTreeList();
        auto tree = new FenixBackup::FileTree();

        FenixBackup::file_params params = {};
        auto f1 = tree->AddFile(tree->GetRoot(), "souborB.txt", params);
        auto d1 = tree->AddDirectory(tree->GetRoot(), "slozka", params);
        auto f2 = tree->AddFile(d1, "souborAA.txt", params);
        auto f3 = tree->AddFile(d1, "souborAAAA.txt", params);

        tree->SaveTree();
	} catch(FenixBackup::FenixException &ex) {
		cerr << ex.what();
		return(EXIT_FAILURE);
	}

    /*std::ofstream os("test.test");
    cereal::JSONOutputArchive archive(os);

    auto FI = make_shared<FenixBackup::FileInfo>();
    FenixBackup::FileInfo FI2;

    archive(FI);*/

	return(EXIT_SUCCESS);
}
