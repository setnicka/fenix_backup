#include "Functions.hpp"

#include <istream>
#include <sha256.h>

namespace FenixBackup {

std::string Functions::ComputeFileHash(std::istream& file) {
        SHA256 sha256;
        auto buffer = new char[1024];

        int position = file.tellg();
        file.clear();
        file.seekg(0);
        while (!file.eof()) {
                file.read(buffer, 1024);
                sha256.add(buffer, file.gcount());
        }
        file.clear();
        file.seekg(position); // Reset to original position
        return sha256.getHash();
}

}
