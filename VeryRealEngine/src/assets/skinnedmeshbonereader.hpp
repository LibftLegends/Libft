/**
 * @file skinnedmeshbonereader.hpp
 * @brief Reads a skinned-mesh JSON file's "bones" array into a Skeleton.
 */
#pragma once

#include "../animation/skeleton.hpp"
#include "../vre.hpp"
#include "jsonvalue.hpp"

namespace vre
{
class SkinnedMeshBoneReader
{
  public:
	SkinnedMeshBoneReader();
	SkinnedMeshBoneReader(const SkinnedMeshBoneReader &other);
	SkinnedMeshBoneReader &operator=(const SkinnedMeshBoneReader &other);
	~SkinnedMeshBoneReader();

	/**
		* @brief Reads each bone in `bones_field` (each with optional
		* "name"/"parent"/"position"/"rotation" fields) into `out_skeleton`,
		* then computes its bind pose.
		* @return true on success.
		*/
	static bool read(const JsonValue &bones_field, Skeleton *out_skeleton);
};

} // namespace vre
