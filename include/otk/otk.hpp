#ifndef OTK_HPP
#define OTK_HPP

#include <filesystem>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <odb_API.h>

#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkPoints.h>

#include <nlohmann/json.hpp>

namespace otk
{

std::filesystem::path find_file(std::filesystem::path current_path);

class Odb
{
  public:
    Odb(std::filesystem::path path);
    ~Odb();

    void write_info();
    void write_vtu();

    friend std::ostream &operator<<(std::ostream &os, const Odb &odb);

    std::string path() const;
    std::string name() const;

    nlohmann::json instances() const;
    nlohmann::json nodes(const std::string &instance_name) const;
    nlohmann::json elements(const std::string &instance_name) const;

    nlohmann::json steps() const;
    nlohmann::json frames(const std::string &step_name) const;
    nlohmann::json fields(const std::string &step_name,
                          const int frame_number) const;

  private:
    std::filesystem::path path_;
    odb_Odb *odb_;
};

void get_points_from_nodes(vtkPoints *points,
                           std::unordered_map<int, vtkIdType> &node_map,
                           const odb_SequenceNode &node_sequence,
                           odb_Enum::odb_DimensionEnum instance_type);

std::vector<VTKCellType> get_cell_types(
    const odb_SequenceElement &element_sequence);

std::string get_element_type_base(const std::string &element_type);

void get_cells_from_elements(
    std::unordered_map<VTKCellType, vtkCellArray *> &cells,
    std::unordered_map<int, vtkIdType> &node_map,
    const odb_SequenceElement &element_sequence);

const std::unordered_map<std::string, VTKCellType> abq2vtk_cell_map{
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

} // namespace otk

#endif // !OTK_HPP