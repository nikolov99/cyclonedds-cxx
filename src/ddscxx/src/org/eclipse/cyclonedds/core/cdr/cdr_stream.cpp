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
#include <cstring>
#include <assert.h>

#include <org/eclipse/cyclonedds/core/cdr/cdr_stream.hpp>

namespace org {
namespace eclipse {
namespace cyclonedds {
namespace core {
namespace cdr {

entity_properties_t cdr_stream::m_final = final_entry();

void cdr_stream::set_buffer(void* toset, size_t buffer_size)
{
  m_buffer = static_cast<char*>(toset);
  m_buffer_size = buffer_size;
  reset();
}

size_t cdr_stream::align(size_t newalignment, bool add_zeroes)
{
  if (m_current_alignment == newalignment)
    return 0;

  m_current_alignment = std::min(newalignment, m_max_alignment);

  size_t tomove = (m_current_alignment - m_position % m_current_alignment) % m_current_alignment;
  if (tomove && add_zeroes && m_buffer) {
    auto cursor = get_cursor();
    assert(cursor);
    memset(cursor, 0, tomove);
  }

  m_position += tomove;

  return tomove;
}

void cdr_stream::finish_member(entity_properties_t &prop, bool)
{
  if (m_mode == stream_mode::read && !prop.is_present)
    go_to_next_member(prop);
}

void cdr_stream::finish_struct(entity_properties_t &props)
{
  check_struct_completeness(props, m_key ? member_list_type::key : member_list_type::member_by_seq);
}

bool cdr_stream::inside_buffer(size_t n_bytes)
{
  return m_position + n_bytes <= m_buffer_size;
}

entity_properties_t& cdr_stream::next_prop(entity_properties_t &props, member_list_type list_type, bool &firstcall)
{
  if (firstcall) {
    std::list<entity_properties_t>::iterator it;
    switch (list_type) {
      case member_list_type::member_by_seq:
        it = props.m_members_by_seq.begin();
        break;
      case member_list_type::member_by_id:
        it = props.m_members_by_id.begin();
        break;
      case member_list_type::key:
        it = props.m_keys.begin();
        break;
      default:
        assert(0);
    }
    m_stack.push(it);
    firstcall = false;
    return *it;
  }

  assert(m_stack.size());

  if (*m_stack.top())  //we have not yet reached the end of the entities in the list, so we can go to the next
    m_stack.top()++;

  entity_properties_t &entity = *m_stack.top();
  if (!entity) //we have reached the end of the list
    m_stack.pop();

  return entity;
}

void cdr_stream::reset()
{
  m_position = 0;
  m_current_alignment = 0;
  m_status = 0;
  m_stack = std::stack<proplist::iterator>();
}

void cdr_stream::skip_entity(const entity_properties_t &prop)
{
  incr_position(prop.e_sz);
  alignment(0);
}

void cdr_stream::start_member(entity_properties_t &prop, bool)
{
  record_member_start(prop);
}

entity_properties_t& cdr_stream::top_of_stack()
{
  assert(m_stack.size());
  return *(m_stack.top());
}

void cdr_stream::record_member_start(entity_properties_t &prop)
{
  prop.e_off = position();
  prop.is_present = true;
}

void cdr_stream::go_to_next_member(entity_properties_t &prop)
{
  if (prop.e_sz > 0 && m_mode == stream_mode::read) {
    m_position = prop.e_off + prop.e_sz;
    m_current_alignment = 0;
  }
}

void cdr_stream::record_struct_start(entity_properties_t &props)
{
  props.is_present = true;
  props.d_off = position();
}

void cdr_stream::check_struct_completeness(entity_properties_t &props, member_list_type list_type)
{
  if (m_mode != stream_mode::read)
    return;

  if (abort_status()) {
    props.is_present = false;
    return;
  }

  proplist::iterator it;
  switch (list_type) {
    case member_list_type::member_by_seq:
      it = props.m_members_by_seq.begin();
      break;
    case member_list_type::member_by_id:
      it = props.m_members_by_id.begin();
      break;
    case member_list_type::key:
      it = props.m_keys.begin();
      break;
    default:
      assert(0);
  }

  while (*it) {
    if (it->must_understand_local && !it->is_present) {
      props.is_present = false;
      break;
    }
    it++;
  }
}

}
}
}
}
}  /* namespace org / eclipse / cyclonedds / core / cdr */
