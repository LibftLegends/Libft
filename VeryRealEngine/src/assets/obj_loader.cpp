#include "obj_loader.hpp"

namespace vre
{

ObjLoader::ObjLoader()
{
}

ObjLoader::ObjLoader(const ObjLoader &)
{
}

ObjLoader &ObjLoader::operator=(const ObjLoader &)
{
	return (*this);
}

ObjLoader::~ObjLoader()
{
}

std::string ObjLoader::directory_of(const std::string &path)
{
	size_t	slash;

	slash = path.find_last_of("/\\");
	if (slash == std::string::npos)
		return ("");
	return (path.substr(0, slash + 1));
}

bool ObjLoader::load_mtl(const std::string &path,
	const std::string &base_directory, std::vector<MaterialData> *out_materials)
{
	MaterialData	*current;
			float r;
			float g;
			float b;
			float roughness;
			float metallic;

	std::ifstream file(path);
	if (!file.is_open())
	{
		std::fprintf(stderr, "obj_loader: failed to open mtl file \"%s\"\n",
			path.c_str());
		return (false);
	}
	current = nullptr;
	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream stream(line);
		std::string keyword;
		stream >> keyword;
		if (keyword == "newmtl")
		{
			std::string name;
			stream >> name;
			out_materials->push_back(MaterialData());
			current = &out_materials->back();
			current->set_name(name);
		}
		else if (keyword == "Kd" && current != nullptr)
		{
			stream >> r >> g >> b;
			current->set_diffuse_color(0, r);
			current->set_diffuse_color(1, g);
			current->set_diffuse_color(2, b);
		}
		else if (keyword == "map_Kd" && current != nullptr)
		{
			std::string texture_name;
			stream >> texture_name;
			current->set_diffuse_texture_path(base_directory + texture_name);
		}
		else if (keyword == "Pr" && current != nullptr)
		{
			// Roughness/metallic PBR extension to .mtl (Pr/Pm), the same
			// convention Blender's OBJ exporter and several other tools use.
			stream >> roughness;
			current->set_roughness(roughness);
		}
		else if (keyword == "Pm" && current != nullptr)
		{
			stream >> metallic;
			current->set_metallic(metallic);
		}
	}
	return (true);
}

int64_t ObjLoader::resolve_index(const std::string &part, size_t count)
{
	int64_t	value;

	if (part.empty())
		return (-1);
	value = std::stoll(part);
	if (value < 0)
		return (static_cast<int64_t>(count) + value);
	return (value - 1);
}

void ObjLoader::parse_face_index_token(const std::string &token,
	size_t position_count, size_t uv_count, size_t normal_count,
	int64_t *out_position, int64_t *out_uv, int64_t *out_normal)
{
	int	part_index;

	*out_position = -1;
	*out_uv = -1;
	*out_normal = -1;
	std::string parts[3];
	part_index = 0;
	for (char c : token)
	{
		if (c == '/')
		{
			part_index++;
			if (part_index > 2)
				break ;
		}
		else
		{
			parts[part_index] += c;
		}
	}
	if (!parts[0].empty())
		*out_position = resolve_index(parts[0], position_count);
	if (!parts[1].empty())
		*out_uv = resolve_index(parts[1], uv_count);
	if (!parts[2].empty())
		*out_normal = resolve_index(parts[2], normal_count);
}

} // namespace vre
