#ifndef OTK_CONVERTER_HPP
#define OTK_CONVERTER_HPP

#include <filesystem>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include "otk/odb.hpp"

namespace fs = std::filesystem;

namespace otk {

class Converter {
    using PointArray = vtkSmartPointer<vtkPoints>;
    using CellArray = vtkSmartPointer<vtkCellArray>;
    using CellArrayPair = std::pair<std::vector<int>, CellArray>;
    using CellData = vtkSmartPointer<vtkDoubleArray>;
    using PointData = vtkSmartPointer<vtkDoubleArray>;
    using CellDataArray = std::vector<CellData>;
    using PointDataArray = std::vector<PointData>;
    using ElementMap = std::unordered_map<std::string, odb_SequenceElement>;
    using ElementLabelMap = std::unordered_map<int, int>;

   public:
    // -----------------------------------------------------------------------------------
    //
    //   Constructor
    //
    // -----------------------------------------------------------------------------------
    Converter(const nlohmann::json &output_request) : output_request_(output_request) {}

    // -----------------------------------------------------------------------------------
    //
    //   Convert ODB file to VTK format
    //
    // -----------------------------------------------------------------------------------
    void convert(otk::Odb &odb, fs::path file);

   protected:
    // -----------------------------------------------------------------------------------
    //
    //   Convert mesh data to VTK format
    //
    // -----------------------------------------------------------------------------------
    void convert_mesh(otk::Odb &odb);

    // -----------------------------------------------------------------------------------
    //
    //   Convert field data to VTK format
    //
    // -----------------------------------------------------------------------------------
    void convert_fields(otk::Odb &odb, fs::path file, nlohmann::json field_summary,
                        nlohmann::json instance_summary, nlohmann::json output_summary,
                        nlohmann::json matches);

    // -----------------------------------------------------------------------------------
    //
    //   Write mesh data to VTU file
    //
    // -----------------------------------------------------------------------------------
    void write(fs::path file, int frame_id);

    // -----------------------------------------------------------------------------------
    //
    //   Get the base element type without derivatives
    //
    // -----------------------------------------------------------------------------------
    std::string get_base_element_type(const std::string &element_type);

    // -----------------------------------------------------------------------------------
    //
    //   Get the cell types from an element sequence
    //
    // -----------------------------------------------------------------------------------
    std::set<VTKCellType> get_cell_types(const odb_SequenceElement &element_sequence);

    // -----------------------------------------------------------------------------------
    //
    //   Get vtkCellArrays from an element sequence
    //
    // -----------------------------------------------------------------------------------
    CellArrayPair get_cells(const std::unordered_map<int, vtkIdType> &node_map,
                            const odb_SequenceElement &element_sequence,
                            const std::string &instance_name,
                            const odb_Instance &instance);

    // -----------------------------------------------------------------------------------
    //
    //   Get vtkPoints from a node sequence
    //
    // -----------------------------------------------------------------------------------
    PointArray get_points(std::unordered_map<int, vtkIdType> &node_map,
                          const odb_SequenceNode &node_sequence,
                          odb_Enum::odb_DimensionEnum instance_type);

    // -----------------------------------------------------------------------------------
    //
    //   Process summary JSON from Odb class
    //
    // -----------------------------------------------------------------------------------
    nlohmann::json process_field_summary(const nlohmann::json &summary);

    // -----------------------------------------------------------------------------------
    //
    //   Match output request to available data
    //
    // -----------------------------------------------------------------------------------
    nlohmann::json match_request_to_available_data(const nlohmann::json &frames,
                                                   const nlohmann::json &fields);

    // -----------------------------------------------------------------------------------
    //
    //   Load field data from Odb class
    //
    // -----------------------------------------------------------------------------------
    nlohmann::json load_field_data(otk::Odb &odb, const nlohmann::json &request,
                                   const std::string &step_name, int frame_id);

    // -----------------------------------------------------------------------------------
    //
    //   Extract field data from Odb class
    //
    // -----------------------------------------------------------------------------------
    void extract_field_data(otk::Odb &odb, const nlohmann::json &data,
                            const nlohmann::json &instance_summary,
                            const std::string &step_name, int frame_id);

    // -----------------------------------------------------------------------------------
    //
    //   Extract field data from Instance
    //
    // -----------------------------------------------------------------------------------
    void extract_instance_field_data(otk::Odb &odb, const nlohmann::json &data,
                                     odb_Instance &instance, bool composite,
                                     const std::string &step_name, int frame_id);

    // -----------------------------------------------------------------------------------
    //
    //   Extract scalar field data
    //
    // -----------------------------------------------------------------------------------
    void extract_scalar_field(const odb_FieldOutput &field_output,
                              const std::vector<odb_Set> &element_sets,
                              const odb_Instance &instance, bool composite);

    // -----------------------------------------------------------------------------------
    //
    //   Extract vector field data
    //
    // -----------------------------------------------------------------------------------
    void extract_vector_field(const odb_FieldOutput &field_output,
                              const std::vector<odb_Set> &element_sets,
                              const odb_Instance &instance, bool composite);

    // -----------------------------------------------------------------------------------
    //
    //   Extract tensor field data
    //
    // -----------------------------------------------------------------------------------
    void extract_tensor_field(const odb_FieldOutput &field_output,
                              const std::vector<odb_Set> &element_sets,
                              const odb_Instance &instance, bool composite);

   private:
    nlohmann::json output_request_;
    std::vector<odb_FieldOutput> field_outputs_;
    std::unordered_map<std::string, PointArray> points_;
    std::unordered_map<std::string, CellArrayPair> cells_;
    std::unordered_map<std::string, CellDataArray> cell_data_;
    std::unordered_map<std::string, PointDataArray> point_data_;
    std::unordered_map<std::string, ElementMap> section_elements_;
    std::unordered_map<std::string, ElementLabelMap> element_map_;
};

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

template <typename Key, typename Value>
std::vector<Key> extract_keys(const std::unordered_map<Key, Value> &map) {
    std::vector<Key> keys;
    for (const auto &pair : map) {
        keys.push_back(pair.first);
    }
    return keys;
}

template <typename Key, typename Value>
std::vector<Value> extract_values(const std::unordered_map<Key, Value> &map) {
    std::vector<Value> values;
    for (const auto &pair : map) {
        values.push_back(pair.second);
    }
    return values;
}

}  // namespace otk

#endif  // !OTK_CONVERTER_HPP