#ifndef GEOM_HPP_
#define GEOM_HPP_

#include <iostream>

namespace cyhair {

class real3 {
public:
		real3() : x(0.0f), y(0.0f), z(0.0f) {}
		real3(float v) : x(v), y(v), z(v) {}
		real3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
		//~real3() {}

		real3 operator+(const real3 &r) const {
				return real3(x + r.x, y + r.y, z + r.z);
		}
		real3 operator*(const real3 &r) const {
				return real3(x * r.x, y * r.y, z * r.z);
		}
		real3 operator-(const real3 &r) const {
				return real3(x - r.x, y - r.y, z - r.z);
		}
		real3 operator/(const real3 &r) const {
				return real3(x / r.x, y / r.y, z / r.z);
		}

		real3 operator/(const float f) const { float inv = 1.0f / f; return real3(x * inv, y * inv, z * inv); }

		const float &operator[](int i) const {
				if (i == 0) return x;
				if (i == 1) return y;
				return z;
		}

		float &operator[](int i) {
				if (i == 0) return x;
				if (i == 1) return y;
				return z;
		}

		bool operator==(const real3 &r) const { return x == r.x && y == r.y && z == r.z; }
		bool operator!=(const real3 &r) const { return x != r.x || y != r.y || z != r.z; }

		friend std::ostream &operator<<(std::ostream &os, const real3 &r) {
				os << "[ " << r.x << ", " << r.y << ", " << r.z << " ]";
				return os;
		}

		float x, y, z;
};

inline real3 operator*(float f, const real3 &v) {
		return real3(v.x * f, v.y * f, v.z * f);
}

class Bounds3f {
public:
		// Bounds3 Public Methods
		Bounds3f() {
				float minNum = std::numeric_limits<float>::lowest();
				float maxNum = std::numeric_limits<float>::max();
				pMin = real3(maxNum, maxNum, maxNum);
				pMax = real3(minNum, minNum, minNum);
		}

		Bounds3f(const real3 &p) : pMin(p), pMax(p) {}

		Bounds3f(const real3 &p1, const real3 &p2)
				: pMin(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::min(p1.z, p2.z)),
					pMax(std::max(p1.x, p2.x), std::max(p1.y, p2.y), std::max(p1.z, p2.z)) {}

		const real3 &operator[](int i) const;
		real3 &operator[](int i);

		bool operator==(const Bounds3f &b) const {
				return b.pMin == pMin && b.pMax == pMax;
		}

		bool operator!=(const Bounds3f &b) const {
				return b.pMin != pMin || b.pMax != pMax;
		}

		int MaximumExtent() const {
				real3 d = pMax - pMin;
				if (d.x > d.y && d.x > d.z)
						return 0;
				else if (d.y > d.z)
						return 1;
				else
						return 2;
		}

		friend std::ostream &operator<<(std::ostream &os, const Bounds3f &b) {
				os << "[ " << b.pMin << " - " << b.pMax << " ]";
				return os;
		}

		// Bounds3 Public Data
		real3 pMin, pMax;
};

real3 Min(const real3 &p1, const real3 &p2) {
		return real3(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::min(p1.z, p2.z));
}

real3 Max(const real3 &p1, const real3 &p2) {
		return real3(std::max(p1.x, p2.x), std::max(p1.y, p2.y), std::max(p1.z, p2.z));
}

inline const real3 &Bounds3f::operator[](int i) const {
		return (i == 0) ? pMin : pMax;
}

inline real3 &Bounds3f::operator[](int i) {
		return (i == 0) ? pMin : pMax;
}

Bounds3f Union(const Bounds3f &b, const real3 &p) {
		Bounds3f ret;
		ret.pMin = Min(b.pMin, p);
		ret.pMax = Max(b.pMax, p);
		return ret;
}

Bounds3f Union(const Bounds3f &b1, const Bounds3f &b2) {
		Bounds3f ret;
		ret.pMin = Min(b1.pMin, b2.pMin);
		ret.pMax = Max(b1.pMax, b2.pMax);
		return ret;
}


} // namespace cyhair
#endif //GEOM_HPP_