/**
 * MIT License
 *
 * Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define BOOST_TEST_DYN_LINK

#include <tsl/sparse_map.h>

#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "utils.h"

BOOST_AUTO_TEST_SUITE(test_sparse_map)

using test_types = boost::mpl::list<
    tsl::sparse_map<std::int64_t, std::int64_t>,
    tsl::sparse_map<std::string, std::string>,
    // Test with hash having a lot of collisions
    tsl::sparse_map<std::int64_t, std::int64_t, mod_hash<9>>,
    tsl::sparse_map<std::string, std::string, mod_hash<9>>,
    tsl::sparse_map<move_only_test, move_only_test, mod_hash<9>>,
    tsl::sparse_map<copy_only_test, copy_only_test, mod_hash<9>>,
    tsl::sparse_map<self_reference_member_test, self_reference_member_test,
                    mod_hash<9>>,

    // Others GrowthPolicy
    tsl::sparse_map<move_only_test, move_only_test, mod_hash<9>,
                    std::equal_to<move_only_test>,
                    std::allocator<std::pair<move_only_test, move_only_test>>,
                    tsl::sh::power_of_two_growth_policy<4>>,
    tsl::sparse_pg_map<move_only_test, move_only_test, mod_hash<9>>,
    tsl::sparse_map<move_only_test, move_only_test, mod_hash<9>,
                    std::equal_to<move_only_test>,
                    std::allocator<std::pair<move_only_test, move_only_test>>,
                    tsl::sh::mod_growth_policy<>>,

    tsl::sparse_map<copy_only_test, copy_only_test, mod_hash<9>,
                    std::equal_to<copy_only_test>,
                    std::allocator<std::pair<copy_only_test, copy_only_test>>,
                    tsl::sh::power_of_two_growth_policy<4>>,
    tsl::sparse_pg_map<copy_only_test, copy_only_test, mod_hash<9>>,
    tsl::sparse_map<copy_only_test, copy_only_test, mod_hash<9>,
                    std::equal_to<copy_only_test>,
                    std::allocator<std::pair<copy_only_test, copy_only_test>>,
                    tsl::sh::mod_growth_policy<>>,

    // Strong exception guarantee
    tsl::sparse_map<std::string, std::string, mod_hash<9>,
                    std::equal_to<std::string>,
                    std::allocator<std::pair<std::string, std::string>>,
                    tsl::sh::power_of_two_growth_policy<2>,
                    tsl::sh::exception_safety::strong>,

    // Others sparsity
    tsl::sparse_map<std::string, std::string, mod_hash<9>,
                    std::equal_to<std::string>,
                    std::allocator<std::pair<std::string, std::string>>,
                    tsl::sh::power_of_two_growth_policy<2>,
                    tsl::sh::exception_safety::basic, tsl::sh::sparsity::high>,
    tsl::sparse_map<std::string, std::string, mod_hash<9>,
                    std::equal_to<std::string>,
                    std::allocator<std::pair<std::string, std::string>>,
                    tsl::sh::power_of_two_growth_policy<2>,
                    tsl::sh::exception_safety::basic, tsl::sh::sparsity::low>>;

/**
 * insert
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HMap, test_types) {
  // insert x values, insert them again, check values
  using key_t = typename HMap::key_type;
  using value_t = typename HMap::mapped_type;

  const std::size_t nb_values = 1000;
  HMap map(0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);

  typename HMap::iterator it;
  bool inserted;

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) = map.insert(
        {utils::get_key<key_t>(i), utils::get_value<value_t>(i + 1)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
    BOOST_CHECK(!inserted);
  }

  for (std::size_t i = 0; i < nb_values; i++) {
    it = map.find(utils::get_key<key_t>(i));

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
  }
}

BOOST_AUTO_TEST_CASE(test_range_insert) {
  // create a vector<std::pair> of values to insert, insert part of them in the
  // map, check values
  const int nb_values = 1000;
  std::vector<std::pair<int, int>> values_to_insert(nb_values);
  for (int i = 0; i < nb_values; i++) {
    values_to_insert[i] = std::make_pair(i, i + 1);
  }

  tsl::sparse_map<int, int> map = {{-1, 1}, {-2, 2}};
  map.insert(std::next(values_to_insert.begin(), 10),
             values_to_insert.end() - 5);

  BOOST_CHECK_EQUAL(map.size(), 987);

  BOOST_CHECK_EQUAL(map[-1], 1);
  BOOST_CHECK_EQUAL(map[-2], 2);

  for (int i = 10; i < nb_values - 5; i++) {
    BOOST_CHECK_EQUAL(map[i], i + 1);
  }
}

BOOST_AUTO_TEST_CASE(test_insert_with_hint) {
  tsl::sparse_map<int, int> map{{1, 0}, {2, 1}, {3, 2}};

  // Wrong hint
  BOOST_CHECK(map.insert(map.find(2), std::make_pair(3, 4)) == map.find(3));

  // Good hint
  BOOST_CHECK(map.insert(map.find(2), std::make_pair(2, 4)) == map.find(2));

  // end() hint
  BOOST_CHECK(map.insert(map.find(10), std::make_pair(2, 4)) == map.find(2));

  BOOST_CHECK_EQUAL(map.size(), 3);

  // end() hint, new value
  BOOST_CHECK_EQUAL(map.insert(map.find(10), std::make_pair(4, 3))->first, 4);

  // Wrong hint, new value
  BOOST_CHECK_EQUAL(map.insert(map.find(2), std::make_pair(5, 4))->first, 5);

  BOOST_CHECK_EQUAL(map.size(), 5);
}

/**
 * emplace_hint
 */
BOOST_AUTO_TEST_CASE(test_emplace_hint) {
  tsl::sparse_map<int, int> map{{1, 0}, {2, 1}, {3, 2}};

  // Wrong hint
  BOOST_CHECK(map.emplace_hint(map.find(2), std::piecewise_construct,
                               std::forward_as_tuple(3),
                               std::forward_as_tuple(4)) == map.find(3));

  // Good hint
  BOOST_CHECK(map.emplace_hint(map.find(2), std::piecewise_construct,
                               std::forward_as_tuple(2),
                               std::forward_as_tuple(4)) == map.find(2));

  // end() hint
  BOOST_CHECK(map.emplace_hint(map.find(10), std::piecewise_construct,
                               std::forward_as_tuple(2),
                               std::forward_as_tuple(4)) == map.find(2));

  BOOST_CHECK_EQUAL(map.size(), 3);

  // end() hint, new value
  BOOST_CHECK_EQUAL(
      map.emplace_hint(map.find(10), std::piecewise_construct,
                       std::forward_as_tuple(4), std::forward_as_tuple(3))
          ->first,
      4);

  // Wrong hint, new value
  BOOST_CHECK_EQUAL(
      map.emplace_hint(map.find(2), std::piecewise_construct,
                       std::forward_as_tuple(5), std::forward_as_tuple(4))
          ->first,
      5);

  BOOST_CHECK_EQUAL(map.size(), 5);
}

/**
 * emplace
 */
BOOST_AUTO_TEST_CASE(test_emplace) {
  tsl::sparse_map<std::int64_t, move_only_test> map;
  tsl::sparse_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(10),
                  std::forward_as_tuple(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(10),
                  std::forward_as_tuple(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(!inserted);
}

/**
 * try_emplace
 */
BOOST_AUTO_TEST_CASE(test_try_emplace) {
  tsl::sparse_map<std::int64_t, move_only_test> map;
  tsl::sparse_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) = map.try_emplace(10, 1);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) = map.try_emplace(10, 3);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(!inserted);
}

BOOST_AUTO_TEST_CASE(test_try_emplace_2) {
  // Insert x values with try_emplace, insert them again, check with find.
  tsl::sparse_map<std::string, move_only_test> map;
  tsl::sparse_map<std::string, move_only_test>::iterator it;
  bool inserted;

  const std::size_t nb_values = 1000;
  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) = map.try_emplace(utils::get_key<std::string>(i), i);

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  for (std::size_t i = 0; i < nb_values; i++) {
    std::tie(it, inserted) =
        map.try_emplace(utils::get_key<std::string>(i), i + 1);

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
    BOOST_CHECK(!inserted);
  }

  for (std::size_t i = 0; i < nb_values; i++) {
    it = map.find(utils::get_key<std::string>(i));

    BOOST_CHECK_EQUAL(it->first, utils::get_key<std::string>(i));
    BOOST_CHECK_EQUAL(it->second, move_only_test(i));
  }
}

BOOST_AUTO_TEST_CASE(test_try_emplace_hint) {
  tsl::sparse_map<std::int64_t, move_only_test> map(0);

  // end() hint, new value
  auto it = map.try_emplace(map.find(10), 10, 1);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Good hint
  it = map.try_emplace(map.find(10), 10, 3);
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Wrong hint, new value
  it = map.try_emplace(map.find(10), 1, 3);
  BOOST_CHECK_EQUAL(it->first, 1);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
}

/**
 * insert_or_assign
 */
BOOST_AUTO_TEST_CASE(test_insert_or_assign) {
  tsl::sparse_map<std::int64_t, move_only_test> map;
  tsl::sparse_map<std::int64_t, move_only_test>::iterator it;
  bool inserted;

  std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));
  BOOST_CHECK(inserted);

  std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
  BOOST_CHECK(!inserted);
}

BOOST_AUTO_TEST_CASE(test_insert_or_assign_hint) {
  tsl::sparse_map<std::int64_t, move_only_test> map(0);

  // end() hint, new value
  auto it = map.insert_or_assign(map.find(10), 10, move_only_test(1));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(1));

  // Good hint
  it = map.insert_or_assign(map.find(10), 10, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 10);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));

  // Bad hint, new value
  it = map.insert_or_assign(map.find(10), 1, move_only_test(3));
  BOOST_CHECK_EQUAL(it->first, 1);
  BOOST_CHECK_EQUAL(it->second, move_only_test(3));
}

/**
 * erase
 */
BOOST_AUTO_TEST_CASE(test_range_erase_all) {
  // insert x values, delete all
  using HMap = tsl::sparse_map<std::string, std::int64_t>;

  const std::size_t nb_values = 1000;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.erase(map.begin(), map.end());
  BOOST_CHECK(it == map.end());
  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE(test_range_erase) {
  // insert x values, delete all except 10 first and 780 last values
  using HMap = tsl::sparse_map<std::string, std::int64_t>;

  const std::size_t nb_values = 1000;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it_first = std::next(map.begin(), 10);
  auto it_last = std::next(map.begin(), 220);

  auto it = map.erase(it_first, it_last);
  BOOST_CHECK_EQUAL(std::distance(it, map.end()), 780);
  BOOST_CHECK_EQUAL(map.size(), 790);
  BOOST_CHECK_EQUAL(std::distance(map.begin(), map.end()), 790);

  for (auto& val : map) {
    BOOST_CHECK_EQUAL(map.count(val.first), 1);
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_loop, HMap, test_types) {
  // insert x values, delete all one by one with iterator
  std::size_t nb_values = 1000;

  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map2 = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.begin();
  // Use second map to check for key after delete as we may not copy the key
  // with move-only types.
  auto it2 = map2.begin();
  while (it != map.end()) {
    it = map.erase(it);
    --nb_values;

    BOOST_CHECK_EQUAL(map.count(it2->first), 0);
    BOOST_CHECK_EQUAL(map.size(), nb_values);
    ++it2;
  }

  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_loop_range, HMap, test_types) {
  // insert x values, delete all five by five with iterators
  const std::size_t hop = 5;
  std::size_t nb_values = 1000;

  BOOST_REQUIRE_EQUAL(nb_values % hop, 0);

  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  auto it = map.begin();
  while (it != map.end()) {
    it = map.erase(it, std::next(it, hop));
    nb_values -= hop;

    BOOST_CHECK_EQUAL(map.size(), nb_values);
  }

  BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert_erase_insert, HMap, test_types) {
  // insert x/2 values, delete x/4 values, insert x/2 values, find each value
  using key_t = typename HMap::key_type;
  using value_t = typename HMap::mapped_type;

  const std::size_t nb_values = 2000;
  HMap map(10);
  typename HMap::iterator it;
  bool inserted;

  // Insert nb_values/2
  for (std::size_t i = 0; i < nb_values / 2; i++) {
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values / 2);

  // Delete nb_values/4
  for (std::size_t i = 0; i < nb_values / 2; i++) {
    if (i % 2 == 0) {
      BOOST_CHECK_EQUAL(map.erase(utils::get_key<key_t>(i)), 1);
    }
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values / 4);

  // Insert nb_values/2
  for (std::size_t i = nb_values / 2; i < nb_values; i++) {
    std::tie(it, inserted) =
        map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});

    BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
    BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
    BOOST_CHECK(inserted);
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values - nb_values / 4);

  // Find
  for (std::size_t i = 0; i < nb_values; i++) {
    if (i % 2 == 0 && i < nb_values / 2) {
      it = map.find(utils::get_key<key_t>(i));

      BOOST_CHECK(it == map.end());
    } else {
      it = map.find(utils::get_key<key_t>(i));

      BOOST_REQUIRE(it != map.end());
      BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
      BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(test_range_erase_same_iterators) {
  // insert x values, test erase with same iterator as each parameter, check if
  // returned mutable iterator is valid.
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::sparse_map<std::int64_t, std::int64_t>>(
          nb_values);

  tsl::sparse_map<std::int64_t, std::int64_t>::const_iterator it_const =
      map.cbegin();
  std::advance(it_const, 10);

  tsl::sparse_map<std::int64_t, std::int64_t>::iterator it_mutable =
      map.erase(it_const, it_const);
  BOOST_CHECK(it_const == it_mutable);
  BOOST_CHECK(map.mutable_iterator(it_const) == it_mutable);
  BOOST_CHECK_EQUAL(map.size(), 100);

  it_mutable.value() = -100;
  BOOST_CHECK_EQUAL(it_const.value(), -100);
}

/**
 * rehash
 */
BOOST_AUTO_TEST_CASE(test_rehash_empty) {
  // test rehash(0), test find/erase/insert on map.
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::sparse_map<std::int64_t, std::int64_t>>(
          nb_values);

  const std::size_t bucket_count = map.bucket_count();
  BOOST_CHECK(bucket_count >= nb_values);

  map.clear();
  BOOST_CHECK_EQUAL(map.bucket_count(), bucket_count);
  BOOST_CHECK(map.empty());

  map.rehash(0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK(map.empty());

  BOOST_CHECK(map.find(1) == map.end());
  BOOST_CHECK_EQUAL(map.erase(1), 0);
  BOOST_CHECK(map.insert({1, 10}).second);
  BOOST_CHECK_EQUAL(map.at(1), 10);
}

/**
 * operator== and operator!=
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_compare, HMap, test_types) {
  const tsl::sparse_map<std::string, std::int64_t> map1 = {
      {"a", 1}, {"e", 5}, {"d", 4}, {"c", 3}, {"b", 2}};
  const tsl::sparse_map<std::string, std::int64_t> map1_copy = {
      {"e", 5}, {"c", 3}, {"b", 2}, {"a", 1}, {"d", 4}};
  const tsl::sparse_map<std::string, std::int64_t> map2 = {
      {"e", 5}, {"c", 3}, {"b", 2}, {"a", 1}, {"d", 4}, {"f", 6}};
  const tsl::sparse_map<std::string, std::int64_t> map3 = {
      {"e", 5}, {"c", 3}, {"b", 2}, {"a", 1}};
  const tsl::sparse_map<std::string, std::int64_t> map4 = {
      {"a", 1}, {"e", 5}, {"d", 4}, {"c", 3}, {"b", 26}};
  const tsl::sparse_map<std::string, std::int64_t> map5 = {
      {"a", 1}, {"e", 5}, {"d", 4}, {"c", 3}, {"z", 2}};

  BOOST_CHECK(map1 == map1_copy);
  BOOST_CHECK(map1_copy == map1);

  BOOST_CHECK(map1 != map2);
  BOOST_CHECK(map2 != map1);

  BOOST_CHECK(map1 != map3);
  BOOST_CHECK(map3 != map1);

  BOOST_CHECK(map1 != map4);
  BOOST_CHECK(map4 != map1);

  BOOST_CHECK(map1 != map5);
  BOOST_CHECK(map5 != map1);

  BOOST_CHECK(map2 != map3);
  BOOST_CHECK(map3 != map2);

  BOOST_CHECK(map2 != map4);
  BOOST_CHECK(map4 != map2);

  BOOST_CHECK(map2 != map5);
  BOOST_CHECK(map5 != map2);

  BOOST_CHECK(map3 != map4);
  BOOST_CHECK(map4 != map3);

  BOOST_CHECK(map3 != map5);
  BOOST_CHECK(map5 != map3);

  BOOST_CHECK(map4 != map5);
  BOOST_CHECK(map5 != map4);
}

/**
 * clear
 */
BOOST_AUTO_TEST_CASE(test_clear) {
  // insert x values, clear map
  using HMap = tsl::sparse_map<std::int64_t, std::int64_t>;

  const std::size_t nb_values = 1000;
  auto map = utils::get_filled_hash_map<HMap>(nb_values);

  map.clear();
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(std::distance(map.begin(), map.end()), 0);

  map.insert({5, -5});
  map.insert({{1, -1}, {2, -1}, {4, -4}, {3, -3}});

  BOOST_CHECK(map == (HMap({{5, -5}, {1, -1}, {2, -1}, {4, -4}, {3, -3}})));
}

/**
 * iterator.value()
 */
BOOST_AUTO_TEST_CASE(test_modify_value_through_iterator) {
  // insert x values, modify value of even keys, check values
  const std::size_t nb_values = 100;
  auto map =
      utils::get_filled_hash_map<tsl::sparse_map<std::int64_t, std::int64_t>>(
          nb_values);

  for (auto it = map.begin(); it != map.end(); it++) {
    if (it.key() % 2 == 0) {
      it.value() = -1;
    }
  }

  for (auto& val : map) {
    if (val.first % 2 == 0) {
      BOOST_CHECK_EQUAL(val.second, -1);
    } else {
      BOOST_CHECK_NE(val.second, -1);
    }
  }
}

/**
 * constructor
 */
BOOST_AUTO_TEST_CASE(test_extreme_bucket_count_value_construction) {
  BOOST_CHECK_THROW(
      (tsl::sparse_map<int, int, std::hash<int>, std::equal_to<int>,
                       std::allocator<std::pair<int, int>>,
                       tsl::sh::power_of_two_growth_policy<2>>(
          std::numeric_limits<std::size_t>::max())),
      std::length_error);

  BOOST_CHECK_THROW(
      (tsl::sparse_map<int, int, std::hash<int>, std::equal_to<int>,
                       std::allocator<std::pair<int, int>>,
                       tsl::sh::power_of_two_growth_policy<2>>(
          std::numeric_limits<std::size_t>::max() / 2 + 1)),
      std::length_error);

  BOOST_CHECK_THROW(
      (tsl::sparse_map<int, int, std::hash<int>, std::equal_to<int>,
                       std::allocator<std::pair<int, int>>,
                       tsl::sh::prime_growth_policy>(
          std::numeric_limits<std::size_t>::max())),
      std::length_error);

  BOOST_CHECK_THROW(
      (tsl::sparse_map<int, int, std::hash<int>, std::equal_to<int>,
                       std::allocator<std::pair<int, int>>,
                       tsl::sh::prime_growth_policy>(
          std::numeric_limits<std::size_t>::max() / 2)),
      std::length_error);

  BOOST_CHECK_THROW(
      (tsl::sparse_map<int, int, std::hash<int>, std::equal_to<int>,
                       std::allocator<std::pair<int, int>>,
                       tsl::sh::mod_growth_policy<>>(
          std::numeric_limits<std::size_t>::max())),
      std::length_error);
}

BOOST_AUTO_TEST_CASE(test_range_construct) {
  tsl::sparse_map<int, int> map = {{2, 1}, {1, 0}, {3, 2}};

  tsl::sparse_map<int, int> map2(map.begin(), map.end());
  tsl::sparse_map<int, int> map3(map.cbegin(), map.cend());
}

/**
 * operator=(std::initializer_list)
 */
BOOST_AUTO_TEST_CASE(test_assign_operator) {
  tsl::sparse_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};
  BOOST_CHECK_EQUAL(map.size(), 2);

  map = {{1, 3}, {2, 4}};
  BOOST_CHECK_EQUAL(map.size(), 2);
  BOOST_CHECK_EQUAL(map.at(1), 3);
  BOOST_CHECK_EQUAL(map.at(2), 4);
  BOOST_CHECK(map.find(0) == map.end());

  map = {};
  BOOST_CHECK(map.empty());
}

/**
 * move/copy constructor/operator
 */
BOOST_AUTO_TEST_CASE(test_move_constructor) {
  // insert x values in map, move map into map_move with move constructor, check
  // map and map_move, insert additional values in map_move, check map_move
  using HMap = tsl::sparse_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(std::move(map));

  BOOST_CHECK(map_move == utils::get_filled_hash_map<HMap>(nb_values));
  BOOST_CHECK(map == (HMap()));

  for (std::size_t i = nb_values; i < nb_values * 2; i++) {
    map_move.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map_move.size(), nb_values * 2);
  BOOST_CHECK(map_move == utils::get_filled_hash_map<HMap>(nb_values * 2));
}

BOOST_AUTO_TEST_CASE(test_move_constructor_empty) {
  tsl::sparse_map<std::string, move_only_test> map(0);
  tsl::sparse_map<std::string, move_only_test> map_move(std::move(map));

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_move.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_move.find("") == map_move.end());
}

BOOST_AUTO_TEST_CASE(test_move_operator) {
  // insert x values in map, move map into map_move, check map and map_move,
  // insert additional values in map_move, check map_move
  using HMap = tsl::sparse_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move = utils::get_filled_hash_map<HMap>(1);
  map_move = std::move(map);

  BOOST_CHECK(map_move == utils::get_filled_hash_map<HMap>(nb_values));
  BOOST_CHECK(map == (HMap()));

  for (std::size_t i = nb_values; i < nb_values * 2; i++) {
    map_move.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map_move.size(), nb_values * 2);
  BOOST_CHECK(map_move == utils::get_filled_hash_map<HMap>(nb_values * 2));
}

BOOST_AUTO_TEST_CASE(test_move_operator_empty) {
  tsl::sparse_map<std::string, move_only_test> map(0);
  tsl::sparse_map<std::string, move_only_test> map_move;
  map_move = (std::move(map));

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_move.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_move.find("") == map_move.end());
}

BOOST_AUTO_TEST_CASE(test_reassign_moved_object_move_constructor) {
  using HMap = tsl::sparse_map<std::string, std::string>;

  HMap map = {{"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  HMap map_move(std::move(map));

  BOOST_CHECK_EQUAL(map_move.size(), 3);
  BOOST_CHECK_EQUAL(map.size(), 0);

  map = {{"Key4", "Value4"}, {"Key5", "Value5"}};
  BOOST_CHECK(map == (HMap({{"Key4", "Value4"}, {"Key5", "Value5"}})));
}

BOOST_AUTO_TEST_CASE(test_reassign_moved_object_move_operator) {
  using HMap = tsl::sparse_map<std::string, std::string>;

  HMap map = {{"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  HMap map_move = std::move(map);

  BOOST_CHECK_EQUAL(map_move.size(), 3);
  BOOST_CHECK_EQUAL(map.size(), 0);

  map = {{"Key4", "Value4"}, {"Key5", "Value5"}};
  BOOST_CHECK(map == (HMap({{"Key4", "Value4"}, {"Key5", "Value5"}})));
}

BOOST_AUTO_TEST_CASE(test_use_after_move_constructor) {
  using HMap = tsl::sparse_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(std::move(map));

  BOOST_CHECK(map == (HMap()));
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.erase("a"), 0);
  BOOST_CHECK(map.find("a") == map.end());

  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map.size(), nb_values);
  BOOST_CHECK(map == map_move);
}

BOOST_AUTO_TEST_CASE(test_use_after_move_operator) {
  using HMap = tsl::sparse_map<std::string, move_only_test>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);
  HMap map_move(0);
  map_move = std::move(map);

  BOOST_CHECK(map == (HMap()));
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.erase("a"), 0);
  BOOST_CHECK(map.find("a") == map.end());

  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  BOOST_CHECK_EQUAL(map.size(), nb_values);
  BOOST_CHECK(map == map_move);
}

BOOST_AUTO_TEST_CASE(test_copy_constructor_and_operator) {
  using HMap = tsl::sparse_map<std::string, std::string, mod_hash<9>>;

  const std::size_t nb_values = 100;
  HMap map = utils::get_filled_hash_map<HMap>(nb_values);

  HMap map_copy = map;
  HMap map_copy2(map);
  HMap map_copy3 = utils::get_filled_hash_map<HMap>(1);
  map_copy3 = map;

  BOOST_CHECK(map == map_copy);
  map.clear();

  BOOST_CHECK(map_copy == map_copy2);
  BOOST_CHECK(map_copy == map_copy3);
}

BOOST_AUTO_TEST_CASE(test_copy_constructor_empty) {
  tsl::sparse_map<std::string, int> map(0);
  tsl::sparse_map<std::string, int> map_copy(map);

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_copy.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_copy.find("") == map_copy.end());
}

BOOST_AUTO_TEST_CASE(test_copy_operator_empty) {
  tsl::sparse_map<std::string, int> map(0);
  tsl::sparse_map<std::string, int> map_copy(16);
  map_copy = map;

  BOOST_CHECK(map.empty());
  BOOST_CHECK(map_copy.empty());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map_copy.find("") == map_copy.end());
}

/**
 * at
 */
BOOST_AUTO_TEST_CASE(test_at) {
  // insert x values, use at for known and unknown values.
  const tsl::sparse_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK_EQUAL(map.at(0), 10);
  BOOST_CHECK_EQUAL(map.at(-2), 20);
  BOOST_CHECK_THROW(map.at(1), std::out_of_range);
}

/**
 * contains
 */
BOOST_AUTO_TEST_CASE(test_contains) {
  const tsl::sparse_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK(map.contains(0));
  BOOST_CHECK(map.contains(-2));
  BOOST_CHECK(!map.contains(-3));
}

/**
 * equal_range
 */
BOOST_AUTO_TEST_CASE(test_equal_range) {
  const tsl::sparse_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  auto it_pair = map.equal_range(0);
  BOOST_REQUIRE_EQUAL(std::distance(it_pair.first, it_pair.second), 1);
  BOOST_CHECK_EQUAL(it_pair.first->second, 10);

  it_pair = map.equal_range(1);
  BOOST_CHECK(it_pair.first == it_pair.second);
  BOOST_CHECK(it_pair.first == map.end());
}

/**
 * operator[]
 */
BOOST_AUTO_TEST_CASE(test_access_operator) {
  // insert x values, use at for known and unknown values.
  tsl::sparse_map<std::int64_t, std::int64_t> map = {{0, 10}, {-2, 20}};

  BOOST_CHECK_EQUAL(map[0], 10);
  BOOST_CHECK_EQUAL(map[-2], 20);
  BOOST_CHECK_EQUAL(map[2], std::int64_t());

  BOOST_CHECK_EQUAL(map.size(), 3);
}

/**
 * swap
 */
BOOST_AUTO_TEST_CASE(test_swap) {
  tsl::sparse_map<std::int64_t, std::int64_t> map = {{1, 10}, {8, 80}, {3, 30}};
  tsl::sparse_map<std::int64_t, std::int64_t> map2 = {{4, 40}, {5, 50}};

  using std::swap;
  swap(map, map2);

  BOOST_CHECK(map ==
              (tsl::sparse_map<std::int64_t, std::int64_t>{{4, 40}, {5, 50}}));
  BOOST_CHECK(map2 == (tsl::sparse_map<std::int64_t, std::int64_t>{
                          {1, 10}, {8, 80}, {3, 30}}));

  map.insert({6, 60});
  map2.insert({4, 40});

  BOOST_CHECK(map == (tsl::sparse_map<std::int64_t, std::int64_t>{
                         {4, 40}, {5, 50}, {6, 60}}));
  BOOST_CHECK(map2 == (tsl::sparse_map<std::int64_t, std::int64_t>{
                          {1, 10}, {8, 80}, {3, 30}, {4, 40}}));
}

BOOST_AUTO_TEST_CASE(test_swap_empty) {
  tsl::sparse_map<std::int64_t, std::int64_t> map = {{1, 10}, {8, 80}, {3, 30}};
  tsl::sparse_map<std::int64_t, std::int64_t> map2;

  using std::swap;
  swap(map, map2);

  BOOST_CHECK(map == (tsl::sparse_map<std::int64_t, std::int64_t>{}));
  BOOST_CHECK(map2 == (tsl::sparse_map<std::int64_t, std::int64_t>{
                          {1, 10}, {8, 80}, {3, 30}}));

  map.insert({6, 60});
  map2.insert({4, 40});

  BOOST_CHECK(map == (tsl::sparse_map<std::int64_t, std::int64_t>{{6, 60}}));
  BOOST_CHECK(map2 == (tsl::sparse_map<std::int64_t, std::int64_t>{
                          {1, 10}, {8, 80}, {3, 30}, {4, 40}}));
}

/**
 * serialize and deserialize
 */
BOOST_AUTO_TEST_CASE(test_serialize_deserialize_empty) {
  // serialize empty map; deserialize in new map; check equal.
  // for deserialization, test it with and without hash compatibility.
  const tsl::sparse_map<std::string, move_only_test> empty_map(0);

  serializer serial;
  empty_map.serialize(serial);

  deserializer dserial(serial.str());
  auto empty_map_deserialized = decltype(empty_map)::deserialize(dserial, true);
  BOOST_CHECK(empty_map_deserialized == empty_map);

  deserializer dserial2(serial.str());
  empty_map_deserialized = decltype(empty_map)::deserialize(dserial2, false);
  BOOST_CHECK(empty_map_deserialized == empty_map);
}

BOOST_AUTO_TEST_CASE(test_serialize_deserialize_few) {
  // insert x values that fits into one sparse bucket; delete some values;
  // serialize map; deserialize in new map; check equal. for deserialization,
  // test it with and without hash compatibility.
  const tsl::sparse_map<std::int64_t, std::int64_t> map{
      {10, 100}, {4, 14}, {9, 201}};

  serializer serial;
  map.serialize(serial);

  deserializer dserial(serial.str());
  auto map_deserialized = decltype(map)::deserialize(dserial, true);
  BOOST_CHECK(map_deserialized == map);

  deserializer dserial2(serial.str());
  map_deserialized = decltype(map)::deserialize(dserial2, false);
  BOOST_CHECK(map_deserialized == map);
}

BOOST_AUTO_TEST_CASE(test_serialize_deserialize) {
  // insert x values; delete some values; serialize map; deserialize in new map;
  // check equal. for deserialization, test it with and without hash
  // compatibility.
  const std::size_t nb_values = 1000;

  tsl::sparse_map<std::string, move_only_test> map;
  for (std::size_t i = 0; i < nb_values + 40; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }

  for (std::size_t i = nb_values; i < nb_values + 40; i++) {
    map.erase(utils::get_key<std::string>(i));
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  serializer serial;
  map.serialize(serial);

  deserializer dserial(serial.str());
  auto map_deserialized = decltype(map)::deserialize(dserial, true);
  BOOST_CHECK(map == map_deserialized);

  deserializer dserial2(serial.str());
  map_deserialized = decltype(map)::deserialize(dserial2, false);
  BOOST_CHECK(map_deserialized == map);
}

BOOST_AUTO_TEST_CASE(test_serialize_deserialize_with_different_hash) {
  // insert x values; serialize map; deserialize in new map which has a
  // different hash; check equal
  struct hash_str_diff {
    std::size_t operator()(const std::string& str) const {
      return std::hash<std::string>()(str) + 123;
    }
  };

  const std::size_t nb_values = 1000;

  tsl::sparse_map<std::string, move_only_test> map;
  for (std::size_t i = 0; i < nb_values; i++) {
    map.insert(
        {utils::get_key<std::string>(i), utils::get_value<move_only_test>(i)});
  }
  BOOST_CHECK_EQUAL(map.size(), nb_values);

  serializer serial;
  map.serialize(serial);

  deserializer dserial(serial.str());
  auto map_deserialized =
      tsl::sparse_map<std::string, move_only_test, hash_str_diff>::deserialize(
          dserial, false);

  BOOST_CHECK_EQUAL(map_deserialized.size(), map.size());
  for (const auto& val : map) {
    BOOST_CHECK(map_deserialized.find(val.first) != map_deserialized.end());
  }
}

/**
 * KeyEqual
 */
BOOST_AUTO_TEST_CASE(test_key_equal) {
  // Use a KeyEqual and Hash where any odd unsigned number 'x' is equal to
  // 'x-1'. Make sure that KeyEqual is called (and not ==).
  struct hash {
    std::size_t operator()(std::uint64_t v) const {
      if (v % 2u == 1u) {
        return std::hash<std::uint64_t>()(v - 1);
      } else {
        return std::hash<std::uint64_t>()(v);
      }
    }
  };

  struct key_equal {
    bool operator()(std::uint64_t lhs, std::uint64_t rhs) const {
      if (lhs % 2u == 1u) {
        lhs--;
      }

      if (rhs % 2u == 1u) {
        rhs--;
      }

      return lhs == rhs;
    }
  };

  tsl::sparse_map<std::uint64_t, std::uint64_t, hash, key_equal> map;
  BOOST_CHECK(map.insert({2, 10}).second);
  BOOST_CHECK_EQUAL(map.at(2), 10);
  BOOST_CHECK_EQUAL(map.at(3), 10);
  BOOST_CHECK(!map.insert({3, 10}).second);

  BOOST_CHECK_EQUAL(map.size(), 1);
}

/**
 * other
 */
BOOST_AUTO_TEST_CASE(
    test_operations_with_all_buckets_marked_as_deleted_or_with_a_value) {
  // Test find/erase/insert operations on a map which we craft to have all its
  // buckets marked as deleted or containing a value to be sure that everything
  // works well in this edge case. Intrusive test (it's tightly coupled with the
  // implementation of the map).
  struct identity_hash {
    std::size_t operator()(unsigned int value) const {
      return std::size_t(value);
    }
  };

  tsl::sparse_map<unsigned int, unsigned int, identity_hash> map;
  map.max_load_factor(0.8f);
  map.rehash(64);

  BOOST_CHECK_EQUAL(map.bucket_count(), 64);
  BOOST_CHECK_EQUAL(map.max_load_factor(), 0.8f);

  for (unsigned int i = 0; i < 51; i++) {
    BOOST_CHECK(map.insert({i, i}).second);
  }

  for (unsigned int i = 0; i < 14; i++) {
    BOOST_CHECK_EQUAL(map.erase(i), 1);
  }

  for (unsigned int i = 51; i < 64; i++) {
    BOOST_CHECK(map.insert({i, i}).second);
  }

  BOOST_CHECK_EQUAL(map.size(), 50);
  BOOST_CHECK_EQUAL(map.bucket_count(), 64);

  /**
   * Map full of buckets marked as deleted or with a value. Check that find,
   * erase and insert operations work well.
   */

  // Find inexistent values.
  for (unsigned int i = 0; i < 14; i++) {
    BOOST_CHECK(map.find(i) == map.end());
  }

  // Erase inexistent values.
  for (unsigned int i = 0; i < 14; i++) {
    BOOST_CHECK_EQUAL(map.erase(i), 0);
  }
  BOOST_CHECK_EQUAL(map.size(), 50);
  BOOST_CHECK_EQUAL(map.bucket_count(), 64);

  // Try to insert existing values.
  for (unsigned int i = 14; i < 64; i++) {
    BOOST_CHECK(!map.insert({i, i}).second);
  }
  BOOST_CHECK_EQUAL(map.size(), 50);
  BOOST_CHECK_EQUAL(map.bucket_count(), 64);

  // Insert new values
  for (unsigned int i = 0; i < 14; i++) {
    BOOST_CHECK(map.insert({i, i}).second);
  }
  BOOST_CHECK_EQUAL(map.size(), 64);
  BOOST_CHECK_EQUAL(map.bucket_count(), 128);
}

BOOST_AUTO_TEST_CASE(test_heterogeneous_lookups) {
  struct hash_ptr {
    std::size_t operator()(const std::unique_ptr<int>& p) const {
      return std::hash<std::uintptr_t>()(
          reinterpret_cast<std::uintptr_t>(p.get()));
    }

    std::size_t operator()(std::uintptr_t p) const {
      return std::hash<std::uintptr_t>()(p);
    }

    std::size_t operator()(const int* const& p) const {
      return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(p));
    }
  };

  struct equal_to_ptr {
    using is_transparent = std::true_type;

    bool operator()(const std::unique_ptr<int>& p1,
                    const std::unique_ptr<int>& p2) const {
      return p1 == p2;
    }

    bool operator()(const std::unique_ptr<int>& p1, std::uintptr_t p2) const {
      return reinterpret_cast<std::uintptr_t>(p1.get()) == p2;
    }

    bool operator()(std::uintptr_t p1, const std::unique_ptr<int>& p2) const {
      return p1 == reinterpret_cast<std::uintptr_t>(p2.get());
    }

    bool operator()(const std::unique_ptr<int>& p1,
                    const int* const& p2) const {
      return p1.get() == p2;
    }

    bool operator()(const int* const& p1,
                    const std::unique_ptr<int>& p2) const {
      return p1 == p2.get();
    }
  };

  std::unique_ptr<int> ptr1(new int(1));
  std::unique_ptr<int> ptr2(new int(2));
  std::unique_ptr<int> ptr3(new int(3));
  int other = -1;

  const std::uintptr_t addr1 = reinterpret_cast<std::uintptr_t>(ptr1.get());
  const int* const addr2 = ptr2.get();
  const int* const addr_unknown = &other;

  tsl::sparse_map<std::unique_ptr<int>, int, hash_ptr, equal_to_ptr> map;
  map.insert({std::move(ptr1), 4});
  map.insert({std::move(ptr2), 5});
  map.insert({std::move(ptr3), 6});

  BOOST_CHECK_EQUAL(map.size(), 3);

  BOOST_CHECK_EQUAL(map.at(addr1), 4);
  BOOST_CHECK_EQUAL(map.at(addr2), 5);
  BOOST_CHECK_THROW(map.at(addr_unknown), std::out_of_range);

  BOOST_REQUIRE(map.find(addr1) != map.end());
  BOOST_CHECK_EQUAL(*map.find(addr1)->first, 1);

  BOOST_REQUIRE(map.find(addr2) != map.end());
  BOOST_CHECK_EQUAL(*map.find(addr2)->first, 2);

  BOOST_CHECK(map.find(addr_unknown) == map.end());

  BOOST_CHECK_EQUAL(map.count(addr1), 1);
  BOOST_CHECK_EQUAL(map.count(addr2), 1);
  BOOST_CHECK_EQUAL(map.count(addr_unknown), 0);

  BOOST_CHECK_EQUAL(map.erase(addr1), 1);
  BOOST_CHECK_EQUAL(map.erase(addr2), 1);
  BOOST_CHECK_EQUAL(map.erase(addr_unknown), 0);

  BOOST_CHECK_EQUAL(map.size(), 1);
}

/**
 * Various operations on empty map
 */
BOOST_AUTO_TEST_CASE(test_empty_map) {
  tsl::sparse_map<std::string, int> map(0);

  BOOST_CHECK_EQUAL(map.bucket_count(), 0);
  BOOST_CHECK_EQUAL(map.size(), 0);
  BOOST_CHECK_EQUAL(map.load_factor(), 0);
  BOOST_CHECK(map.empty());

  BOOST_CHECK(map.begin() == map.end());
  BOOST_CHECK(map.begin() == map.cend());
  BOOST_CHECK(map.cbegin() == map.cend());

  BOOST_CHECK(map.find("") == map.end());
  BOOST_CHECK(map.find("test") == map.end());

  BOOST_CHECK_EQUAL(map.count(""), 0);
  BOOST_CHECK_EQUAL(map.count("test"), 0);

  BOOST_CHECK(!map.contains(""));
  BOOST_CHECK(!map.contains("test"));

  BOOST_CHECK_THROW(map.at(""), std::out_of_range);
  BOOST_CHECK_THROW(map.at("test"), std::out_of_range);

  auto range = map.equal_range("test");
  BOOST_CHECK(range.first == range.second);

  BOOST_CHECK_EQUAL(map.erase("test"), 0);
  BOOST_CHECK(map.erase(map.begin(), map.end()) == map.end());

  BOOST_CHECK_EQUAL(map["new value"], int{});
}

/**
 * Test precalculated hash
 */
BOOST_AUTO_TEST_CASE(test_precalculated_hash) {
  tsl::sparse_map<int, int, identity_hash<int>> map = {
      {1, -1}, {2, -2}, {3, -3}, {4, -4}, {5, -5}, {6, -6}};
  const tsl::sparse_map<int, int, identity_hash<int>> map_const = map;

  /**
   * find
   */
  BOOST_REQUIRE(map.find(3, map.hash_function()(3)) != map.end());
  BOOST_CHECK_EQUAL(map.find(3, map.hash_function()(3))->second, -3);

  BOOST_REQUIRE(map_const.find(3, map_const.hash_function()(3)) !=
                map_const.end());
  BOOST_CHECK_EQUAL(map_const.find(3, map_const.hash_function()(3))->second,
                    -3);

  /**
   * at
   */
  BOOST_CHECK_EQUAL(map.at(3, map.hash_function()(3)), -3);
  BOOST_CHECK_EQUAL(map_const.at(3, map_const.hash_function()(3)), -3);

  /**
   * contains
   */
  BOOST_CHECK(map.contains(3, map.hash_function()(3)));
  BOOST_CHECK(map_const.contains(3, map_const.hash_function()(3)));

  /**
   * count
   */
  BOOST_CHECK_EQUAL(map.count(3, map.hash_function()(3)), 1);
  BOOST_CHECK_EQUAL(map_const.count(3, map_const.hash_function()(3)), 1);

  /**
   * equal_range
   */
  auto it_range = map.equal_range(3, map.hash_function()(3));
  BOOST_REQUIRE_EQUAL(std::distance(it_range.first, it_range.second), 1);
  BOOST_CHECK_EQUAL(it_range.first->second, -3);

  auto it_range_const = map_const.equal_range(3, map_const.hash_function()(3));
  BOOST_REQUIRE_EQUAL(
      std::distance(it_range_const.first, it_range_const.second), 1);
  BOOST_CHECK_EQUAL(it_range_const.first->second, -3);

  /**
   * erase
   */
  BOOST_CHECK_EQUAL(map.erase(3, map.hash_function()(3)), 1);
}

/**
 * Test for correct value const-ness
 * This is a compilation test, not a behavior test.
 */
BOOST_AUTO_TEST_CASE(test_iterator_constness) {
  using map_type = tsl::sparse_map<int, int>;

  map_type map;
  map.insert(std::make_pair(0, 1));

  auto it = map.find(0);
  int* int_ptr = &it->second;
  BOOST_CHECK_EQUAL(*int_ptr, 1);

  const map_type* map_ptr = &map;
  auto const_it = map_ptr->find(0);
  auto const_int_ptr = &const_it->second;
  static_assert(
    std::is_same<decltype(const_int_ptr), const int*>::value == true,
    "const iterator should have const value type"
  );
}

BOOST_AUTO_TEST_SUITE_END()
