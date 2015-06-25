#include <libconfig.h++>
#include <sstream>

#include "Config.hpp"
#include "FenixExceptions.hpp"

namespace FenixBackup {

bool Config::loaded = false;
struct ConfigData Config::data;

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

    // 2. Optional fields
    config_file.lookupValue("treeSubdir", data.treeSubdir);
    config_file.lookupValue("dataSubdir", data.dataSubdir);

    loaded = true;
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
