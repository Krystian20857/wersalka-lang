//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/zone.h"

#include <gtest/gtest.h>

namespace wersalka {
namespace lang {
namespace runtime {

TEST(ZoneListTest, AddWithinCapacity) {
  Zone zone;

  ZoneList<int> list(&zone, 5);
  list.Add(&zone, 1);
  list.Add(&zone, 2);

  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list[0], 1);
  ASSERT_EQ(list[1], 2);
}

TEST(ZoneListTest, AddGrowing) {
  Zone zone;

  ZoneList<int> list(&zone, 1);
  list.Add(&zone, 1);
  list.Add(&zone, 2);

  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list[0], 1);
  ASSERT_EQ(list[1], 2);
}

TEST(ZoneListTest, AddGrowingWithZeroCapacity) {
  Zone zone;

  ZoneList<int> list(&zone, 0);
  list.Add(&zone, 1);
  list.Add(&zone, 2);

  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list[0], 1);
  ASSERT_EQ(list[1], 2);
}

TEST(ZoneListTest, ToVector) {
  Zone zone;

  ZoneList<int> list(&zone, 0);
  list.Add(&zone, 1);
  list.Add(&zone, 2);

  const auto vector = list.ToVector();
  ASSERT_EQ(vector.size(), 2);
  ASSERT_EQ(vector[0], 1);
  ASSERT_EQ(vector[1], 2);
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
