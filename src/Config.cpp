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
    struct RulesInternal{
        bool scan; bool scan_set = false;
        bool backup; bool backup_set = false;
        int priority; bool priority_set = false;
        int history; bool history_set = false;
    };

    struct RulesFilter {
        std::string regex = "";
        std::string path_regex = "";
        size_t size_at_least = 0;
        size_t size_at_most = 0;
    };

    void SetRules(RulesInternal rules);
    void AddRules(RulesFilter filter, RulesInternal rules);
    void Apply(const std::string& path, const file_params& params, Rules& rules);

    /// Return whole path with multiple subdirs (and create them, if they doesnt exists)
    static std::shared_ptr<Dir> GetDirByPath(const std::string& path, bool create = false);

    static void ParseRules(const libconfig::Setting& source, RulesInternal& target);
    static void ParseRulesFilter(const libconfig::Setting& source, RulesFilter& target);

    static void ApplyRules(const RulesInternal& internal, Rules& rules);
    static bool RulesFilterTest(const RulesFilter& filter, const std::string& path, const file_params& params);

  private:
    /// Return subdir (and create it, if it doesnt exists)
    std::shared_ptr<Dir> GetSubdir(const std::string& subdir, bool create = false);

    std::unordered_map<std::string, std::shared_ptr<Dir>> subdirs;
    RulesInternal dir_rules;
    std::vector<std::pair<RulesFilter, RulesInternal>> file_rules;
};

void Config::Dir::SetRules(Config::Dir::RulesInternal rules) { this->dir_rules = rules; }
void Config::Dir::AddRules(Config::Dir::RulesFilter filter, Config::Dir::RulesInternal rules) { file_rules.push_back(std::make_pair(filter, rules)); }

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

void Config::Dir::ParseRules(const libconfig::Setting& source, Config::Dir::RulesInternal& target) {
    if (source.lookupValue("scan", target.scan)) target.scan_set = true;
    if (source.lookupValue("backup", target.backup)) target.backup_set = true;
    if (source.lookupValue("priority", target.priority)) target.priority_set = true;
    if (source.lookupValue("history", target.history)) target.history_set = true;
}

void Config::Dir::ParseRulesFilter(const libconfig::Setting& source, Config::Dir::RulesFilter& target) {
    source.lookupValue("regex", target.regex);
    source.lookupValue("path_regex", target.path_regex);
    source.lookupValue("size_at_least", (unsigned int&)target.size_at_least);
    source.lookupValue("size_at_most", (unsigned int&)target.size_at_most);
}

void Config::Dir::ApplyRules(const Config::Dir::RulesInternal& internal, Config::Rules& rules) {
    if (internal.scan_set) rules.scan = internal.scan;
    if (internal.backup_set) rules.backup = internal.backup;
    if (internal.priority_set) rules.priority = internal.priority;
    if (internal.history_set) rules.history = internal.history;
}

bool Config::Dir::RulesFilterTest(const Config::Dir::RulesFilter& filter, const std::string& path, const file_params& params) {

    // 1. Regex tests for filename and path (dirname)
    if (!filter.regex.empty() || !filter.path_regex.empty()) {
        auto pos = path.rfind('/');
        std::string dirname;
        std::string filename;
        if (pos == std::string::npos) filename = path;
        else {
            dirname = path.substr(0, pos);
            filename = path.substr(pos + 1);
        }
        // Tests:
        if (!filter.regex.empty() && !regex_match(filename, std::regex(filter.regex))) return false;
        if (!filter.path_regex.empty() && !regex_match(dirname, std::regex(filter.path_regex))) return false;
    }

    // 2. Filesize tests
    if (filter.size_at_least != 0 && params.file_size < filter.size_at_least) return false;
    if (filter.size_at_most != 0 && params.file_size > filter.size_at_most) return false;

    // Final - all test passed, return true
    return true;
}

void Config::Dir::Apply(const std::string& path, const file_params& params, Config::Rules& rules) {
    // 1. Apply my rules, if there is any
    ApplyRules(dir_rules, rules);

    // 2. Try each file_rule if it matches
    if (!path.empty()) {
        // Try to match all file rules
        for (auto& f: file_rules) {
            if (RulesFilterTest(f.first, path, params)) ApplyRules(f.second, rules);
        }
    }

    // 3: If it is subdir path, ask subdir
    auto pos = path.find('/');
    if (pos != std::string::npos) {
        auto subdir = GetSubdir(path.substr(0, pos));
        if (subdir) return subdir->Apply(path.substr(pos+1), params, rules);
    }
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
	if (!config_file.lookupValue("adapter.type", data.adapter)) throw ConfigException("Missing 'adapter.type' in the config file '"+filename+"'\n");

    // 2. Optional fields
    config_file.lookupValue("treeSubdir", data.treeSubdir);
    config_file.lookupValue("dataSubdir", data.dataSubdir);
    config_file.lookupValue("tempSubdir", data.tempSubdir);

    // 3. Create root_rules
    root_rules = std::make_shared<Dir>();

    // 4. Rules for directories
    if (config_file.exists("paths") && config_file.lookup("paths").isList()) {
        auto& all_rules = config_file.lookup("paths");
        for (int i = 0; i < all_rules.getLength(); i++) {
            auto& rules = all_rules[i];
            if (!rules.exists("path")) throw ConfigException("No path for the paths index "+std::to_string(i)+"\n");
            std::string path = rules["path"];

            Dir::RulesInternal parsed_rules;
            Dir::ParseRules(rules, parsed_rules);

            // Save rules to given path
            if (!path.empty() && path[0] == '/') path = path.substr(1);
            auto dir = Dir::GetDirByPath(path, true);
            dir->SetRules(parsed_rules);

            // File rules
            if (rules.exists("file_rules")) {
                for (int j = 0; j < rules["file_rules"].getLength(); j++) {

                    auto& file_rules = rules["file_rules"][j];
                    Dir::RulesInternal parsed_file_rules;
                    Dir::ParseRules(file_rules, parsed_file_rules);
                    Dir::RulesFilter parsed_filter;
                    Dir::ParseRulesFilter(file_rules, parsed_filter);

                    dir->AddRules(parsed_filter, parsed_file_rules);
                }
            }
        }
    }

    loaded = true;
}

const Config::Rules Config::GetRules(const std::string& path, const file_params& params) {
    Rules rules;

    // Let all dirs on the path modify the rules
    // Start asking the root dir

    // If starting with '/' remove it before asking root_rules
    if (!path.empty() && path[0] == '/') root_rules->Apply(path.substr(1), params, rules);
    else root_rules->Apply(path, params, rules);

    return rules;
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
