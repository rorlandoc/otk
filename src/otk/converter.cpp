#include "otk/converter.hpp"

#include <fmt/format.h>

#include <odb_API.h>

#include <vtkPartitionedDataSet.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkXMLPartitionedDataSetCollectionWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Convert mesh data to VTK format
//
// ---------------------------------------------------------------------------------------
void Converter::convert_mesh(otk::Odb& odb) {
    const odb_Assembly& root_assembly = odb.handle()->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        const odb_Instance& instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();
        std::string instance_name{instance.name().cStr()};

        fmt::print("Converting mesh data for instance: {}\n", instance_name);

        const odb_SequenceNode& instance_nodes = instance.nodes();
        const odb_SequenceElement& instance_elements = instance.elements();

        std::vector<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty()) {
            fmt::print("No supported elements found for instance {}\n", instance_name);
            continue;
        }

        std::unordered_map<int, vtkIdType> node_map;
        points_[instance_name] = get_points(node_map, instance_nodes, instance_type);
        cells_[instance_name] = get_cells(node_map, instance_elements);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Write mesh data to VTU file
//
// ---------------------------------------------------------------------------------------
void Converter::write_mesh(fs::path file) {
    auto writer = vtkSmartPointer<vtkXMLPartitionedDataSetCollectionWriter>::New();
    auto collection = vtkSmartPointer<vtkPartitionedDataSetCollection>::New();

    std::vector<std::string> instance_names = extract_keys(points_);

    int instance_id = 0;
    for (auto instance_name : instance_names) {
        fmt::print("Creating unstructured grid for instance: {}\n", instance_name);

        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        grid->SetPoints(points_[instance_name]);
        for (auto& [cell_type, cell_array] : cells_[instance_name]) {
            grid->SetCells(cell_type, cell_array);
        }

        collection->SetPartition(instance_id, 0, grid);
        instance_id++;
    }

    fmt::print("Writing collection file\n");

    writer->SetFileName(file.replace_extension(".vtpc").string().c_str());
    writer->SetInputData(collection);
    writer->Write();
}

// ---------------------------------------------------------------------------------------
//
//   Get the base element type without derivatives
//
// ---------------------------------------------------------------------------------------
std::string Converter::get_base_element_type(const std::string& element_type) {
    std::vector<std::string> supported_elements = extract_keys(ABQ_VTK_CELL_MAP);
    for (auto supported_element : supported_elements) {
        if (element_type.rfind(supported_element, 0) != std::string::npos) {
            return supported_element;
        }
    }
    return "Unsupported";
}

// ---------------------------------------------------------------------------------------
//
//   Get the cell types from an element sequence
//
// ---------------------------------------------------------------------------------------
std::vector<VTKCellType> Converter::get_cell_types(
    const odb_SequenceElement& element_sequence) {
    std::vector<VTKCellType> cell_types;
    int num_elements = element_sequence.size();
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element& element = element_sequence[i];
        std::string element_type = get_base_element_type(element.type().CStr());

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
Converter::CellArrayMap Converter::get_cells(
    const std::unordered_map<int, vtkIdType>& node_map,
    const odb_SequenceElement& element_sequence) {
    CellArrayMap cells;

    int num_elements = element_sequence.size();
    int num_nodes = 0;
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element& element = element_sequence[i];
        int element_label = element.label();
        std::string element_type = get_base_element_type(element.type().CStr());
        VTKCellType cell_type;

        if (element_type != "Unsupported") {
            cell_type = ABQ_VTK_CELL_MAP.at(element_type);

            const int* const element_connectivity = element.connectivity(num_nodes);

            std::vector<vtkIdType> connectivity(num_nodes);
            for (int j = 0; j < num_nodes; ++j) {
                connectivity[j] = node_map.at(element_connectivity[j]);
            }

            if (cells.find(cell_type) == cells.end()) {
                cells[cell_type] = vtkSmartPointer<vtkCellArray>::New();
            }
            cells[cell_type]->InsertNextCell(num_nodes, connectivity.data());
        } else {
            fmt::print("WARNING: Element type {} is not supported.\n", element_type);
            fmt::print("This element will be ignored.\n");
            fmt::print("This may lead to incorrect results.\n");
        }
    }

    return cells;
}

// -----------------------------------------------------------------------------------
//
//   Get vtkPoints from a node sequence
//
// -----------------------------------------------------------------------------------
Converter::PointArray Converter::get_points(std::unordered_map<int, vtkIdType>& node_map,
                                            const odb_SequenceNode& node_sequence,
                                            odb_Enum::odb_DimensionEnum instance_type) {
    auto points = vtkSmartPointer<vtkPoints>::New();

    int num_nodes = node_sequence.size();
    for (int i = 0; i < num_nodes; ++i) {
        const odb_Node& node = node_sequence[i];
        int node_label = node.label();
        const float* const node_coordinates = node.coordinates();

        if (instance_type == odb_Enum::THREE_D) {
            node_map[node_label] = points->InsertNextPoint(node_coordinates);
        } else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                   (instance_type == odb_Enum::AXISYMMETRIC)) {
            node_map[node_label] =
                points->InsertNextPoint(node_coordinates[0], node_coordinates[1], 0.0);
        }
    }

    return points;
}

}  // namespace otk