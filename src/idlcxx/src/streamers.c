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
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "idl/print.h"
#include "idl/stream.h"

#include "generator.h"

#define CHUNK (4096)

static idl_retcode_t vputf(idl_buffer_t *buf, const char *fmt, va_list ap)
{
  va_list aq;
  int cnt;
  char str[1], *data = str;
  size_t size = 0;

  assert(buf);
  assert(fmt);

  va_copy(aq, ap);
  if (buf->data && (size = (buf->size - buf->used)) > 0)
    data = buf->data + buf->used;
  cnt = idl_vsnprintf(data, size+1, fmt, aq);
  va_end(aq);

  if (cnt >= 0 && size <= (size_t)cnt) {
    size = buf->size + ((((size_t)cnt - size) / CHUNK) + 1) * CHUNK;
    if (!(data = realloc(buf->data, size+1)))
      return IDL_RETCODE_NO_MEMORY;
    buf->data = data;
    buf->size = size;
    cnt = idl_vsnprintf(buf->data + buf->used, size, fmt, ap);
  }

  if (cnt < 0)
    return IDL_RETCODE_NO_MEMORY;
  buf->used += (size_t)cnt;
  return IDL_RETCODE_OK;
}

static idl_retcode_t putf(idl_buffer_t *buf, const char *fmt, ...)
{
  va_list ap;
  idl_retcode_t ret;

  va_start(ap, fmt);
  ret = vputf(buf, fmt, ap);
  va_end(ap);
  return ret;
}

static int get_array_accessor(char* str, size_t size, const void* node, void* user_data)
{
  (void)node;
  uint32_t depth = *((uint32_t*)user_data);
  return idl_snprintf(str, size, "a_%u", depth);
}

struct sequence_holder {
  const char* sequence_accessor;
  size_t depth;
};
typedef struct sequence_holder sequence_holder_t;

static int get_sequence_member_accessor(char* str, size_t size, const void* node, void* user_data)
{
  (void)node;
  sequence_holder_t* sh = (sequence_holder_t*)user_data;
  static const char *fmt = "%1$s[i_%2$u]";
  return idl_snprintf(str, size, fmt, sh->sequence_accessor, (uint32_t)sh->depth);
}

enum instance_mask {
  TYPEDEF           = 0x1 << 0,
  UNION_BRANCH      = 0x1 << 1,
  SEQUENCE          = 0x1 << 2,
  ARRAY             = 0x1 << 3,
  OPTIONAL          = 0x1 << 4
};

struct instance_location {
  char *parent;
  uint32_t type;
};
typedef struct instance_location instance_location_t;

static int get_instance_accessor(char* str, size_t size, const void* node, void* user_data)
{
  instance_location_t loc = *(instance_location_t *)user_data;

  if (loc.type & TYPEDEF) {
    return idl_snprintf(str, size, "%s", loc.parent);
  } else {
    const char *opt = "";
    if (loc.type & OPTIONAL)
      opt = "*";

    const idl_declarator_t* decl = (const idl_declarator_t*)node;
    const char* name = get_cpp11_name(decl);
    return idl_snprintf(str, size, "%s%s.%s()", opt, loc.parent, name);
  }
}

struct streams {
  struct generator *generator;
  idl_buffer_t write;
  idl_buffer_t read;
  idl_buffer_t move;
  idl_buffer_t max;
  idl_buffer_t props;
};

static void setup_streams(struct streams* str, struct generator* gen)
{
  assert(str);
  memset(str, 0, sizeof(struct streams));
  str->generator = gen;
}

static void cleanup_streams(struct streams* str)
{
  if (str->write.data)
    free(str->write.data);
  if (str->read.data)
    free(str->read.data);
  if (str->move.data)
    free(str->move.data);
  if (str->max.data)
    free(str->max.data);
  if (str->props.data)
    free(str->props.data);
}

static idl_retcode_t flush_stream(idl_buffer_t* str, FILE* f)
{
  if (str->data && fputs(str->data, f) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (str->size &&
      str->data)
      str->data[0] = '\0';
  str->used = 0;

  return IDL_RETCODE_OK;
}

static idl_retcode_t flush(struct generator* gen, struct streams* streams)
{
  if (IDL_RETCODE_OK != flush_stream(&streams->props, gen->impl.handle)
   || IDL_RETCODE_OK != flush_stream(&streams->write, gen->header.handle)
   || IDL_RETCODE_OK != flush_stream(&streams->read, gen->header.handle)
   || IDL_RETCODE_OK != flush_stream(&streams->move, gen->header.handle)
   || IDL_RETCODE_OK != flush_stream(&streams->max, gen->header.handle))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

#define WRITE (1u<<0)
#define READ (1u<<1)
#define MOVE (1u<<2)
#define MAX (1u<<3)
#define CONST (WRITE | MOVE | MAX)
#define ALL (CONST | READ)
#define NOMAX (ALL & ~MAX)

//mapping of streaming flags and token replacements
static char tokens[2] = {'T', 'C'};

static struct { uint32_t id; size_t O; const char *token_replacements[2]; } map[] = {
  { WRITE, offsetof(struct streams, write), {"write", "const "} },
  { READ,  offsetof(struct streams, read),  {"read",  ""} },
  { MOVE,  offsetof(struct streams, move),  {"move",  "const "} },
  { MAX,   offsetof(struct streams, max),   {"max",   "const "} }
};

/* scan over string looking for {tok} */
static idl_retcode_t print_until_token(struct streams *out, uint32_t mask, const char *fmt, size_t *fmt_position)
{
  int err = 0;
  size_t start_pos = *fmt_position;
  bool end_found = false;
  while (!end_found) {
    if (fmt[*fmt_position] == '\0') {
      end_found = true;
    }
    if (fmt[*fmt_position] == '{') {
      for (size_t i = 0, n = sizeof(tokens)/sizeof(tokens[0]); i < n; i++)
        if (fmt[*fmt_position+1] == tokens[i])
          end_found = true;
    }
    if (!end_found)
      (void)(*fmt_position)++;
  }

  size_t sub_len = *fmt_position-start_pos;
  if (sub_len) {
    char *substring = NULL;
    if ((substring = malloc(sub_len+1))) {
      memcpy(substring, fmt+start_pos, sub_len);
      substring[sub_len] = '\0';

      for (uint32_t i=0, n=(sizeof(map)/sizeof(map[0])); i < n && !err; i++) {
        if (!(map[i].id & mask))
          continue;

        if (putf((idl_buffer_t*)((char*)out+map[i].O), substring))
          err = 1;
      }
      free (substring);
    } else {
      err = 1;
    }
  }

  return err ? IDL_RETCODE_NO_MEMORY : IDL_RETCODE_OK;
}

static idl_retcode_t replace_token(struct streams *out, uint32_t mask, const char *fmt, size_t *fmt_position) {
  int err = 0;

  for (size_t i = 0, ntoks = sizeof(tokens)/sizeof(tokens[0]); i < ntoks && !err; i++) {
    if (fmt[*fmt_position+1] != tokens[i])
      continue;

    for (uint32_t j=0, n=(sizeof(map)/sizeof(map[0])); j < n && !err; j++) {
      if (!(map[j].id & mask))
        continue;

      if (putf((idl_buffer_t*)((char*)out+map[j].O), map[j].token_replacements[i]))
        err = 1;
    }
  }

  *fmt_position += 3;
  return err ? IDL_RETCODE_NO_MEMORY : IDL_RETCODE_OK;
}

static idl_retcode_t multi_putf(struct streams *out, uint32_t mask, const char *fmt, ...)
{
  char *withtokens = NULL;
  size_t tlen;
  va_list ap, aq;
  int err = 0;

  va_start(ap, fmt);
  va_copy(aq, ap);
  int cnt = idl_vsnprintf(withtokens, 0, fmt, aq);
  va_end(aq);
  if (cnt >= 0) {
    tlen = (size_t)cnt;
    if (tlen != SIZE_MAX) {
      withtokens = malloc(tlen + 1u);
      if (withtokens) {
        cnt = idl_vsnprintf(withtokens, tlen + 1u, fmt, ap);
        err = ((size_t)cnt != tlen);
      } else {
        err = 1;
      }
    } else {
      err = 1;
    }
  } else {
    err = 1;
  }
  va_end(ap);

  if (!err) {
    size_t str_pos = 0;
    while (!err && withtokens[str_pos]) {
      if (print_until_token(out, mask, withtokens, &str_pos))
        err = 1;
      if (!err && withtokens[str_pos]) {
        if (replace_token(out, mask, withtokens, &str_pos))
          err = 1;
      }
    }
  }

  if (withtokens)
    free(withtokens);

  return err ? IDL_RETCODE_NO_MEMORY : IDL_RETCODE_OK;
}

static idl_retcode_t
write_string_streaming_functions(
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const char* accessor,
  const char* read_accessor)
{
  uint32_t maximum = ((const idl_string_t*)type_spec)->maximum;

  static const char* fmt =
    "      {T}_string(streamer, %1$s, %2$"PRIu32");\n";
  static const char* mfmt =
    "      {T}_string(streamer, %1$s(), %2$"PRIu32");\n";
  char *type = NULL;

  if (IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0
   || multi_putf(streams, WRITE | MOVE, fmt, accessor, maximum)
   || multi_putf(streams, MAX, mfmt, type, maximum)
   || multi_putf(streams, READ, fmt, read_accessor, maximum))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
write_typedef_streaming_functions(
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const char* accessor,
  const char* read_accessor)
{
  static const char* fmt =
    "      {T}_%1$s(streamer, %2$s, as_key);\n";
  static const char* mfmt =
    "      {T}_%1$s(streamer, %2$s(), as_key);\n";
  char* name = NULL;
  char* type = NULL;
  if (IDL_PRINTA(&name, get_cpp11_name_typedef, type_spec, streams->generator) < 0
   || IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0
   || multi_putf(streams, WRITE | MOVE, fmt, name, accessor)
   || multi_putf(streams, MAX, mfmt, name, type)
   || multi_putf(streams, READ, fmt, name, read_accessor))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
write_constructed_type_streaming_functions(
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const char* accessor,
  const char* read_accessor)
{
  static const char* fmt =
    "      {T}(streamer, %1$s, prop, as_key);\n";
  static const char* mfmt =
    "      {T}(streamer, %1$s(), prop, as_key);\n";
  char *type = NULL;

  if (IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0
   || multi_putf(streams, WRITE | MOVE, fmt, accessor)
   || multi_putf(streams, MAX, mfmt, type)
   || multi_putf(streams, READ, fmt, read_accessor))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
write_base_type_streaming_functions(
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const char* accessor,
  const char* read_accessor,
  instance_location_t loc)
{
  const char* fmt =
    "      {T}(streamer, %1$s);\n";
  const char* mfmt =
    "      {T}(streamer, %1$s());\n";
  const char* read_fmt = fmt;
  char *type = NULL;

  if (loc.type & SEQUENCE
   && idl_mask(type_spec) == IDL_BOOL) {
    read_fmt =
      "      {\n"
      "        bool b(false);\n"
      "        read(streamer, b);\n"
      "        %1$s = b;\n"
      "      }\n";

    fmt =
        "      {\n"
        "        const bool b(%1$s);\n"
        "        {T}(streamer, b);\n"
        "      }\n";
  }

  if (IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0
   || multi_putf(streams, WRITE | MOVE, fmt, accessor)
   || multi_putf(streams, MAX, mfmt, type)
   || multi_putf(streams, READ, read_fmt, read_accessor))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
write_streaming_functions(
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const char* accessor,
  const char* read_accessor,
  instance_location_t loc)
{
  if (idl_is_alias(type_spec))
    return write_typedef_streaming_functions(streams, type_spec, accessor, read_accessor);
  else if (idl_is_string(type_spec))
    return write_string_streaming_functions(streams, type_spec, accessor, read_accessor);
  else if (idl_is_union(type_spec) || idl_is_struct(type_spec))
    return write_constructed_type_streaming_functions(streams, type_spec, accessor, read_accessor);
  else
    return write_base_type_streaming_functions(streams, type_spec, accessor, read_accessor, loc);
}

static idl_retcode_t
unroll_sequence(const idl_pstate_t* pstate,
  struct streams* streams,
  const idl_sequence_t* seq,
  size_t depth,
  const char* accessor,
  const char* read_accessor,
  instance_location_t loc);

static idl_retcode_t
sequence_writes(const idl_pstate_t* pstate,
  struct streams* streams,
  const idl_sequence_t* seq,
  size_t depth,
  const char* accessor,
  const char* read_accessor,
  instance_location_t loc)
{
  const idl_type_spec_t *type_spec = seq->type_spec;

  if ((idl_is_base_type(type_spec) || idl_is_enum(type_spec))
    && (idl_mask(type_spec) & IDL_BOOL) != IDL_BOOL) {
    const char* sfmt = "      {T}(streamer, %1$s[0], se_%2$u);\n";
    const char* mfmt = "      {T}(streamer, %1$s(), se_%2$u);\n";
    char* type = NULL;

    if (IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0
      || multi_putf(streams, MOVE | MAX, mfmt, type, depth)
      || multi_putf(streams, WRITE, sfmt, accessor, depth)
      || multi_putf(streams, READ, sfmt, read_accessor, depth))
        return IDL_RETCODE_NO_MEMORY;

    return IDL_RETCODE_OK;
  }

  const char* fmt = "      for (uint32_t i_%1$u = 0; i_%1$u < se_%1$u; i_%1$u++) {\n";
  if (multi_putf(streams, ALL, fmt, depth))
    return IDL_RETCODE_NO_MEMORY;

  sequence_holder_t sh = (sequence_holder_t){ .sequence_accessor = accessor, .depth = depth};
  char* new_accessor = NULL;
  if (IDL_PRINTA(&new_accessor, get_sequence_member_accessor, &sh, &sh) < 0)
    return IDL_RETCODE_NO_MEMORY;

  sh.sequence_accessor = read_accessor;
  char* new_read_accessor = NULL;
  if (IDL_PRINTA(&new_read_accessor, get_sequence_member_accessor, &sh, &sh) < 0)
    return IDL_RETCODE_NO_MEMORY;

  loc.type |= SEQUENCE;

  if (idl_is_sequence(type_spec)) {
    if (unroll_sequence (pstate, streams, (idl_sequence_t*)type_spec, depth + 1, new_accessor, new_read_accessor, loc))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    if (write_streaming_functions (streams, type_spec, new_accessor, new_read_accessor, loc))
      return IDL_RETCODE_NO_MEMORY;
  }

  fmt = "      }  //i_%1$u\n";
  if (multi_putf(streams, ALL, fmt, depth))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

idl_retcode_t
unroll_sequence(const idl_pstate_t* pstate,
  struct streams* streams,
  const idl_sequence_t* seq,
  size_t depth,
  const char* accessor,
  const char* read_accessor,
  instance_location_t loc)
{
  uint32_t maximum = seq->maximum;

  const char* wfmt = maximum ? "      {\n"\
                               "      uint32_t se_%1$u = uint32_t(%2$s.size());\n"\
                               "      if (se_%1$u > %3$u &&\n"
                               "          streamer.status(serialization_status::{T}_bound_exceeded))\n"
                               "        return;\n"\
                               "      {T}(streamer, se_%1$u);\n"\
                               "      if (se_%1$u > 0)\n"\
                               "      {\n"
                             : "      {\n"\
                               "      uint32_t se_%1$u = uint32_t(%2$s.size());\n"\
                               "      {T}(streamer, se_%1$u);\n"\
                               "      if (se_%1$u > 0)\n"\
                               "      {\n";
  const char* rfmt = maximum ? "      {\n"\
                               "      uint32_t se_%1$u = 0;\n"\
                               "      read(streamer, se_%1$u);\n"\
                               "      if (se_%1$u > %3$u &&\n"
                               "          streamer.status(serialization_status::read_bound_exceeded))\n"
                               "        return;\n"\
                               "      %2$s.resize(se_%1$u);\n"\
                               "      if (se_%1$u > 0)\n"\
                               "      {\n"
                             : "      {\n"\
                               "      uint32_t se_%1$u = 0;\n"\
                               "      read(streamer, se_%1$u);\n"\
                               "      %2$s.resize(se_%1$u);\n"\
                               "      if (se_%1$u > 0)\n"\
                               "      {\n";
  const char* mfmt = "      {\n"\
                     "      uint32_t se_%1$u = %2$u;\n"\
                     "      max(streamer, uint32_t(0));\n";

  if (putf(&streams->read, rfmt, depth, read_accessor, maximum)
   || multi_putf(streams, (WRITE | MOVE), wfmt, depth, accessor, maximum)
   || putf(&streams->max, mfmt, depth, maximum))
    return IDL_RETCODE_NO_MEMORY;

  if (sequence_writes(pstate, streams, seq, depth, accessor, read_accessor, loc))
    return IDL_RETCODE_NO_MEMORY;

  //close sequence
  if (multi_putf(streams, NOMAX,  "      }\n")
   || multi_putf(streams, ALL, "      }  //end sequence\n"))
    return IDL_RETCODE_NO_MEMORY;

  if (maximum == 0
   && putf(&streams->max, "      streamer.position(SIZE_MAX);\n"))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
unroll_array(
  struct streams* streams,
  char *accessor,
  uint32_t array_depth)
{
  if (array_depth) {
    const char* afmt = "      for ({C}auto & a_%1$u:a_%2$u)\n";
    if (multi_putf(streams, ALL, afmt, array_depth+1, array_depth))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    const char* afmt = "      for ({C}auto & a_%1$u:%2$s)\n";
    if (multi_putf(streams, ALL, afmt, array_depth+1, accessor))
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
insert_array_primitives_copy(
  struct streams *streams,
  const idl_declarator_t* declarator,
  const idl_literal_t *lit,
  size_t n_arr,
  char *accessor)
{
  uint32_t a_size = lit->value.uint32;

  if (n_arr && IDL_PRINTA(&accessor, get_array_accessor, declarator, &n_arr) < 0)
    return IDL_RETCODE_NO_MEMORY;

  const char *fmt = "      {T}(streamer, %1$s[0], %2$u);\n";

  if (multi_putf(streams, ALL, fmt, accessor, a_size))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_entity(
  const idl_pstate_t *pstate,
  struct streams *streams,
  const idl_declarator_t* declarator,
  const idl_type_spec_t* type_spec,
  instance_location_t loc)
{
  if (idl_is_array(declarator))
    loc.type |= ARRAY;
  if (idl_is_sequence(type_spec))
    loc.type |= SEQUENCE;

  char* accessor = NULL;
  if (IDL_PRINTA(&accessor, get_instance_accessor, declarator, &loc) < 0)
    return IDL_RETCODE_NO_MEMORY;

  //unroll arrays
  if (idl_is_array(declarator)) {
    uint32_t n_arr = 0;
    const idl_literal_t* lit = (const idl_literal_t*)declarator->const_expr;
    idl_retcode_t ret = IDL_RETCODE_OK;
    while (lit) {
      const idl_literal_t* next = (const idl_literal_t*)((const idl_node_t*)lit)->next;

      if (next == NULL &&
          (idl_is_base_type(type_spec) || idl_is_enum(type_spec))) {
        return insert_array_primitives_copy(streams, declarator, lit, n_arr, accessor);
      } else if ((ret = unroll_array(streams, accessor, n_arr++)) != IDL_RETCODE_OK) {
        return ret;
      }

      lit = next;
    }

    //update accessor to become "a_$n_arr$"
    if (IDL_PRINTA(&accessor, get_array_accessor, declarator, &n_arr) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  const char* read_accessor;
  if (loc.type & UNION_BRANCH)
    read_accessor = "obj";
  else
    read_accessor = accessor;

  //unroll sequences (if any)
  if (idl_is_sequence(type_spec)) {
    if (unroll_sequence(pstate, streams, (idl_sequence_t*)type_spec, 1, accessor, read_accessor, loc))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    if (write_streaming_functions(streams, type_spec, accessor, read_accessor, loc))
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_extensibility_t
get_extensibility(const void *node)
{
  if (idl_is_enum(node)) {
    const idl_enum_t *ptr = node;
    return ptr->extensibility.value;
  } else if (idl_is_union(node)) {
    const idl_union_t *ptr = node;
    return ptr->extensibility.value;
  } else if (idl_is_struct(node)) {
    const idl_struct_t *ptr = node;
    return ptr->extensibility.value;
  }
  return IDL_FINAL;
}

static idl_retcode_t
generate_entity_properties(
  const idl_node_t *parent,
  const idl_type_spec_t *type_spec,
  struct streams *streams,
  const char *addto,
  uint32_t member_id)
{
  const idl_node_t *nd = ((const idl_node_t*)type_spec)->parent;

  if (idl_is_struct(type_spec)
   || idl_is_union(type_spec)) {
    char *type = NULL;
    if (IDL_PRINTA(&type, get_cpp11_fully_scoped_name, type_spec, streams->generator) < 0)
      return IDL_RETCODE_NO_MEMORY;

    /* structs and unions need to set their properties as members through the set_member_props
     * function as they are copied from the static references of the class*/
    if (putf(&streams->props, "    %1$s.push_back(get_type_props<%2$s>());\n"
                              "    %1$s.back().set_member_props", addto, type))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    if (putf(&streams->props, "    %1$s.push_back(entity_properties_t", addto))
      return IDL_RETCODE_NO_MEMORY;
  }

  const char *opt = (idl_is_member(nd) && ((const idl_member_t*)nd)->optional.value) ? "true" : "false";
  if (putf(&streams->props, "(%1$"PRIu32",%2$s)%3$s;\n",
      member_id,
      opt,
      idl_is_struct(type_spec) || idl_is_union(type_spec) ? "" : ")"))
    return IDL_RETCODE_NO_MEMORY;

  switch (get_extensibility(parent)) {
    case IDL_APPENDABLE:
      if (putf(&streams->props, "    %1$s.back().p_ext = ext_appendable;\n", addto))
        return IDL_RETCODE_NO_MEMORY;
      break;
    case IDL_MUTABLE:
      if (putf(&streams->props, "    %1$s.back().p_ext = ext_mutable;\n", addto))
        return IDL_RETCODE_NO_MEMORY;
      break;
    default:
      break;
  }

  switch (get_extensibility(type_spec)) {
    case IDL_APPENDABLE:
      if (putf(&streams->props, "    %1$s.back().e_ext = ext_appendable;\n", addto))
        return IDL_RETCODE_NO_MEMORY;
      break;
    case IDL_MUTABLE:
      if (putf(&streams->props, "    %1$s.back().e_ext = ext_mutable;\n", addto))
        return IDL_RETCODE_NO_MEMORY;
      break;
    default:
      break;
  }

  const char *bb = NULL;
  if (idl_is_base_type(type_spec)) {
    uint32_t tp = idl_mask(type_spec) & (IDL_BASE_TYPE*2-1);
    switch (tp) {
      case IDL_CHAR:
      case IDL_BOOL:
      case IDL_OCTET:
      case IDL_INT8:
      case IDL_UINT8:
        bb = "bb_8_bits";
        break;
      case IDL_SHORT:
      case IDL_USHORT:
      case IDL_INT16:
      case IDL_UINT16:
        bb = "bb_16_bits";
        break;
      case IDL_LONG:
      case IDL_ULONG:
      case IDL_INT32:
      case IDL_UINT32:
      case IDL_FLOAT:
        bb = "bb_32_bits";
        break;
      case IDL_LLONG:
      case IDL_ULLONG:
      case IDL_INT64:
      case IDL_UINT64:
      case IDL_DOUBLE:
        bb = "bb_64_bits";
        break;
  /*IDL_WCHAR:
  IDL_ANY:
  IDL_LDOUBLE:*/
    }
  } else if (idl_is_enum(type_spec)) {
    const idl_enum_t *en = type_spec;

    const idl_annotation_appl_t *appl = NULL;
    bb = "bb_32_bits";
    IDL_FOREACH(appl, en->node.annotations) {
      if (appl->annotation
       && appl->annotation->name
       && 0 == strcmp(appl->annotation->name->identifier,"bit_bound")) {
        uint32_t val = 32;  //how to get the bit bound value from the annotations
        if (val > 32)
          bb = "bb_64_bits";
        else if (val > 16)
          bb = "bb_32_bits";
        else if (val > 8)
          bb = "bb_16_bits";
        else
          bb = "bb_8_bits";
      }
    }
  }

  if (bb && putf(&streams->props, "    %1$s.back().e_bb = %2$s;\n", addto, bb))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_member_start(
  const idl_member_t *mem,
  const idl_declarator_t *decl,
  struct streams *streams)
{
  instance_location_t loc = {.parent = "instance"};
  char *accessor = NULL;
  char *type = NULL;

  const idl_type_spec_t *type_spec = NULL;
  if (idl_is_array(decl))
    type_spec = decl;
  else
    type_spec = idl_type_spec(decl);

  if (IDL_PRINTA(&accessor, get_instance_accessor, decl, &loc) < 0
   || IDL_PRINTA(&type, get_cpp11_type, type_spec, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (multi_putf(streams, ALL, "      streamer.start_member(prop, cdr_stream::stream_mode::{T}, "))
    return IDL_RETCODE_NO_MEMORY;

  if (mem->optional.value) {
    if (multi_putf(streams, ALL, "%1$s.has_value());\n", accessor)
     || multi_putf(streams, (WRITE|MOVE), "      if (%1$s.has_value()) {\n", accessor)
     || putf(&streams->read, "      %1$s = %2$s();\n", accessor, type))
      return IDL_RETCODE_NO_MEMORY;
  } else {
  if (multi_putf(streams, ALL, "true);\n"))
    return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_member_finish(
  const idl_member_t *mem,
  const idl_declarator_t *decl,
  struct streams *streams)
{
  if (mem->optional.value) {
    instance_location_t loc = {.parent = "instance"};
    char *accessor = NULL;
    if (IDL_PRINTA(&accessor, get_instance_accessor, decl, &loc) < 0
     || multi_putf(streams, (WRITE|MOVE), "      }\n")
     || multi_putf(streams, ALL, "      streamer.finish_member(prop, cdr_stream::stream_mode::{T}, %1$s.has_value());\n", accessor))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    if (multi_putf(streams, ALL, "      streamer.finish_member(prop, cdr_stream::stream_mode::{T}, true);\n"))
      return IDL_RETCODE_NO_MEMORY;
  }

  if (multi_putf(streams, ALL, "      break;\n"))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_member(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void)revisit;
  (void)path;

  struct streams *streams = user_data;
  const idl_member_t *mem = node;
  const idl_declarator_t *declarator = NULL;
  const idl_type_spec_t *type_spec = mem->type_spec;

  IDL_FOREACH(declarator, mem->declarators) {
    //generate case
    const char *fmt =
      "      case %"PRIu32":\n";

    if (multi_putf(streams, ALL, fmt, declarator->id.value)
     || add_member_start(mem, declarator, streams))
      return IDL_RETCODE_NO_MEMORY;

    instance_location_t loc = {.parent = "instance"};
    if (mem->optional.value)
      loc.type |= OPTIONAL;

    if (generate_entity_properties(mem->node.parent, type_spec, streams, "props.m_members_by_seq", declarator->id.value))
      return IDL_RETCODE_NO_MEMORY;

    // only use the @key annotations when you do not use the keylist
    if (!(pstate->flags & IDL_FLAG_KEYLIST) &&
        mem->key.value &&
        generate_entity_properties(mem->node.parent, type_spec, streams, "props.m_keys_by_seq", declarator->id.value))
      return IDL_RETCODE_NO_MEMORY;

    if (process_entity(pstate, streams, declarator, type_spec, loc)
     || add_member_finish(mem, declarator, streams))
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_case(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void)path;

  struct streams *streams = user_data;
  const idl_case_t* _case = (const idl_case_t*)node;
  const idl_switch_type_spec_t* _switch = ((const idl_union_t*)_case->node.parent)->switch_type_spec;
  bool single = (idl_degree(_case->labels) == 1),
       simple = idl_is_base_type(_case->type_spec);
  instance_location_t loc = { .parent = "instance", .type = UNION_BRANCH };

  static const char *max_start =
    "  {\n"
    "    size_t pos = streamer.position();\n"
    "    size_t alignment = streamer.alignment();\n";
  static const char *max_end =
    "    if (union_max < streamer.position()) {\n"
    "      union_max = streamer.position();\n"
    "      alignment_max = streamer.alignment();\n"
    "    }\n"
    "    streamer.position(pos);\n"
    "    streamer.alignment(alignment);\n"
    "  }\n";

  const char* read_start = simple ? "    {\n"
                                    "      %1$s obj = %2$s;\n"
                                  : "    {\n"
                                    "      %1$s obj;\n";

  const char* read_end = single   ? "      instance.%1$s(obj);\n"
                                    "    }\n"
                                    "    break;\n"
                                  : "      instance.%1$s(obj, d);\n"
                                    "    }\n"
                                    "    break;\n";

  if (revisit) {
    const char *name = get_cpp11_name(_case->declarator);

    char *accessor = NULL, *type = NULL, *value = NULL;
    if (IDL_PRINTA(&accessor, get_instance_accessor, _case->declarator, &loc) < 0 ||
        IDL_PRINTA(&type, get_cpp11_type, _case->type_spec, streams->generator) < 0 ||
        (simple && IDL_PRINTA(&value, get_cpp11_default_value, _case->type_spec, streams->generator) < 0))
      return IDL_RETCODE_NO_MEMORY;

    if (putf(&streams->read, read_start, type, value)
     || putf(&streams->max, max_start))
      return IDL_RETCODE_NO_MEMORY;

    //only read the field if the union is not read as a key stream
    if ((_switch->key.value && multi_putf(streams, ALL, "      if (!as_key) {\n"))
     || process_entity(pstate, streams, _case->declarator, _case->type_spec, loc)
     || (_switch->key.value && multi_putf(streams, ALL, "      } //!as_key\n")))
      return IDL_RETCODE_NO_MEMORY;

    if (multi_putf(streams, (WRITE | MOVE), "      break;\n")
     || putf(&streams->read, read_end, name)
     || putf(&streams->max, max_end))
      return IDL_RETCODE_NO_MEMORY;

    if (idl_next(_case))
      return IDL_RETCODE_OK;

    if (multi_putf(streams, NOMAX, "  }\n"))
      return IDL_RETCODE_NO_MEMORY;
  } else {
    if (idl_previous(_case))
      return IDL_VISIT_REVISIT;
    if (multi_putf(streams, NOMAX,  "  switch(d)\n  {\n"))
      return IDL_RETCODE_NO_MEMORY;
    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static const idl_declarator_t*
resolve_member(const idl_struct_t *type_spec, const char *member_name)
{
  if (idl_is_struct(type_spec)) {
    const idl_struct_t *_struct = (const idl_struct_t *)type_spec;
    const idl_member_t *member = NULL;
    const idl_declarator_t *decl = NULL;
    IDL_FOREACH(member, _struct->members) {
      IDL_FOREACH(decl, member->declarators) {
        if (0 == idl_strcasecmp(decl->name->identifier, member_name))
          return decl;
      }
    }
  }
  return NULL;
}

static idl_retcode_t
process_key(
  struct streams *streams,
  const idl_struct_t *_struct,
  const idl_key_t *key)
{
  const idl_type_spec_t *type_spec = _struct;
  const idl_declarator_t *decl = NULL;
  const char *fmt =
    "    {\n"
    "      entity_properties_t *ptr = &props;\n";

  if (putf(&streams->props, fmt))
    return IDL_RETCODE_NO_MEMORY;

  for (size_t i = 0; i < key->field_name->length; i++) {
    if (!(decl = resolve_member(type_spec, key->field_name->names[i]->identifier))) {
      //this happens if the key field name points to something that does not exist
      //or something that cannot be resolved, should never occur in a correctly
      //parsed idl file
      assert(0);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

    const idl_member_t *mem = (const idl_member_t *)((const idl_node_t *)decl)->parent;
    type_spec = mem->type_spec;

    if (i != 0
     && putf(&streams->props, "      ptr->m_keys_by_seq.clear();\n"
                              "      ptr->m_members_by_seq.clear();\n"
                              "      ptr->m_keys_by_id.clear();\n"
                              "      ptr->m_members_by_id.clear();\n"))
      return IDL_RETCODE_NO_MEMORY;

    if (generate_entity_properties((const idl_node_t*)_struct,type_spec,streams,"  ptr->m_keys_by_seq",  decl->id.value))
      return IDL_RETCODE_NO_MEMORY;

    if (i != 0) {
      if (putf(&streams->props, "      ptr->m_keys_by_seq.push_back(final_entry());\n"
                                "      ptr = &(*(++(ptr->m_keys_by_seq.rbegin())));\n"))
        return IDL_RETCODE_NO_MEMORY;
    } else {
      if (putf(&streams->props, "      ptr = &(*((ptr->m_keys_by_seq.rbegin())));\n"))
        return IDL_RETCODE_NO_MEMORY;
    }

    _struct = type_spec;
  }

  if (putf(&streams->props, "    }\n"))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_keylist(
  struct streams *streams,
  const idl_struct_t *_struct)
{
  const idl_key_t *key = NULL;

  if (putf(&streams->props, "    props.keylist_is_pragma = true;\n"))
    return IDL_RETCODE_NO_MEMORY;

  IDL_FOREACH(key, _struct->keylist->keys) {
    if (process_key(streams, _struct, key))
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_constructed_type_open(struct streams *streams, const idl_node_t *node)
{
  char* name = NULL;
  if (IDL_PRINTA(&name, get_cpp11_fully_scoped_name, node, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  const char *fmt =
    "template<typename T, std::enable_if_t<std::is_base_of<cdr_stream, T>::value, bool> = true >\n"
    "void {T}(T& streamer, {C}%1$s& instance, entity_properties_t &props, bool as_key) {\n";
  const char *pfmt1 =
    "template<>\n"
    "entity_properties_t& get_type_props<%s>()%s";
  const char *pfmt2 =
    " {\n"
    "  thread_local static bool initialized = false;\n"
    "  thread_local static entity_properties_t props;\n"
    "  if (!initialized) {\n";
  const char *sfmt =
    "  streamer.start_struct(props,cdr_stream::stream_mode::{T},as_key);\n";

  if (multi_putf(streams, ALL, fmt, name)
   || putf(&streams->props, pfmt1, name, pfmt2)
   || idl_fprintf(streams->generator->header.handle, pfmt1, name, ";\n\n") < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (multi_putf(streams, ALL, sfmt))
    return IDL_RETCODE_NO_MEMORY;

  const char *estr = NULL;
  switch (get_extensibility(node)) {
    case IDL_APPENDABLE:
      estr = "ext_appendable";
      break;
    case IDL_MUTABLE:
      estr = "ext_mutable";
      break;
    default:
      break;
  }

  if (estr && putf(&streams->props, "    props.e_ext = %1$s;\n", estr))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_switchbox_open(struct streams *streams)
{
  const char *fmt =
    "  bool firstcall = true;\n"
    "  while (auto &prop = streamer.next_entity(props, as_key, cdr_stream::stream_mode::{T}, firstcall)) {\n"
    "%1$s"
    "    switch (prop.m_id) {\n";
  const char *skipfmt =
    "    if (props.ignore) {\n"
    "      streamer.skip_entity(prop);\n"
    "      continue;\n"
    "    }\n";

  if (multi_putf(streams, CONST, fmt, "")
   || multi_putf(streams, READ, fmt, skipfmt))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_constructed_type_close(
  const idl_pstate_t* pstate,
  const void *node,
  struct streams *streams)
{
  static const char *fmt =
    "  streamer.finish_struct(props,cdr_stream::stream_mode::{T},as_key);\n"
    "  (void)instance;\n"
    "}\n\n";
  static const char *pfmt =
    "    props.m_members_by_seq.push_back(final_entry());\n"
    "    props.m_keys_by_seq.push_back(final_entry());\n"
    "    props.finish();\n"
    "    initialized = true;\n"
    "%1$s"
    "  }\n"
    "  return props;\n"
    "}\n\n";
  static const char *mixing_check =
    "    assert(!props.keylist_is_pragma);\n";

  bool keylist = idl_is_struct(node)
              && (pstate->flags & IDL_FLAG_KEYLIST)
              && ((const idl_struct_t*)node)->keylist;
  if (multi_putf(streams, ALL, fmt)
   || putf(&streams->props, pfmt, keylist ? "" : mixing_check))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_switchbox_close(struct streams *streams)
{
  static const char *fmt =
    "    }\n"
    "  }\n";
  static const char *rfmt =
    "      default:\n"
    "      if (prop.must_understand\n"
    "       && streamer.status(must_understand_fail))\n"
    "        return;\n"
    "      else\n"
    "        streamer.skip_entity(prop);\n"
    "      break;\n";

  if (putf(&streams->read, rfmt)
   || multi_putf(streams, ALL, fmt))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_entry_point_functions(
  struct streams *streams,
  char *fullname)
{
  const char *fmt =
    "template<typename S, std::enable_if_t<std::is_base_of<cdr_stream, S>::value, bool> = true >\n"
    "void {T}(S& str, {C}%1$s& instance, bool as_key) {\n"
    "  auto &props = get_type_props<%1$s>();\n"
    "  {T}(str, instance, props, as_key); \n"
    "}\n\n";

  if (multi_putf(streams, ALL, fmt, fullname))
    return IDL_RETCODE_NO_MEMORY;

return IDL_RETCODE_OK;
}

static idl_retcode_t
process_struct_contents(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const idl_struct_t *_struct,
  struct streams *streams)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  bool keylist = (pstate->flags & IDL_FLAG_KEYLIST) && _struct->keylist;

  size_t to_unroll = 1;
  const idl_struct_t *base = _struct;
  while (base->inherit_spec) {
    base =  (const idl_struct_t *)(base->inherit_spec->base);
    to_unroll++;
  }

  do {
    size_t depth_to_go = --to_unroll;
    base = _struct;
    while (depth_to_go--)
      base =  (const idl_struct_t *)(base->inherit_spec->base);

    if (keylist
     && (ret = process_keylist(streams, base)))
      return ret;

    const idl_member_t *member = NULL;
    IDL_FOREACH(member, base->members) {
      if ((ret = process_member(pstate, revisit, path, member, streams)))
        return ret;
    }

  } while (to_unroll);

  return ret;
}

static idl_retcode_t
process_struct(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void)path;
  struct streams *streams = user_data;
  const idl_struct_t *_struct = node;

  char *fullname = NULL;
  if (IDL_PRINTA(&fullname, get_cpp11_fully_scoped_name, node, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (revisit) {
    if (print_switchbox_close(user_data)
     || print_constructed_type_close(pstate, node, streams)
     || print_entry_point_functions(streams, fullname))
      return IDL_RETCODE_NO_MEMORY;

    return flush(streams->generator, streams);
  } else {

    idl_retcode_t ret = IDL_RETCODE_OK;
    if ((ret = print_constructed_type_open(user_data, node))
     || (ret = print_switchbox_open(user_data))
     || (ret = process_struct_contents(pstate, revisit, path, _struct, streams)))
      return ret;

    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
process_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  static const char *fmt =
    "  auto d = instance._d();\n"
    "  {T}(streamer, d);\n";
  static const char *maxfmt =
    "  max(streamer, instance._d());\n"
    "  size_t union_max = streamer.position();\n"
    "  size_t alignment_max = streamer.alignment();\n";

  struct streams *streams = user_data;

  (void)pstate;
  (void)revisit;
  (void)path;
  (void)node;

  if (multi_putf(streams, NOMAX, fmt)
   || putf(&streams->max, maxfmt))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_union(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  struct streams *streams = user_data;

  (void)pstate;
  (void)path;

  static const char *pfmt =
    "  streamer.position(union_max);\n"
    "  streamer.alignment(alignment_max);\n";

  char *fullname = NULL;
  if (IDL_PRINTA(&fullname, get_cpp11_fully_scoped_name, node, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (revisit) {
    // Force the discriminant value to be the actual one read from the CDR, not the
    // default value associated with the setter we used.  E.g.:
    //   union U switch (long) {
    //     case 1: case 2: long a;
    //     default: string b;
    //   };
    // setting a value with a(...) will initialize the discriminant to 1, even when
    // the intended discrimant value is 2; the same problem exists with the default
    // case, as well as when where there is no "default:" but the discrimant value
    // doesn't map to any case.
    //
    // FIXME: for the vast majority of the unions, this doesn't come into play this
    // simply wastes time.
    if (putf(&streams->read, "  instance._d(d);\n"))
      return IDL_RETCODE_NO_MEMORY;

    if (putf(&streams->max, pfmt)
     || print_constructed_type_close(pstate, node, user_data))
      return IDL_RETCODE_NO_MEMORY;

    return flush(streams->generator, streams);
  } else {
    if (print_constructed_type_open(user_data, node))
      return IDL_RETCODE_NO_MEMORY;
    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
process_case_label(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  struct streams *streams = user_data;
  const idl_literal_t *literal = ((const idl_case_label_t *)node)->const_expr;
  char *value = "";
  const char *casefmt;

  (void)pstate;
  (void)revisit;
  (void)path;

  if (idl_mask(node) == IDL_DEFAULT_CASE_LABEL) {
    casefmt = "    default:\n";
  } else {
    casefmt = "    case %s:\n";
    if (IDL_PRINTA(&value, get_cpp11_value, literal, streams->generator) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  if (multi_putf(streams, NOMAX, casefmt, value))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_typedef_decl(
  const idl_pstate_t* pstate,
  struct streams* streams,
  const idl_type_spec_t* type_spec,
  const idl_declarator_t* declarator)
{
  instance_location_t loc = { .parent = "instance", .type = TYPEDEF };

  static const char* fmt =
    "template<typename T, std::enable_if_t<std::is_base_of<cdr_stream, T>::value, bool> = true >\n"
    "void {T}_%1$s(T& streamer, {C}%2$s& instance, bool as_key) {\n"
    "   auto &prop = get_type_props<%3$s>();\n";
  char* name = NULL;
  if (IDL_PRINTA(&name, get_cpp11_name_typedef, declarator, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  char* fullname = NULL;
  if (IDL_PRINTA(&fullname, get_cpp11_fully_scoped_name, declarator, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  char* unrolled_name = NULL;
  const idl_type_spec_t* ts = type_spec;
  while (idl_is_sequence(ts)) {
    ts = ((const idl_sequence_t*)type_spec)->type_spec;
  }
  if (IDL_PRINTA(&unrolled_name, get_cpp11_fully_scoped_name, ts, streams->generator) < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (multi_putf(streams, ALL, fmt, name, fullname, unrolled_name))
    return IDL_RETCODE_NO_MEMORY;

  if (process_entity(pstate, streams, declarator, type_spec, loc))
    return IDL_RETCODE_NO_MEMORY;

  if (multi_putf(streams, ALL, "  (void)instance;\n}\n\n"))
    return IDL_RETCODE_NO_MEMORY;

  return flush(streams->generator, streams);
}

static idl_retcode_t
process_typedef(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void)revisit;
  (void)path;

  struct streams* streams = user_data;
  idl_typedef_t* td = (idl_typedef_t*)node;
  const idl_declarator_t* declarator;

  IDL_FOREACH(declarator, td->declarators) {
    if (process_typedef_decl(pstate, streams, td->type_spec, declarator))
     return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
process_enum(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  struct streams *str = (struct streams*)user_data;
  struct generator *gen = str->generator;
  const idl_enum_t *_enum = (const idl_enum_t *)node;
  const idl_enumerator_t *enumerator;
  uint32_t value;
  const char *enum_name = NULL;

  (void)pstate;
  (void)revisit;
  (void)path;

  char *fullname = NULL;
  if (IDL_PRINTA(&fullname, get_cpp11_fully_scoped_name, _enum, gen) < 0)
    return IDL_RETCODE_NO_MEMORY;

  const char *fmt = "template<>\n"\
                    "%s enum_conversion<%s>(uint32_t in)%s";

  if (putf(&str->props, fmt, fullname, fullname, " {\n  switch (in) {\n")
   || idl_fprintf(gen->header.handle, fmt, fullname, fullname, ";\n\n") < 0)
    return IDL_RETCODE_NO_MEMORY;

  //array of values already encountered
  uint32_t already_encountered[232],
           n_already_encountered = 0;

  IDL_FOREACH(enumerator, _enum->enumerators) {
    enum_name = get_cpp11_name(enumerator);
    value = enumerator->value.value;
    bool already_present = false;
    for (uint32_t i = 0; i < n_already_encountered && !already_present; i++) {
      if (value == already_encountered[i])
        already_present = true;
    }
    if (already_present)
      continue;

    if (n_already_encountered >= 232)  //protection against buffer overflow in already_encountered[]
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    already_encountered[n_already_encountered++] = value;

    if (putf(&str->props, "    %scase %"PRIu32":\n"
                          "    return %s::%s;\n"
                          "    break;\n",
                          enumerator == _enum->default_enumerator ? "default:\n    " : "",
                          value,
                          fullname,
                          enum_name) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  if (putf(&str->props,"  }\n}\n\n"))
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

idl_retcode_t
generate_streamers(const idl_pstate_t* pstate, struct generator *gen)
{
  struct streams streams;
  idl_visitor_t visitor;
  const char *sources[] = { NULL, NULL };

  setup_streams(&streams, gen);

  memset(&visitor, 0, sizeof(visitor));

  assert(pstate->sources);
  sources[0] = pstate->sources->path->name;
  visitor.sources = sources;

  const char *fmt = "namespace org{\n"
                    "namespace eclipse{\n"
                    "namespace cyclonedds{\n"
                    "namespace core{\n"
                    "namespace cdr{\n\n";
  if (idl_fprintf(gen->header.handle, "%s", fmt) < 0
   || idl_fprintf(gen->impl.handle, "%s", fmt) < 0)
    return IDL_RETCODE_NO_MEMORY;

  visitor.visit = IDL_STRUCT | IDL_UNION | IDL_CASE | IDL_CASE_LABEL | IDL_SWITCH_TYPE_SPEC | IDL_TYPEDEF | IDL_ENUM;
  visitor.accept[IDL_ACCEPT_STRUCT] = &process_struct;
  visitor.accept[IDL_ACCEPT_UNION] = &process_union;
  visitor.accept[IDL_ACCEPT_CASE] = &process_case;
  visitor.accept[IDL_ACCEPT_CASE_LABEL] = &process_case_label;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &process_switch_type_spec;
  visitor.accept[IDL_ACCEPT_TYPEDEF] = &process_typedef;
  visitor.accept[IDL_ACCEPT_ENUM] = &process_enum;

  if (idl_visit(pstate, pstate->root, &visitor, &streams)
   || flush(gen, &streams))
    return IDL_RETCODE_NO_MEMORY;

  fmt = "} //namespace cdr\n"
        "} //namespace core\n"
        "} //namespace cyclonedds\n"
        "} //namespace eclipse\n"
        "} //namespace org\n\n";
  if (idl_fprintf(gen->header.handle, "%s", fmt) < 0
   || idl_fprintf(gen->impl.handle, "%s", fmt) < 0)
    return IDL_RETCODE_NO_MEMORY;

  cleanup_streams(&streams);

  return IDL_RETCODE_OK;
}
