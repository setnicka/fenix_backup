#include <string>

#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

namespace FenixBackup {

class Functions {
  public:
    Functions() = delete;

    static std::string ComputeFileHash(std::istream& file);
};

}

#endif // FUNCTIONS_HPP
