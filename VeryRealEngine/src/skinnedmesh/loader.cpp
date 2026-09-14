#include "loader.hpp"

namespace vre
{
SkinnedMeshLoader::SkinnedMeshLoader()
{
}

SkinnedMeshLoader::SkinnedMeshLoader(const SkinnedMeshLoader &)
{
}

SkinnedMeshLoader &SkinnedMeshLoader::operator=(const SkinnedMeshLoader &)
{
	return (*this);
}

SkinnedMeshLoader::~SkinnedMeshLoader()
{
}

bool SkinnedMeshLoader::load(const char *path, SkinnedAsset *out_asset)
{
	JsonValue			root;
	const JsonValue		*bones_field;
	const JsonValue		*vertices_field;
	const JsonValue		*indices_field;
	SkinnedAsset		asset;
	SubMesh				submesh;

	if (!JsonParser::load_file(path, &root))
	{
		std::fprintf(stderr,
			"skinnedmeshloader: could not parse \"%s\" as JSON\n", path);
		return (false);
	}
	bones_field = root.find("bones");
	vertices_field = root.find("vertices");
	indices_field = root.find("indices");
	if (bones_field == nullptr || !bones_field->is_array()
		|| vertices_field == nullptr || !vertices_field->is_array()
		|| indices_field == nullptr || !indices_field->is_array())
	{
		std::fprintf(stderr,
			"skinnedmeshloader: \"%s\" is missing a \"bones\", "
			"\"vertices\", or \"indices\" array\n", path);
		return (false);
	}
	SkinnedMeshBoneReader::read(*bones_field, &asset.skeleton());
	for (const JsonValue &vertex_json : vertices_field->array_elements())
		asset.mesh().vertices().push_back(
			SkinnedMeshVertexReader::read(vertex_json));
	for (const JsonValue &index_json : indices_field->array_elements())
		asset.mesh().indices().push_back(
			static_cast<uint32_t>(index_json.as_number()));
	submesh.set_index_offset(0);
	submesh.set_index_count(static_cast<uint32_t>(
			asset.mesh().indices().size()));
	submesh.set_material_index(-1);
		// untextured default material — see mtlloader.hpp's own convention
	asset.mesh().submeshes().push_back(submesh);
	SkinnedMeshAnimationReader::read(root, &asset.clip());
	std::fprintf(stderr,
		"Renderer: loaded skinned asset \"%s\": %zu bone(s), %zu "
		"vertices, %zu indices, %zu animation track(s)\n", path,
		asset.skeleton().bones().size(), asset.mesh().vertices().size(),
		asset.mesh().indices().size(), asset.clip().tracks().size());
	*out_asset = std::move(asset);
	return (true);
}

} // namespace vre
