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
#include <inttypes.h>
#include "idlcxx/backend.h"

char*
get_cpp11_name(const char* name);

char *
get_cpp11_type(const idl_node_t *node);

char *
get_cpp11_fully_scoped_name(const idl_node_t *node);

char *
get_default_value(idl_backend_ctx ctx, const idl_node_t *node);

char *
get_cpp11_const_value(const idl_literal_t *literal);

uint64_t
array_entries(const idl_const_expr_t* ce);

uint64_t
generate_array_expression(char** type_name, const idl_const_expr_t* ce);

void
resolve_namespace(idl_node_t* node, char** up);
