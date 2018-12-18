#ifndef HAIRUTIL_HPP_
#define HAIRUTIL_HPP_

#include <vector>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <memory>

#include "geom.h"

namespace cyhair {
	struct Hair {
			size_t size() {
					return cps.size();
			}

			void resize(size_t size) {
					cps.resize(size);
					radii.resize(size);
					cpInfo.resize(size);
			}
			
			Bounds3f bounds;
			std::vector<real3> cps;
			std::vector<float> radii;
			std::vector<std::array<unsigned, 4>> cpInfo;
	};

	inline float ManhattanDistance(real3 p0, real3 p1)
	{
		float dx = abs(p1.x - p0.x);
		float dy = abs(p1.y - p0.y);
		float dz = abs(p1.z - p0.z);

		return dx + dy + dz;
	}

	inline float withinManhattanDistance(const real3& p0, const real3& p1, const float distance) {
		float dx = p1.x - p0.x;
		float dy = p1.y - p0.y;
		float dz = p1.z - p0.z;

		if (dx > distance) return 0;
		if (dy > distance) return 0;
		if (dz > distance) return 0;

		return 1;
	}

	inline float distance(const real3& p0, const real3& p1) {
		float dx = p1.x - p0.x;
		float dy = p1.y - p0.y;
		float dz = p1.z - p0.z;

		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	//class DirectionSorter
	//{
	//	real3 dir;
	//public:
	//	DirectionSorter(real3 d) : dir(d) {}
	//	bool operator()(Hair const &h1, Hair const &h2) const {
	//		return sortHairByDirection(h1.p0, h2.p0, dir);
	//	}
	//};

}

#endif /*HAIRUTIL_HPP_*/