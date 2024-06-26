#include "otk/converter.hpp"

#include <fmt/format.h>

#include <odb_API.h>

#include <vtkInformation.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkXMLPartitionedDataSetCollectionWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <iostream>
#include <regex>
#include <set>
#include <thread>
#include <vector>

using namespace nlohmann;

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Convert ODB file to VTK format
//
// ---------------------------------------------------------------------------------------
void Converter::convert(otk::Odb& odb, fs::path file) {
    json field_summary = odb.field_summary(output_request_["frames"]);
    json instance_summary = odb.instance_summary();

    json output_summary = process_field_summary(field_summary);
    json matches = match_request_to_available_data(output_summary["available_frames"],
                                                   output_summary["available_fields"]);

    for (const auto& [step, step_data] : matches.items()) {
        if (step_data["fields"].is_null()) {
            fmt::print("ERROR - No matching fields found in {}.\n", step);
            return;
        }
        if (step_data["frames"].empty()) {
            fmt::print("ERROR - No matching frames found in {}.\n", step);
            return;
        }
    }

    convert_mesh(odb);
    convert_fields(odb, file, field_summary, instance_summary, output_summary, matches);
}

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

        std::cout << fmt::format("Converting mesh data for {}...  ", instance_name);
        std::cout << std::flush;

        const odb_SequenceNode& instance_nodes = instance.nodes();
        const odb_SequenceElement& instance_elements = instance.elements();

        std::set<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty()) {
            fmt::print("skipping (no supported elements found)\n");
            continue;
        }

        std::unordered_map<int, vtkIdType> node_map;
        points_[instance_name] = get_points(node_map, instance_nodes, instance_type);
        cells_[instance_name] =
            get_cells(node_map, instance_elements, instance_name, instance);

        std::cout << fmt::format("done\n");
        std::cout << std::flush;
    }
}

// ---------------------------------------------------------------------------------------
//
//   Convert field data to VTK format
//
// ---------------------------------------------------------------------------------------
void Converter::convert_fields(otk::Odb& odb, fs::path file, json field_summary,
                               json instance_summary, json output_summary, json matches) {
    std::cout << fmt::format("Started field data conversion.\n");
    std::cout << std::flush;

    for (auto& [step, step_data] : matches.items()) {
        for (auto& frame_data : step_data["frames"]) {
            int frame_id = frame_data.get<int>();

            cell_data_.clear();
            point_data_.clear();
            field_outputs_.clear();

            std::cout << fmt::format("Converting field data for {} frame {}:\n", step,
                                     frame_id);
            std::cout << std::flush;

            json field_data = load_field_data(odb, matches, step, frame_id);
            extract_field_data(odb, field_data, instance_summary, step, frame_id);
            write(file, frame_id);
        }
    }

    std::cout << fmt::format("Completed field data conversion.\n");
    std::cout << std::flush;
}

// ---------------------------------------------------------------------------------------
//
//   Write mesh data to VTU file
//
// ---------------------------------------------------------------------------------------
void Converter::write(fs::path file, int frame_id) {
    auto writer = vtkSmartPointer<vtkXMLPartitionedDataSetCollectionWriter>::New();
    auto collection = vtkSmartPointer<vtkPartitionedDataSetCollection>::New();

    std::vector<std::string> instance_names = extract_keys(points_);
    std::sort(instance_names.begin(), instance_names.end());

    int instance_id = 0;

    std::cout << fmt::format("    - Writing frame...  ", frame_id);
    std::cout << std::flush;

    for (auto& instance_name : instance_names) {
        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        grid->SetPoints(points_[instance_name]);
        grid->SetCells(cells_[instance_name].first.data(), cells_[instance_name].second);

        for (auto& cell_array : cell_data_[instance_name]) {
            grid->GetCellData()->AddArray(cell_array);
        }
        for (auto& point_array : point_data_[instance_name]) {
            if (point_array->GetNumberOfComponents() == 3) {
                grid->GetPointData()->SetVectors(point_array);
            } else {
                grid->GetPointData()->AddArray(point_array);
            }
        }

        collection->SetPartition(instance_id, 0, grid);
        collection->GetMetaData(instance_id)
            ->Set(vtkCompositeDataSet::NAME(), instance_name);
        instance_id++;
    }

    writer->SetFileName(fmt::format("{}/{}/{}_{}.vtpc", file.parent_path().string(),
                                    file.stem().string(), file.stem().string(), frame_id)
                            .c_str());
    writer->SetInputData(collection);
    writer->Write();

    std::cout << fmt::format("done\n");
    std::cout << std::flush;
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
std::set<VTKCellType> Converter::get_cell_types(
    const odb_SequenceElement& element_sequence) {
    std::set<VTKCellType> cell_types;
    int num_elements = element_sequence.size();
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element& element = element_sequence[i];
        std::string element_type = get_base_element_type(element.type().CStr());

        if (element_type == "Unsupported") {
            continue;
        }

        cell_types.insert(ABQ_VTK_CELL_MAP.at(element_type));
    }
    return cell_types;
}

// ---------------------------------------------------------------------------------------
//
//   Get vtkCellArrays from an element sequence
//
// ---------------------------------------------------------------------------------------
Converter::CellArrayPair Converter::get_cells(
    const std::unordered_map<int, vtkIdType>& node_map,
    const odb_SequenceElement& element_sequence, const std::string& instance_name,
    const odb_Instance& instance) {
    CellArrayPair cells;

    int num_elements = element_sequence.size();
    int num_nodes = 0;

    ElementLabelMap element_labels;

    cells.second = vtkSmartPointer<vtkCellArray>::New();
    cells.first.reserve(num_elements);

    for (int i = 0; i < num_elements; ++i) {
        const odb_Element& element = element_sequence[i];
        int element_label = element.label();
        std::string raw_type{element.type().CStr()};
        std::string element_type = get_base_element_type(raw_type);
        std::string section_category{element.sectionCategory().name().CStr()};
        std::string key = fmt::format("{} {}", section_category, raw_type);
        int cell_type;

        if (element_type != "Unsupported") {
            cell_type = static_cast<int>(ABQ_VTK_CELL_MAP.at(element_type));

            const int* const element_connectivity = element.connectivity(num_nodes);

            std::vector<vtkIdType> connectivity(num_nodes);
            for (int j = 0; j < num_nodes; ++j) {
                connectivity[j] = node_map.at(element_connectivity[j]);
            }

            cells.first.push_back(cell_type);
            cells.second->InsertNextCell(num_nodes, connectivity.data());

            if (!section_elements_[instance_name].contains(key)) {
                odb_SequenceElement temp(instance);
                temp.append(element);
                section_elements_[instance_name][key] = temp;
            } else {
                section_elements_[instance_name][key].append(element);
            }
            element_labels[element_label] = i;

        } else {
            fmt::print("WARNING: Element type {} is not supported.\n", element_type);
            fmt::print("This element will be ignored.\n");
            fmt::print("This may lead to incorrect results.\n");
        }
    }

    element_map_[instance_name] = element_labels;

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

    std::cout << fmt::format("Processing ODB field summary...  ");
    std::cout << std::flush;

    for (const auto& step : summary["steps"]) {
        auto step_name = step["name"].template get<std::string>();
        for (const auto& frame : step["frames"]) {
            auto frame_number = frame["index"].template get<int>();
            frame_numbers[step_name].push_back(frame_number);
            for (const auto& field : frame["fields"]) {
                auto field_name = field["name"].template get<std::string>();
                field_names[step_name][std::to_string(frame_number)].push_back(
                    field_name);
            }
        }
    }
    json output;
    output["available_frames"] = frame_numbers;
    output["available_fields"] = field_names;

    std::cout << fmt::format("done\n");
    std::cout << std::flush;

    return output;
}

// ---------------------------------------------------------------------------------------
//
//   Match output request to available data
//
// ---------------------------------------------------------------------------------------
json Converter::match_request_to_available_data(const json& frames, const json& fields) {
    json matches;

    std::cout << fmt::format("Matching output request to available data...  ");
    std::cout << std::flush;

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
                const auto& field_names = fields[step_name][std::to_string(frame)]
                                              .template get<std::vector<std::string>>();
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

    std::cout << fmt::format("done\n");
    std::cout << std::flush;

    // std::cout << "\nframes:\n" << frames.dump(2) << "\n\n" << std::flush;
    // std::cout << "\nfields:\n" << fields.dump(2) << "\n\n" << std::flush;
    // std::cout << "\nmatches\n" << matches.dump(2) << "\n\n" << std::flush;

    return matches;
}

// ---------------------------------------------------------------------------------------
//
//   Load field data from Odb class
//
// ---------------------------------------------------------------------------------------
json Converter::load_field_data(otk::Odb& odb, const json& request,
                                const std::string& step_name, int frame_id) {
    json data;

    std::cout << fmt::format("    - Loading field data...  ");
    std::cout << std::flush;

    json frame_data;

    const odb_Step& step_obj = odb.handle()->steps().constGet(step_name.c_str());

    const auto& fields_request = request[step_name]["fields"];
    for (const auto& field_info : fields_request) {
        int frame = field_info["frame"].get<int>();
        if (frame != frame_id) {
            continue;
        }

        auto fields = field_info["fields"].get<std::vector<std::string>>();
        frame_data["frame"] = frame;

        const odb_Frame& frame_obj = step_obj.frames().constGet(frame);
        const odb_FieldOutputRepository& fields_repo = frame_obj.fieldOutputs();

        for (const auto& field : fields) {
            field_outputs_.push_back(std::move(fields_repo.constGet(field.c_str())));
            frame_data["fields"][field] = field_outputs_.size() - 1;
        }
    }
    data[step_name] = frame_data;

    std::cout << fmt::format("done\n");
    std::cout << std::flush;

    return data;
}

// ---------------------------------------------------------------------------------------
//
//   Extract field data from Odb class
//
// ---------------------------------------------------------------------------------------
void Converter::extract_field_data(otk::Odb& odb, const json& data,
                                   const json& instance_summary,
                                   const std::string& step_name, int frame_id) {
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
        extract_instance_field_data(odb, data,
                                    root_assembly.instances().get(it.currentKey()),
                                    composite, step_name, frame_id);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Extract field data from Instance
//
// ---------------------------------------------------------------------------------------
void Converter::extract_instance_field_data(otk::Odb& odb, const json& data,
                                            odb_Instance& instance, bool composite,
                                            const std::string& step_name, int frame_id) {
    std::cout << fmt::format("    - Filtering {} elements and sections... ",
                             instance.name().cStr());
    std::cout << std::flush;

    std::vector<odb_Set> element_sets;
    for (const auto& [key, elements] : section_elements_[instance.name().cStr()]) {
        const odb_String set_name{key.c_str()};
        odb_Set set;
        if (instance.elementSets().isMember(set_name) == false) {
            set = instance.ElementSet(set_name, elements);
        } else {
            set = instance.elementSets().get(set_name);
        }
        element_sets.push_back(set);
    }

    std::cout << fmt::format("done\n");
    std::cout << std::flush;

    std::cout << fmt::format("    - Processing {}... ", instance.name().cStr());
    std::cout << std::flush;

    for (auto& [field, field_data] : data[step_name]["fields"].items()) {
        int field_id = field_data.get<int>();
        const odb_FieldOutput& field_output = field_outputs_[field_id];
        const odb_FieldOutput& instance_field = field_output.getSubset(instance);
        if (instance_field.locations().size() == 0) {
            continue;
        }

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
            extract_scalar_field(instance_field, element_sets, instance, composite);
            continue;
        }
        if (is_vector) {
            extract_vector_field(instance_field, element_sets, instance, composite);
            continue;
        }
        extract_tensor_field(instance_field, element_sets, instance, composite);
    }

    std::cout << fmt::format("done\n");
    std::cout << std::flush;
}

// ---------------------------------------------------------------------------------------
//
//   Extract scalar field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_scalar_field(const odb_FieldOutput& field,
                                     const std::vector<odb_Set>& element_sets,
                                     const odb_Instance& instance, bool composite) {
    std::string field_name{field.name().cStr()};
    std::string instance_name{instance.name().cStr()};

    ElementLabelMap element_labels = element_map_[instance_name];

    int num_instance_elements = instance.elements().size();
    int num_instance_nodes = instance.nodes().size();
    int num_section_assignments = instance.sectionAssignments().size();

    std::vector<double> data_buffer(std::max(num_instance_elements, num_instance_nodes),
                                    0.0);
    std::vector<int> node_counts(num_instance_nodes, 0);
    bool use_point_data = false;
    bool use_cell_data = false;
    bool requires_extrapolation = false;  // Interpolation to nodes
    bool may_require_reduction = false;   // Reduction across section points

    for (const auto& set : element_sets) {
        odb_FieldOutput localized_field = field.getSubset(set);

        const odb_SequenceFieldLocation locations = localized_field.locations();
        int num_locations = locations.size();
        if (num_locations == 0) {
            continue;
        }

        int location_index = 0;
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
                    may_require_reduction = true;
                    break;
                default:
                    fmt::print("Unsupported field output position for {} {} ({}).\n",
                               field_name, instance_name,
                               static_cast<int>(locations[ilocation].position()));
                    return;
            }
        }
        const odb_FieldLocation location = locations[location_index];
        localized_field = localized_field.getSubset(location);

        const odb_SequenceSectionPoint section_pts =
            localized_field.locations()[0].sectionPoint();
        int num_section_pts = section_pts.size();

        if (requires_extrapolation) {
            localized_field = localized_field.getSubset(
                odb_Enum::odb_ResultPositionEnum::ELEMENT_NODAL);
        }
        if (composite && may_require_reduction) {
            odb_SequenceFieldOutput composite_fields(num_section_pts);
            for (int i = 0; i < num_section_pts; ++i) {
                const odb_SectionPoint section_pt = section_pts.constGet(i);
                odb_FieldOutput temp_field = localized_field.getSubset(section_pt);
                temp_field = abs(temp_field);
                composite_fields.append(temp_field);
            }
            composite_fields.append(localized_field);
            composite_fields = maxEnvelope(composite_fields);
            localized_field = composite_fields[0];
        }

        const odb_SequenceFieldBulkData& blocks = localized_field.bulkDataBlocks();
        int num_blocks = blocks.size();

        if (location.position() == odb_Enum::odb_ResultPositionEnum::WHOLE_ELEMENT) {
            use_cell_data = true;
            for (int iblock = 0; iblock < num_blocks; ++iblock) {
                const odb_FieldBulkData& block = blocks[iblock];
                int num_elements = block.numberOfElements();
                int* labels = block.elementLabels();

                if (block.width() != 1) {
                    fmt::print("Unsupported field width for {} {} (block {}, {}).\n",
                               field_name, instance_name, iblock, block.width());
                    return;
                }

                switch (block.precision()) {
                    case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                        double* data = block.dataDouble();
                        for (int i = 0; i < num_elements; ++i) {
                            int id = element_labels[labels[i]];
                            data_buffer[id] = data[i];
                        }
                        break;
                    }
                    case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                        float* data = block.data();
                        for (int i = 0; i < num_elements; ++i) {
                            int id = element_labels[labels[i]];
                            data_buffer[id] = data[i];
                        }
                        break;
                    }
                }
            }

        } else {
            use_point_data = true;
            for (int iblock = 0; iblock < num_blocks; ++iblock) {
                const odb_FieldBulkData& block = blocks[iblock];
                int num_nodes = block.numberOfNodes();
                int* labels = block.nodeLabels();

                if (block.width() != 1) {
                    fmt::print("Unsupported field width for {} {} (block {}, {}).\n",
                               field_name, instance_name, iblock, block.width());
                    return;
                }

                switch (block.precision()) {
                    case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                        double* data = block.dataDouble();
                        for (int i = 0; i < num_nodes; ++i) {
                            data_buffer[labels[i] - 1] += data[i];
                            node_counts[labels[i] - 1]++;
                        }
                        break;
                    }
                    case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                        float* data = block.data();
                        for (int i = 0; i < num_nodes; ++i) {
                            data_buffer[labels[i] - 1] += data[i];
                            node_counts[labels[i] - 1]++;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (use_cell_data) {
        data_buffer.resize(num_instance_elements);
        cell_data_[instance_name].push_back(vtkSmartPointer<vtkDoubleArray>::New());
        auto& array = cell_data_[instance_name].back();
        array->SetName(field_name.c_str());
        array->SetNumberOfComponents(1);
        for (const auto& value : data_buffer) {
            array->InsertNextValue(value);
        }
    } else if (use_point_data) {
        data_buffer.resize(num_instance_nodes);
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
}

// ---------------------------------------------------------------------------------------
//
//   Extract vector field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_vector_field(const odb_FieldOutput& field_output,
                                     const std::vector<odb_Set>& element_sets,
                                     const odb_Instance& instance, bool composite) {
    std::string field_name{field_output.name().cStr()};
    std::string instance_name{instance.name().cStr()};

    int num_instance_elements = instance.elements().size();
    int num_instance_nodes = instance.nodes().size();
    int num_section_assignments = instance.sectionAssignments().size();

    std::vector<std::vector<double>> data_buffer(num_instance_nodes, {0.0, 0.0, 0.0});
    const odb_SequenceFieldBulkData& blocks = field_output.bulkDataBlocks();
    int num_blocks = blocks.size();

    for (int iblock = 0; iblock < num_blocks; ++iblock) {
        const odb_FieldBulkData& block = blocks[iblock];
        int num_nodes = block.numberOfNodes();
        int num_components = block.width();
        int* labels = block.nodeLabels();

        if (num_components != 3 && num_components != 2) {
            fmt::print("Unsupported field width for {} {} (block {}, {}).\n", field_name,
                       instance_name, iblock, block.width());
            return;
        }

        switch (block.precision()) {
            case odb_Enum::odb_PrecisionEnum::DOUBLE_PRECISION: {
                double* data = block.dataDouble();
                int pos = 0;
                for (int i = 0; i < num_nodes; ++i) {
                    for (int j = 0; j < num_components; ++j) {
                        data_buffer[labels[i] - 1][j] = data[pos++];
                    }
                }
                break;
            }
            case odb_Enum::odb_PrecisionEnum::SINGLE_PRECISION: {
                float* data = block.data();
                int pos = 0;
                for (int i = 0; i < num_nodes; ++i) {
                    for (int j = 0; j < num_components; ++j) {
                        data_buffer[labels[i] - 1][j] = data[pos++];
                    }
                }
                break;
            }
        }
    }

    point_data_[instance_name].push_back(vtkSmartPointer<vtkDoubleArray>::New());
    auto& array = point_data_[instance_name].back();
    array->SetName(field_name.c_str());
    array->SetNumberOfComponents(3);
    for (const auto& value : data_buffer) {
        array->InsertNextTuple3(value[0], value[1], value[2]);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Extract tensor field data
//
// ---------------------------------------------------------------------------------------
void Converter::extract_tensor_field(const odb_FieldOutput& field_output,
                                     const std::vector<odb_Set>& element_sets,
                                     const odb_Instance& instance, bool composite) {}

}  // namespace otk