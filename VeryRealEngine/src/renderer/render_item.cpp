#include "render_item.hpp"

namespace vre
{

RenderItem::RenderItem() : _occlusion_id(no_occlusion_id())
{
}

RenderItem::RenderItem(const RenderItem &other) : _mesh(other._mesh),
	_model(other._model), _occlusion_id(other._occlusion_id)
{
}

RenderItem &RenderItem::operator=(const RenderItem &other)
{
	if (this != &other)
	{
		_mesh = other._mesh;
		_model = other._model;
		_occlusion_id = other._occlusion_id;
	}
	return (*this);
}

RenderItem::~RenderItem()
{
}

RenderItem::RenderItem(MeshHandle mesh, const mat4 &model) : _mesh(mesh),
	_model(model), _occlusion_id(no_occlusion_id())
{
}

MeshHandle RenderItem::mesh() const
{
	return (_mesh);
}

void RenderItem::set_mesh(MeshHandle value)
{
	_mesh = value;
}

const mat4 &RenderItem::model() const
{
	return (_model);
}

void RenderItem::set_model(const mat4 &value)
{
	_model = value;
}

uint32_t RenderItem::occlusion_id() const
{
	return (_occlusion_id);
}

void RenderItem::set_occlusion_id(uint32_t value)
{
	_occlusion_id = value;
}

uint32_t RenderItem::no_occlusion_id()
{
	return (UINT32_MAX);
}

} // namespace vre
