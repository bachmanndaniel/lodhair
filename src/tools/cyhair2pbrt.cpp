//
// cyhair2pbrt.cpp
//
// Convert CyHair files to PBRT.
// Hair vertices are interpreted as Catmull-Rom spline points.
// The tool simply converts Catmull-Rom spline points to cubic Bezier points.
//
// MIT license
//

///////////// start of cyhair_loader

// clang-format off

/*
The MIT License (MIT)

Copyright (c) 2016 Light Transport Entertainment, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// Simple Cyhair loader.
#include "hairutil.h"
#include <boost/random.hpp>

//#include "cyhair_loader.h"

namespace cyhair {

static const float toC2B[4][4] = {
    {0.0f, 6.0f / 6.0f, 0.0f, 0.0f},
    {-1.0f / 6.0f, 6.0f / 6.0f, 1.0f / 6.0f, 0.0f},
    {0.0f, 1.0f / 6.0f, 6.0f / 6.0f, -1.0f / 6.0f},
    {0.0f, 0.0, 6.0f / 6.0f, 0.0f}};

static const float toC2B0[4][4] = {
    {0.0f, 6.0f / 6.0f, 0.0f, 0.0f},
    {0.0f, 3.0f / 6.0f, 4.0f / 6.0f, -1.0f / 6.0f},
    {0.0f, 1.0f / 6.0f, 6.0f / 6.0f, -1.0f / 6.0f},
    {0.0f, 0.0f, 6.0f / 6.0f, 0.0f}};

static const float toC2B1[4][4] = {
    {0.0f, 6.0f / 6.0f, 0.0f, 0.0f},
    {-1.0f / 6.0f, 6.0f / 6.0f, 1.0f / 6.0f, 0.0f},
    {-1.0f / 6.0f, 4.0f / 6.0f, 3.0f / 6.0f, 0.0f},
    {0.0f, 0.0f, 6.0f / 6.0f, 0.0f}};

static void mul_matrix(real3 out[4], const float mat[4][4], const real3 pt[4]) {
  for (int i = 0; i < 4; i++) {
    out[i] = mat[i][0] * pt[0] + mat[i][1] * pt[1] + mat[i][2] * pt[2] +
             mat[i][3] * pt[3];
  }
}

static void CamullRomToCubicBezier(real3 Q[4], const real3 *cps, int cps_size, int seg_idx) 
{
  size_t sz = static_cast<size_t>(cps_size);
  if (sz == 2) {
    Q[0] = cps[seg_idx];
    Q[1] = cps[seg_idx] * 2.0f / 3.0f + cps[seg_idx + 1] * 1.0f / 3.0f;
    Q[2] = cps[seg_idx] * 1.0f / 3.0f + cps[seg_idx + 1] * 2.0f / 3.0f;
    Q[3] = cps[seg_idx + 1];
  } else {
    real3 P[4];
    if (seg_idx == 0) {
      P[0] = real3(0.0f);
      P[1] = cps[seg_idx + 0];
      P[2] = cps[seg_idx + 1];
      P[3] = cps[seg_idx + 2];
      mul_matrix(Q, toC2B0, P);
    } else if (seg_idx == static_cast<int>(sz - 2)) {
      P[0] = cps[seg_idx - 1];
      P[1] = cps[seg_idx + 0];
      P[2] = cps[seg_idx + 1];
      P[3] = real3(0.0f);
      mul_matrix(Q, toC2B1, P);
    } else {
      P[0] = cps[seg_idx - 1];
      P[1] = cps[seg_idx + 0];
      P[2] = cps[seg_idx + 1];
      P[3] = cps[seg_idx + 2];
      mul_matrix(Q, toC2B, P);
    }
  }
}

struct CyHairHeader {
  char magic[4];
  unsigned int num_strands;
  unsigned int total_points;
  unsigned int flags;
  unsigned int default_segments;
  float default_thickness;
  float default_transparency;
  float default_color[3];
  char infomation[88];
};

class CyHair {
 public:
  CyHair()
      : flags_(0),
        num_strands_(0),
        total_points_(0),
        default_segments_(-1),
        default_thickness_(0.01f),
        default_transparency_(1.0f) {
    default_color_[0] = 0.5f;
    default_color_[1] = 0.5f;
    default_color_[2] = 0.5f;
  }

  ~CyHair() {}

  /// Load CyHair data from a file.
  bool Load(const char *filename);

  /// Convert to cubic bezier curves.
  /// 4(cubic) * 3(xyz) * num_curves = vertices.size()
  /// 4(cubic) * num_curves = radiuss.size()
  /// `max_strands` limits the number of strands to convert. -1 = convert all
  /// strands.
  /// `thickness` overwrites strand thickness if it have positive value.
  /// Apply `vertex_translate` after `vertex_scale`.
  /// TODO(syoyo) return strand/segment information
  bool ToCubicBezierCurves(std::vector<float> *vertices,
                           std::vector<float> *radiuss,
                           const float vertex_scale[3],
                           const float vertex_translate[3],
                           const int max_strands = -1,
                           const float thickness = -1.0f,
													 const int lod_level = -1);

  CyHairHeader header_;

  // Raw CyHair values
  std::vector<unsigned short> segments_;
  std::vector<float> points_;  // xyz
  std::vector<float> thicknesses_;
  std::vector<float> transparencies_;
  std::vector<float> colors_;  // rgb
  unsigned int flags_;
  unsigned int num_strands_;
  unsigned int total_points_;
  int default_segments_;
  float default_thickness_;
  float default_transparency_;
  float default_color_[3];
  int pad0;

  // Processed CyHair values
  std::vector<unsigned int> strand_offsets_;
};



bool CyHair::Load(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    return false;
  }

  assert(sizeof(CyHairHeader) == 128);
  CyHairHeader header;

  if (1 != fread(&header, 128, 1, fp)) {
    fclose(fp);
    return false;
  }
  if (memcmp(header.magic, "HAIR", 4) != 0) {
    fclose(fp);
    return false;
  }

  flags_ = header.flags;
  default_thickness_ = header.default_thickness;
  default_transparency_ = header.default_transparency;
  default_segments_ = static_cast<int>(header.default_segments);
  default_color_[0] = header.default_color[0];
  default_color_[1] = header.default_color[1];
  default_color_[2] = header.default_color[2];

  const bool has_segments = flags_ & 0x1;
  const bool has_points = flags_ & 0x2;
  const bool has_thickness = flags_ & 0x4;
  const bool has_transparency = flags_ & 0x8;
  const bool has_color = flags_ & 0x10;

  num_strands_ = header.num_strands;
  total_points_ = header.total_points;

  if (!has_points) {
    std::cout << "No point data in CyHair." << std::endl;
    return false;
  }

  if ((default_segments_ < 1) && (!has_segments)) {
    std::cout << "No valid segment information in CyHair." << std::endl;
    return false;
  }

  // First read all strand data from a file.
  if (has_segments) {
    segments_.resize(num_strands_);
    if (1 != fread(&segments_[0], sizeof(unsigned short) * num_strands_, 1, fp)) {
      std::cout << "Failed to read CyHair segments data." << std::endl;
      fclose(fp);
      return false;
    }
  }

  if (has_points) {
    std::cout << "[CyHair] Has points." << std::endl;
    points_.resize(3 * total_points_);
    size_t n = fread(&points_[0], total_points_ * sizeof(float) * 3, 1, fp);
    if (1 != n) {
      std::cout << "Failed to read CyHair points data." << std::endl;
      fclose(fp);
      return false;
    }
  }

  if (has_thickness) {
    std::cout << "[CyHair] Has thickness." << std::endl;
    thicknesses_.resize(total_points_);
    if (1 != fread(&thicknesses_[0], total_points_ * sizeof(float), 1, fp)) {
      std::cout << "Failed to read CyHair thickness data." << std::endl;
      fclose(fp);
      return false;
    }
  }

  if (has_transparency) {
    std::cout << "[CyHair] Has transparency." << std::endl;
    transparencies_.resize(total_points_);
    if (1 != fread(&transparencies_[0], total_points_ * sizeof(float), 1, fp)) {
      std::cout << "Failed to read CyHair transparencies data." << std::endl;
      fclose(fp);
      return false;
    }
  }

  if (has_color) {
    std::cout << "[CyHair] Has color." << std::endl;
    colors_.resize(3 * total_points_);
    if (1 != fread(&colors_[0], total_points_ * sizeof(float) * 3, 1, fp)) {
      std::cout << "Failed to read CyHair colors data." << std::endl;
      fclose(fp);
      return false;
    }
  }

  // Build strand offset table.
  strand_offsets_.resize(num_strands_);
  strand_offsets_[0] = 0;
  for (size_t i = 1; i < num_strands_; i++) {
    int num_segments = segments_.empty() ? default_segments_ : segments_[i - 1];
    strand_offsets_[i] =
        strand_offsets_[i - 1] + static_cast<unsigned int>(num_segments + 1);
  }

  return true;
}

bool CyHair::ToCubicBezierCurves(std::vector<float> *vertices,
                                 std::vector<float> *radiuss,
                                 const float vertex_scale[3],
                                 const float vertex_translate[3],
                                 const int max_strands, const float user_thickness, const int lod_level) {
  if (points_.empty() || strand_offsets_.empty()) {
    return false;
  }

  vertices->clear();
  radiuss->clear();

  int num_strands = static_cast<int>(num_strands_);

  if ((max_strands > 0) && (max_strands < num_strands)) {
    num_strands = max_strands;
  }

	std::vector<Hair> hairs(num_strands);

  std::cout << "[Hair] Convert first " << num_strands << " strands from "
            << max_strands << " strands in the original hair data."
            << std::endl;

  // Assume input points are CatmullRom spline.
  for (size_t i = 0; i < static_cast<size_t>(num_strands); i++) {
    if ((i % 1000) == 0) {
      std::cout << i << " / " << num_strands_ << std::endl;
    }

    int num_segments = segments_.empty() ? default_segments_ : segments_[i];
    if (num_segments < 2) {
      continue;
    }

    std::vector<real3> segment_points;
    for (size_t k = 0; k < static_cast<size_t>(num_segments); k++) {
      // Zup -> Yup
      real3 p(points_[3 * (strand_offsets_[i] + k) + 0],
              points_[3 * (strand_offsets_[i] + k) + 2],
              points_[3 * (strand_offsets_[i] + k) + 1]);
      segment_points.push_back(p);
    }

    // Skip both endpoints
    for (int s = 1; s < num_segments - 1; s++) {
      int seg_idx = s - 1;
      real3 q[4];
      CamullRomToCubicBezier(q, segment_points.data(), num_segments, seg_idx);

	    q[0].x = vertex_scale[0] * q[0].x + vertex_translate[0];
	    q[0].y = vertex_scale[1] * q[0].y + vertex_translate[1];
	    q[0].z = vertex_scale[2] * q[0].z + vertex_translate[2];
	    q[1].x = vertex_scale[0] * q[1].x + vertex_translate[0];
	    q[1].y = vertex_scale[1] * q[1].y + vertex_translate[1];
	    q[1].z = vertex_scale[2] * q[1].z + vertex_translate[2];
	    q[2].x = vertex_scale[0] * q[2].x + vertex_translate[0];
	    q[2].y = vertex_scale[1] * q[2].y + vertex_translate[1];
	    q[2].z = vertex_scale[2] * q[2].z + vertex_translate[2];
	    q[3].x = vertex_scale[0] * q[3].x + vertex_translate[0];
	    q[3].y = vertex_scale[1] * q[3].y + vertex_translate[1];
	    q[3].z = vertex_scale[2] * q[3].z + vertex_translate[2];

	    if(lod_level <= 0) {
					vertices->push_back(q[0].x);
					vertices->push_back(q[0].y);
					vertices->push_back(q[0].z);
					vertices->push_back(q[1].x);
					vertices->push_back(q[1].y);
					vertices->push_back(q[1].z);
					vertices->push_back(q[2].x);
					vertices->push_back(q[2].y);
					vertices->push_back(q[2].z);
					vertices->push_back(q[3].x);
					vertices->push_back(q[3].y);
					vertices->push_back(q[3].z);

					//vertices->push_back(vertex_scale[0] * q[0].x + vertex_translate[0]);
					//vertices->push_back(vertex_scale[1] * q[0].y + vertex_translate[1]);
					//vertices->push_back(vertex_scale[2] * q[0].z + vertex_translate[2]);
					//vertices->push_back(vertex_scale[0] * q[1].x + vertex_translate[0]);
					//vertices->push_back(vertex_scale[1] * q[1].y + vertex_translate[1]);
					//vertices->push_back(vertex_scale[2] * q[1].z + vertex_translate[2]);
					//vertices->push_back(vertex_scale[0] * q[2].x + vertex_translate[0]);
					//vertices->push_back(vertex_scale[1] * q[2].y + vertex_translate[1]);
					//vertices->push_back(vertex_scale[2] * q[2].z + vertex_translate[2]);
					//vertices->push_back(vertex_scale[0] * q[3].x + vertex_translate[0]);
					//vertices->push_back(vertex_scale[1] * q[3].y + vertex_translate[1]);
					//vertices->push_back(vertex_scale[2] * q[3].z + vertex_translate[2]);

					if (user_thickness > 0) {
						// Use user supplied thickness.
						radiuss->push_back(user_thickness);
						radiuss->push_back(user_thickness);
						radiuss->push_back(user_thickness);
						radiuss->push_back(user_thickness);
					} else {
						// TODO(syoyo) Support per point/segment thickness
						radiuss->push_back(default_thickness_);
						radiuss->push_back(default_thickness_);
						radiuss->push_back(default_thickness_);
						radiuss->push_back(default_thickness_);
					}
			}
	    else {
				hairs[i].cps.push_back(q[0]);
				hairs[i].cps.push_back(q[1]);
				hairs[i].cps.push_back(q[2]);
				hairs[i].cps.push_back(q[3]);

				//Union(hairs[i].bounds, q[0]);
				//Union(hairs[i].bounds, q[1]);
				//Union(hairs[i].bounds, q[2]);
				//Union(hairs[i].bounds, q[3]);
	    }
    }
  }


	//LOD hair combination
  if (lod_level > 0) {
	  std::cout << "begin LOD with " << hairs.size() << " hairs.\n";

		const float MAX_HAIR_RADIUS = 2.0f; //TODO: user defined input for radius in lod levels
		const float MAX_DISTANCE = MAX_HAIR_RADIUS * 2.0f;

		size_t count = 0;
		Hair cmp_hair;
		std::vector<Hair> combined;

		auto sortBySegments = [](const Hair& h1, const Hair& h2) {
				return h1.cps.size() < h2.cps.size();
		};

		//Sort by distance of hair root CPs
		auto sortByRootPointDistance = [&cmp_hair](const Hair& h1, const Hair& h2) {
				return distance(cmp_hair.cps[0], h1.cps[0]) < distance(cmp_hair.cps[0], h2.cps[0]);
		};

		//Sort by distance of hair start and end CPs
		auto sortByStartAndEndPointDistance = [&cmp_hair] (const Hair& h1, const Hair& h2)
		{
		float rd1 = distance(h1.cps[0], cmp_hair.cps[0]);
		float ed1 = distance(h1.cps.back(), cmp_hair.cps.back());

		float rd2 = distance(h2.cps[0], cmp_hair.cps[0]);
		float ed2 = distance(h2.cps.back(), cmp_hair.cps.back());

		return rd1 + ed1 < rd2 + ed2;
		};

		//Sort by distance of sample CPs (matching indices) of hair
		auto sortBySamplePointDistance = [&cmp_hair](const Hair& h1, const Hair& h2) -> bool {					
				int nSamples = 2;
				int m = std::min(h1.cps.size(), std::min(h2.cps.size(), cmp_hair.cps.size()));
				nSamples = std::min(m / 4, nSamples);

				boost::random::mt19937 rng;
				boost::random::uniform_int_distribution<> range(0, m - 1);

				float d1 = 0;
				float d2 = 0;
				for (size_t i = 0; i < nSamples; i++)
				{
						size_t index = range(rng);
						d1 += distance(h1.cps[index], cmp_hair.cps[index]);
						d2 += distance(h2.cps[index], cmp_hair.cps[index]);
				}

				return d1 < d2;
		};

		//start combination loop
		while (count != hairs.size() - 1) 
		{
		
			{
					Bounds3f rootBounds;
					int axis = 0;
					size_t min_ind = count;

					size_t c = count;
					std::for_each(hairs.begin() + count, hairs.end(), [&c, &hairs, &rootBounds](const Hair& h) { rootBounds = Union(rootBounds, hairs[c].cps[0]); c++; });			
					axis = rootBounds.MaximumExtent();

					c = count;
					std::for_each(hairs.begin() + count, hairs.end(), [&c, &hairs, &axis, &min_ind](const Hair& h) { if (h.cps[0][axis] < hairs[min_ind].cps[0][axis]) min_ind = c;  c++; });

					cmp_hair = hairs[min_ind];
					//sortByRootPointDistance, sortByStartAndEndPointDistance, sortBySamplePointDistance
					std::sort(hairs.begin() + count, hairs.end(), sortBySamplePointDistance);
			}

			//count how many hairs to combine
			int nHairs = 1;
			int sameSizeHairs = 1;
			float rootDist = 0;
			while (true) {
				int i = count + nHairs;
				if (i == hairs.size() - 1) break;
				
				rootDist = distance(cmp_hair.cps[0], hairs[i].cps[0]);
				if (rootDist >= MAX_DISTANCE) break;

				if (cmp_hair.size() >= hairs[i].size()) sameSizeHairs++;
				nHairs++;
			}

			std::sort(hairs.begin() + count + 1, hairs.begin() + count + nHairs, sortBySegments);
			nHairs = sameSizeHairs;

			// combine from count to cmp_ind + nHairs
			float inv = 1.0f / nHairs;
			Hair accum;
			accum.resize(cmp_hair.size(), user_thickness);
			std::vector<int> additions(accum.size(), 0);
			for (size_t i = count; i < count + nHairs; i++)
			{
				
				for (size_t k = 0; k < hairs[i].size(); k++)
				{
						//average cp
						accum.cps[k] = accum.cps[k] + hairs[i].cps[k];
						additions[k]++;

						//search for biggest distance radius for each cp
						float d = distance(hairs[i].cps[k], hairs[count].cps[k]);
						accum.radii[k] = std::min(std::max(accum.radii[k], d), MAX_HAIR_RADIUS);
				}
			}
		
			size_t c = 0;
			std::for_each(additions.begin(), additions.end(), [&c, &accum](const int &a) {accum.cps[c] = accum.cps[c] / a; c++; });

			combined.push_back(std::move(accum));
			count += nHairs;
			
		} // combination loop

		std::cout << "end LOD, combined hair to " << combined.size() << " hairs.\n";
		std::cout << "move combined hair to vertices array \n";

		for (size_t i = 0; i < combined.size(); i++)
		{
			for (size_t j = 0; j < combined[i].size(); j++)
			{
				vertices->push_back(combined[i].cps[j].x);
				vertices->push_back(combined[i].cps[j].y);
				vertices->push_back(combined[i].cps[j].z);

				radiuss->push_back(combined[i].radii[j]);
			}
		}

		std::cout << "move complete. moved " << vertices->size() << " vertices\n";
  } // lod

  return true;
}

}  // namespace cyhair

// clang-format on

///////////////////////////////////////////////////////////////////////////
// The above is cyhair_loader.{h,cc} basically directly; pbrt specific
// code follows...

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

int main(int argc, char *argv[]) {
    if (argc <= 2 || strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-h") == 0) {
        fprintf(stderr,
                "usage: cyhair2pbrt [CyHair filename] [pbrt output filename] "
                "(lod level) (max strands) (thickness)\n");
        return EXIT_FAILURE;
    }

    FILE *f = (strcmp(argv[2], "-") == 0) ? stdout : fopen(argv[2], "w");
    if (!f) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }

	int lod_level = -1;
	if (argc > 3)
		lod_level = atoi(argv[3]);

    int max_strands = -1;         // -1 = Convert all strands
    float user_thickness = 1.0f;  // -1 = Use thickness in CyHair file.
    if (argc > 4) {
        max_strands = atoi(argv[4]);
    }

    if (argc > 5) {
        user_thickness = atof(argv[5]);
    }

    cyhair::CyHair hair;
    bool ret = hair.Load(argv[1]);
    if (!ret) {
        fprintf(stderr, "Failed to load CyHair file [ %s ]\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::vector<float> points;
    std::vector<float> radiuss;
    const float vertex_scale[3] = {1.0f, 1.0f, 1.0f};
    const float vertex_translate[3] = {0.0f, 0.0f, 0.0f};
    ret = hair.ToCubicBezierCurves(&points, &radiuss, vertex_scale,
                                 vertex_translate, max_strands, user_thickness, lod_level);
    if (!ret) {
        fprintf(stderr, "Failed to convert CyHair data\n");
        return EXIT_FAILURE;
    }

    double bounds[2][3] = {{1e30, 1e30, 1e30}, {-1e30, -1e30, -1e30}};
    for (size_t i = 0; i < points.size() / 3; ++i) {
        const double thickness = static_cast<double>(radiuss[i]);
        for (size_t c = 0; c < 3; ++c) {
            bounds[0][c] =
                std::min(bounds[0][c],
                         static_cast<double>(points[3 * i + c]) - thickness);
            bounds[1][c] =
                std::max(bounds[1][c],
                         static_cast<double>(points[3 * i + c]) + thickness);
        }
    }
    fprintf(f, "# Converted from \"%s\" by cyhair2pbrt\n", argv[1]);
    fprintf(f, "# The number of strands = %d. user_thickness = %f\n",
            static_cast<int>(radiuss.size() / 4),
            static_cast<double>(user_thickness));
    fprintf(f, "# Scene bounds: (%f, %f, %f) - (%f, %f, %f)\n\n\n",
            bounds[0][0], bounds[0][1], bounds[0][2], bounds[1][0],
            bounds[1][1], bounds[1][2]);

    const size_t num_curves = radiuss.size() / 4;
    for (size_t i = 0; i < num_curves; i++) {
        fprintf(f, "Shape \"curve\" \"string type\" [ \"cylinder\" ] \"point P\" [ ");

        for (size_t j = 0; j < 12; j++) {
            fprintf(f, "%f ", static_cast<double>(points[12 * i + j]));
        }

        fprintf(f, " ] \"float width0\" [ %f ] \"float width1\" [ %f ]\n",
                static_cast<double>(radiuss[4 * i + 0]),
                static_cast<double>(radiuss[4 * i + 3]));		
    }

    if (f != stdout) fclose(f);

    fprintf(stderr, "Converted %d strands.\n",
            static_cast<int>(radiuss.size() / 4));

    return EXIT_SUCCESS;
}
