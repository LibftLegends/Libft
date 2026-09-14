#include "objgeometrybuilder.hpp"

namespace vre
{
ObjGeometryBuilder::ObjGeometryBuilder() : _out_mesh(nullptr),
	_out_materials(nullptr), _current_material_index(-1),
	_current_submesh_start(0), _has_open_submesh(false)
{
}

ObjGeometryBuilder::~ObjGeometryBuilder()
{
}

ObjGeometryBuilder::ObjGeometryBuilder(const std::string &base_directory,
	MeshData *out_mesh, std::vector<MaterialData> *out_materials) :
	_base_directory(base_directory), _out_mesh(out_mesh),
	_out_materials(out_materials), _current_material_index(-1),
	_current_submesh_start(0), _has_open_submesh(false)
{
}

void ObjGeometryBuilder::parse_line(const std::string &line)
{
	std::istringstream	stream(line);
	std::string			keyword;

	stream >> keyword;
	if (keyword == "v")
		parse_position(&stream);
	else if (keyword == "vt")
		parse_uv(&stream);
	else if (keyword == "vn")
		parse_normal(&stream);
	else if (keyword == "mtllib")
		parse_mtllib(&stream);
	else if (keyword == "usemtl")
		parse_usemtl(&stream);
	else if (keyword == "f")
		parse_face(&stream);
}

void ObjGeometryBuilder::parse_position(std::istringstream *stream)
{
	Vec3f	p;

	*stream >> p.v[0] >> p.v[1] >> p.v[2];
	_positions.push_back(p);
}

void ObjGeometryBuilder::parse_uv(std::istringstream *stream)
{
	Vec2f	t;

	*stream >> t.v[0] >> t.v[1];
	_uvs.push_back(t);
}

void ObjGeometryBuilder::parse_normal(std::istringstream *stream)
{
	Vec3f	n;

	*stream >> n.v[0] >> n.v[1] >> n.v[2];
	_normals.push_back(n);
}

void ObjGeometryBuilder::parse_mtllib(std::istringstream *stream)
{
	std::string	mtl_file;

	*stream >> mtl_file;
	MtlLoader::load(_base_directory + mtl_file, _base_directory,
		_out_materials);
	for (size_t i = 0; i < _out_materials->size(); i++)
		_material_name_to_index[(*_out_materials)[i].name()] =
			static_cast<int32_t>(i);
}

void ObjGeometryBuilder::close_current_submesh()
{
	SubMesh	submesh;

	if (!_has_open_submesh
		|| _out_mesh->indices().size() <= _current_submesh_start)
		return ;
	submesh.set_index_offset(_current_submesh_start);
	submesh.set_index_count(
			static_cast<uint32_t>(_out_mesh->indices().size())
			- _current_submesh_start);
	submesh.set_material_index(_current_material_index);
	_out_mesh->submeshes().push_back(submesh);
}

void ObjGeometryBuilder::parse_usemtl(std::istringstream *stream)
{
	std::string							material_name;
	std::map<std::string, int32_t>::const_iterator it;

	*stream >> material_name;
	close_current_submesh();
	it = _material_name_to_index.find(material_name);
	_current_material_index = (it != _material_name_to_index.end())
		? it->second : -1;
	_current_submesh_start =
		static_cast<uint32_t>(_out_mesh->indices().size());
	_has_open_submesh = true;
}

void ObjGeometryBuilder::parse_face(std::istringstream *stream)
{
	std::vector<uint32_t>	face_indices;
	std::string				token;
	int64_t					position_index;
	int64_t					uv_index;
	int64_t					normal_index;
	std::tuple<int64_t, int64_t, int64_t>	key;
	MeshVertex				vertex;
	uint32_t				new_index;
	std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t>::const_iterator
		cached;

	if (!_has_open_submesh)
	{
		_current_submesh_start =
			static_cast<uint32_t>(_out_mesh->indices().size());
		_has_open_submesh = true;
	}
	while (*stream >> token)
	{
		ObjFaceIndexParser::parse(token, _positions.size(), _uvs.size(),
			_normals.size(), &position_index, &uv_index, &normal_index);
		key = std::make_tuple(position_index, uv_index, normal_index);
		cached = _vertex_cache.find(key);
		if (cached != _vertex_cache.end())
		{
			face_indices.push_back(cached->second);
			continue ;
		}
		if (position_index >= 0
			&& position_index < static_cast<int64_t>(_positions.size()))
			vertex.set_position(_positions[position_index].v);
		if (normal_index >= 0
			&& normal_index < static_cast<int64_t>(_normals.size()))
			vertex.set_normal(_normals[normal_index].v);
		if (uv_index >= 0 && uv_index < static_cast<int64_t>(_uvs.size()))
			vertex.set_uv(_uvs[uv_index].v);
		new_index = static_cast<uint32_t>(_out_mesh->vertices().size());
		_out_mesh->vertices().push_back(vertex);
		_vertex_cache[key] = new_index;
		face_indices.push_back(new_index);
	}
	// Fan-triangulate n-gons (n >= 3): (0,1,2), (0,2,3), (0,3,4), ...
	for (size_t i = 1; i + 1 < face_indices.size(); i++)
	{
		_out_mesh->indices().push_back(face_indices[0]);
		_out_mesh->indices().push_back(face_indices[i]);
		_out_mesh->indices().push_back(face_indices[i + 1]);
	}
}

void ObjGeometryBuilder::finish()
{
	SubMesh	submesh;

	close_current_submesh();
	if (_out_mesh->submeshes().empty() && !_out_mesh->indices().empty())
	{
		submesh.set_index_offset(0);
		submesh.set_index_count(
				static_cast<uint32_t>(_out_mesh->indices().size()));
		submesh.set_material_index(-1);
		_out_mesh->submeshes().push_back(submesh);
	}
}

} // namespace vre
