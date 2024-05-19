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
bool is_output_request_valid(const json &output_request) { return false; }

// ---------------------------------------------------------------------------------------
//
//   Write the VTK file
//
// ---------------------------------------------------------------------------------------
void write_vtu(otk::Odb &odb, const json &output_request) {
    const odb_Assembly &root_assembly = odb.handle()->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    vtkNew<vtkXMLUnstructuredGridWriter> writer;

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        vtkNew<vtkPoints> points;
        vtkNew<vtkUnstructuredGrid> grid;

        std::unordered_map<int, vtkIdType> node_map;
        std::unordered_map<std::string, const float *> element_map;

        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();

        const odb_SequenceNode &instance_nodes = instance.nodes();
        const odb_SequenceElement &instance_elements = instance.elements();

        std::vector<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty()) {
            std::cout << "No supported cell types found.\n";
            return;
        }

        std::unordered_map<VTKCellType, vtkCellArray *> cells;
        for (auto cell_type : cell_types) {
            cells[cell_type] = vtkCellArray::New();
        }

        get_points_from_nodes(points.GetPointer(), node_map, instance_nodes,
                              instance_type);

        get_cells_from_elements(cells, node_map, instance_elements);

        grid->SetPoints(points);
        for (auto cell_type : cell_types) {
            grid->SetCells(cell_type, cells[cell_type]);
        }

        writer->SetFileName(
            fs::path{odb.name()}.replace_extension(".vtu").string().c_str());
        writer->SetInputData(grid);
        writer->Write();
    }
}

// ---------------------------------------------------------------------------------------
//
//   Get vtkPoints from a node sequence
//
// ---------------------------------------------------------------------------------------
void get_points_from_nodes(vtkPoints *points,
                           std::unordered_map<int, vtkIdType> &node_map,
                           const odb_SequenceNode &node_sequence,
                           odb_Enum::odb_DimensionEnum instance_type) {
    int num_nodes = node_sequence.size();
    for (int i = 0; i < num_nodes; ++i) {
        const odb_Node &node = node_sequence[i];
        int node_label = node.label();
        const float *const node_coordinates = node.coordinates();

        if (instance_type == odb_Enum::THREE_D) {
            node_map[node_label] = points->InsertNextPoint(node_coordinates);
        } else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                   (instance_type == odb_Enum::AXISYMMETRIC)) {
            node_map[node_label] =
                points->InsertNextPoint(node_coordinates[0], node_coordinates[1], 0.0);
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   Get the base element type without derivatives
//
// ---------------------------------------------------------------------------------------
std::string get_element_type_base(const std::string &element_type) {
    std::string element_name_base;
    for (auto c : element_type) {
        if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' ||
            c == '6' || c == '7' || c == '8' || c == '9') {
            element_name_base += c;
            break;
        }
        element_name_base += c;
    }
    return element_name_base;
}

// ---------------------------------------------------------------------------------------
//
//   Get the cell types from an element sequence
//
// ---------------------------------------------------------------------------------------
std::vector<VTKCellType> get_cell_types(const odb_SequenceElement &element_sequence) {
    std::vector<VTKCellType> cell_types;
    int num_elements = element_sequence.size();
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element &element = element_sequence[i];
        std::string element_type = get_element_type_base(element.type().CStr());

        if (auto it = ABQ_VTK_CELL_MAP.find(element_type); it != ABQ_VTK_CELL_MAP.end()) {
            auto v_it = std::find_if(
                cell_types.begin(), cell_types.end(),
                [&](VTKCellType cell_type) { return cell_type == it->second; });
            if (v_it == cell_types.end()) {
                cell_types.push_back(it->second);
            }
        } else {
            std::cout << "Element type " << element_type << " is not supported.\n";
        }
    }
    return cell_types;
}

// ---------------------------------------------------------------------------------------
//
//   Get vtkCellArrays from an element sequence
//
// ---------------------------------------------------------------------------------------
void get_cells_from_elements(std::unordered_map<VTKCellType, vtkCellArray *> &cells,
                             std::unordered_map<int, vtkIdType> &node_map,
                             const odb_SequenceElement &element_sequence) {
    int num_elements = element_sequence.size();
    int num_nodes = 0;
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element &element = element_sequence[i];
        int element_label = element.label();
        std::string element_type = get_element_type_base(element.type().CStr());
        VTKCellType cell_type;

        if (auto it = ABQ_VTK_CELL_MAP.find(element_type); it != ABQ_VTK_CELL_MAP.end()) {
            cell_type = it->second;
        } else {
            std::cout << "Element type " << element_type << " is not supported.\n";
            return;
        }

        const int *const element_connectivity = element.connectivity(num_nodes);

        std::vector<vtkIdType> connectivity(num_nodes);
        for (int i = 0; i < num_nodes; i++) {
            connectivity[i] = node_map[element_connectivity[i]];
        }
        cells[cell_type]->InsertNextCell(connectivity.size(), connectivity.data());
    }
}

}  // namespace otk
