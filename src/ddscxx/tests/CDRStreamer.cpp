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
#include "CdrDataModels.hpp"
#include "CdrDataModels_pragma.hpp"

using namespace org::eclipse::cyclonedds::core::cdr;
using namespace CDR_testing;

typedef std::vector<unsigned char> bytes;

/**
 * Fixture for the DataWriter tests
 */
class CDRStreamer : public ::testing::Test
{
public:

    bytes BS_basic_normal {
        0x00, 0x01, 0xE2, 0x40 /*basicstruct.l*/,
        'g' /*basicstruct.c*/,
        0x00, 0x00, 0x00 /*padding bytes (3)*/,
        0x00, 0x00, 0x00, 0x07 /*basicstruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*basicstruct.str.c_str*/,
        0x00, 0x00, 0x00, 0x00, 0x00 /*padding bytes (5)*/,
        0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*basicstruct.d*/};
    bytes BS_basic_key {'g'/*basicstruct.c*/};

    /*xcdrv2 is max aligned to 4 bytes*/
    bytes BS_xcdrv2_normal {
        0x00, 0x01, 0xE2, 0x40 /*basicstruct.l*/,
        'g' /*basicstruct.c*/,
        0x00, 0x00, 0x00 /*padding bytes (3)*/,
        0x00, 0x00, 0x00, 0x07 /*basicstruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*basicstruct.str.c_str*/,
        0x00 /*padding bytes (1)*/,
        0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*basicstruct.d*/};
    bytes AS_xcdr_v2_normal {
        0x00, 0x00, 0x00, 0x1C/*dheader*/,
        0x00, 0x01, 0xE2, 0x40 /*appendablestruct.l*/,
        'g' /*appendablestruct.c*/,
        0x00, 0x00, 0x00 /*padding bytes (3)*/,
        0x00, 0x00, 0x00, 0x07 /*appendablestruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*appendablestruct.str.c_str*/,
        0x00 /*padding bytes (1)*/,
        0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*appendablestruct.d*/};

    CDRStreamer()
    {
    }

    void SetUp() { }

    void TearDown() { }

};

template<typename T, typename S>
void VerifyWrite(const T& in, const bytes &out, S stream, bool as_key)
{
  bytes buffer;
  move(stream, in, as_key);
  ASSERT_EQ(stream.status(),0);
  buffer.resize(stream.position());
  stream.set_buffer(buffer.data(), buffer.size());
  write(stream, in, as_key);
  ASSERT_EQ(stream.status(),0);
  ASSERT_EQ(buffer, out);
}

template<typename T, typename S>
void VerifyRead(const bytes &in, const T& out, S stream, bool as_key)
{
  bytes incopy(in);
  T buffer;
  stream.set_buffer(incopy.data(), incopy.size());
  read(stream, buffer, as_key);
  ASSERT_EQ(stream.status(),0);

  if (as_key)
    ASSERT_EQ(buffer.c(), out.c());
  else
    ASSERT_EQ(buffer, out);
}

template<typename T, typename S>
void VerifyReadOneDeeper(const bytes &in, const T& out, S stream, bool as_key)
{
  bytes incopy(in);
  T buffer;

  stream.set_buffer(incopy.data(), incopy.size());
  read(stream, buffer, as_key);
  ASSERT_EQ(stream.status(),0);

  if (as_key) {
    ASSERT_EQ(buffer.c().size(), out.c().size());
    for (size_t i = 0; i < buffer.c().size() && i < out.c().size(); i++)
      ASSERT_EQ(buffer.c()[i].c(), out.c()[i].c());
  } else {
    ASSERT_EQ(buffer, out);
  }
}

#define read_test(test_struct, normal_bytes, key_bytes, streamer)\
VerifyRead(normal_bytes, test_struct, streamer, false);\
VerifyRead(key_bytes, test_struct, streamer, true);

#define read_deeper_test(test_struct, normal_bytes, key_bytes, streamer)\
VerifyRead(normal_bytes, test_struct, streamer, false);\
VerifyReadOneDeeper(key_bytes, test_struct, streamer, true);

#define write_test(test_struct, normal_bytes, key_bytes, streamer)\
VerifyWrite(test_struct, normal_bytes, streamer, false);\
VerifyWrite(test_struct, key_bytes, streamer, true);

#define readwrite_test(test_struct, normal_bytes, key_bytes, streamer)\
read_test(test_struct, normal_bytes, key_bytes, streamer)\
write_test(test_struct, normal_bytes, key_bytes, streamer)

#define readwrite_deeper_test(test_struct, normal_bytes, key_bytes, streamer)\
read_deeper_test(test_struct, normal_bytes, key_bytes, streamer)\
write_test(test_struct, normal_bytes, key_bytes, streamer)

#define stream_test(test_struct, cdr_normal_bytes, xcdr_v1_normal_bytes, xcdr_v2_normal_bytes, key_bytes)\
readwrite_test(test_struct, cdr_normal_bytes, key_bytes, basic_cdr_stream(endianness::big_endian))\
readwrite_test(test_struct, xcdr_v1_normal_bytes, key_bytes, xcdr_v1_stream(endianness::big_endian))\
readwrite_test(test_struct, xcdr_v2_normal_bytes, key_bytes, xcdr_v2_stream(endianness::big_endian))

#define stream_deeper_test(test_struct, cdr_normal_bytes, xcdr_v1_normal_bytes, xcdr_v2_normal_bytes, key_bytes)\
readwrite_deeper_test(test_struct, cdr_normal_bytes, key_bytes, basic_cdr_stream(endianness::big_endian))\
readwrite_deeper_test(test_struct, xcdr_v1_normal_bytes, key_bytes, xcdr_v1_stream(endianness::big_endian))\
readwrite_deeper_test(test_struct, xcdr_v2_normal_bytes, key_bytes, xcdr_v2_stream(endianness::big_endian))

/*verifying streamer will not read/write beyond the end of the indicated buffer*/

TEST_F(CDRStreamer, cdr_boundary)
{
  basicstruct BS(123456, 'g', "abcdef", 654.321);
  /*this struct should be 4 + 1 + 3 + 4 + 7 + 5 + 8 = 32 bytes long, in basic cdr serialization*/
  basicstruct BS2;

  std::vector<char> buffer(32,0x0);

  basic_cdr_stream str;
  str.set_buffer(buffer.data(), 12);
  write(str, BS, false); /*this write should fail, as the buffer limit is too small*/

  ASSERT_EQ(str.status(), serialization_status::buffer_size_exceeded);
  ASSERT_TRUE(str.abort_status());

  str.reset_position();
  read(str, BS2, false); /*this read should fail too, as the buffer limit is too small*/

  ASSERT_EQ(str.status(), serialization_status::buffer_size_exceeded);
  ASSERT_TRUE(str.abort_status());

  str.set_buffer(buffer.data(), 32);
  write(str, BS, false); /*this write should finish, as the buffer limit is set as "unlimited"*/

  ASSERT_EQ(str.status(), 0x0);
  ASSERT_FALSE(str.abort_status());

  str.reset_position();
  read(str, BS2, false); /*this write should finish, as the buffer limit is set as "unlimited"*/

  ASSERT_EQ(str.status(), 0x0);
  ASSERT_FALSE(str.abort_status());
  ASSERT_EQ(BS, BS2);
}

/*verifying reads/writes of a basic struct*/

TEST_F(CDRStreamer, cdr_basic)
{
  basicstruct BS(123456, 'g', "abcdef", 654.321);

  stream_test(BS, BS_basic_normal, BS_basic_normal, BS_xcdrv2_normal, BS_basic_key)
}

/*verifying reads/writes of an appendable struct*/

TEST_F(CDRStreamer, cdr_appendable)
{
  appendablestruct AS(123456, 'g', "abcdef", 654.321);

  stream_test(AS, BS_basic_normal, BS_basic_normal, AS_xcdr_v2_normal, BS_basic_key)
}

/*verifying reads/writes of a mutable struct*/

TEST_F(CDRStreamer, cdr_mutable)
{
  mutablestruct MS(123456, 'g', "abcdef", 654.321);

  bytes MS_xcdr_v1_normal {
      0x00, 0x07, 0x00, 0x04 /*mutablestruct.l.mheader*/,
      0x00, 0x01, 0xE2, 0x40 /*mutablestruct.l*/,
      0x40, 0x05, 0x00, 0x01 /*mutablestruct.c.mheader*/,
      'g' /*mutablestruct.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x01, 0x00, 0x08 /*mutablestruct.str.mheader (pid_list_extended + length = 8)*/,
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0B /*mutablestruct.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x07 /*mutablestruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*mutablestruct.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x00, 0x01, 0x00, 0x0C /*mutablestruct.d.mheader*/,
      0x00, 0x00, 0x00, 0x00 /*padding bytes (4)*/,
      0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*mutablestruct.d*/,
      0x7F, 0x02, 0x00, 0x00 /*mutablestruct list termination header*/
      };
  bytes MS_xcdr_v1_normal_reordered {
      0x00, 0x01, 0x00, 0x0C /*mutablestruct.d.mheader*/,
      0x00, 0x00, 0x00, 0x00 /*padding bytes (4)*/,
      0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*mutablestruct.d*/,
      0x7F, 0x01, 0x00, 0x08 /*mutablestruct.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0B /*mutablestruct.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x07 /*mutablestruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*mutablestruct.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x40, 0x05, 0x00, 0x01 /*mutablestruct.c.mheader*/,
      'g' /*mutablestruct.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x07, 0x00, 0x04 /*mutablestruct.l.mheader*/,
      0x00, 0x01, 0xE2, 0x40 /*mutablestruct.l*/,
      0x7F, 0x02, 0x00, 0x00 /*mutablestruct list termination header*/
      };
  bytes MS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x30 /*dheader*/,
      0x20, 0x00, 0x00, 0x07 /*mutablestruct.l.emheader*/,
      0x00, 0x01, 0xE2, 0x40 /*mutablestruct.l*/,
      0x80, 0x00, 0x00, 0x05 /*mutablestruct.c.emheader*/,
      'g' /*mutablestruct.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x40, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0B /*mutablestruct.str.emheader*/,
      0x00, 0x00, 0x00, 0x07 /*mutablestruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*mutablestruct.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x30, 0x00, 0x00, 0x01 /*mutablestruct.d.emheader*/,
      0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*mutablestruct.d*/};
  bytes MS_xcdr_v2_normal_reordered {
      0x00, 0x00, 0x00, 0x30 /*dheader*/,
      0x30, 0x00, 0x00, 0x01 /*mutablestruct.d.emheader*/,
      0x40, 0x84, 0x72, 0x91, 0x68, 0x72, 0xB0, 0x21 /*mutablestruct.d*/,
      0x40, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0B /*mutablestruct.str.emheader*/,
      0x00, 0x00, 0x00, 0x07 /*mutablestruct.str.length*/, 'a', 'b', 'c', 'd', 'e', 'f', '\0' /*mutablestruct.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x80, 0x00, 0x00, 0x05 /*mutablestruct.c.emheader*/,
      'g' /*mutablestruct.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x20, 0x00, 0x00, 0x07 /*mutablestruct.l.emheader*/,
      0x00, 0x01, 0xE2, 0x40 /*mutablestruct.l*/};

  stream_test(MS, BS_basic_normal, MS_xcdr_v1_normal, MS_xcdr_v2_normal, BS_basic_key)
  VerifyRead(MS_xcdr_v1_normal_reordered, MS, xcdr_v1_stream(endianness::big_endian), false);
  VerifyRead(MS_xcdr_v2_normal_reordered, MS, xcdr_v2_stream(endianness::big_endian), false);
}

/*verifying reads/writes of a nested struct*/

TEST_F(CDRStreamer, cdr_nested)
{
  outer NS(inner('a',123), inner('b', 456), inner('c', 789));

  bytes NS_basic_normal {
      'a' /*outer.a.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x7B /*outer.a.l_inner*/,
      'b' /*outer.b.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x01, 0xC8 /*outer.b.l_inner*/,
      'c' /*outer.c.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x03, 0x15 /*outer.c.l_inner*/};
  bytes NS_basic_key {
      'c' /*outer.c.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x03, 0x15 /*outer.c.l_inner*/};
  bytes NS_xcdr_v1_normal {
      0x7F, 0x01, 0x00, 0x08 /*outer.a.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14 /*outer.a.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x01 /*outer.a.c_inner.mheader*/,
      'a' /*outer.a.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x01, 0x00, 0x04 /*outer.a.l_inner.mheader*/,
      0x00, 0x00, 0x00, 0x7B /*outer.a.l_inner*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*outer.b.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x14 /*outer.b.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x01 /*outer.b.c_inner.mheader*/,
      'b' /*outer.b.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x01, 0x00, 0x04 /*outer.b.l_inner.mheader*/,
      0x00, 0x00, 0x01, 0xC8 /*outer.b.l_inner*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*outer.c.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14 /*outer.c.mheader (extended)*/,
      0x40, 0x00, 0x00, 0x01 /*outer.c.c_inner.mheader*/,
      'c' /*outer.c.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x40, 0x01, 0x00, 0x04 /*outer.c.l_inner.mheader*/,
      0x00, 0x00, 0x03, 0x15 /*outer.c.l_inner*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x02, 0x00, 0x00 /*outer list termination header*/};
  bytes NS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x54 /*outer.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*outer.a.emheader*/,
      0x00, 0x00, 0x00, 0x14 /*outer.a.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x10 /*outer.a.dheader*/,
      0x00, 0x00, 0x00, 0x00 /*outer.a.c_inner.emheader*/,
      'a' /*outer.a.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x20, 0x00, 0x00, 0x01 /*outer.a.l_inner.emheader*/,
      0x00, 0x00, 0x00, 0x7B /*outer.a.l_inner*/,
      0x40, 0x00, 0x00, 0x01 /*outer.b.emheader*/,
      0x00, 0x00, 0x00, 0x14 /*outer.b.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x10 /*outer.b.dheader*/,
      0x00, 0x00, 0x00, 0x00 /*outer.b.c_inner.emheader*/,
      'b' /*outer.b.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x20, 0x00, 0x00, 0x01 /*outer.b.l_inner.emheader*/,
      0x00, 0x00, 0x01, 0xC8 /*outer.b.l_inner*/,
      0xC0, 0x00, 0x00, 0x02 /*outer.c.emheader*/,
      0x00, 0x00, 0x00, 0x14 /*outer.c.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x10 /*outer.c.dheader*/,
      0x80, 0x00, 0x00, 0x00 /*outer.c.c_inner.emheader*/,
      'c' /*outer.c.c_inner*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0xA0, 0x00, 0x00, 0x01 /*outer.c.l_inner.emheader*/,
      0x00, 0x00, 0x03, 0x15 /*outer.c.l_inner*/};

  stream_test(NS, NS_basic_normal, NS_xcdr_v1_normal, NS_xcdr_v2_normal, NS_basic_key)
}

/*verifying reads/writes of a struct containing inheritance*/

TEST_F(CDRStreamer, cdr_inherited)
{
  derived DS("gfedcb", 'a');
  DS.str("hjklmn");
  DS.c('o');

  bytes DS_basic_normal {
      0x00, 0x00, 0x00, 0x07 /*derived::base.str.length*/, 'h', 'j', 'k', 'l', 'm', 'n', '\0' /*derived::base.str.c_str*/,
      'o'/*derived::base.c*/,
      0x00, 0x00, 0x00, 0x07 /*derived.str_d.length*/, 'g', 'f', 'e', 'd', 'c', 'b', '\0'/*derived.str_d.c_str*/,
      'a'/*derived.c_d*/
      };
  bytes DS_basic_key {
      'o'/*derived::base.c*/
      };
  bytes DS_xcdr_v1_normal {
      0x7F, 0x01, 0x00, 0x08 /*derived::base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B /*derived::base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x07 /*derived::base.str.length*/, 'h', 'j', 'k', 'l', 'm', 'n', '\0' /*derived::base.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x40, 0x01, 0x00, 0x01 /*derived::base.c.mheader*/,
      'o'/*derived::base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x01, 0x00, 0x08 /*derived.str_d.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x7B, 0x00, 0x00, 0x00, 0x0B /*derived.str_d.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x07 /*derived.str_d.length*/, 'g', 'f', 'e', 'd', 'c', 'b', '\0'/*derived.str_d.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x00, 0xEA, 0x00, 0x01 /*derived.c_d.mheader*/,
      'a'/*derived.c_d*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/
      };
  bytes DS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x35 /*derived.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*derived::base.str.emheader*/,
      0x00, 0x00, 0x00, 0x0B /*derived::base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x07 /*derived::base.str.length*/, 'h', 'j', 'k', 'l', 'm', 'n', '\0' /*derived::base.str.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x80, 0x00, 0x00, 0x01 /*derived::base.c.emheader*/,
      'o'/*derived::base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x40, 0x00, 0x00, 0x7B /*derived.str_d.emheader*/,
      0x00, 0x00, 0x00, 0x0B /*derived.str_d.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x07 /*derived.str_d.length*/, 'g', 'f', 'e', 'd', 'c', 'b', '\0'/*derived.str_d.c_str*/,
      0x00 /*padding bytes (1)*/,
      0x00, 0x00, 0x00, 0xEA /*derived.c_d.emheader*/,
      'a' /*derived.c_d*/
      };

  stream_test(DS, DS_basic_normal, DS_xcdr_v1_normal, DS_xcdr_v2_normal, DS_basic_key)
}

/*verifying reads/writes of a struct containing sequences*/

TEST_F(CDRStreamer, cdr_sequence)
{
  sequence_struct SS({'z','y','x'}, {4,3,2,1});

  bytes SS_basic_normal {
      0x00, 0x00, 0x00, 0x03/*sequence_struct.c.length*/, 'z', 'y', 'x'/*sequence_struct.c.data*/,
      0x00 /*padding bytes (1)*/,
      0x00, 0x00, 0x00, 0x04/*sequence_struct.l.length*/, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01/*sequence_struct.l.data*/
      };
  bytes SS_basic_key {
      0x00, 0x00, 0x00, 0x03/*sequence_struct.c.length*/, 'z', 'y', 'x'/*sequence_struct.c.data*/
      };
  bytes SS_xcdr_v1_normal {
      0x7F, 0x01, 0x00, 0x08 /*sequence_struct.c.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07 /*sequence_struct.c.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x03/*sequence_struct.c.length*/, 'z', 'y', 'x'/*sequence_struct.c.data*/,
      0x00 /*padding bytes (1)*/,
      0x7F, 0x01, 0x00, 0x08 /*sequence_struct.l.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x14 /*sequence_struct.l.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04/*sequence_struct.l.length*/, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01/*sequence_struct.l.data*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/
      };
  bytes SS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x2C/*sequence_struct.dheader*/,
      0xC0, 0x00, 0x00, 0x00 /*sequence_struct.c.emheader*/,
      0x00, 0x00, 0x00, 0x07 /*sequence_struct.c.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x03/*sequence_struct.c.length*/, 'z', 'y', 'x'/*sequence_struct.c.data*/,
      0x00 /*padding bytes (1)*/,
      0x40, 0x00, 0x00, 0x01 /*derived.l.emheader*/,
      0x00, 0x00, 0x00, 0x14 /*derived.l.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04/*sequence_struct.l.length*/, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01/*sequence_struct.l.data*/
      };
  /*different length code, overlapping nextint with the length of the sequence
    our streamer implementation does not write this way, but it must be able to
    read it*/
  bytes SS_xcdr_v2_normal_lc_not_4 {
      0x00, 0x00, 0x00, 0x24/*sequence_struct.dheader*/,
      0xD0, 0x00, 0x00, 0x00 /*derived.c_d.emheader*/, /*lc = 5: length = sequence_struct.c.length*1*/
      0x00, 0x00, 0x00, 0x03/*sequence_struct.c.length*/, 'z', 'y', 'x'/*sequence_struct.c.data*/,
      0x00 /*padding bytes (1)*/,
      0x60, 0x00, 0x00, 0x01 /*derived.l.emheader*/, /*lc = 6: length = sequence_struct.c.length*4*/
      0x00, 0x00, 0x00, 0x04/*sequence_struct.l.length*/, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01/*sequence_struct.l.data*/
      };

  stream_test(SS, SS_basic_normal, SS_xcdr_v1_normal, SS_xcdr_v2_normal, SS_basic_key)

  read_test(SS, SS_xcdr_v2_normal_lc_not_4, SS_basic_key, xcdr_v2_stream(endianness::big_endian))
}

/*verifying reads/writes of a struct containing arrays*/

TEST_F(CDRStreamer, cdr_array)
{
  array_struct ARS({'e','d','c','b','a'},{123,234,345,456,567});

  bytes ARS_normal {
      'e', 'd', 'c', 'b', 'a'/*array_struct.c*/,
      0x00, 0x00, 0x00 /*padding bytes*/,
      0x00, 0x00, 0x00, 0x7B,
      0x00, 0x00, 0x00, 0xEA,
      0x00, 0x00, 0x01, 0x59,
      0x00, 0x00, 0x01, 0xC8,
      0x00, 0x00, 0x02, 0x37 /*array_struct.l*/,
      };
  bytes ARS_key {
      'e', 'd', 'c', 'b', 'a'/*array_struct.c*/
      };

  stream_test(ARS, ARS_normal, ARS_normal, ARS_normal, ARS_key)
}

/*verifying reads/writes of a struct containing typedefs*/

TEST_F(CDRStreamer, cdr_typedef)
{
  typedef_struct TDS({base("qwe",'a'),base("wer",'b'),base("ert",'c'),base("rty",'d')},{base("tyu",'e'),base("yui",'f'),base("uio",'g')});

  bytes TDS_basic_normal {
      0x00, 0x00, 0x00, 0x04/*typedef_struct.c.length*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'q', 'w', 'e', '\0' /*base.str.c_str*/,
      'a'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'w', 'e', 'r', '\0' /*base.str.c_str*/,
      'b'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'e', 'r', 't', '\0' /*base.str.c_str*/,
      'c'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'r', 't', 'y', '\0' /*base.str.c_str*/,
      'd'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x03/*typedef_struct.l.length*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 't', 'y', 'u', '\0' /*base.str.c_str*/,
      'e'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'y', 'u', 'i', '\0' /*base.str.c_str*/,
      'f'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x04/*base.str.length*/, 'u', 'i', 'o', '\0' /*base.str.c_str*/,
      'g'/*base.c*/
      };
  bytes TDS_basic_key {
      0x00, 0x00, 0x00, 0x04/*typedef_struct.c.length*/,
      'a'/*base.c*/,
      'b'/*base.c*/,
      'c'/*base.c*/,
      'd'/*base.c*/
      };
  bytes TDS_xcdr_v1_normal {
      0x7F, 0x01, 0x00, 0x08 /*typedef_struct.c.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84 /*typedef_struct.c.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04/*typedef_struct.c.length*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'q', 'w', 'e', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'a'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'w', 'e', 'r', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'b'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'e', 'r', 't', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'c'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'r', 't', 'y', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'd'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*typedef_struct.l.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x64 /*typedef_struct.l.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x03/*typedef_struct.l.length*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 't', 'y', 'u', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'e'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'y', 'u', 'i', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'f'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x01, 0x00, 0x08 /*base.str.mheader (pid_list_extended + must_understand + length = 8)*/,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 /*base.str.mheader (extended)*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'u', 'i', 'o', '\0' /*base.str.c_str*/,
      0x40, 0x01, 0x00, 0x01 /*base.c.mheader*/,
      'g'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*inner list termination header*/,
      0x7F, 0x02, 0x00, 0x00 /*list termination header*/
      };
  bytes TDS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0xD9 /*typedef_struct.dheader*/, /*8 + 113 + 3 + 8 + 85 = 217*/
      0xC0, 0x00, 0x00, 0x00 /*typedef_struct.c.emheader*/,
      0x00, 0x00, 0x00, 0x71 /*typedef_struct.c.emheader.nextint*/, /*4 + (21 + 4) * 4 + 3 * 3 = 113 = 0x71*/
      0x00, 0x00, 0x00, 0x04 /*typedef_struct.c.length*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'q', 'w', 'e', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'a'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'w', 'e', 'r', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'b'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'e', 'r', 't', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'c'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'r', 't', 'y', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'd'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x40, 0x00, 0x00, 0x01 /*typedef_struct.l.emheader*/,
      0x00, 0x00, 0x00, 0x55 /*typedef_struct.l.emheader.nextint*/, /*4 + (21 + 4) * 3 + 2 * 3 = 85 = 0x55*/
      0x00, 0x00, 0x00, 0x03 /*typedef_struct.l.length*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 't', 'y', 'u', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'e'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'y', 'u', 'i', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'f'/*base.c*/,
      0x00, 0x00, 0x00 /*padding bytes (3)*/,
      0x00, 0x00, 0x00, 0x15 /*base.dheader*/,
      0x40, 0x00, 0x00, 0x00 /*base.str.emheader*/,
      0x00, 0x00, 0x00, 0x08 /*base.str.emheader.nextint*/,
      0x00, 0x00, 0x00, 0x04 /*base.str.length*/, 'u', 'i', 'o', '\0' /*base.str.c_str*/,
      0x80, 0x00, 0x00, 0x01 /*base.c.emheader*/,
      'g'/*base.c*/,
      };

  stream_deeper_test(TDS, TDS_basic_normal, TDS_xcdr_v1_normal, TDS_xcdr_v2_normal, TDS_basic_key)
}

/*verifying reads/writes of a struct containing unions*/

TEST_F(CDRStreamer, cdr_union)
{
  un _c;
  _c.c('b','a');
  union_struct US(_c);

  un_k _c_k;
  _c_k.c('b','a');
  union_struct_k US_k(_c_k);

  _c_k.c('\0','a');
  union_struct_k US_k_read(_c_k);


  bytes US_normal {
      'a'/*union_struct.c.switch*/,
      'b'/*union_struct.c.c*/
      };
  bytes US_k_key {
      'a'/*union_struct.c.switch*/
      };

  stream_test(US, US_normal, US_normal, US_normal, US_normal)

  VerifyRead(US_normal, US_k, basic_cdr_stream(endianness::big_endian), false);
  VerifyRead(US_normal, US_k, xcdr_v1_stream(endianness::big_endian), false);
  VerifyRead(US_normal, US_k, xcdr_v2_stream(endianness::big_endian), false);

  VerifyRead(US_k_key, US_k_read, basic_cdr_stream(endianness::big_endian), true);
  VerifyRead(US_k_key, US_k_read, xcdr_v1_stream(endianness::big_endian), true);
  VerifyRead(US_k_key, US_k_read, xcdr_v2_stream(endianness::big_endian), true);

  write_test(US_k, US_normal, US_k_key, basic_cdr_stream(endianness::big_endian))
  write_test(US_k, US_normal, US_k_key, xcdr_v1_stream(endianness::big_endian))
  write_test(US_k, US_normal, US_k_key, xcdr_v2_stream(endianness::big_endian))
}

/*verifying reads/writes of structs using pragma keylist*/

TEST_F(CDRStreamer, cdr_pragma)
{
  pragma_keys PS(sub_2(sub_1(123,234),sub_1(345,456)),sub_2(sub_1(567,678),sub_1(789,890))),
              PS_key_test(sub_2(sub_1(0,234),sub_1(0,456)),sub_2(sub_1(0,678),sub_1(0,890)));

  bytes PS_basic_normal {
      0x00, 0x00, 0x00, 0x7B/*pragma_keys.c.s_1.l_1*/,
      0x00, 0x00, 0x00, 0xEA/*pragma_keys.c.s_1.l_2*/,
      0x00, 0x00, 0x01, 0x59/*pragma_keys.c.s_2.l_1*/,
      0x00, 0x00, 0x01, 0xC8/*pragma_keys.c.s_2.l_2*/,
      0x00, 0x00, 0x02, 0x37/*pragma_keys.d.s_1.l_1*/,
      0x00, 0x00, 0x02, 0xA6/*pragma_keys.d.s_1.l_2*/,
      0x00, 0x00, 0x03, 0x15/*pragma_keys.d.s_2.l_1*/,
      0x00, 0x00, 0x03, 0x7A/*pragma_keys.d.s_2.l_2*/
      };
  bytes PS_basic_key {
      0x00, 0x00, 0x00, 0xEA/*pragma_keys.c.s_1.l_2*/,
      0x00, 0x00, 0x01, 0xC8/*pragma_keys.c.s_2.l_2*/,
      0x00, 0x00, 0x02, 0xA6/*pragma_keys.d.s_1.l_2*/,
      0x00, 0x00, 0x03, 0x7A/*pragma_keys.d.s_2.l_2*/
      };

  VerifyRead(PS_basic_normal, PS, basic_cdr_stream(endianness::big_endian), false);
  VerifyRead(PS_basic_normal, PS, xcdr_v1_stream(endianness::big_endian), false);
  VerifyRead(PS_basic_normal, PS, xcdr_v2_stream(endianness::big_endian), false);

  VerifyRead(PS_basic_key, PS_key_test, basic_cdr_stream(endianness::big_endian), true);
  VerifyRead(PS_basic_key, PS_key_test, xcdr_v1_stream(endianness::big_endian), true);
  VerifyRead(PS_basic_key, PS_key_test, xcdr_v2_stream(endianness::big_endian), true);

  write_test(PS, PS_basic_normal, PS_basic_key, basic_cdr_stream(endianness::big_endian))
  write_test(PS, PS_basic_normal, PS_basic_key, xcdr_v1_stream(endianness::big_endian))
  write_test(PS, PS_basic_normal, PS_basic_key, xcdr_v2_stream(endianness::big_endian))
}

/*verifying reads/writes of a struct containing enums*/

TEST_F(CDRStreamer, cdr_enum)
{
  enum_struct ES(enum_8::second_8, enum_16::third_16, enum_32::fourth_32);

  /*basic cdr treats all enums as 32 bit integers*/
  bytes ES_basic_normal {
      0x00, 0x00, 0x00 ,0x01 /*enum_struct.c*/,
      0x00, 0x00, 0x00, 0x02 /*enum_struct.b*/,
      0x00, 0x00, 0x00, 0x03 /*enum_struct.a*/
      };
  bytes ES_basic_key {
      0x00, 0x00, 0x00, 0x01 /*enum_struct.c*/
      };
  /*xcdr_v1 and xcdr_v2 treat bitbounded enums in the same manner*/
  bytes ES_xcdr_v1_normal {
      0x01 /*enum_struct.c*/,
      0x00 /*padding bytes (1)*/,
      0x00, 0x02 /*enum_struct.b*/,
      0x00, 0x00, 0x00, 0x03 /*enum_struct.a*/
      };

  stream_test(ES, ES_basic_normal, ES_xcdr_v1_normal, ES_xcdr_v1_normal, ES_basic_key)
}

/*verifying reads/writes of structs containing optional fields*/

TEST_F(CDRStreamer, cdr_optional)
{
  optional_final_struct OFS(std::nullopt, 'b', 'c');
  optional_appendable_struct OAS(std::nullopt, 'b', 'c');
  optional_mutable_struct OMS(std::nullopt, 'b', 'c');

  /*no basic cdr, since it does not support optional fields*/
  bytes OFS_xcdr_v1_normal {
      0x00, 0x00, 0x00, 0x00 /*optional_final_struct.a.mheader*/,
      'b'/*optional_final_struct.a*/,
      'c'/*optional_final_struct.c*/
      };
  bytes OFS_key {
      'c'/*optional_final_struct.c*/
      };
  bytes OMS_xcdr_v1_normal {
      0x00, 0x01, 0x00, 0x01 /*optional_mutable_struct.b.mheader*/,
      'b'/*optional_mutable_struct.b*/,
      0x00, 0x00, 0x00/*padding bytes (3)*/,
      0x40, 0x02, 0x00, 0x01 /*optional_mutable_struct.c.mheader*/,
      'c'/*optional_final_struct.c*/,
      0x00, 0x00, 0x00/*padding bytes (3)*/,
      0x7F, 0x02, 0x00, 0x00 /*optional_mutable_struct list termination header*/
      };
  bytes OFS_xcdr_v2_normal {
      0x00/*optional_final_struct.a.is_present*/,
      'b'/*optional_final_struct.b*/,
      'c'/*optional_final_struct.c*/
      };
    bytes OAS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x03/*dheader*/,
      0x00/*optional_appendable_struct.a.is_present*/,
      'b'/*optional_appendable_struct.b*/,
      'c'/*optional_appendable_struct.c*/
      };
    bytes OMS_xcdr_v2_normal {
      0x00, 0x00, 0x00, 0x0D/*dheader*/,
      0x00, 0x00, 0x00, 0x01/*optional_mutable_struct.b.emheader*/,
      'b'/*optional_appendable_struct.b*/,
      0x00, 0x00, 0x00/*padding bytes (3)*/,
      0x80, 0x00, 0x00, 0x02/*optional_mutable_struct.c.emheader*/,
      'c'/*optional_appendable_struct.c*/
      };

  /* basic cdr does not support optional fields,
     therefore the streamer should enter error status
     when the streamer is asked to write them */
  bytes in_bytes {'a', 'b', 'c'};
  optional_final_struct out_struct;
  basic_cdr_stream b(endianness::big_endian);
  b.set_buffer(in_bytes.data(), in_bytes.size());
  read(b, out_struct, false);

  ASSERT_EQ(b.status(), uint64_t(serialization_status::unsupported_property));

  bytes out_bytes(3, 0);
  b.set_buffer(out_bytes.data(), out_bytes.size());
  write(b, OFS, false);

  ASSERT_EQ(b.status(), uint64_t(serialization_status::unsupported_property));

  readwrite_test(OFS, OFS_xcdr_v1_normal, OFS_key, xcdr_v1_stream(endianness::big_endian))
  readwrite_test(OAS, OFS_xcdr_v1_normal, OFS_key, xcdr_v1_stream(endianness::big_endian))
  readwrite_test(OMS, OMS_xcdr_v1_normal, OFS_key, xcdr_v1_stream(endianness::big_endian))

  readwrite_test(OFS, OFS_xcdr_v2_normal, OFS_key, xcdr_v2_stream(endianness::big_endian))
  readwrite_test(OAS, OAS_xcdr_v2_normal, OFS_key, xcdr_v2_stream(endianness::big_endian))
  readwrite_test(OMS, OMS_xcdr_v2_normal, OFS_key, xcdr_v2_stream(endianness::big_endian))
}
