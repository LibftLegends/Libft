#include "frame_stats.hpp"

namespace vre
{

FrameStats::FrameStats() : _total_items(0), _frustum_visible(0), _drawn(0)
{
}

FrameStats::FrameStats(const FrameStats &other) : _total_items(other._total_items),
	_frustum_visible(other._frustum_visible), _drawn(other._drawn)
{
}

FrameStats &FrameStats::operator=(const FrameStats &other)
{
	if (this != &other)
	{
		_total_items = other._total_items;
		_frustum_visible = other._frustum_visible;
		_drawn = other._drawn;
	}
	return (*this);
}

FrameStats::~FrameStats()
{
}

uint32_t FrameStats::total_items() const
{
	return (_total_items);
}

void FrameStats::set_total_items(uint32_t value)
{
	_total_items = value;
}

uint32_t FrameStats::frustum_visible() const
{
	return (_frustum_visible);
}

void FrameStats::set_frustum_visible(uint32_t value)
{
	_frustum_visible = value;
}

uint32_t FrameStats::drawn() const
{
	return (_drawn);
}

void FrameStats::set_drawn(uint32_t value)
{
	_drawn = value;
}

} // namespace vre
