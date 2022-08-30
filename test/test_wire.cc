#include "catch.hpp"

#include "../src/serialization/wire_serialization.h"
#include <larcoreobj/SimpleTypesAndConstants/RawTypes.h>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>

#include <fstream>
#include <sstream>

namespace lar {
  bool operator==(const svf& a, const svf& b) {
      if (a.size() != b.size()) return false;
      std::vector<svfrange>::const_iterator current_a = a.begin_range();
      auto current_b = b.begin_range();
      auto end_a = a.end_range();
      auto end_b = b.end_range();
      while (current_a != end_a) {
        if (current_b == end_b) return false;
        if (!(*current_a == *current_b)) return false;
        ++current_a;
        ++current_b;
      }
      return current_b == end_b; 
  }
}

template<typename T>
  T write_and_recover(T const& a) {
    std::stringstream ss{};
    boost::archive::binary_oarchive oa(ss);
    oa << a;
    boost::archive::binary_iarchive ia(ss);
    T b{};
    ia >> b;
    return b;
}

TEST_CASE("SVFRange empty") 
{
  svfrange a{};
  CHECK(a.data().size() == 0); // verify that the data are consistent with the offset
  CHECK(a.offset == 0);
  CHECK(a.last == 0);
  svfrange b = write_and_recover(a);
  CHECK(a == b);
}

TEST_CASE("SVFRange non-empty")
{
  svfrange a(7, {1., 2., 3.});
  CHECK(a.data().size() == 3); // verify that the data are consistent with the offset
  CHECK(a.offset == 7);
  CHECK(a.last == 10);
  svfrange b = write_and_recover(a);
  CHECK(a == b);
}

TEST_CASE("empty sparse vector works") 
{
  svf a{};
  CHECK(a.empty());
  svf b = write_and_recover(a);
  CHECK(b.empty());
  CHECK(a == b);
}

TEST_CASE("dense sparse vector works") 
{
  std::vector<float> tmp{2.2, 4.3, 6.5, 8.7, 9.0};
  svf a(tmp, 0);
  CHECK(a.size() == 5);
  std::stringstream ss{};
  boost::archive::binary_oarchive oa(ss);
  oa << a;
  boost::archive::binary_iarchive ia(ss);
  svf b{};
  ia >> b;
  CHECK(b.size() == a.size());
  CHECK(a == b); 
}

TEST_CASE("Wire comparison works")
{
  std::vector<float> tmp{2.2, 4.3, 6.5, 8.7, 9.0};
  raw::ChannelID_t channel = 56;
  geo::View_t view = geo::k3D;
  svf sv(tmp, 0);
  recob::Wire wire(sv, channel, view);
  std::vector<float> tmp1{2.2, 4.3, 6.6, 8.7, 9.0};
  svf sv1(tmp1, 0);
  recob::Wire wire1(sv1, channel, view);
  CHECK(!(wire1 == wire)); // to check wires constructed from unequal sparse_vectors
}

TEST_CASE("writing a wire works")
{
  std::vector<float> tmp{2.2, 4.3, 6.5, 8.7, 9.0};
  raw::ChannelID_t channel = 56;
  geo::View_t view = geo::k3D;
  svf sv(tmp, 0);
  recob::Wire wire(sv, channel, view);
  {
    std::ofstream ofs("wireout");
    boost::archive::binary_oarchive oa(ofs);
    oa << wire;
    CHECK(ofs.good());
    ofs.close();
  }
  std::ifstream ifs("wireout");
  CHECK(ifs.good());
  boost::archive::binary_iarchive ia(ifs);
  recob::Wire w;
  CHECK(!(w == wire));
  ia >> w;
  auto a = w.SignalROI();
  CHECK( wire == w );
}
