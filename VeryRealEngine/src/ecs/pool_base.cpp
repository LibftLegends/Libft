#include "pool_base.hpp"

namespace vre::ecs
{

PoolBase::PoolBase()
{
}

PoolBase::PoolBase(const PoolBase &)
{
}

PoolBase &PoolBase::operator=(const PoolBase &)
{
	return (*this);
}

PoolBase::~PoolBase()
{
}

} // namespace vre::ecs
