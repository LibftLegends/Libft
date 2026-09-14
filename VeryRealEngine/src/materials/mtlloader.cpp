#include "mtlloader.hpp"

namespace vre
{
MtlLoader::MtlLoader()
{
}

MtlLoader::MtlLoader(const MtlLoader &)
{
}

MtlLoader &MtlLoader::operator=(const MtlLoader &)
{
	return (*this);
}

MtlLoader::~MtlLoader()
{
}

bool MtlLoader::load(const std::string &path,
	const std::string &base_directory,
	std::vector<MaterialData> *out_materials)
{
	MaterialData	*current;
	float			r;
	float			g;
	float			b;
	float			roughness;
	float			metallic;
	std::string		line;

	std::ifstream file(path);
	if (!file.is_open())
	{
		std::fprintf(stderr, "mtlloader: failed to open \"%s\"\n",
			path.c_str());
		return (false);
	}
	current = nullptr;
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

} // namespace vre
