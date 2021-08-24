/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef CDR_STREAM_HPP_
#define CDR_STREAM_HPP_

#include "dds/ddsrt/endian.h"
#include <org/eclipse/cyclonedds/core/type_helpers.hpp>
#include <org/eclipse/cyclonedds/core/cdr/entity_properties.hpp>
#include <stdint.h>
#include <string>
#include <stdexcept>
#include <stack>
#include <cassert>
#include <dds/core/macros.hpp>

namespace org {
namespace eclipse {
namespace cyclonedds {
namespace core {
namespace cdr {

/**
 * @brief
 * Byte swapping function, is only enabled for arithmetic (base) types.
 *
 * Determines the number of bytes to swap by the size of the template parameter.
 *
 * @param[in, out] toswap The entity whose bytes will be swapped.
 */
template<typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
void byte_swap(T& toswap) {
    union { T a; uint16_t u2; uint32_t u4; uint64_t u8; } u;
    u.a = toswap;
    DDSCXX_WARNING_MSVC_OFF(6326)
    switch (sizeof(T)) {
    case 1:
        break;
    case 2:
        u.u2 = static_cast<uint16_t>((u.u2 & 0xFF00) >> 8)
             | static_cast<uint16_t>((u.u2 & 0x00FF) << 8);
        break;
    case 4:
        u.u4 = static_cast<uint32_t>((u.u4 & 0xFFFF0000) >> 16)
             | static_cast<uint32_t>((u.u4 & 0x0000FFFF) << 16);
        u.u4 = static_cast<uint32_t>((u.u4 & 0xFF00FF00) >> 8)
             | static_cast<uint32_t>((u.u4 & 0x00FF00FF) << 8);
        break;
    case 8:
        u.u8 = static_cast<uint64_t>((u.u8 & 0xFFFFFFFF00000000) >> 32)
             | static_cast<uint64_t>((u.u8 & 0x00000000FFFFFFFF) << 32);
        u.u8 = static_cast<uint64_t>((u.u8 & 0xFFFF0000FFFF0000) >> 16)
             | static_cast<uint64_t>((u.u8 & 0x0000FFFF0000FFFF) << 16);
        u.u8 = static_cast<uint64_t>((u.u8 & 0xFF00FF00FF00FF00) >> 8)
             | static_cast<uint64_t>((u.u8 & 0x00FF00FF00FF00FF) << 8);
        break;
    default:
        throw std::invalid_argument(std::string("attempted byteswap on variable of invalid size: ") + std::to_string(sizeof(T)));
    }
    DDSCXX_WARNING_MSVC_ON(6326)
    toswap = u.a;
}

/**
 * @brief
 * Endianness types.
 *
 * @enum endianness C++ implementation of cyclonedds's DDSRT_ENDIAN endianness defines
 *
 * @var endianness::little_endian Little endianness.
 * @var endianness::big_endian Big endianness.
 */
enum class endianness {
    little_endian = DDSRT_LITTLE_ENDIAN,
    big_endian = DDSRT_BIG_ENDIAN
};

/**
 * @brief
 * Returns the endianness of the local system.
 *
 * Takes the value from the DDSRT_ENDIAN definition and converts it to the c++ enum class value.
 *
 * @retval little_endian If the system is little endian.
 * @retval big_endian If the system is big endian.
 */
constexpr endianness native_endianness() { return endianness(DDSRT_ENDIAN); }


/**
 * @brief
 * Returns whether a byte swap is necessary for an incoming data set.
 *
 * @param[in] remote The remote (incoming) data endianness.
 *
 * @return Whether the local and remote datasets have the same endianness.
 */
inline bool swap_necessary(endianness remote) {return native_endianness() != remote;}

/**
 * @brief
 * Serialization status bitmasks.
 *
 * @enum serialization_status Describes the serialization status of a cdr stream.
 *
 * These are stored as an bitfields in an int in cdr streams, since more than one serialization fault can be encountered.
 *
 * @var serialization_status::move_bound_exceeded The serialization has encountered a field which has exceeded the bounds set for it.
 * @var serialization_status::write_bound_exceeded The serialization has encountered a field which has exceeded the bounds set for it.
 * @var serialization_status::read_bound_exceeded The serialization has encountered a field which has exceeded the bounds set for it.
 * @var serialization_status::illegal_field_value The serialization has encountered a field with a value which should never occur in a valid CDR stream.
 * @var serialization_status::invalid_pl_entry The serialization has encountered a field which it cannot parse.
 */
enum serialization_status {
  move_bound_exceeded   = 0x1 << 0,
  write_bound_exceeded  = 0x1 << 1,
  read_bound_exceeded   = 0x1 << 2,
  invalid_pl_entry      = 0x1 << 3,
  invalid_dl_entry      = 0x1 << 4,
  illegal_field_value   = 0x1 << 5,
  unsupported_property  = 0x1 << 6
};

/**
 * @brief
 * Base cdr_stream class.
 *
 * This class implements the base functions which all "real" cdr stream implementations will use.
 */
class OMG_DDS_API cdr_stream {
public:
    /**
     * @brief
     * Constructor.
     *
     * Sets the stream endianness to end, and maximum alignment to max_align.
     *
     * @param[in] end The endianness to set for the data stream, default to the local system endianness.
     * @param[in] max_align The maximum size that the stream will align CDR primitives to.
     * @param[in] ignore_faults Bitmask for ignoring faults, can be composed of bit fields from the serialization_status enumerator.
     */
    cdr_stream(endianness end, size_t max_align, uint64_t ignore_faults = 0x0) : m_stream_endianness(end), m_max_alignment(max_align), m_fault_mask(~ignore_faults) { ; }

    /**
     * @brief
     * Returns the current stream alignment.
     *
     * @return The current stream alignment.
     */
    size_t alignment() const { return m_current_alignment; }

    /**
     * @brief
     * Sets the new stream alignment.
     *
     * Also returns the value the alignment has been set to.
     *
     * @param[in] newalignment The new alignment to set.
     *
     * @return The value the alignment has been set to.
     */
    size_t alignment(size_t newalignment) { return m_current_alignment = newalignment; }

    /**
     * @brief
     * Returns the current cursor offset.
     *
     * @retval SIZE_MAX In this case, a maximum size calculation was being done, and the maximum size was determined to be unbounded.
     * @return The current cursor offset.
     */
    size_t position() const { return m_position; }

    /**
     * @brief
     * Sets the new cursor offset.
     *
     * Also returs the value the offset has been set to.
     *
     * @param[in] newposition The new offset to set.
     *
     * @return The value the offset has been set to.
     */
    size_t position(size_t newposition) { return m_position = newposition; }

    /**
     * @brief
     * Cursor move function.
     *
     * Moves the current position offset by incr_by if it is not at SIZE_MAX.
     * Returns the position value after this operation.
     *
     * @param[in] incr_by The amount to move the cursor position by.
     *
     * @return The cursor position after this operation.
     */
    size_t incr_position(size_t incr_by) { if (m_position != SIZE_MAX) m_position += incr_by; return m_position; }

    /**
     * @brief
     * Resets the current cursor position and alignment to 0.
     */
    void reset_position() { m_position = 0; m_current_alignment = 0; }

    /**
     * @brief
     * Buffer set function.
     *
     * Sets the buffer pointer to toset.
     * As a side effect, the current position and alignment are reset, since these are not associated with the new buffer.
     *
     * @param[in] toset The new pointer of the buffer to set.
     */
    void set_buffer(void* toset);

    /**
     * @brief
     * Gets the current cursor pointer.
     *
     * If the current position is SIZE_MAX or the buffer pointer is not set, it returns nullptr.
     *
     * @retval nullptr If the current buffer is not set, or if the cursor offset is not valid.
     * @return The current cursor pointer.
     */
    char* get_cursor() const { return ((m_position != SIZE_MAX && m_buffer != nullptr) ? (m_buffer + m_position) : nullptr); }

    /**
     * @brief
     * Local system endianness getter.
     *
     * This is used to determine whether the data read or written from the stream needs to have their bytes swapped.
     *
     * @return The local endianness.
     */
    endianness local_endianness() const { return m_local_endianness; }

    /**
     * @brief
     * Stream endianness getter.
     *
     * This is used to determine whether the data read or written from the stream needs to have their bytes swapped.
     *
     * @return The stream endianness.
     */
    endianness stream_endianness() const { return m_stream_endianness; }

    /**
     * @brief
     * Determines whether the local and stream endianness are the same.
     *
     * This is used to determine whether the data read or written from the stream needs to have their bytes swapped.
     *
     * @retval false If the stream endianness DOES match the local endianness.
     * @retval true If the stream endianness DOES NOT match the local endianness.
     */
    bool swap_endianness() const { return m_stream_endianness != m_local_endianness; }

    /**
     * @brief
     * Aligns the current stream to a new alignment.
     *
     * Aligns the current stream to newalignment, moves the cursor be at newalignment.
     * Aligns to maximum m_max_alignment (which is stream-type specific).
     * Zeroes the bytes the cursor is moved if add_zeroes is true.
     * Nothing happens if the stream is already aligned to newalignment.
     *
     * @param[in] newalignment The new alignment to align the stream to.
     * @param[in] add_zeroes Whether the bytes that the cursor moves need to be zeroed.
     *
     * @return The number of bytes that the cursor was moved.
     */
    size_t align(size_t newalignment, bool add_zeroes);

    /**
     * @brief
     * Returns the current status of serialization.
     *
     * Can be a composition of multiple bit fields from serialization_status.
     *
     * @return The current status of serialization.
     */
    uint64_t status() const { return m_status; }

    /**
     * @brief
     * Serialization status update function.
     *
     * Adds to the current status of serialization and returns whether abort status has been reached.
     *
     * @param[in] toadd The serialization status error to add.
     *
     * @retval false If the serialization status of the stream HAS NOT YET reached one of the serialization errors which it is not set to ignore.
     * @retval true If the serialization status of the stream HAS reached one of the serialization errors which it is not set to ignore.
     */
    bool status(serialization_status toadd) { m_status |= static_cast<uint64_t>(toadd); return abort_status(); }

    /**
     * @brief
     * Returns true when the stream has encountered an error which it is not set to ignore.
     *
     * All streaming functions should become NOOPs after this status is encountered.
     *
     * @retval false If the serialization status of the stream HAS NOT YET reached one of the serialization errors which it is not set to ignore.
     * @retval true If the serialization status of the stream HAS reached one of the serialization errors which it is not set to ignore.
     */
    bool abort_status() const { return m_status & m_fault_mask; }

    /**
     * @brief
     * Returns the entity currently on top of the stack.
     *
     * @return The entity currently on top of the stack.
     */
    entity_properties_t& top_of_stack();

    /**
     * @brief
     * Mapping function between entity properties and a generalized id.
     *
     * This id combines the sequence id of the entity (the sequential member number in the struct)
     * with the member id of the entity, which may be specified through the idl file into a single
     * 64 bit generalized id. Which is then used to determine from either an entity property
     * originating from a tree or a header read from a stream which member to operate on.
     *
     * @param[in] props The property to convert.
     *
     * @return member_id * 2^32 + sequence_id
     */
    static uint64_t props_to_id(const entity_properties_t &props);

    /**
     * @brief
     * Type of streaming operation to be done.
     *
     * @var stream_mode::read Reads from the stream into an instance.
     * @var stream_mode::write Writes from the instance to the stream.
     * @var stream_mode::move Moves the cursor by the same amount as would has been done through stream_mode::write, without copying any data to the stream.
     * @var stream_mode::max Same as stream_mode::move, but by the maximum amount possible for an entity of that type.
     */
    enum class stream_mode {
      read,
      write,
      move,
      max
    };

    /**
     * @brief
     * Function declaration for starting a new member.
     *
     * This function is called by next_entity for each entity which is iterated over.
     * Depending on the implementation and mode headers may be read from/written to the stream.
     * This function is to be implemented in cdr streaming implementations.
     *
     * @param[in] prop Properties of the entity to start.
     * @param[in] mode The mode in which the entity is started.
     * @param[in] present Whether the entity represented by prop is present, if it is an optional entity.
     */
    virtual void start_member(entity_properties_t &prop, stream_mode mode, bool present) = 0;

    /**
     * @brief
     * Function declaration for finishing an existing member.
     *
     * This function is called by next_entity for each entity which is iterated over.
     * Depending on the implementation and mode header length fields may be completed.
     * This function is to be implemented in cdr streaming implementations.
     *
     * @param[in] prop Properties of the entity to finish.
     * @param[in] mode The mode in which the entity is finished.
     * @param[in] present Whether the entity represented by prop is present, if it is an optional entity.
     */
    virtual void finish_member(entity_properties_t &prop, stream_mode mode, bool present) = 0;

    /**
     * @brief
     * Function declaration for skipping entities without involving them on the stack.
     *
     * This function is called by the instance implementation switchbox, when it encounters an id which
     * does not resolve to an id pointing to a member it knows. It will move to the next entity in the
     * stream.
     * This function is to be implemented in cdr streaming implementations.
     *
     * @param[in] props The entity to skip/ignore.
     */
    virtual void skip_entity(const entity_properties_t &props) = 0;

    /**
     * @brief
     * Function declaration for retrieving the next entity to be operated on by the streamer.
     *
     * This function is called by the instance implementation switchbox and will return the next entity to operate on by calling next_prop.
     * This will also call the implementation specific push/pop entity functions to write/finish headers where necessary.
     *
     * @param[in, out] props The property tree to get the next entity from.
     * @param[in] as_key Whether to take the key entities, or the normal member entities.
     * @param[in] mode Which mode to push/pop entities.
     * @param[in, out] firstcall Whether it is the first time calling the function for props, will store first iterator if true, and then set to false.
     *
     * @return The next entity to be processed, or the final entity if the current tree level does not hold more entities.
     */
    virtual entity_properties_t& next_entity(entity_properties_t &props, bool as_key, stream_mode mode, bool &firstcall) = 0;

    /**
     * @brief
     * Function declaration for starting a parameter list.
     *
     * This function is called by the generated functions for the entity, and will trigger the necessary actions on starting a new struct.
     * I.E. starting a new parameter list, writing headers.
     *
     * @param[in,out] props The entity whose members might be represented by a parameter list.
     * @param[in] mode The current mode which is being used.
     */
    virtual void start_struct(entity_properties_t &props, stream_mode mode) = 0;

    /**
     * @brief
     * Function declaration for finishing a parameter list.
     *
     * This function is called by the generated functions for the entity, and will trigger the necessary actions on finishing the current struct.
     * I.E. finishing headers, writing length fields.
     *
     * @param[in,out] props The entity whose members might be represented by a parameter list.
     * @param[in] mode The current mode which is being used.
     */
    virtual void finish_struct(entity_properties_t &props, stream_mode mode) = 0;

protected:

    /**
     * @brief
     * Function for retrieving the next entity to be operated on by the streamer.
     *
     * When it is called the first time, the iterator of the (member/key)entities to be iterated over is stored on the stack.
     * This iterator is then used in successive calls, until the end of the valid entities is reached, at which point the iterator is popped off the stack.
     * This function is to be implemented in cdr streaming implementations.
     *
     * @param[in, out] props The property tree to get the next entity from.
     * @param[in] as_key Whether to take the key entities, or the normal member entities.
     * @param[in, out] firstcall Whether it is the first time calling the function for props, will store first iterator if true, and then set to false.
     *
     * @return The next entity to be processed, or the final entity if the current tree level does not hold more entities.
     */
    entity_properties_t& next_prop(entity_properties_t &props, bool as_key, bool &firstcall);

    endianness m_stream_endianness,               //the endianness of the stream
        m_local_endianness = native_endianness(); //the local endianness
    size_t m_position = 0,                        //the current offset position in the stream
        m_max_alignment,                          //the maximum bytes that can be aligned to
        m_current_alignment = 1;                  //the current alignment
    char* m_buffer = nullptr;                     //the current buffer in use
    uint64_t m_status = 0,                        //the current status of streaming
             m_fault_mask;                        //the mask for statuses that will causes streaming to be aborted

    static entity_properties_t m_final;
    entity_properties_t m_current_header;

    DDSCXX_WARNING_MSVC_OFF(4251)
    std::stack<std::vector<entity_properties_t>::iterator> m_stack; //current iterators the stream is working over
    DDSCXX_WARNING_MSVC_ON(4251)
};

/**
 * @brief
 * Primitive type stream manipulation functions.
 *
 * These are "endpoints" for write functions, since composit
 * (sequence/array/constructed type) functions will decay to these
 * calls.
 */

/**
 * @brief
 * Primitive type read function.
 *
 * Aligns the stream to the alignment of type T.
 * Reads the value from the current position of the stream str into
 * toread, will swap bytes if necessary.
 * Moves the cursor of the stream by the size of T.
 * This function is only enabled for arithmetic types and enums.
 *
 * @param[in, out] str The stream which is read from.
 * @param[out] toread The variable to read into.
 * @param[in] N The number of entities to read.
 */
template<typename S, typename T, std::enable_if_t<std::is_arithmetic<T>::value
                                               && !std::is_enum<T>::value
                                               && std::is_base_of<cdr_stream, S>::value, bool> = true >
void read(S &str, T& toread, size_t N = 1)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  str.align(sizeof(T), false);

  auto from = str.get_cursor();
  T *to = &toread;

  assert(from);

  memcpy(to,from,sizeof(T)*N);

  if (str.swap_endianness()) {
    for (size_t i = 0; i < N; i++, to++)
      byte_swap(*to);
  }

  str.incr_position(sizeof(T)*N);
}

/**
 * @brief
 * Primitive type write function.
 *
 * Aligns str to the type to be written.
 * Writes towrite to str.
 * Swaps bytes written to str if the endiannesses do not match up.
 * Moves the cursor of str by the size of towrite.
 * This function is only enabled for arithmetic types.
 *
 * @param[in, out] str The stream which is written to.
 * @param[in] towrite The variable to write.
 * @param[in] N The number of entities to write.
 */
template<typename S, typename T, std::enable_if_t<std::is_arithmetic<T>::value
                                               && !std::is_enum<T>::value
                                               && std::is_base_of<cdr_stream, S>::value, bool> = true >
void write(S& str, const T& towrite, size_t N = 1)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  str.align(sizeof(T), true);

  auto to = reinterpret_cast<T*>(str.get_cursor());
  const T *from = &towrite;

  assert(to);

  memcpy(to,from,sizeof(T)*N);

  if (str.swap_endianness()) {
    for (size_t i = 0; i < N; i++, to++)
      byte_swap(*to);
  }

  str.incr_position(sizeof(T)*N);
}

/**
 * @brief
 * Primitive type cursor move function.
 *
 * Used in determining the size of a type when written to the stream.
 * Aligns str to the size of toincr.
 * Moves the cursor of str by the size of toincr.
 * This function is only enabled for arithmetic types.
 *
 * @param[in, out] str The stream whose cursor is moved.
 * @param[in] N The number of entities to move.
 */
template<typename S, typename T, std::enable_if_t<std::is_arithmetic<T>::value
                                               && !std::is_enum<T>::value
                                               && std::is_base_of<cdr_stream, S>::value, bool> = true >
void move(S& str, const T&, size_t N = 1)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  str.align(sizeof(T), false);

  str.incr_position(sizeof(T)*N);
}

/**
 * @brief
 * Primitive type max stream move function.
 *
 * Used in determining the maximum stream size of a constructed type.
 * Moves the cursor to the maximum position it could occupy after
 * writing max_sz to the stream.
 * Is in essence the same as the primitive type cursor move function,
 * but additionally checks for whether the cursor it at the "end",
 * which may happen if unbounded members (strings/sequences/...)
 * are part of the constructed type.
 * This function is only enabled for arithmetic types.
 *
 * @param[in, out] str The stream whose cursor is moved.
 * @param[in] max_sz The variable to move the cursor by, no contents of this variable are used, it is just used to determine the template.
 * @param[in] N The number of entities at most to move.
 */
template<typename S, typename T, std::enable_if_t<std::is_arithmetic<T>::value
                                               && !std::is_enum<T>::value
                                               && std::is_base_of<cdr_stream, S>::value, bool> = true >
void max(S& str, const T& max_sz, size_t N = 1)
{
  move(str, max_sz, N);
}

 /**
 * @brief
 * String type stream manipulation functions
 *
 * These are "endpoints" for write functions, since compound
 * (sequence/array/constructed type) functions will decay to these
 * calls.
 */

/**
 * @brief
 * Bounded string read function.
 *
 * Reads the length from str, but then initializes toread with at most N characters from it.
 * It does move the cursor by length read, since that is the number of characters in the stream.
 * If N is 0, then the string is taken to be unbounded.
 *
 * @param[in, out] str The stream to read from.
 * @param[out] toread The string to read to.
 * @param[in] N The maximum number of characters to read from the stream.
 */
template<typename S, typename T, std::enable_if_t<std::is_base_of<cdr_stream, S>::value, bool> = true >
void read_string(S& str, T& toread, size_t N)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  uint32_t string_length = 0;

  read(str, string_length);

  if (string_length == 0
   && str.status(serialization_status::illegal_field_value))
    return;

  if (N
   && string_length > N + 1
   && str.status(serialization_status::read_bound_exceeded))
      return;

  auto cursor = str.get_cursor();
  toread.assign(cursor, cursor + std::min<size_t>(string_length - 1, N ? N : SIZE_MAX));  //remove 1 for terminating NULL

  str.incr_position(string_length);

  //aligned to chars
  str.alignment(1);
}

/**
 * @brief
 * Bounded string write function.
 *
 * Attempts to write the length of towrite to str, where the bound is checked.
 * Then writes the contents of towrite to str.
 * If N is 0, then the string is taken to be unbounded.
 *
 * @param[in, out] str The stream to write to.
 * @param[in] towrite The string to write.
 * @param[in] N The maximum number of characters to write to the stream.
 */
template<typename S, typename T, std::enable_if_t<std::is_base_of<cdr_stream, S>::value, bool> = true >
void write_string(S& str, const T& towrite, size_t N)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  size_t string_length = towrite.length() + 1;  //add 1 extra for terminating NULL

  if (N
   && string_length > N + 1
   && str.status(serialization_status::write_bound_exceeded))
      return;

  write(str, uint32_t(string_length));

  memcpy(str.get_cursor(), towrite.c_str(), string_length);

  str.incr_position(string_length);

  //aligned to chars
  str.alignment(1);

}

/**
 * @brief
 * Bounded string cursor move function.
 *
 * Attempts to move the cursor for the length field, where the bound is checked.
 * Then moves the cursor for the length of the string.
 * If N is 0, then the string is taken to be unbounded.
 *
 * @param[in, out] str The stream whose cursor is moved.
 * @param[in] toincr The string used to move the cursor.
 * @param[in] N The maximum number of characters in the string which the stream is moved by.
 */
template<typename S, typename T, std::enable_if_t<std::is_base_of<cdr_stream, S>::value, bool> = true >
void move_string(S& str, const T& toincr, size_t N)
{
  if (str.abort_status() || str.position() == SIZE_MAX)
    return;

  size_t string_length = toincr.length() + 1;  //add 1 extra for terminating NULL

  if (N
   && string_length > N + 1
   && str.status(serialization_status::move_bound_exceeded))
      return;

  move(str, uint32_t());

  str.incr_position(string_length);

  //aligned to chars
  str.alignment(1);
}

/**
 * @brief
 * Bounded string cursor max move function.
 *
 * Similar to the string move function, with the additional checks that no move
 * is done if the cursor is already at its maximum position, and that the cursor
 * is set to its maximum position if the bound is equal to 0 (unbounded).
 *
 * @param[in, out] str The stream whose cursor is moved.
 * @param[in] max_sz The string used to move the cursor.
 * @param[in] N The maximum number of characters in the string which the stream is at most moved by.
 */
template<typename S, typename T, std::enable_if_t<std::is_base_of<cdr_stream, S>::value, bool> = true >
void max_string(S& str, const T& max_sz, size_t N)
{
  if (N == 0)
    str.position(SIZE_MAX); //unbounded string, theoretical length unlimited
  else
    move_string(str, max_sz, N);
}

}
}
}
}
} /* namespace org / eclipse / cyclonedds / core / cdr */
#endif
