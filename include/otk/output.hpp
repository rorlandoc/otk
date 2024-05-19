#ifndef OTK_OUTPUT_HPP
#define OTK_OUTPUT_HPP

#include <nlohmann/json.hpp>

#include <odb_API.h>

#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

#include "otk/odb.hpp"

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Validate the JSON output request file
//
// ---------------------------------------------------------------------------------------
bool is_output_request_valid(const nlohmann::json &output_request);

}  // namespace otk

#endif  // !OTK_OUTPUT_HPP