#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <string>
#include <memory>

namespace FenixBackup {

struct ConfigData {
    std::string baseDir;
    std::string treeSubdir = "trees";
    std::string dataSubdir = "data";

    std::string treeFileExtension = ".fenixtree";
    std::string treeFilePattern = "%Y-%m-%d_%H%M%S";
};

class Config {
  public:
	static void Load(std::string filename);

    static const ConfigData& GetConfig();

	static const std::string GetTreeDir();
	static const std::string GetDataDir();
	static const std::string GetTreeFilename(const std::string& name);

  private:
    static struct ConfigData data;
    static bool loaded;
};


}

#endif // CONFIG_HPP
