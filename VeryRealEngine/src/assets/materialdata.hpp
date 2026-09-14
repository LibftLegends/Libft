/**
 * @file materialdata.hpp
 * @brief Parsed .mtl material data.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class MaterialData
{
  public:
	MaterialData();
	MaterialData(const MaterialData &other);
	MaterialData &operator=(const MaterialData &other);
	~MaterialData();

	const std::string &name() const;
	void set_name(const std::string &value);

	float diffuse_color(size_t index) const;
	void set_diffuse_color(size_t index, float value);

	/// Empty => untextured.
	const std::string &diffuse_texture_path() const;
	void set_diffuse_texture_path(const std::string &value);

	/**
		* PBR roughness/metallic workflow (the "Pr"/"Pm" extension to the
		* classic .mtl format, used by Blender's OBJ exporter and others) —
		* this is what actually distinguishes "wood-like" from "metallic"
		* materials in the shading model, not just a different diffuse
		* texture. Defaults describe an ordinary rough dielectric (e.g.
		* unfinished wood).
		*/
	float roughness() const; ///< 0 = mirror-smooth, 1 = fully rough.
	void set_roughness(float value);

	float metallic() const; ///< 0 = dielectric (wood, plastic, ...), 1 = metal.
	void set_metallic(float value);

  private:
	std::string _name;
	float _diffuse_color[3];
	std::string _diffuse_texture_path;
	float _roughness;
	float _metallic;
};

} // namespace vre
