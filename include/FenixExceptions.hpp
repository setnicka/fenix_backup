#include <stdexcept>

#ifndef FENIXEXCEPTIONS_HPP
#define FENIXEXCEPTIONS_HPP

namespace FenixBackup {

class FenixException : public std::runtime_error {
  public:
    FenixException(std::string message): std::runtime_error(message) {}
};

class ConfigException : public FenixException {
  public:
	ConfigException(std::string message): FenixException("Config error: "+message) {}
};

class FileInfoException : public FenixException {
  public:
	FileInfoException(std::string message): FenixException("FileInfo error: "+message) {}
};

class FileTreeException : public FenixException {
  public:
	FileTreeException(std::string message): FenixException("FileTree error: "+message) {}
};

class FileChunkException : public FenixException {
  public:
	FileChunkException(std::string message): FenixException("FileChunk error: "+message) {}
};

}

#endif // FENIXEXCEPTIONS_HPP
