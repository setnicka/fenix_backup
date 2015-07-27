#include <vector>
#include <fstream>
#include <algorithm>

#include "FenixExceptions.hpp"
#include "Config.hpp"
#include "FileChunk.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>

#include <google/vcencoder.h>
#include <google/vcdecoder.h>

namespace FenixBackup {

std::unordered_map<std::string, std::shared_ptr<FileChunk>> FileChunk::loaded_chunks;

class FileChunk::FileChunkData {
  public:
    std::string chunk_name;
    std::string ancestor_chunk_name;
    int depth = 0;
    std::vector<std::string> derived_chunks;
    size_t chunk_size = 0;

    FileChunkData(std::string chunk_name): chunk_name{chunk_name} {}

    void SaveChunkInfo();
    void LoadChunkInfo();

    template <class Archive>
    void serialize(Archive & ar, std::uint32_t const version) {
        ar(
            cereal::make_nvp("chunk_name", chunk_name),
            cereal::make_nvp("ancestor_chunk_name", ancestor_chunk_name),
            cereal::make_nvp("depth", depth),
            cereal::make_nvp("chunk_size", chunk_size),
            cereal::make_nvp("derived_chunks", derived_chunks)
        );
    }
};

void FileChunk::FileChunkData::SaveChunkInfo() {
    std::ofstream os(Config::GetChunkFilename(chunk_name));
    cereal::JSONOutputArchive archive(os);

    archive(*this);
    //serialize(archive);
}

void FileChunk::FileChunkData::LoadChunkInfo() {
	// Load data from given FileChunk meta file name
    std::ifstream is(Config::GetChunkFilename(chunk_name));
    if (!is.good()) throw FileChunkException("Couldn't load FileChunk '"+chunk_name+"' meta info (from filename '"+Config::GetChunkFilename(chunk_name)+"')\n");
    cereal::JSONInputArchive archive(is);

    archive(*this);
}

////////

std::shared_ptr<FileChunk> FileChunk::GetChunk(std::string chunk_name) {
	if (loaded_chunks.find(chunk_name) == loaded_chunks.end()) {
        if(!std::ifstream(Config::GetChunkFilename(chunk_name)).good()) return nullptr;
		loaded_chunks.insert(std::make_pair(chunk_name, std::make_shared<FileChunk>(chunk_name, true)));
    }

	return loaded_chunks[chunk_name];
}

FileChunk::FileChunk(std::string chunk_name, bool load): data{new FileChunkData(chunk_name)} {
    if (load) data->LoadChunkInfo();
}

FileChunk::~FileChunk() {}

// Saving and loading
void FileChunk::ProcessStringAndSave(std::string ancestor_name, std::string content) {
    data->ancestor_chunk_name = ancestor_name;
    // 1. Get source to diff against
    std::string source;

    std::shared_ptr<FileChunk> ancestor;
    if (!ancestor_name.empty()) {
        ancestor = GetChunk(data->ancestor_chunk_name);
        if (ancestor == nullptr) throw FileChunkException("Cannot load ancestor '"+data->ancestor_chunk_name+"' of the FileChunk '"+data->chunk_name+"'\n");
        source = ancestor->LoadAndReturn();
        data->depth = ancestor->GetDepth() + 1;
    }

    // 2. Encode new content using VCDIFF against source
    std::string output_string;
    open_vcdiff::VCDiffEncoder encoder(source.data(), source.size());
    encoder.Encode(content.data(), content.size(), &output_string);

    // 3. Save new content
    std::ofstream storage(Config::GetChunkFilename(data->chunk_name, true), std::ios::binary);
    storage.write(output_string.data(), output_string.size());
    storage.close();
    data->chunk_size = output_string.size();
    // TODO: Count size of chunk metadata to the final size?

    // 4. Save chunk info
    data->SaveChunkInfo();
    // Update ancestor in this moment, when derived chunk is saved
    if (!ancestor_name.empty()) ancestor->AddDerivedChunk(data->chunk_name);
}

void FileChunk::ProcessFileAndSave(std::string ancestor_name, std::string source_path) {
    std::ifstream ifs(source_path, std::ios::binary);
    if (!ifs.good()) throw FileChunkException("Cannot read file '"+source_path+"'");
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()   ));
    ProcessStringAndSave(ancestor_name, content);
}

std::string FileChunk::LoadAndReturn() {
    // 1. Get dictionary (source file) from ancestors
    std::string source;
    if (!data->ancestor_chunk_name.empty()) {
        auto ancestor = GetChunk(data->ancestor_chunk_name);
        if (ancestor == nullptr) throw FileChunkException("Cannot load ancestor '"+data->ancestor_chunk_name+"' of the FileChunk '"+data->chunk_name+"'\n");
        source = ancestor->LoadAndReturn();
    }

    // 2. Open data file with VCDIFF
    std::ifstream storage(Config::GetChunkFilename(data->chunk_name, true), std::ios::binary);
    std::string delta(  (std::istreambuf_iterator<char>(storage)),
                        (std::istreambuf_iterator<char>()       ));

    // 3. Compute original file
    open_vcdiff::VCDiffDecoder decoder;
    std::string output;
    decoder.Decode(source.data(), source.size(), delta, &output);
    return output;
}

void FileChunk::LoadAndExtract(std::string target_path) {
    std::ofstream output(target_path);
    std::string content = LoadAndReturn();
    output.write(content.data(), content.length());
    output.close();
}

const std::string& FileChunk::GetAncestorName() { return data->ancestor_chunk_name; }
int FileChunk::GetDepth() { return data->depth; }
size_t FileChunk::GetSize() { return data->chunk_size; }

void FileChunk::SkipAncestor() {
    // 1. Get new ancestor
    if (data->ancestor_chunk_name.empty()) throw FileChunkException("FileChunk '"+data->chunk_name+"' has no ancestor, cannot skip ancestor\n");
    auto ancestor = GetChunk(data->ancestor_chunk_name);
    if (ancestor == nullptr) throw FileChunkException("Cannot load ancestor '"+data->ancestor_chunk_name+"' of the FileChunk '"+data->chunk_name+"'\n");
    std::string new_ancestor_name = ancestor->GetAncestorName();
    if (GetChunk(new_ancestor_name) == nullptr) throw FileChunkException("Cannot load new ancestor '"+new_ancestor_name+"' for the '"+data->chunk_name+"'\n");

    // TODO: Maybe some way to merge VCDIFFs instead of counting new?
    // 2. Get this chunk content, and compute new VCDIFF
    auto content = LoadAndReturn();

    ProcessStringAndSave(new_ancestor_name, content);
}

void FileChunk::DeleteChunk() {
    for (auto& chunk_name: data->derived_chunks) {
        GetChunk(chunk_name)->SkipAncestor();
    }
    remove(Config::GetChunkFilename(data->chunk_name).c_str());
    remove(Config::GetChunkFilename(data->chunk_name, true).c_str());
}

void FileChunk::AddDerivedChunk(std::string name) {
    data->derived_chunks.push_back(name);
    data->SaveChunkInfo();
}

void FileChunk::RemoveDerivedChunk(std::string name) {
    auto it = std::find(data->derived_chunks.begin(), data->derived_chunks.end(), name);
    if(it != data->derived_chunks.end()) data->derived_chunks.erase(it);
    data->SaveChunkInfo();
}

}
CEREAL_CLASS_VERSION(FenixBackup::FileChunk::FileChunkData, 1);
