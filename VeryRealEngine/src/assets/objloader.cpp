#include "objloader.hpp"

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

bool ObjLoader::load(const char *path, MeshData *out_mesh,
	std::vector<MaterialData> *out_materials)
{
	std::string	line;

	std::ifstream file(path);
	if (!file.is_open())
	{
		std::fprintf(stderr, "objloader: failed to open \"%s\"\n", path);
		return (false);
	}

	ObjGeometryBuilder builder(directory_of(path), out_mesh, out_materials);
	while (std::getline(file, line))
		builder.parse_line(line);
	builder.finish();
	return (true);
}

} // namespace vre
