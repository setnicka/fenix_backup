#include <libconfig.h++>
#include <sstream>
#include <regex>
#include <iostream>

#include "Config.hpp"
#include "FenixExceptions.hpp"

namespace FenixBackup {

bool Config::loaded = false;
struct ConfigData Config::data;
std::shared_ptr<Config::Dir> Config::root_rules = nullptr;

class Config::Dir {
  public:
    void SetRules(Rules rules);
    void SetRules(std::string regex_expression, Rules rules);
    const Rules& GetRules(const std::string& path);

    /// Return whole path with multiple subdirs (and create them, if they doesnt exists)
    static std::shared_ptr<Dir> GetDirByPath(const std::string& path, bool create = false);

  private:
    /// Return subdir (and create it, if it doesnt exists)
    std::shared_ptr<Dir> GetSubdir(const std::string& subdir, bool create = false);

    std::unordered_map<std::string, std::shared_ptr<Dir>> subdirs;
    Rules rules;
    std::unordered_map<std::string, Rules> files;
};

void Config::Dir::SetRules(Config::Rules rules) { this->rules = rules; }
void Config::Dir::SetRules(std::string regex_expression, Config::Rules rules) { this->files[regex_expression] = rules; }

std::shared_ptr<Config::Dir> Config::Dir::GetDirByPath(const std::string& path, bool create) {
    // 1. Normalize path
    size_t start = 0; size_t len = path.length();
    // Remove '/' from both ends
    if (len > 0 && path[start] == '/') start++;
    if (len > 0 && path[len-1] == '/') len--;

    // 2. Basics
    if (len == 0) return root_rules;

    // 3. Recursive search
    auto dir = root_rules;
    auto pos = path.find('/', start);
    while (pos != std::string::npos && pos < len) {
        dir = dir->GetSubdir(path.substr(start, pos - start), create);
        if (dir == nullptr) return nullptr;
        start = pos + 1;
        pos = path.find('/', start);
    }
    return dir->GetSubdir(path.substr(start, len - start), create);
}

/// Return subdir (and create it, if it doesnt exists)
std::shared_ptr<Config::Dir> Config::Dir::GetSubdir(const std::string& subdir, bool create) {
    auto it = subdirs.find(subdir);
    if (it != subdirs.end()) {
        return it->second;
    } else if (create) {
        auto dir = std::make_shared<Config::Dir>();
        subdirs.insert(std::make_pair(subdir, dir));
        return dir;
    } else return nullptr;
}

const Config::Rules& Config::Dir::GetRules(const std::string& path) {
    // Is this subdir path?
    auto pos = path.find('/');

    // 1: Try to match file rules in this Dir
    if (!path.empty()) {
        // Try to match all file rules
        for (auto& f: files) {
            if (pos == std::string::npos) {  // no subdir path
                if (regex_match(path, std::regex("^"+f.first+"$"))) return f.second;
            } else if (f.second.inherit) {  // subdir path -> only rules with inheritance
                if (regex_match(path, std::regex(f.first+"$"))) return f.second;
            }
        }
    }

    // 2: If it is subdir path, ask subdir
    auto subdir = GetSubdir(path.substr(0, pos));
    if (subdir) return subdir->GetRules(path.substr(pos+1));

    // 4: Else return default rules for this directory
    return rules;
}

////////////////////////////////////////////////////////////////////////////////

void Config::Load(std::string filename) {
	libconfig::Config config_file;

	// Load config file
	try {
		config_file.readFile(filename.c_str());
	} catch(const libconfig::FileIOException) {
		std::ostringstream oss;
		oss << "Config: I/O error while reading file '" << filename << "'" << std::endl;
		throw ConfigException(oss.str());
	} catch(const libconfig::ParseException &pex) {
		std::ostringstream oss;
		oss << "Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
		throw ConfigException(oss.str());
	}

    // Get config from config file
    // 1. Required fields
	if (!config_file.lookupValue("baseDir", data.baseDir)) throw ConfigException("Missing 'baseDir' in the config file '"+filename+"'\n");
	if (!config_file.lookupValue("adapter", data.adapter)) throw ConfigException("Missing 'adapter' in the config file '"+filename+"'\n");

    // 2. Optional fields
    config_file.lookupValue("treeSubdir", data.treeSubdir);
    config_file.lookupValue("dataSubdir", data.dataSubdir);
    config_file.lookupValue("tempSubdir", data.tempSubdir);

    // 3. Create root_rules
    root_rules = std::make_shared<Dir>();

    // 4. Rules for directories
    if (config_file.exists("rules") && config_file.lookup("rules").isList()) {
        auto& all_rules = config_file.lookup("rules");
        for (int i = 0; i < all_rules.getLength(); i++) {
            auto& rules = all_rules[i];
            if (!rules.exists("path")) throw ConfigException("No path for the rules index "+std::to_string(i)+"\n");
            std::string path = rules["path"];
            Rules parsed_rules;
            rules.lookupValue("backup", parsed_rules.backup);
            rules.lookupValue("priority", parsed_rules.priority);
            rules.lookupValue("history", parsed_rules.history);

            // Save rules to given path
            if (!path.empty() && path[0] == '/') path = path.substr(1);
            auto dir = Dir::GetDirByPath(path, true);
            dir->SetRules(parsed_rules);

            // File rules
            if (rules.exists("files")) {
                for (int j = 0; j < rules["files"].getLength(); j++) {
                    auto& file = rules["files"][j];
                    Rules parsed_file_rules;

                    if (!file.exists("regex")) throw ConfigException("No regex for the regex rules index "+std::to_string(i)+" (path '"+path+"')\n");

                    file.lookupValue("inherit", parsed_file_rules.inherit);
                    file.lookupValue("backup", parsed_file_rules.backup);
                    file.lookupValue("priority", parsed_file_rules.priority);
                    file.lookupValue("history", parsed_file_rules.history);

                    dir->SetRules(file["regex"], parsed_file_rules);
                }
            }
        }
    }

    loaded = true;
}

const Config::Rules& Config::GetRules(const std::string& path) {
    // If starting with '/' remove it before asking root_rules
    if (!path.empty() && path[0] == '/') return root_rules->GetRules(path.substr(1));
    else return root_rules->GetRules(path);
}

const ConfigData& Config::GetConfig() {
    return data;
}

const std::string Config::GetTreeDir() {
    if (!loaded) throw ConfigException("No loaded config file!\n");
    return data.baseDir + "/" + data.treeSubdir;
}

const std::string Config::GetDataDir() {
    if (!loaded) throw ConfigException("No loaded config file!\n");
    return data.baseDir + "/" + data.dataSubdir;
}

const std::string Config::GetTempDir() {
    if (!loaded) throw ConfigException("No loaded config file!\n");
    return data.baseDir + "/" + data.tempSubdir;
}

const std::string Config::GetTreeFilename(const std::string& filename) {
    return GetTreeDir() + "/" + filename + data.treeFileExtension;
}

const std::string Config::GetChunkFilename(const std::string& name, bool is_data) {
    return GetDataDir() + "/" + name + (is_data ? data.chunkDataExtension : data.chunkMetaExtension);
}

}
