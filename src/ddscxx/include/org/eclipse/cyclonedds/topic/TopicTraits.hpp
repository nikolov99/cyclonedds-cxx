/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */


/**
 * @file
 */

#ifndef CYCLONEDDS_TOPIC_TOPICTRAITS_HPP_
#define CYCLONEDDS_TOPIC_TOPICTRAITS_HPP_

#include <vector>

#include "org/eclipse/cyclonedds/topic/DataRepresentation.hpp"

struct ddsi_sertype;

namespace org
{
namespace eclipse
{
namespace cyclonedds
{
namespace topic
{

/**
 * @brief
 * Entity extensibility descriptors.
 *
 * @enum extensibility Describes the extensibility of entities.
 *
 * This value is set for entities and their parents.
 *
 * @var extensibility::ext_final The entity representation is complete, no fields can be added or removed.
 * @var extensibility::ext_appendable The entity representation can be extended, no fields can be removed.
 * @var extensibility::ext_mutable The entity representation can be modified, fields can be removed or added.
 */
enum class extensibility {
  ext_final,
  ext_appendable,
  ext_mutable
};

template <class TOPIC> class TopicTraits
{
public:
    static ::org::eclipse::cyclonedds::topic::DataRepresentationId_t getDataRepresentationId()
    {
        return ::org::eclipse::cyclonedds::topic::INVALID_REPRESENTATION;
    }

    static ::std::vector<uint8_t> getMetaData()
    {
        return ::std::vector<uint8_t>();
    }

    static ::std::vector<uint8_t> getTypeHash()
    {
        return ::std::vector<uint8_t>();
    }

    static ::std::vector<uint8_t> getExtentions()
    {
        return ::std::vector<uint8_t>();
    }

    static constexpr bool isKeyless()
    {
        return true;
    }

    static constexpr const char *getTypeName()
    {
        return "ExampleName";
    }

    static ddsi_sertype *getSerType()
    {
        return NULL;
    }

    static constexpr size_t getSampleSize()
    {
        return 0;
    }

    static constexpr bool isSelfContained()
    {
      return true;
    }

    static constexpr bool requiresXTypes()
    {
      return true;
    }

    static constexpr extensibility getExtensibility()
    {
      return extensibility::ext_final;
    }
};

}
}
}
}

#endif /* CYCLONEDDS_TOPIC_TOPICTRAITS_HPP_ */
