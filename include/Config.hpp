#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <string>
#include <memory>
#include <unordered_map>

namespace FenixBackup {

struct ConfigData {
    std::string baseDir;
    std::string adapter;

    std::string treeSubdir = "trees";
    std::string dataSubdir = "data";
    std::string tempSubdir = "temp";

    std::string treeFileExtension = ".fenixtree";
    std::string treeFilePattern = "%Y-%m-%d_%H%M%S";

    std::string chunkMetaExtension = ".meta";
    std::string chunkDataExtension = ".data";
};

class Config {
  public:
	static void Load(std::string filename);

    static const ConfigData& GetConfig();

	static const std::string GetTreeDir();
	static const std::string GetDataDir();
	static const std::string GetTempDir();

	static const std::string GetTreeFilename(const std::string& name);
	static const std::string GetChunkFilename(const std::string& name, bool is_data = false);

    struct Rules {
        bool inherit = false;  // Used for regex matching inheritance:
            // false: match only files in this directory
            // true: if match, use this Rules even to some files in subdirs (and not use subdir Rules for them except backup=none fro dir)
        bool backup = true;
        int priority = 0;
        int history = 10;
    };

    static const Rules& GetRules(const std::string& path);

    class Dir;
  private:
    static std::shared_ptr<Dir> root_rules;

    static struct ConfigData data;
    static bool loaded;
};

}

#endif // CONFIG_HPP
