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
    void ProcessStringAndSave(std::string ancestor_name, std::string content);
    void ProcessFileAndSave(std::string ancestor_name, std::string source_path);
    /// Return file content
    std::string LoadAndReturn();
    void LoadAndExtract(std::string target_path);

    std::string GetAncestorName();

    /// Skip one level of ancestor and conpute new VCDIFF
    void SkipAncestor();
    /// Delete this chunk and call SkipAncestor on all its childs
    void DeleteChunk();

    void AddDerivedChunk(std::string);
    void RemoveDerivedChunk(std::string);

    // Static functions
	static std::shared_ptr<FileChunk> GetChunk(std::string name);

  private:
    class FileChunkData;
    std::unique_ptr<FileChunkData> data;

	static std::unordered_map<std::string, std::shared_ptr<FileChunk>> loaded_chunks;
};

}

#endif // FILECHUNK_HPP
