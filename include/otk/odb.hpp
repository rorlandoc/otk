#ifndef OTK_ODB_HPP
#define OTK_ODB_HPP

#define STR(X) XSTR(X)
#define XSTR(X) #X

#include <filesystem>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <odb_API.h>

#include <nlohmann/json.hpp>

#include "otk/cli.hpp"

namespace otk {

// =======================================================================================
//
//   Odb class
//
// =======================================================================================
class Odb {
   public:
    // -----------------------------------------------------------------------------------
    //
    //   Constructors and destructors
    //
    // -----------------------------------------------------------------------------------
    Odb(fs::path path);
    ~Odb();

    // -----------------------------------------------------------------------------------
    //
    //   Class is non-copyable
    //
    // -----------------------------------------------------------------------------------
    Odb(const Odb &) = delete;
    Odb &operator=(const Odb &) = delete;

    // -----------------------------------------------------------------------------------
    //
    //   Getters
    //
    // -----------------------------------------------------------------------------------
    inline std::string path() const { return fs::absolute(path_.parent_path()).string(); }
    inline std::string name() const { return path_.filename().string(); }
    inline size_t size() const { return fs::file_size(path_); }

    // -----------------------------------------------------------------------------------
    //
    //   JSON summary functions (used by the otk::Converter class)
    //
    // -----------------------------------------------------------------------------------
    nlohmann::json field_summary(const nlohmann::json &frames) const;
    nlohmann::json instance_summary() const;

    // -----------------------------------------------------------------------------------
    //
    //   Access the native ODB handle
    //
    // -----------------------------------------------------------------------------------
    inline const odb_Odb *handle() const { return odb_; }
    inline odb_Odb *handle() { return odb_; }

    // -----------------------------------------------------------------------------------
    //
    //   General info print function
    //
    // -----------------------------------------------------------------------------------
    void odb_info(bool verbose = false) const;

   protected:
    // -----------------------------------------------------------------------------------
    //
    //   Specific info print functions
    //
    // -----------------------------------------------------------------------------------
    void instances_info(bool verbose = false) const;
    void nodes_info(const std::string &instance, bool verbose = false) const;
    void elements_info(const std::string &instance, bool verbose = false) const;
    void sections_info(const std::string &instance, bool verbose = false) const;
    void steps_info(bool verbose = false) const;
    void frames_info(const std::string &step, bool verbose = false) const;
    void fields_info(const std::string &step, int frame, bool verbose = false) const;

   private:
    fs::path path_;
    odb_Odb *odb_;
};

// =======================================================================================
// Helper functions
// =======================================================================================

}  // namespace otk

#endif  // !OTK_ODB_HPP
