#include "asset.hpp"

namespace vre
{
SkinnedAsset::SkinnedAsset()
{
}

SkinnedAsset::SkinnedAsset(const SkinnedAsset &other) : _mesh(other._mesh),
	_skeleton(other._skeleton), _clip(other._clip)
{
}

SkinnedAsset &SkinnedAsset::operator=(const SkinnedAsset &other)
{
	if (this != &other)
	{
		_mesh = other._mesh;
		_skeleton = other._skeleton;
		_clip = other._clip;
	}
	return (*this);
}

SkinnedAsset::~SkinnedAsset()
{
}

MeshData &SkinnedAsset::mesh()
{
	return (_mesh);
}

const MeshData &SkinnedAsset::mesh() const
{
	return (_mesh);
}

Skeleton &SkinnedAsset::skeleton()
{
	return (_skeleton);
}

const Skeleton &SkinnedAsset::skeleton() const
{
	return (_skeleton);
}

AnimationClip &SkinnedAsset::clip()
{
	return (_clip);
}

const AnimationClip &SkinnedAsset::clip() const
{
	return (_clip);
}

} // namespace vre
