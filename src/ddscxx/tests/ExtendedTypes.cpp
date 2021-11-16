/*
 * Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/dds.hpp"
#include <gtest/gtest.h>
#include "ExtendedTypesModels.hpp"

using namespace org::eclipse::cyclonedds::core::cdr;
using namespace ExtendedTypes_testing;

typedef std::vector<char> bytes;

/**
 * Fixture for the ExtendedTypes tests
 */
class ExtendedTypes : public ::testing::Test
{
public:
  ExtendedTypes()
  {
  }

  void SetUp() { }

  void TearDown() { }

  template<class MSGIN, class MSGOUT, class S>
  void validate(const MSGIN& in, bool exp_read_result = true)
  {
    S str;
    ASSERT_TRUE(move(str, in, false));

    bytes buffer(str.position(), 0x0);
    str.set_buffer(buffer.data(), buffer.size());

    ASSERT_TRUE(write(str, in, false));

    MSGOUT out;
    str.reset();
    bool read_result = read(str, out, false);
    ASSERT_EQ(read_result, exp_read_result);

    if (read_result)
    {
      ASSERT_EQ(in.c(), out.c());
      ASSERT_EQ(in.d(), out.d());
    }
  }
};

TEST_F(ExtendedTypes, appendable)
{
  appendablestruct_smaller smaller('c', 'd');
  validate<appendablestruct_smaller, appendablestruct_larger, xcdr_v1_stream>(smaller, false);
  validate<appendablestruct_smaller, appendablestruct_larger, xcdr_v2_stream>(smaller);

  appendablestruct_larger larger('c', 'd', 'e');
  validate<appendablestruct_larger, appendablestruct_smaller, xcdr_v1_stream>(larger);
  validate<appendablestruct_larger, appendablestruct_smaller, xcdr_v2_stream>(larger);
}

TEST_F(ExtendedTypes, mutable)
{
  mutablestruct_a a('b', 'c', 'd');
  validate<mutablestruct_a, mutablestruct_b, xcdr_v1_stream>(a);
  validate<mutablestruct_a, mutablestruct_b, xcdr_v2_stream>(a);

  mutablestruct_b b('c', 'd', 'e');
  validate<mutablestruct_b, mutablestruct_a, xcdr_v1_stream>(b);
  validate<mutablestruct_b, mutablestruct_a, xcdr_v2_stream>(b);

}
