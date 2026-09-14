#include "material_data.hpp"

namespace vre
{

MaterialData::MaterialData() : _roughness(0.8f), _metallic(0.0f)
{
	_diffuse_color[0] = 1.0f;
	_diffuse_color[1] = 1.0f;
	_diffuse_color[2] = 1.0f;
}

MaterialData::MaterialData(const MaterialData &other) : _name(other._name),
	_diffuse_texture_path(other._diffuse_texture_path),
	_roughness(other._roughness), _metallic(other._metallic)
{
	std::memcpy(_diffuse_color, other._diffuse_color, sizeof(_diffuse_color));
}

MaterialData &MaterialData::operator=(const MaterialData &other)
{
	if (this != &other)
	{
		_name = other._name;
		std::memcpy(_diffuse_color, other._diffuse_color,
			sizeof(_diffuse_color));
		_diffuse_texture_path = other._diffuse_texture_path;
		_roughness = other._roughness;
		_metallic = other._metallic;
	}
	return (*this);
}

MaterialData::~MaterialData()
{
}

const std::string &MaterialData::name() const
{
	return (_name);
}

void MaterialData::set_name(const std::string &value)
{
	_name = value;
}

float MaterialData::diffuse_color(size_t index) const
{
	return (_diffuse_color[index]);
}

void MaterialData::set_diffuse_color(size_t index, float value)
{
	_diffuse_color[index] = value;
}

const std::string &MaterialData::diffuse_texture_path() const
{
	return (_diffuse_texture_path);
}

void MaterialData::set_diffuse_texture_path(const std::string &value)
{
	_diffuse_texture_path = value;
}

float MaterialData::roughness() const
{
	return (_roughness);
}

void MaterialData::set_roughness(float value)
{
	_roughness = value;
}

float MaterialData::metallic() const
{
	return (_metallic);
}

void MaterialData::set_metallic(float value)
{
	_metallic = value;
}

} // namespace vre
