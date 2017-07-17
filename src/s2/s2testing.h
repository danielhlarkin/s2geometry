// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2TESTING_H_
#define S2_S2TESTING_H_

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gflags/gflags.h>

#include "s2/_fpcontractoff.h"
#include "s2/r2.h"
#include "s2/s1angle.h"
#include "s2/s2cellid.h"
#include "s2/third_party/absl/base/integral_types.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/util/math/matrix3x3.h"

class S1Angle;
class S2Cap;
class S2CellUnion;
class S2LatLng;
class S2LatLngRect;
class S2Loop;
class S2Polygon;
class S2Polyline;
class S2Region;

// You can optionally call S2Testing::rnd.Reset(FLAGS_s2_random_seed) at the
// start of a test or benchmark to ensure that its results do not depend on
// which other tests of benchmarks have run previously.  This can help with
// debugging.
//
// This flag currently does *not* affect the initial seed value for
// S2Testing::rnd.  TODO(user): Fix this.
DECLARE_int32(s2_random_seed);

// This class defines various static functions that are useful for writing
// unit tests.
class S2Testing {
 public:
  // Returns a vector of points shaped as a regular polygon with
  // num_vertices vertices, all on a circle of the specified angular
  // radius around the center.  The radius is the actual distance from
  // the center to the circle along the sphere.
  //
  // If you want to construct a regular polygon, try this:
  //   S2Polygon polygon(S2Loop::MakeRegularLoop(center, radius, num_vertices));
  static std::vector<S2Point> MakeRegularPoints(S2Point const& center,
                                           S1Angle radius,
                                           int num_vertices);

  // Append the vertices of "loop" to "vertices".
  static void AppendLoopVertices(S2Loop const& loop,
                                 std::vector<S2Point>* vertices);

  // A simple class that generates "Koch snowflake" fractals (see Wikipedia
  // for an introduction).  There is an option to control the fractal
  // dimension (between 1.0 and 2.0); values between 1.02 and 1.50 are
  // reasonable simulations of various coastlines.  The default dimension
  // (about 1.26) corresponds to the standard Koch snowflake.  (The west coast
  // of Britain has a fractal dimension of approximately 1.25.)
  //
  // The fractal is obtained by starting with an equilateral triangle and
  // recursively subdividing each edge into four segments of equal length.
  // Therefore the shape at level "n" consists of 3*(4**n) edges.  Multi-level
  // fractals are also supported: if you set min_level() to a non-negative
  // value, then the recursive subdivision has an equal probability of
  // stopping at any of the levels between the given min and max (inclusive).
  // This yields a fractal where the perimeter of the original triangle is
  // approximately equally divided between fractals at the various possible
  // levels.  If there are k distinct levels {min,..,max}, the expected number
  // of edges at each level "i" is approximately 3*(4**i)/k.
  class Fractal {
   public:
    // You must call set_max_level() or SetLevelForApproxMaxEdges() before
    // calling MakeLoop().
    Fractal();

    // Set the maximum subdivision level for the fractal (see above).
    // REQUIRES: max_level >= 0
    void set_max_level(int max_level);
    int max_level() const { return max_level_; }

    // Set the minimum subdivision level for the fractal (see above).  The
    // default value of -1 causes the min and max levels to be the same.  A
    // min_level of 0 should be avoided since this creates a significant
    // chance that none of the three original edges will be subdivided at all.
    //
    // DEFAULT: max_level()
    void set_min_level(int min_level_arg);
    int min_level() const { return min_level_arg_; }

    // Set the min and/or max level to produce approximately the given number
    // of edges.  (The values are rounded to a nearby value of 3*(4**n).)
    void SetLevelForApproxMinEdges(int min_edges);
    void SetLevelForApproxMaxEdges(int max_edges);

    // Set the fractal dimension.  The default value of approximately 1.26
    // corresponds to the stardard Koch curve.  The value must lie in the
    // range [1.0, 2.0).
    //
    // DEFAULT: log(4) / log(3) ~= 1.26
    void set_fractal_dimension(double dimension);
    double fractal_dimension() const { return dimension_; }

    // Return a lower bound on ratio (Rmin / R), where "R" is the radius
    // passed to MakeLoop() and "Rmin" is the minimum distance from the
    // fractal boundary to its center, where all distances are measured in the
    // tangent plane at the fractal's center.  This can be used to inscribe
    // another geometric figure within the fractal without intersection.
    double min_radius_factor() const;

    // Return the ratio (Rmax / R), where "R" is the radius passed to
    // MakeLoop() and "Rmax" is the maximum distance from the fractal boundary
    // to its center, where all distances are measured in the tangent plane at
    // the fractal's center.  This can be used to inscribe the fractal within
    // some other geometric figure without intersection.
    double max_radius_factor() const;

    // Return a fractal loop centered around the z-axis of the given
    // coordinate frame, with the first vertex in the direction of the
    // positive x-axis.  In order to avoid self-intersections, the fractal is
    // generated by first drawing it in a 2D tangent plane to the unit sphere
    // (touching at the fractal's center point) and then projecting the edges
    // onto the sphere.  This has the side effect of shrinking the fractal
    // slightly compared to its nominal radius.
    std::unique_ptr<S2Loop> MakeLoop(Matrix3x3_d const& frame,
                                     S1Angle nominal_radius) const;

   private:
    void ComputeMinLevel();
    void ComputeOffsets();
    void GetR2Vertices(std::vector<R2Point>* vertices) const;
    void GetR2VerticesHelper(R2Point const& v0, R2Point const& v4, int level,
                             std::vector<R2Point>* vertices) const;

    int max_level_;
    int min_level_arg_;  // Value set by user
    int min_level_;      // Actual min level (depends on max_level_)
    double dimension_;

    // The ratio of the sub-edge length to the original edge length at each
    // subdivision step.
    double edge_fraction_;

    // The distance from the original edge to the middle vertex at each
    // subdivision step, as a fraction of the original edge length.
    double offset_fraction_;

    Fractal(Fractal const&) = delete;
    void operator=(Fractal const&) = delete;
  };

  // Convert a distance on the Earth's surface to an angle.
  // Do not use these methods in non-testing code; use s2earth.h instead.
  static S1Angle MetersToAngle(double meters);
  static S1Angle KmToAngle(double km);

  // Convert an area in steradians (as returned by the S2 area methods) to
  // square meters or square kilometers.
  static double AreaToMeters2(double steradians);
  static double AreaToKm2(double steradians);

  // The Earth's mean radius in kilometers (according to NASA).
  static double const kEarthRadiusKm;

  // A deterministically-seeded random number generator.
  class Random;

  static Random rnd;

  // Return a random unit-length vector.
  static S2Point RandomPoint();

  // Return a right-handed coordinate frame (three orthonormal vectors).
  static void GetRandomFrame(S2Point* x, S2Point* y, S2Point* z);
  static Matrix3x3_d GetRandomFrame();

  // Given a unit-length z-axis, compute x- and y-axes such that (x,y,z) is a
  // right-handed coordinate frame (three orthonormal vectors).
  static void GetRandomFrameAt(S2Point const& z, S2Point* x, S2Point *y);
  static Matrix3x3_d GetRandomFrameAt(S2Point const& z);

  // Return a cap with a random axis such that the log of its area is
  // uniformly distributed between the logs of the two given values.
  // (The log of the cap angle is also approximately uniformly distributed.)
  static S2Cap GetRandomCap(double min_area, double max_area);

  // Return a point chosen uniformly at random (with respect to area)
  // from the given cap.
  static S2Point SamplePoint(S2Cap const& cap);

  // Return a point chosen uniformly at random (with respect to area on the
  // sphere) from the given latitude-longitude rectangle.
  static S2Point SamplePoint(S2LatLngRect const& rect);

  // Return a random cell id at the given level or at a randomly chosen
  // level.  The distribution is uniform over the space of cell ids,
  // but only approximately uniform over the surface of the sphere.
  static S2CellId GetRandomCellId(int level);
  static S2CellId GetRandomCellId();

  // Return a polygon with the specified center, number of concentric loops
  // and vertices per loop.
  static void ConcentricLoopsPolygon(S2Point const& center,
                                     int num_loops,
                                     int num_vertices_per_loop,
                                     S2Polygon* polygon);

  // Checks that "covering" completely covers the given region.  If
  // "check_tight" is true, also checks that it does not contain any cells
  // that do not intersect the given region.  ("id" is only used internally.)
  static void CheckCovering(S2Region const& region,
                            S2CellUnion const& covering,
                            bool check_tight,
                            S2CellId id = S2CellId());

  // Returns the user time consumed by this process, in seconds.
  static double GetCpuTime();

 private:
  // Contains static methods
  S2Testing() = delete;
  S2Testing(S2Testing const&) = delete;
  void operator=(S2Testing const&) = delete;
};

// Functions in this class return random numbers that are as good as random()
// is.  The results are reproducible since the seed is deterministic.  This
// class is *NOT* thread-safe; it is only intended for testing purposes.
class S2Testing::Random {
 public:
  // Initialize using a deterministic seed.
  Random();

  // Reset the generator state using the given seed.
  void Reset(int32 seed);

  // Return a uniformly distributed 64-bit unsigned integer.
  uint64 Rand64();

  // Return a uniformly distributed 32-bit unsigned integer.
  uint32 Rand32();

  // Return a uniformly distributed "double" in the range [0,1).  Note that
  // the values returned are all multiples of 2**-53, which means that not all
  // possible values in this range are returned.
  double RandDouble();

  // Return a uniformly distributed integer in the range [0,n).
  int32 Uniform(int32 n);

  // Return a uniformly distributed "double" in the range [min, limit).
  double UniformDouble(double min, double limit);

  // A functor-style version of Uniform, so that this class can be used with
  // STL functions that require a RandomNumberGenerator concept.
  int32 operator() (int32 n) {
    return Uniform(n);
  }

  // Return true with probability 1 in n.
  bool OneIn(int32 n);

  // Skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  The effect is to pick a number in the
  // range [0,2^max_log-1] with bias towards smaller numbers.
  int32 Skewed(int max_log);

 private:
  // Currently this class is based on random(), therefore it makes no sense to
  // make a copy.
  Random(Random const&) = delete;
  void operator=(Random const&) = delete;
};

// Compare two sets of "closest" items, where "expected" is computed via brute
// force (i.e., considering every possible candidate) and "actual" is computed
// using a spatial data structure.  Here "max_size" is a bound on the maximum
// number of items, "max_distance" is a limit on the distance to any item, and
// "max_error" is the maximum error allowed when selecting which items are
// closest (see S2ClosestEdgeQuery::Options::max_error).
template <typename Id>
bool CheckDistanceResults(
    std::vector<std::pair<S1Angle, Id>> const& expected,
    std::vector<std::pair<S1Angle, Id>> const& actual,
    int max_size, S1Angle max_distance, S1Angle max_error);


//////////////////// Implementation Details Follow ////////////////////////


namespace S2 {
namespace internal {

template <typename T1, typename T2>
struct CompareFirst {
  inline bool operator()(std::pair<T1, T2> const& x,
                         std::pair<T1, T2> const& y) {
    return x.first < y.first;
  }
};

// Check that result set "x" contains all the expected results from "y", and
// does not include any duplicate results.
template <typename Id>
bool CheckResultSet(std::vector<std::pair<S1Angle, Id>> const& x,
                    std::vector<std::pair<S1Angle, Id>> const& y,
                    int max_size, S1Angle max_distance, S1Angle max_error,
                    S1Angle max_pruning_error, string const& label) {
  // Results should be sorted by distance.
  CompareFirst<S1Angle, Id> cmp;
  EXPECT_TRUE(std::is_sorted(x.begin(), x.end(), cmp));

  // Make sure there are no duplicate values.
  std::set<std::pair<S1Angle, Id>> x_set(x.begin(), x.end());
  EXPECT_EQ(x_set.size(), x.size()) << "Result set contains duplicates";

  // Result set X should contain all the items from U whose distance is less
  // than "limit" computed below.
  S1Angle limit = S1Angle::Zero();
  if (x.size() < max_size) {
    // Result set X was not limited by "max_size", so it should contain all
    // the items up to "max_distance", except that a few items right near the
    // distance limit may be missed because the distance measurements used for
    // pruning S2Cells are not conservative.
    limit = max_distance - max_pruning_error;
  } else if (!x.empty()) {
    // Result set X contains only the closest "max_size" items, to within a
    // tolerance of "max_error + max_pruning_error".
    limit = x.back().first - max_error - max_pruning_error;
  }
  bool result = true;
  for (auto const& p : y) {
    if (p.first < limit && std::count(x.begin(), x.end(), p) != 1) {
      result = false;
      std::cout << label << " distance = " << p.first
                << ", id = " << p.second << std::endl;
    }
  }
  return result;
}

}  // namespace internal
}  // namespace S2

template <typename Id>
bool CheckDistanceResults(
    std::vector<std::pair<S1Angle, Id>> const& expected,
    std::vector<std::pair<S1Angle, Id>> const& actual,
    int max_size, S1Angle max_distance, S1Angle max_error) {
  static S1Angle const kMaxPruningError = S1Angle::Radians(1e-15);
  return (S2::internal::CheckResultSet(
              actual, expected, max_size, max_distance, max_error,
              kMaxPruningError, "Missing") & /*not &&*/
          S2::internal::CheckResultSet(
              expected, actual, max_size, max_distance, max_error,
              S1Angle::Zero(), "Extra"));
}

#endif  // S2_S2TESTING_H_
