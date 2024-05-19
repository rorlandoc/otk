#include "otk/output.hpp"

#include <vtkCellArray.h>
#include <vtkHexahedron.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkQuad.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

namespace fs = std::filesystem;
using namespace nlohmann;

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Validate the JSON output request file
//
// ---------------------------------------------------------------------------------------
bool is_output_request_valid(const json &output_request) {
    if (!output_request.contains("instances")) {
        return false;
    }
    if (!(output_request["instances"].is_array()) &&
        !(output_request["instances"].is_string())) {
        return false;
    }
    if (output_request["instances"].is_array()) {
        if (output_request["instances"].size() <= 0ull) {
            return false;
        }
        for (auto instance : output_request["instances"]) {
            if (!instance.is_string()) {
                return false;
            }
        }
    }
    if (!output_request.contains("frames")) {
        return false;
    }
    if (!output_request["frames"].is_array()) {
        return false;
    }
    if (output_request["frames"].size() <= 0ull) {
        return false;
    }
    for (auto frame : output_request["frames"]) {
        if (!frame.contains("step")) {
            return false;
        }
        if (!frame["step"].is_string()) {
            return false;
        }
        if (!frame.contains("list")) {
            return false;
        }
        if (!frame["list"].is_array()) {
            return false;
        }
        if (frame["list"].size() <= 0ull) {
            return false;
        }
        for (auto item : frame["list"]) {
            if (!item.is_number_integer()) {
                return false;
            }
        }
    }
    if (!output_request.contains("fields")) {
        return false;
    }
    if (!output_request["fields"].is_array()) {
        return false;
    }
    if (output_request["fields"].size() <= 0ull) {
        return false;
    }
    for (auto field : output_request["fields"]) {
        if (!field.contains("key")) {
            return false;
        }
        if (!field["key"].is_string()) {
            return false;
        }
    }
    return true;
}

}  // namespace otk
