#include "faceindexparser.hpp"

namespace vre
{
ObjFaceIndexParser::ObjFaceIndexParser()
{
}

ObjFaceIndexParser::ObjFaceIndexParser(const ObjFaceIndexParser &)
{
}

ObjFaceIndexParser &ObjFaceIndexParser::operator=(
	const ObjFaceIndexParser &)
{
	return (*this);
}

ObjFaceIndexParser::~ObjFaceIndexParser()
{
}

int64_t ObjFaceIndexParser::resolve_index(const std::string &part,
	size_t count)
{
	int64_t	value;

	if (part.empty())
		return (-1);
	value = std::stoll(part);
	if (value < 0)
		return (static_cast<int64_t>(count) + value);
	return (value - 1);
}

void ObjFaceIndexParser::parse(const std::string &token,
	size_t position_count, size_t uv_count, size_t normal_count,
	int64_t *out_position, int64_t *out_uv, int64_t *out_normal)
{
	std::string	parts[3];
	int			part_index;

	*out_position = -1;
	*out_uv = -1;
	*out_normal = -1;
	part_index = 0;
	for (char c : token)
	{
		if (c != '/')
		{
			parts[part_index] += c;
			continue ;
		}
		part_index++;
		if (part_index > 2)
			break ;
	}
	if (!parts[0].empty())
		*out_position = resolve_index(parts[0], position_count);
	if (!parts[1].empty())
		*out_uv = resolve_index(parts[1], uv_count);
	if (!parts[2].empty())
		*out_normal = resolve_index(parts[2], normal_count);
}

} // namespace vre
