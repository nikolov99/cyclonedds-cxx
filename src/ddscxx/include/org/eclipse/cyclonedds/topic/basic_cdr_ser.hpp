/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef BASIC_CDR_SERIALIZATION_HPP_
#define BASIC_CDR_SERIALIZATION_HPP_

#include "cdr_stream.hpp"
#include <org/eclipse/cyclonedds/core/type_helpers.hpp>
#include <stdexcept>
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

class basic_cdr_stream : public cdr_stream {
public:
  basic_cdr_stream(endianness end = native_endianness()) : cdr_stream(end, 8) { ; }

  //primitive functions

  template<typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void read_primitive(T& toread) {
    align(sizeof(T), false);

    transfer_and_swap(
      *(reinterpret_cast<T*>(m_buffer + m_position)),
      toread,
      m_stream_endianness != m_local_endianness);
  }

  template<typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void write_primitive(const T& towrite) {
    align(sizeof(T), true);

    transfer_and_swap(
      towrite,
      *(reinterpret_cast<T*>(m_buffer + m_position)),
      m_stream_endianness != m_local_endianness);
  }

  template<typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void incr_primitive(const T& toincr) {
    (void)toincr;

    align(sizeof(T), false);

    m_position += sizeof(T);
  }

  template<typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void max_size_primitive(const T& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    incr_primitive(max_sz);
  }

  //length functions

  void write_length(size_t length, size_t bound) {
    if (bound &&
      length > bound)
      throw 1;  //replace throw

    write_primitive(static_cast<uint32_t>(length));
  }

  void read_length(uint32_t& length) {
    read_primitive(length);
  }

  void incr_length(size_t length, size_t bound) {
    if (bound &&
      length > bound)
      throw 1;  //replace throw

    incr_primitive(length);
  }

  template<typename T, size_t N>
  void read_vec_resize(idl_bounded_sequence<T,N>& toread, uint32_t& seq_length) {
    read_length(seq_length);

    auto read_length = std::min<size_t>(seq_length, N ? N : SIZE_MAX);

    toread.resize(read_length);
  }

  //string functions
  template<size_t N>
  void read_string(idl_bounded_string<N>& toread) {
    uint32_t string_length = 0;

    read_length(string_length);

    toread.assign(m_buffer + m_position, m_buffer + m_position + std::min<size_t>(string_length - 1, N ? N : SIZE_MAX));  //remove 1 for terminating NULL

    m_position += string_length;
  }

  template<size_t N>
  void write_string(const idl_bounded_string<N>& towrite) {
    size_t string_length = towrite.length() + 1;  //add 1 extra for terminating NULL

    write_length(string_length, N);

    //no check on string length necessary after this since it is already checked in write_length

    memcpy(m_buffer + m_position, towrite.c_str(), string_length);

    m_position += string_length;
  }

  template<size_t N>
  void incr_string(const idl_bounded_string<N>& toincr) {
    size_t string_length = toincr.length() + 1;  //add 1 extra for terminating NULL

    incr_length(string_length, N);

    //no check on string length necessary after this since it is already checked in incr_length

    m_position += string_length;
  }

  template<size_t N>
  void max_size_string(const idl_bounded_string<N>& max_sz) {
    (void)max_sz;

    if (m_position == SIZE_MAX)
      return;

    if (N == 0) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    m_position += N + 1;  //add 1 extra for terminating NULL
  }

  //array functions

  //T is a primitive
  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void read_array(idl_array<T, N>& toread) {
    align(sizeof(T), false);

    memcpy(toread.data(), m_buffer + m_position, sizeof(T) * N);

    if (m_stream_endianness != m_local_endianness &&
      sizeof(T) > 1)
      for (size_t i = 0; i < N; i++)
        byte_swap(toread[i]);

    m_position += N * sizeof(T);
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void write_array(const idl_array<T, N>& towrite) {
    align(sizeof(T), true);

    memcpy(m_buffer + m_position, towrite.data(), sizeof(T) * N);

    if (m_stream_endianness != m_local_endianness &&
      sizeof(T) > 1)
      for (size_t i = 0; i < N; i++)
        byte_swap(*(reinterpret_cast<T*>(m_buffer + m_position + i * sizeof(T))));

    m_position += N * sizeof(T);
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void incr_array(const idl_array<T, N>& toincr) {
    (void)toincr;

    align(sizeof(T), false);

    m_position += N * sizeof(T);
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void max_size_array(const idl_array<T, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    incr_array(max_sz);
  }

  //T is an enum
  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void read_array(idl_array<T, N>& toread) {
    for (auto& e : toread)
      read_enum(e);
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void write_array(const idl_array<T, N>& towrite) {
    for (const auto& e : towrite)
      write_enum(e);
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void incr_array(const idl_array<T, N>& toincr) {
    for (const auto& e : toincr)
      incr_enum(e);
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void max_size_array(const idl_array<T, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    incr_array(max_sz);
  }

  //T is an IDL struct
  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void read_array(idl_array<T, N>& toread) {
    for (auto& e : toread)
      read(e,*this);
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void write_array(const idl_array<T, N>& towrite) {
    for (const auto& e : towrite)
      write(e,*this);
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void incr_array(const idl_array<T, N>& toincr) {
    for (const auto& e : toincr)
      write_size(e,*this);
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void max_size_array(const idl_array<T, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    for (const auto& e : max_sz)
      write_size_max(e,*this);
  }

  //T is a string
  template<size_t N, size_t M>
  void read_array(idl_array<idl_bounded_string<M>, N>& toread) {
    for (auto& e : toread)
      read_string(e);
  }

  template<size_t N, size_t M>
  void write_array(const idl_array<idl_bounded_string<M>, N>& towrite) {
    for (const auto& e : towrite)
      write_string(e);
  }

  template<size_t N, size_t M>
  void incr_array(const idl_array<idl_bounded_string<M>, N>& toincr) {
    for (const auto& e : toincr)
      incr_string(e);
  }

  template<size_t N, size_t M>
  void max_size_array(const idl_array<idl_bounded_string<M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    for (const auto& e : max_sz)
      max_size_string(e);
  }

  //T is an array
  template<typename T, size_t N, size_t M>
  void read_array(idl_array<idl_array<T, M>, N>& toread) {
    for (auto& e : toread)
      read_array(e);
  }

  template<typename T, size_t N, size_t M>
  void write_array(const idl_array<idl_array<T, M>, N>& towrite) {
    for (const auto& e : towrite)
      write_array(e);
  }

  template<typename T, size_t N, size_t M>
  void incr_array(const idl_array<idl_array<T, M>, N>& toincr) {
    for (const auto& e : toincr)
      incr_array(e);
  }

  template<typename T, size_t N, size_t M>
  void max_size_array(const idl_array<idl_array<T, M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    for (const auto& e : max_sz)
      max_size_array(e);
  }

  //T is a vector
  template<typename T, size_t N, size_t M>
  void read_array(idl_array<idl_bounded_sequence<T,M>, N>& toread) {
    for (const auto& e : toread)
      read_sequence(e);
  }

  template<typename T, size_t N, size_t M>
  void write_array(const idl_array<idl_bounded_sequence<T,M>, N>& towrite) {
    for (const auto& e : towrite)
      write_sequence(e);
  }

  template<typename T, size_t N, size_t M>
  void incr_array(const idl_array<idl_bounded_sequence<T,M>, N>& toincr) {
    for (const auto& e : toincr)
      incr_sequence(e);
  }

  template<typename T, size_t N, size_t M>
  void max_size_array(const idl_array<idl_bounded_sequence<T,M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    for (const auto& e : max_sz)
      max_size_sequence(e);
  }

  //sequence functions

  //T is a primitive
  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void read_sequence(idl_bounded_sequence<T,N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    align(sizeof(T), false);

    memcpy(toread.data(), m_buffer + m_position, sizeof(T) * toread.size());

    if (m_stream_endianness != m_local_endianness &&
      sizeof(T) > 1)
      for (size_t i = 0; i < toread.size(); i++)
        byte_swap(toread[i]);

    m_position += sizeof(T) * seq_length;
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void write_sequence(const idl_bounded_sequence<T,N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    align(sizeof(T), false);

    memcpy(m_buffer + m_position, towrite.data(), sizeof(T) * towrite.size());

    if (m_stream_endianness != m_local_endianness &&
      sizeof(T) > 1)
      for (size_t i = 0; i < towrite.size(); i++)
        byte_swap(*(reinterpret_cast<T*>(m_buffer + m_position + i * sizeof(T))));

    m_position += sizeof(T) * towrite.size();
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void incr_sequence(const idl_bounded_sequence<T,N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    align(sizeof(T), false);

    m_position += sizeof(T) * toincr.size();
  }

  template<typename T, size_t N, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
  void max_size_sequence(const idl_bounded_sequence<T,N>& max_sz) {
    (void)max_sz;

    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    align(sizeof(T), false);

    m_position += sizeof(T) * N;
  }

  //T is an enum
  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void read_sequence(idl_bounded_sequence<T,N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (auto& e : toread)
      read_enum(e);

    //dummy reads to move the position indicator
    if (N &&
      seq_length > N) {
      T temp;
      for (size_t i = N; i < seq_length; i++)
        read_enum(temp);
    }
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void write_sequence(const idl_bounded_sequence<T,N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (const auto& e : towrite)
      write_enum(e);
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void incr_sequence(const idl_bounded_sequence<T,N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    for (const auto & e:toincr)
      incr_enum(e);
  }

  template<typename T, size_t N, std::enable_if_t<std::is_enum<T>::value, int> = 1>
  void max_size_sequence(const idl_bounded_sequence<T,N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    for (const auto& e : max_sz)
      max_size_enum(e);
  }

  //T is a bool
  template<size_t N>
  void read_sequence(idl_bounded_sequence<bool, N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (size_t i = 0; i < toread.size(); i++)
      toread[i] = (*(m_buffer + i) ? true : false);  //only 0x0 is false?

    m_position += seq_length + toread.size();
  }

  template<size_t N>
  void write_sequence(const idl_bounded_sequence<bool, N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (const auto& e : towrite)
      *(m_buffer + m_position++) = e ? 0x1 : 0x0;
  }

  //T is an IDL struct
  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void read_sequence(idl_bounded_sequence<T,N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (auto& e : toread)
      read(e,*this);

    //dummy reads to move the position indicator
    if (N &&
      seq_length > N) {
      T temp;
      for (size_t i = N; i < seq_length; i++)
        read(temp,*this);
    }
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void write_sequence(const idl_bounded_sequence<T,N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (auto& e : towrite)
      write(e,*this);
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void incr_sequence(const idl_bounded_sequence<T,N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    for (auto& e : toincr)
      write_size(e,*this);
  }

  template<typename T, size_t N, std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_enum<T>::value, int> = 0>
  void max_size_sequence(const idl_bounded_sequence<T,N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    for (auto& e : max_sz)
      write_size_max(e,*this);
  }

  //T is a string
  template<size_t N, size_t M>
  void read_sequence(idl_bounded_sequence<idl_bounded_string<M>, N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (auto& e : toread)
      read_string(e, 0);

    //dummy reads to move the position indicator
    if (N &&
      seq_length > N) {
      idl_bounded_string<M>  temp;
      for (size_t i = N; i < seq_length; i++)
        read_string(temp);
    }
  }

  template<size_t N, size_t M>
  void write_sequence(const idl_bounded_sequence<idl_bounded_string<M>, N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (auto& e : towrite)
      write_string(e);
  }

  template<size_t N, size_t M>
  void incr_sequence(const idl_bounded_sequence<idl_bounded_string<M>, N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    for (auto& e : toincr)
      incr_string(e);
  }

  template<size_t N, size_t M>
  void max_size_sequence(const idl_bounded_sequence<idl_bounded_string<M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    for (auto& e : max_sz)
      max_size_string(e);
  }

  //T is a vector
  template<typename T, size_t N, size_t M>
  void read_sequence(idl_bounded_sequence<idl_bounded_sequence<T, M>, N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (auto& e : toread)
      read_sequence(e);

    //dummy reads to move the position indicator
    if (N &&
      seq_length > N) {
      idl_bounded_sequence<T, M> temp;
      for (size_t i = N; i < seq_length; i++)
        read_sequence(temp);
    }
  }

  template<typename T, size_t N, size_t M>
  void write_sequence(const idl_bounded_sequence<idl_bounded_sequence<T, M>, N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (auto& e : towrite)
      write_sequence(e);
  }

  template<typename T, size_t N, size_t M>
  void incr_sequence(const idl_bounded_sequence<idl_bounded_sequence<T, M>, N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    for (auto& e : toincr)
      incr_sequence(e);
  }

  template<typename T, size_t N, size_t M>
  void max_size_sequence(const idl_bounded_sequence<idl_bounded_sequence<T, M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    for (auto& e : max_sz)
      max_size_sequence(e);
  }

  //T is an array
  template<typename T, size_t N, size_t M>
  void read_sequence(idl_bounded_sequence<idl_array<T, M>, N>& toread) {
    uint32_t seq_length = 0;  //this is the sequence length retrieved from the stream, not the number of entities to be written to the sequence object
    read_vec_resize(toread, seq_length);

    for (const auto& e : toread)
      read_array(e);

    //dummy reads to move the position indicator
    if (N &&
      seq_length > N) {
      idl_array<T, M> temp;
      for (size_t i = N; i < seq_length; i++)
        read_array(temp);
    }
  }

  //T is an array
  template<typename T, size_t N, size_t M>
  void write_sequence(const idl_bounded_sequence<idl_array<T, M>, N>& towrite) {
    write_length(towrite.size(), N);

    //no check on length necessary after this point, it is done in the write_length function

    for (const auto& e : towrite)
      write_array(e);
  }

  template<typename T, size_t N, size_t M>
  void incr_sequence(const idl_bounded_sequence<idl_array<T, M>, N>& toincr) {
    incr_length(toincr.size(), N);

    //no check on length necessary after this point, it is done in the incr_length function

    for (const auto& e : toincr)
      incr_array(e);
  }

  template<typename T, size_t N, size_t M>
  void max_size_sequence(const idl_bounded_sequence<idl_array<T, M>, N>& max_sz) {
    if (m_position == SIZE_MAX)
      return;

    if (0 == N) {
      m_position = SIZE_MAX;
      return;
    }

    max_size_primitive(uint32_t(0));

    for (const auto& e : max_sz)
      max_size_array(e);
  }

  //enum types

  template<typename T>
  void read_enum(T& toread) {
    (void)toread;
    uint32_t temp = 0;
    read_primitive(temp);
    toread = static_cast<T>(temp);
  }

  template<typename T>
  void write_enum(const T& towrite) {
    (void)towrite;
    write_primitive(static_cast<uint32_t>(towrite));
  }

  template<typename T>
  void incr_enum(const T& toincr) {
    (void)toincr;
    incr_primitive(uint32_t(0));
  }

  template<typename T>
  void max_size_enum(const T& max_sz) {
    (void)max_sz;
    max_size_primitive(uint32_t(0));
  }
};

#endif
