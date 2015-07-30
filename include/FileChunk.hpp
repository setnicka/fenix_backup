#ifndef FILECHUNK_HPP
#define FILECHUNK_HPP

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace FenixBackup {

class FileChunk {
  public:
    FileChunk(std::string chunk_name, bool load = false);
    virtual ~FileChunk();

    /// Compute VCDIFF of the file against given ancestor and save it
    void ProcessStringAndSave(const std::string& ancestor_name, const std::string& content);
    void ProcessStreamAndSave(const std::string& ancestor_name, std::istream& stream);
    void ProcessFileAndSave(const std::string& ancestor_name, const std::string& source_path);
    /// Return file content
    std::string LoadAndReturn();
    void LoadAndExtract(std::string target_path);

    int GetDepth();
    const std::string& GetAncestorName();
    size_t GetSize();

    /// Skip one level of ancestor and conpute new VCDIFF, return size change
    int SkipAncestor();
    /// Delete this chunk and call SkipAncestor on all its childs, return size change
    int DeleteChunk();

    void AddDerivedChunk(std::string);
    void RemoveDerivedChunk(std::string);

    // Static functions
	static std::shared_ptr<FileChunk> GetChunk(std::string name);

    class FileChunkData;
  private:
    std::unique_ptr<FileChunkData> data;

	static std::unordered_map<std::string, std::shared_ptr<FileChunk>> loaded_chunks;
};

}

#endif // FILECHUNK_HPP
