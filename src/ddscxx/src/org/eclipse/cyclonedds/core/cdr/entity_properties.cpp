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

#include <org/eclipse/cyclonedds/core/cdr/entity_properties.hpp>
#include <iostream>
#include <algorithm>
#include <cassert>

namespace org {
namespace eclipse {
namespace cyclonedds {
namespace core {
namespace cdr {

bool entity_properties::member_id_comp(const entity_properties_t &lhs, const entity_properties_t &rhs)
{
  if (!rhs && lhs)
    return true;
  if (rhs && !lhs)
    return false;

  return lhs.m_id < rhs.m_id;
}

void entity_properties::print(bool recurse, size_t depth, const char *prefix) const
{
  std::cout << "d: " << depth;
  for (size_t i = 0; i < depth; i++) std::cout << "  ";
  std::cout << prefix << ": m_id: " << m_id << " final: " << (is_last ? "yes" : "no");

  std::cout << " p_ext: ";
  switch(p_ext) {
    case ext_final:
    std::cout << "FINAL";
    break;
    case ext_appendable:
    std::cout << "APPENDABLE";
    break;
    case ext_mutable:
    std::cout << "MUTABLE";
    break;
  }
  std::cout << " e_ext: ";
  switch(e_ext) {
    case ext_final:
    std::cout << "FINAL";
    break;
    case ext_appendable:
    std::cout << "APPENDABLE";
    break;
    case ext_mutable:
    std::cout << "MUTABLE";
    break;
  }
  std::cout << std::endl;
  if (recurse) {
    for (const auto & e:m_members_by_seq) e.print(true, depth+1, "member_s");
    for (const auto & e:m_members_by_id) e.print(true, depth+1, "member_i");
    for (const auto & e:m_keys) e.print(true, depth+1, "key     ");
  }
}

void entity_properties::set_member_props(uint32_t member_id, bool optional)
{
  m_id = member_id;
  is_optional = optional;
}

void entity_properties::finish(bool at_root)
{
  finish_keys(at_root);
  sort_by_member_id();

  for (auto &e : m_members_by_seq)
    e.finish(false);

  for (auto &e : m_members_by_id)
    e.finish(false);

  for (auto &e : m_keys)
    e.finish(false);

  if (at_root)
    copy_must_understand(m_keys, m_members_by_seq, m_members_by_id);
}

void entity_properties::copy_must_understand(
  const proplist &keys_by_id,
  proplist &members_by_seq,
  proplist &members_by_id)
{
  for (const auto & k:keys_by_id) {
    if (!k)
      continue;

    assert(k.must_understand);

    auto seq = std::find(members_by_seq.begin(), members_by_seq.end(), k);
    auto id = std::find(members_by_id.begin(), members_by_id.end(), k);
    assert(seq != members_by_seq.end() && id != members_by_id.end());

    seq->must_understand = true;
    id->must_understand = true;
    copy_must_understand(k.m_keys, seq->m_members_by_seq, id->m_members_by_id);
  }
}

void entity_properties::finish_keys(bool at_root)
{
  if (!at_root && m_keys.size() < 2)
    m_keys = m_members_by_seq;

  for (auto & e:m_keys) {
    e.must_understand = true;
    e.e_ext = ext_final;
    e.p_ext = ext_final;
  }
}

void entity_properties::sort_by_member_id()
{
  m_members_by_id = sort_proplist(m_members_by_seq);
  m_keys = sort_proplist(m_keys);
}

proplist entity_properties::sort_proplist(
  const proplist &in)
{
  auto out = in;
  out.sort(member_id_comp);

  if (out.size()) {
    auto it2 = out.begin();
    auto it = it2++;

    while (it2 != out.end()) {
      if (it2->m_id == it->m_id && it2->is_last == it->is_last) {
        it->merge(*it2);
        it2 = out.erase(it2);
      } else {
        it = it2++;
      }
    }
  }

  return out;
}

void entity_properties::merge(const entity_properties_t &other)
{
  assert(other.m_id == m_id && other.is_last == is_last);

  m_members_by_seq.insert(m_members_by_seq.end(), other.m_members_by_seq.begin(), other.m_members_by_seq.end());

  m_keys.insert(m_keys.end(), other.m_keys.begin(), other.m_keys.end());
}

}
}
}
}
}  /* namespace org / eclipse / cyclonedds / core / cdr */
