#include "otk/converter.hpp"

#include <fmt/format.h>

#include <odb_API.h>

#include <vtkPartitionedDataSet.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkXMLPartitionedDataSetCollectionWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <regex>
#include <thread>
#include <vector>

using namespace nlohmann;

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

        fmt::print("Converting mesh data for instance {} ... ", instance_name);

        const odb_SequenceNode& instance_nodes = instance.nodes();
        const odb_SequenceElement& instance_elements = instance.elements();

        std::vector<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty()) {
            fmt::print("skipping (no supported elements found)\n");
            continue;
        }

        std::unordered_map<int, vtkIdType> node_map;
        points_[instance_name] = get_points(node_map, instance_nodes, instance_type);
        cells_[instance_name] = get_cells(node_map, instance_elements);

        fmt::print("done\n");
    }
}

// ---------------------------------------------------------------------------------------
//
//   Convert field data to VTK format
//
// ---------------------------------------------------------------------------------------
void Converter::convert_fields(otk::Odb& odb) {
    const odb_Assembly& root_assembly = odb.handle()->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    using namespace nlohmann;

    json field_summary = odb.field_summary();
    json instance_summary = odb.instance_summary();

    json output_summary = process_field_summary(field_summary);
    json matches = match_request_to_available_data(output_summary["available_frames"],
                                                   output_summary["available_fields"]);
    json field_data = load_field_data(odb, matches);

    extract_field_data(odb, field_data, instance_summary);
}

// ---------------------------------------------------------------------------------------
//
//   Write mesh data to VTU file
//
// ---------------------------------------------------------------------------------------
void Converter::write(fs::path file) {
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
        for (auto& cell_array : cell_data_[instance_name]) {
            grid->GetCellData()->AddArray(cell_array);
        }
        for (auto& point_array : point_data_[instance_name]) {
            grid->GetPointData()->AddArray(point_array);
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
            fmt::print("Element type {} is not supported.", element_type);
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

// ---------------------------------------------------------------------------------------
//
//   Process summary JSON from Odb class
//
// ---------------------------------------------------------------------------------------
json Converter::process_field_summary(const json& summary) {
    json frame_numbers;
    json field_names;
    for (const auto& step : summary["steps"]) {
        auto step_name = step["name"].template get<std::string>();
        for (const auto& frame : step["frames"]) {
            auto frame_number = frame["increment"].template get<int>();
            frame_numbers[step_name].push_back(frame_number);
            for (const auto& field : frame["fields"]) {
                auto field_name = field["name"].template get<std::string>();
                field_names[step_name][frame_number].push_back(field_name);
            }
            std::sort(field_names[step_name][frame_number].begin(),
                      field_names[step_name][frame_number].end());
        }
    }
    json output;
    output["available_frames"] = frame_numbers;
    output["available_fields"] = field_names;
    return output;
}

// ---------------------------------------------------------------------------------------
//
//   Match output request to available data
//
// ---------------------------------------------------------------------------------------
json Converter::match_request_to_available_data(const json& frames, const json& fields) {
    json matches;

    // Get all the field names
    json fields_request = output_request_["fields"];
    std::vector<std::string> fields_requested;
    for (const auto& field_info : fields_request) {
        fields_requested.push_back(field_info["key"].template get<std::string>());
    }

    // Get all the frame numbers and match them to the available frames and fields
    json frames_request = output_request_["frames"];
    for (const auto& frame_info : frames_request) {
        auto step_name = frame_info["step"].template get<std::string>();
        auto frame_list = frame_info["list"].template get<std::vector<int>>();
        std::sort(frame_list.begin(), frame_list.end());

        // Intersect the requested frames with the available frames
        std::vector<int> frame_matches;
        std::set_intersection(frame_list.begin(), frame_list.end(),
                              frames[step_name].begin(), frames[step_name].end(),
                              std::back_inserter(frame_matches));

        // Regex match the requested fields with the available fields
        json field_matches;
        for (const auto& frame : frame_matches) {
            json field_matches_frame;
            field_matches_frame["frame"] = frame;
            for (const auto& request : fields_requested) {
                std::regex regex(request);
                const auto& field_names =
                    fields[step_name][frame].template get<std::vector<std::string>>();
                for (const auto& field_name : field_names) {
                    if (std::regex_match(field_name, regex)) {
                        field_matches_frame["fields"].push_back(field_name);
                    }
                }
            }
            field_matches.push_back(field_matches_frame);
        }
        matches[step_name]["fields"] = field_matches;
        matches[step_name]["frames"] = frame_matches;
    }

    return matches;
}

// ---------------------------------------------------------------------------------------
//
//   Load field data from Odb class
//
// ---------------------------------------------------------------------------------------
json Converter::load_field_data(otk::Odb& odb, const json& request) {
    json data;

    for (const auto& [step, request_info] : request.items()) {
        json step_data;

        const odb_Step& step_obj = odb.handle()->steps().constGet(step.c_str());

        const auto& fields_request = request_info["fields"];
        for (const auto& field_info : fields_request) {
            int frame = field_info["frame"].get<int>();
            auto fields = field_info["fields"].get<std::vector<std::string>>();
            json frame_data;
            frame_data["frame"] = frame;

            const odb_Frame& frame_obj = step_obj.frames().constGet(frame);
            const odb_FieldOutputRepository& fields_repo = frame_obj.fieldOutputs();

            for (const auto& field : fields) {
                field_outputs_.push_back(fields_repo.constGet(field.c_str()));
                frame_data["fields"][field] = field_outputs_.size() - 1;
            }

            step_data.push_back(frame_data);
        }
        data[step] = step_data;
    }

    return data;
}

// ---------------------------------------------------------------------------------------
//
//   Extract field data from Odb class
//
// ---------------------------------------------------------------------------------------
void Converter::extract_field_data(otk::Odb& odb, const json& data,
                                   const json& instance_summary) {
    odb_Assembly& root_assembly = odb.handle()->rootAssembly();
    odb_InstanceRepositoryIT it(root_assembly.instances());

    for (it.first(); !it.isDone(); it.next()) {
        std::string instance_name{it.currentValue().name().cStr()};

        bool supported = instance_summary[instance_name]["supported"].get<bool>();
        if (!supported) {
            fmt::print("Instance {} is not supported.\n", instance_name);
            fmt::print("This instance will be ignored.\n");
            fmt::print(
                "The instance has more than one section,"
                " but they are of different types.\n{}\n",
                instance_summary[instance_name]["section_categories"].dump(2));
            continue;
        }

        bool composite = instance_summary[instance_name]["composite"].get<bool>();
        extract_instance_field_data(odb, data, it.currentValue(), composite);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Extract field data from Instance
//
// ---------------------------------------------------------------------------------------
void Converter::extract_instance_field_data(otk::Odb& odb, const json& data,
                                            const odb_Instance& instance,
                                            bool composite) {
    for (auto& [step, step_data] : data.items()) {
        for (auto& frame_data : step_data) {
            int frame_id = frame_data["frame"].get<int>();
            for (auto& [field, field_data] : frame_data["fields"].items()) {
                int field_id = field_data.get<int>();
                const odb_FieldOutput& field_output = field_outputs_[field_id];
                const odb_FieldOutput& instance_field = field_output.getSubset(instance);
                const odb_Enum::odb_DataTypeEnum data_type = instance_field.type();

                const bool is_scalar = (data_type == odb_Enum::SCALAR);
                const bool is_vector = (data_type == odb_Enum::VECTOR);
                const bool is_tensor = (data_type == odb_Enum::TENSOR_3D_FULL ||
                                        data_type == odb_Enum::TENSOR_3D_PLANAR ||
                                        data_type == odb_Enum::TENSOR_2D_PLANAR);

                if (!(is_scalar || is_vector || is_tensor)) {
                    fmt::print("Field {} has unsupported data type ({}).\n", field,
                               static_cast<int>(data_type));
                    continue;
                }

                if (is_scalar) {
                    extract_scalar_field(instance_field, instance, composite);
                    continue;
                }
                if (is_vector) {
                    extract_vector_field(instance_field, instance, composite);
                    continue;
                }
                extract_tensor_field(instance_field, instance, composite);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   Extract scalar field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_scalar_field(const odb_FieldOutput& field,
                                     const odb_Instance& instance, bool composite) {
    const odb_SequenceFieldLocation& locations = field.locations();
    const odb_SequenceString& element_types = field.baseElementTypes();
    const odb_SequenceElement& elements = instance.elements();
    const odb_SequenceNode& nodes = instance.nodes();

    std::string field_name{field.name().cStr()};
    std::string instance_name{instance.name().cStr()};
    int num_elements = elements.size();
    int num_nodes = nodes.size();

    int num_locations = locations.size();
    int num_types = element_types.size();

    if (num_locations == 0) {
        fmt::print("Field {} {} has no output locations.\n", field_name, instance_name);
        return;
    }

    int location_index = 0;               // Default to first location
    bool requires_extrapolation = false;  // Interpolation to nodes

    for (int ilocation = 0; ilocation < num_locations; ++ilocation) {
        switch (locations[ilocation].position()) {
            case odb_Enum::odb_ResultPositionEnum::WHOLE_ELEMENT:
                location_index = ilocation;
                break;
            case odb_Enum::odb_ResultPositionEnum::NODAL:
                location_index = ilocation;
                break;
            case odb_Enum::odb_ResultPositionEnum::INTEGRATION_POINT:
                location_index = ilocation;
                requires_extrapolation = true;
                break;
            default:
                fmt::print("Unsupported field output position for {} {} ({}).\n",
                           field_name, instance_name,
                           static_cast<int>(locations[ilocation].position()));
                return;
        }
    }

    const odb_FieldLocation& location = locations[location_index];
    odb_FieldOutput localized_field = field.getSubset(location);

    if (requires_extrapolation) {
        localized_field =
            localized_field.getSubset(odb_Enum::odb_ResultPositionEnum::ELEMENT_NODAL);
    }
    if (composite) {
        odb_SequenceFieldOutput composite_fields(1);
        composite_fields.insert(0, localized_field);
        composite_fields = maxEnvelope(composite_fields);
        localized_field = composite_fields[0];
    }

    const odb_SequenceFieldBulkData& blocks = localized_field.bulkDataBlocks();
    int num_blocks = blocks.size();

    std::vector<double> data_buffer;
    if (location.position() == odb_Enum::odb_ResultPositionEnum::WHOLE_ELEMENT) {
        data_buffer.resize(num_elements);
        if (num_blocks == 0) {
            const odb_SequenceFieldValue& values = localized_field.values();
            int num_field_values = values.size();
            for (int i = 0; i < num_field_values; ++i) {
                const odb_FieldValue& value = values[i];
                odb_Enum::odb_ResultPositionEnum position = value.position();
                int label = value.elementLabel();

                switch (value.precision()) {
                    case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                        const odb_SequenceDouble& data = value.dataDouble();
                        int data_size = data.size();
                        data_buffer[label - 1] = data[0];
                        break;
                    }
                    case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                        const odb_SequenceFloat& data = value.data();
                        int data_size = data.size();
                        data_buffer[label - 1] = data[0];
                        break;
                    }
                }
            }
        } else {
            for (int iblock = 0; iblock < num_blocks; ++iblock) {
                const odb_FieldBulkData& block = blocks[iblock];
                int* elements = block.elementLabels();
                int num_elements = block.numberOfElements();
                int* labels = block.elementLabels();
                int length = block.length();

                if (block.width() != 1) {
                    fmt::print("Unsupported field width for {} {} (block {}, {}).\n",
                               field_name, instance_name, iblock, block.width());
                    return;
                }
                if (length != num_elements) {
                    fmt::print("Inconsistent block length for {} {} (block {}, {}).\n",
                               field_name, instance_name, iblock, length);
                    return;
                }

                switch (block.precision()) {
                    case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                        double* data = block.dataDouble();
                        for (int i = 0; i < num_elements; ++i) {
                            data_buffer[labels[i] - 1] = data[i];
                        }
                        break;
                    }
                    case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                        float* data = block.data();
                        for (int i = 0; i < num_elements; ++i) {
                            data_buffer[labels[i] - 1] = data[i];
                        }
                        break;
                    }
                }
            }
        }
        cell_data_[instance_name].push_back(vtkSmartPointer<vtkDoubleArray>::New());
        auto& array = cell_data_[instance_name].back();
        array->SetName(field_name.c_str());
        array->SetNumberOfComponents(1);
        for (const auto& value : data_buffer) {
            array->InsertNextValue(value);
        }
    } else {
        data_buffer.resize(num_nodes);
        std::vector<int> node_counts(data_buffer.size(), 0);
        for (int iblock = 0; iblock < num_blocks; ++iblock) {
            const odb_FieldBulkData& block = blocks[iblock];
            int* nodes = block.nodeLabels();
            int num_nodes = block.numberOfNodes();
            int* labels = block.nodeLabels();
            int length = block.length();

            if (block.width() != 1) {
                fmt::print("Unsupported field width for {} {} (block {}, {}).\n",
                           field_name, instance_name, iblock, block.width());
                return;
            }
            if (length != num_nodes) {
                fmt::print("Inconsistent block length for {} {} (block {}, {}).\n",
                           field_name, instance_name, iblock, length);
                return;
            }

            switch (block.precision()) {
                case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                    double* data = block.dataDouble();
                    for (int i = 0; i < num_nodes; ++i) {
                        data_buffer[labels[i] - 1] = data[i];
                        node_counts[labels[i] - 1]++;
                    }
                    break;
                }
                case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                    float* data = block.data();
                    for (int i = 0; i < num_nodes; ++i) {
                        data_buffer[labels[i] - 1] = data[i];
                        node_counts[labels[i] - 1]++;
                    }
                    break;
                }
            }
        }
        if (requires_extrapolation) {
            for (int i = 0; i < data_buffer.size(); ++i) {
                data_buffer[i] /= node_counts[i];
            }
        }
        point_data_[instance_name].push_back(vtkSmartPointer<vtkDoubleArray>::New());
        auto& array = point_data_[instance_name].back();
        array->SetName(field_name.c_str());
        array->SetNumberOfComponents(1);
        for (const auto& value : data_buffer) {
            array->InsertNextValue(value);
        }
    }
    return;
}

// ---------------------------------------------------------------------------------------
//
//   Extract vector field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_vector_field(const odb_FieldOutput& field_output,
                                     const odb_Instance& instance, bool composite) {}

// ---------------------------------------------------------------------------------------
//
//   Extract tensor field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_tensor_field(const odb_FieldOutput& field_output,
                                     const odb_Instance& instance, bool composite) {}

}  // namespace otk