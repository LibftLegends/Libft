/**
 * @file renderitem.hpp
 * @brief One item to draw this frame: a loaded mesh placed in the world by
 * a model matrix.
 *
 * The renderer combines this with the camera's view/projection (passed
 * separately to Renderer::draw_frame()) to build each draw call's MVP.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "meshhandle.hpp"

namespace vre
{
class RenderItem
{
  public:
	RenderItem();
	RenderItem(const RenderItem &other);
	RenderItem &operator=(const RenderItem &other);
	~RenderItem();

	RenderItem(MeshHandle mesh, const mat4 &model);

	/** Which uploaded mesh to draw (see Renderer::load_mesh_from_obj()). */
	MeshHandle mesh() const;
	void set_mesh(MeshHandle value);

	/** World-space model matrix placing the mesh. */
	const mat4 &model() const;
	void set_model(const mat4 &value);

	/**
		* Stable identity used only for occlusion culling, so a query result
		* from a couple of frames ago can be matched back to "the same object"
		* even though RenderItem itself is rebuilt from scratch every frame
		* (Scene::collect_render_items uses each node's index; particles and
		* anything else without a meaningful stable identity leave this at the
		* default, which opts them out of occlusion culling — always drawn if
		* frustum-visible, same as before this feature existed).
		*/
	uint32_t occlusion_id() const;
	void set_occlusion_id(uint32_t value);

	/// @return The sentinel occlusion_id() meaning "not occlusion-tested".
	static uint32_t no_occlusion_id();

  private:
	MeshHandle _mesh;
	mat4 _model;
	uint32_t _occlusion_id;
};

} // namespace vre
