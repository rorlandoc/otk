#ifndef OTK_OUTPUT_HPP
#define OTK_OUTPUT_HPP

#include <nlohmann/json.hpp>

#include <odb_API.h>

#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkPoints.h>

#include "otk/odb.hpp"

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Validate the JSON output request file
//
// ---------------------------------------------------------------------------------------
bool is_output_request_valid(const nlohmann::json &output_request);

// ---------------------------------------------------------------------------------------
//
//   Write the VTK file
//
// ---------------------------------------------------------------------------------------
void write_vtu(otk::Odb &odb, const nlohmann::json &output_request);

// ---------------------------------------------------------------------------------------
//
//   Get vtkPoints from a node sequence
//
// ---------------------------------------------------------------------------------------
void get_points_from_nodes(vtkPoints *points,
                           std::unordered_map<int, vtkIdType> &node_map,
                           const odb_SequenceNode &node_sequence,
                           odb_Enum::odb_DimensionEnum instance_type);

// ---------------------------------------------------------------------------------------
//
//   Get the cell types from an element sequence
//
// ---------------------------------------------------------------------------------------
std::vector<VTKCellType> get_cell_types(const odb_SequenceElement &element_sequence);

// ---------------------------------------------------------------------------------------
//
//   Get the base element type without derivatives
//
// ---------------------------------------------------------------------------------------
std::string get_element_type_base(const std::string &element_type);

// ---------------------------------------------------------------------------------------
//
//   Get vtkCellArrays from an element sequence
//
// ---------------------------------------------------------------------------------------
void get_cells_from_elements(std::unordered_map<VTKCellType, vtkCellArray *> &cells,
                             std::unordered_map<int, vtkIdType> &node_map,
                             const odb_SequenceElement &element_sequence);

// ---------------------------------------------------------------------------------------
//
//   Constant map to convert from Abaqus element type to VTK cell type
//
// ---------------------------------------------------------------------------------------
const std::unordered_map<std::string, VTKCellType> ABQ_VTK_CELL_MAP{
    // 2D Continuum - Plane strain
    {"CPE3", VTK_TRIANGLE},
    {"CPE4", VTK_QUAD},
    {"CPE6", VTK_QUADRATIC_TRIANGLE},
    {"CPE8", VTK_QUADRATIC_QUAD},

    // 2D Continuum - Plane stress
    {"CPS3", VTK_TRIANGLE},
    {"CPS4", VTK_QUAD},
    {"CPS6", VTK_QUADRATIC_TRIANGLE},
    {"CPS8", VTK_QUADRATIC_QUAD},

    // 2D Continuum - Generalized plane strain
    {"CPEG4", VTK_QUAD},
    {"CPEG3", VTK_TRIANGLE},
    {"CPEG8", VTK_QUADRATIC_QUAD},
    {"CPEG6", VTK_QUADRATIC_TRIANGLE},

    // 2D Continuum - Axisymmetric
    {"CAX3", VTK_TRIANGLE},
    {"CAX4", VTK_QUAD},
    {"CAX6", VTK_QUADRATIC_TRIANGLE},
    {"CAX8", VTK_QUADRATIC_QUAD},

    // 3D Continuum
    {"C3D4", VTK_TETRA},
    {"C3D5", VTK_PYRAMID},
    {"C3D6", VTK_WEDGE},
    {"C3D8", VTK_HEXAHEDRON},
    {"C3D10", VTK_QUADRATIC_TETRA},
    {"C3D15", VTK_QUADRATIC_WEDGE},
    {"C3D20", VTK_QUADRATIC_HEXAHEDRON},

    // Shell
    {"STRI3", VTK_TRIANGLE},
    {"S3", VTK_TRIANGLE},
    {"S4", VTK_QUAD},
    {"S8", VTK_QUADRATIC_QUAD},

    // Continuum shell
    {"SC6", VTK_WEDGE},
    {"SC8", VTK_HEXAHEDRON},

    // Continuum solid shell
    {"CSS8", VTK_HEXAHEDRON},
};

}  // namespace otk

#endif  // !OTK_OUTPUT_HPP