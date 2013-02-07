/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_UPLOAD_FILE_PROCESSOR_H_
#define CVMFS_UPLOAD_FILE_PROCESSOR_H_

#include "util_concurrency.h"

namespace upload {

/**
 * Adds a temporary file path to the FileChunk structure
 * This is needed internally before the file is actually stored under it's
 * content hash
 */
class TemporaryFileChunk : public FileChunk {
 public:
  TemporaryFileChunk() {}
  TemporaryFileChunk(const off_t offset, const size_t size) :
    FileChunk(hash::Any(), offset, size) {}

  const std::string& temporary_path() const { return temporary_path_; }

 protected:
  friend class FileProcessor;
  void set_content_hash(const hash::Any &hash)     { content_hash_ = hash; }
  void set_temporary_path(const std::string &path) { temporary_path_ = path; }

 protected:
  std::string temporary_path_; //!< location of the compressed file chunk (generated by FileProcessor)
};
typedef std::vector<TemporaryFileChunk> TemporaryFileChunks;

/**
 * Implements a concurrent compression worker based on the Concurrent-
 * Workers template. File compression is done in parallel when possible.
 */
class FileProcessor : public ConcurrentWorker<FileProcessor> {
 public:

  /**
   * Initialization data for the file processor
   * This will be passed for each spawned FileProcessor by the
   * ConcurrentWorkers implementation
   */
  struct worker_context {
    worker_context(const std::string &temporary_path,
                   const bool         use_file_chunking) :
      temporary_path(temporary_path),
      use_file_chunking(use_file_chunking) {}
    const std::string temporary_path; //!< base path to store processing
                                      //!< results in temporary files
    const bool        use_file_chunking;
  };


  /**
   * Encapuslates all the needed information for one FileProcessor job
   * Will be filled by the user and then scheduled as a job into the
   * ConcurrentWorkers environment.
   */
  struct Parameters {
    Parameters(const std::string &local_path,
               const bool         allow_chunking) :
      local_path(local_path),
      allow_chunking(allow_chunking) {}

    // default constructor to create an 'empty' struct
    // (needed by the ConcurrentWorkers implementation)
    Parameters() :
      local_path(), allow_chunking(false) {}

    const std::string local_path;     //!< path to the local file to be processed
    const bool        allow_chunking; //!< enables file chunking for this job
  };

  /**
   * The results generated for each scheduled FileProcessor job
   * Users get this data structure when registering to the callback interface
   * provided by the ConcurrentWorkers template.
   */
  struct Results {
    Results(const std::string &local_path) :
      return_code(-1),
      local_path(local_path) {}

    int                  return_code; //!< 0 if job was successful
    TemporaryFileChunk   bulk_file;   //!< results of the bulk file processing
    TemporaryFileChunks  file_chunks; //!< list of the generated file chunks
    const std::string    local_path;  //!< path to the local file that was processed (same as given in Parameters)

    inline bool IsChunked() const { return file_chunks.size() > 1; }
  };

  // these typedefs are needed for the ConcurrentWorkers template
  typedef Parameters expected_data;
  typedef Results    returned_data;



 public:
  FileProcessor(const worker_context *context);
  void operator()(const Parameters &data);

 protected:
  bool GenerateFileChunks(const MemoryMappedFile &mmf,
                                Results          &data) const;
  bool GenerateBulkFile(const MemoryMappedFile   &mmf,
                              Results            &data) const;

  bool ProcessFileChunk(const MemoryMappedFile   &mmf,
                              TemporaryFileChunk &chunk) const;

 private:
  const std::string temporary_path_;
  const bool        use_file_chunking_;
};

}

#endif /* UPLOAD_FILE_PROCESSOR */

