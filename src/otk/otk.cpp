#include "otk/otk.hpp"

#include <odb_API.h>
#include <odb_MaterialTypes.h>
#include <odb_SectionTypes.h>
#include <vtkCellArray.h>
#include <vtkHexahedron.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkQuad.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

namespace otk
{

std::filesystem::path find_file(std::filesystem::path current_path)
{
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::directory_iterator(current_path))
    {
        if (entry.is_directory() || entry.path().extension() == ".odb")
            files.push_back(entry.path());
    }
    int width = int(std::log10(files.size())) + 1;
    std::cout << "Contents of " << current_path << "\n\n";

    int i = 0;
    std::cout << std::setw(width) << i++ << ": ..\n";
    for (const auto &file : files)
    {
        std::cout << std::setw(width) << i++ << ": ";
        std::cout << file.filename().string() << "\n";
    }

    int file_index = 0;
    bool valid_input = false;
    while (!valid_input)
    {
        std::cout << "\nSelect file to open: ";
        std::cin >> file_index;
        bool index_valid = (file_index > -1 && file_index <= files.size());
        if (index_valid)
        {
            bool is_parent_dir = (file_index == 0);
            if (is_parent_dir)
            {
                return find_file(current_path.parent_path());
            }
            else
            {
                bool is_dir =
                    std::filesystem::is_directory(files[file_index - 1]);
                if (is_dir)
                {
                    return find_file(files[file_index - 1]);
                }
                else
                {
                    bool is_odb_file =
                        (files[file_index - 1].extension() == ".odb");
                    if (is_odb_file)
                    {
                        valid_input = true;
                    }
                    else
                    {
                        std::cout
                            << "File select is not an ODB file. Try again.\n";
                    }
                }
            }
        }
        else
        {
            std::cout << "Invalid input. Try again.\n";
        }
    }
    std::filesystem::path file_path = files[file_index - 1];
    std::cout << std::endl;
    return file_path.string();
}

Odb::Odb(std::filesystem::path path)
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("File does not exist.");
    }
    if (path.extension().string() != ".odb")
    {
        throw std::runtime_error("File is not an ODB file.");
    }

    odb_initializeAPI();

    odb_ = &openOdb(path.string().c_str());
    path_ = path;
}

Odb::~Odb()
{
    odb_->close();

    odb_finalizeAPI();
}

std::string Odb::path() const
{
    return path_.string();
}

std::string Odb::name() const
{
    return path_.filename().string();
}

nlohmann::json Odb::instances() const
{
    nlohmann::json j;
    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next())
    {
        nlohmann::json i;

        const odb_String &instance_name = instance_iterator.currentKey();
        const odb_Instance &instance = instance_iterator.currentValue();

        i["name"] = instance_name.CStr();
        i["type"] = instance.embeddedSpace();
        i["num_nodes"] = instance.nodes().size();
        i["num_elements"] = instance.elements().size();

        j.push_back(i);
    }
    return j;
}

nlohmann::json Odb::nodes(const std::string &instance_name) const
{
    nlohmann::json j;
    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance =
        root_assembly.instances().constGet(instance_name.c_str());
    odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();
    const odb_SequenceNode &instance_nodes = instance.nodes();

    int num_nodes = instance_nodes.size();
    for (int i = 0; i < num_nodes; ++i)
    {
        const odb_Node &node = instance_nodes[i];
        j[i]["label"] = node.label();
        if (instance_type == odb_Enum::THREE_D)
        {
            j[i]["dimension"] = 3;
            j[i]["coordinates"] = {node.coordinates()[0], node.coordinates()[1],
                                   node.coordinates()[2]};
        }
        else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                 (instance_type == odb_Enum::AXISYMMETRIC))
        {
            j[i]["dimension"] = 2;
            j[i]["coordinates"] = {node.coordinates()[0],
                                   node.coordinates()[1]};
        }
    }
    return j;
}

nlohmann::json Odb::elements(const std::string &instance_name) const
{
    nlohmann::json j;
    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance =
        root_assembly.instances().constGet(instance_name.c_str());
    const odb_SequenceElement &instance_elements = instance.elements();

    int num_elements = instance_elements.size();
    for (int i = 0; i < num_elements; ++i)
    {
        const odb_Element &element = instance_elements[i];
        j[i]["type"] = element.type().CStr();
        j[i]["label"] = element.label();
        j[i]["index"] = element.index();

        int n = 0;
        const int *const element_connectivity = element.connectivity(n);
        j[i]["num_nodes"] = n;

        nlohmann::json connectivity;
        for (int j = 0; j < n; ++j)
        {
            connectivity.push_back(element_connectivity[j]);
        }
        j[i]["connectivity"] = connectivity;
    }
    return j;
}

nlohmann::json Odb::steps() const
{
    nlohmann::json j;
    odb_StepRepositoryIT step_iterator(odb_->steps());
    int num_steps = odb_->steps().size();
    for (step_iterator.first(); !step_iterator.isDone(); step_iterator.next())
    {
        const odb_Step &step = step_iterator.currentValue();
        j[step.name().CStr()]["num_frames"] = step.frames().size();
    }
    return j;
}

nlohmann::json Odb::frames(const std::string &step_name) const
{
    nlohmann::json j;
    const odb_Step &step = odb_->steps().constGet(step_name.c_str());
    const odb_SequenceFrame &frames = step.frames();
    int num_frames = frames.size();
    for (int i = 0; i < num_frames; ++i)
    {
        nlohmann::json f;
        const odb_Frame &frame = frames[i];

        f["id"] = frame.frameId();
        f["index"] = frame.frameIndex();
        f["value"] = frame.frameValue();
        f["increment"] = frame.incrementNumber();
        j.push_back(f);
    }
    return j;
}

nlohmann::json Odb::fields(const std::string &step_name,
                           const int frame_number) const
{
    nlohmann::json j;
    const odb_Step &step = odb_->steps().constGet(step_name.c_str());
    const odb_Frame &frame = step.frames().constGet(frame_number);
    odb_FieldOutputRepository field_outputs = frame.fieldOutputs();
    odb_FieldOutputRepositoryIT field_output_iterator(field_outputs);
    for (field_output_iterator.first(); !field_output_iterator.isDone();
         field_output_iterator.next())
    {
        nlohmann::json f;

        const odb_FieldOutput &field_output =
            field_output_iterator.currentValue();

        f["name"] = field_output.name().CStr();
        f["num_blocks"] = field_output.bulkDataBlocks().size();
        f["has_orientation"] = field_output.hasOrientation();

        const odb_SequenceFieldLocation &locations = field_output.locations();
        int num_locations = locations.size();
        for (int i = 0; i < num_locations; ++i)
        {
            nlohmann::json l;
            const odb_FieldLocation &location = locations[i];

            l["position"] = location.position();
            l["num_section_points"] = location.sectionPoint().size();

            f["locations"].push_back(l);
        }

        j.push_back(f);
    }
    return j;
}

std::ostream &operator<<(std::ostream &os, const Odb &odb)
{
    odb_Assembly &root_assembly = odb.odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next())
    {
        const odb_String &instance_name = instance_iterator.currentKey();
        os << instance_name.CStr() << "\n";

        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();
        os << "Type: " << instance_type << "\n";

        const odb_SequenceNode &instance_nodes = instance.nodes();
        int num_nodes = instance_nodes.size();
        os << "Number of nodes: " << num_nodes << "\n";

        const odb_SequenceElement &instance_elements = instance.elements();
        int num_elements = instance_elements.size();
        os << "Number of elements: " << num_elements << "\n";
    }

    odb_StepRepositoryIT step_iterator(odb.odb_->steps());
    int num_steps = odb.odb_->steps().size();
    os << "Number of steps: " << num_steps << "\n";
    for (step_iterator.first(); !step_iterator.isDone(); step_iterator.next())
    {
        const odb_Step &step = step_iterator.currentValue();
        os << "  - " << step.name().CStr();

        const odb_SequenceFrame &frames = step.frames();
        int num_frames = frames.size();
        os << " (" << num_frames << " frames)\n";

        std::unordered_map<std::string, size_t> field_map;
        std::unordered_map<std::string, size_t> field_bulk_data_map;

        for (int iframe = 0; iframe < num_frames; ++iframe)
        {
            const odb_Frame &frame = frames[iframe];
            odb_FieldOutputRepository field_outputs = frame.fieldOutputs();
            odb_FieldOutputRepositoryIT field_output_iterator(field_outputs);
            for (field_output_iterator.first(); !field_output_iterator.isDone();
                 field_output_iterator.next())
            {
                const odb_FieldOutput &field_output =
                    field_output_iterator.currentValue();
                std::string field_output_name{
                    field_output_iterator.currentKey().CStr()};
                if (field_map.find(field_output_name) == field_map.end())
                {
                    field_map[field_output_name] = 1;
                }
                else
                {
                    field_map[field_output_name]++;
                }
                if (field_bulk_data_map.find(field_output_name) ==
                    field_bulk_data_map.end())
                {
                    field_bulk_data_map[field_output_name] =
                        field_output.bulkDataBlocks().size();
                }
            }
        }

        for (auto &field_info : field_map)
        {
            os << "        " << field_info.first << " (" << field_info.second
               << " frames x " << field_bulk_data_map[field_info.first]
               << " blocks)\n";
        }
    }
    return os;
}

void Odb::write_info()
{
    using namespace nlohmann;
    json j;
    j["path"] = path_.string();

    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());
    j["instances"]["number"] = root_assembly.instances().size();

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next())
    {
        const odb_String &instance_name = instance_iterator.currentKey();
        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();

        json j_instance;
        j_instance["name"] = instance_name.CStr();
        j_instance["type"] = instance_type;

        const odb_SequenceNode &instance_nodes = instance.nodes();
        const odb_SequenceElement &instance_elements = instance.elements();
        int number_nodes = instance_nodes.size();
        int number_elements = instance_elements.size();
        j_instance["nodes"]["number"] = number_nodes;
        j_instance["elements"]["number"] = number_elements;

        for (int i = 0; i < number_elements; ++i)
        {
            json j_element;

            const odb_Element &element = instance_elements[i];
            std::string element_type = element.type().CStr();
            int element_label = element.label();
            int element_index = element.index();

            int n = 0;
            const int *const element_connectivity = element.connectivity(n);

            j_element["type"] = element_type;
            j_element["label"] = element_label;
            j_element["index"] = element_index;
            j_element["number_nodes"] = n;

            for (int j = 0; j < n; ++j)
            {
                j_element["connectivity"].push_back(element_connectivity[j]);
            }
            j_instance["elements"]["data"].push_back(j_element);
        }
        j["instances"]["data"].push_back(j_instance);
    }

    std::ofstream odb_info_file(path_.replace_extension(".json"));
    odb_info_file << j.dump(4) << std::endl;
    odb_info_file.close();
}

void Odb::write_vtu()
{
    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    vtkNew<vtkXMLUnstructuredGridWriter> writer;

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next())
    {
        vtkNew<vtkPoints> points;
        vtkNew<vtkUnstructuredGrid> grid;

        std::unordered_map<int, vtkIdType> node_map;
        std::unordered_map<std::string, const float *> element_map;

        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();

        const odb_SequenceNode &instance_nodes = instance.nodes();
        const odb_SequenceElement &instance_elements = instance.elements();

        std::vector<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty())
        {
            std::cout << "No supported cell types found.\n";
            return;
        }

        std::unordered_map<VTKCellType, vtkCellArray *> cells;
        for (auto cell_type : cell_types)
        {
            cells[cell_type] = vtkCellArray::New();
        }

        get_points_from_nodes(points.GetPointer(), node_map, instance_nodes,
                              instance_type);

        get_cells_from_elements(cells, node_map, instance_elements);

        grid->SetPoints(points);
        for (auto cell_type : cell_types)
        {
            grid->SetCells(cell_type, cells[cell_type]);
        }

        writer->SetFileName(path_.replace_extension(".vtu").string().c_str());
        writer->SetInputData(grid);
        writer->Write();
    }
}

void get_points_from_nodes(vtkPoints *points,
                           std::unordered_map<int, vtkIdType> &node_map,
                           const odb_SequenceNode &node_sequence,
                           odb_Enum::odb_DimensionEnum instance_type)
{
    int num_nodes = node_sequence.size();
    for (int i = 0; i < num_nodes; ++i)
    {
        const odb_Node &node = node_sequence[i];
        int node_label = node.label();
        const float *const node_coordinates = node.coordinates();

        if (instance_type == odb_Enum::THREE_D)
        {
            node_map[node_label] = points->InsertNextPoint(node_coordinates);
        }
        else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                 (instance_type == odb_Enum::AXISYMMETRIC))
        {
            node_map[node_label] = points->InsertNextPoint(
                node_coordinates[0], node_coordinates[1], 0.0);
        }
    }
}

std::string get_element_type_base(const std::string &element_type)
{
    std::string element_name_base;
    for (auto c : element_type)
    {
        if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
            c == '5' || c == '6' || c == '7' || c == '8' || c == '9')
        {
            element_name_base += c;
            break;
        }
        element_name_base += c;
    }
    return element_name_base;
}

std::vector<VTKCellType> get_cell_types(
    const odb_SequenceElement &element_sequence)
{
    std::vector<VTKCellType> cell_types;
    int num_elements = element_sequence.size();
    for (int i = 0; i < num_elements; ++i)
    {
        const odb_Element &element = element_sequence[i];
        std::string element_type = get_element_type_base(element.type().CStr());

        if (auto it = abq2vtk_cell_map.find(element_type);
            it != abq2vtk_cell_map.end())
        {
            auto v_it = std::find_if(
                cell_types.begin(), cell_types.end(),
                [&](VTKCellType cell_type) { return cell_type == it->second; });
            if (v_it == cell_types.end())
            {
                cell_types.push_back(it->second);
            }
        }
        else
        {
            std::cout << "Element type " << element_type
                      << " is not supported.\n";
        }
    }
    return cell_types;
}

void get_cells_from_elements(
    std::unordered_map<VTKCellType, vtkCellArray *> &cells,
    std::unordered_map<int, vtkIdType> &node_map,
    const odb_SequenceElement &element_sequence)
{
    int num_elements = element_sequence.size();
    int num_nodes = 0;
    for (int i = 0; i < num_elements; ++i)
    {
        const odb_Element &element = element_sequence[i];
        int element_label = element.label();
        std::string element_type = get_element_type_base(element.type().CStr());
        VTKCellType cell_type;

        if (auto it = abq2vtk_cell_map.find(element_type);
            it != abq2vtk_cell_map.end())
        {
            cell_type = it->second;
        }
        else
        {
            std::cout << "Element type " << element_type
                      << " is not supported.\n";
            return;
        }

        const int *const element_connectivity = element.connectivity(num_nodes);

        std::vector<vtkIdType> connectivity(num_nodes);
        for (int i = 0; i < num_nodes; i++)
        {
            connectivity[i] = node_map[element_connectivity[i]];
        }
        cells[cell_type]->InsertNextCell(connectivity.size(),
                                         connectivity.data());
    }
}

} // namespace otk