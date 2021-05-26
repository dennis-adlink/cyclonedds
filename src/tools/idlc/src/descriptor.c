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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "idl/print.h"
#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"

#include "generator.h"
#include "descriptor.h"
#include "dds/ddsc/dds_opcodes.h"

#define TYPE (16)
#define SUBTYPE (8)

#define MAX_SIZE (16)

static const uint16_t nop = UINT16_MAX;

/* store each instruction separately for easy post processing and reduced
   complexity. arrays and sequences introduce a new scope and the relative
   offset to the next field is stored with the instructions for the respective
   field. this requires the generator to revert its position. using separate
   streams intruduces too much complexity. the table is also used to generate
   a key offset table after the fact */
struct instruction {
  enum {
    OPCODE,
    OFFSET,
    SIZE,
    CONSTANT,
    COUPLE,
    SINGLE,
    ELEM_OFFSET,
    JEQ_OFFSET,
    MEMBER_OFFSET,
    KEY_OFFSET,
  } type;
  union {
    struct {
      uint32_t code;
      uint32_t order; /**< key order if DDS_OP_FLAG_KEY */
    } opcode;
    struct {
      char *type;
      char *member;
    } offset; /**< name of type and member to generate offsetof */
    struct {
      char *type;
    } size; /**< name of type to generate sizeof */
    struct {
      char *value;
    } constant;
    struct {
      uint16_t high;
      uint16_t low;
    } couple;
    uint32_t single;
    struct {
      const idl_node_t *node;
      union {
        uint32_t opcode;
        uint16_t high;
      } inst;
      uint32_t addr_offs;
      uint32_t elem_offs;
    } inst_offset;
    struct {
      char *key_name;
      uint16_t len;
      uint16_t key_size;
      uint16_t order;
    } key_offset;
  } data;
};

struct constructed_type_fwd {
  const void *node;
  const struct constructed_type_fwd *next;
};

struct instructions {
  uint32_t size;        /**< available number of instructions */
  uint32_t count;       /**< used number of instructions */
  uint32_t offset;      /**< absolute offset in descriptor instructions array */
  struct instruction *table;
};

struct constructed_type {
  struct constructed_type *next;
  const void *node;
  const idl_name_t *name;
  const idl_scope_t *scope;
  struct constructed_type_fwd *fwd_decls;
  uint32_t offset;        /**< offset for the instructions of this type in the topic descriptor instruction array */
  uint32_t pl_offset;     /**< current offset in parameter list for mutable types */
  uint32_t refc;          /**< refcount for this type, used to filter out unused types */
  struct instructions instructions;
};

struct constructed_type_key {
  struct constructed_type_key *next;
  char *name;
  uint32_t size;
  uint32_t offset;
  uint32_t order;
  struct constructed_type_key *sub;
};

struct key_offs {
  uint16_t val[255];
  uint16_t n;
};

struct field {
  struct field *previous;
  const void *node;
};

struct stack_type {
  struct stack_type *previous;
  struct field *fields;
  const void *node;
  struct constructed_type *ctype;
  uint32_t offset;
  uint32_t label, labels;
};

struct alignment {
  int value;
  int ordering;
  const char *rendering;
};

struct descriptor {
  const idl_node_t *topic;
  const struct alignment *alignment; /**< alignment of topic type */
  uint32_t n_keys; /**< number of keys in topic */
  uint32_t n_opcodes; /**< number of opcodes in descriptor */
  uint32_t flags; /**< topic descriptor flag values */
  struct stack_type *type_stack;
  struct constructed_type *constructed_types;
  struct instructions key_offsets;
};

static const struct alignment alignments[] = {
#define ALIGNMENT_1BY (&alignments[0])
  { 1, 0, "1u" },
#define ALIGNMENT_2BY (&alignments[1])
  { 2, 2, "2u" },
#define ALIGNMENT_4BY (&alignments[2])
  { 4, 4, "4u" },
#define ALIGNMENT_PTR (&alignments[3])
  { 0, 6, "sizeof (char *)" },
#define ALIGNMENT_8BY (&alignments[4])
  { 8, 8, "8u" }
};

static const struct alignment *
max_alignment(const struct alignment *a, const struct alignment *b)
{
  if (!a)
    return b;
  if (!b)
    return a;
  return b->ordering > a->ordering ? b : a;
}

static bool
is_node_mutable(const idl_node_t *node)
{
  return (idl_is_struct(node) && ((idl_struct_t *)node)->extensibility == IDL_EXTENSIBILITY_MUTABLE) ||
    (idl_is_union(node) && ((idl_union_t *)node)->extensibility == IDL_EXTENSIBILITY_MUTABLE);
}

static idl_retcode_t push_field(
  struct descriptor *descriptor, const void *node, struct field **fieldp)
{
  struct stack_type *stype;
  struct field *field;
  assert(descriptor);
  assert(idl_is_declarator(node) ||
         idl_is_switch_type_spec(node) ||
         idl_is_case(node));
  stype = descriptor->type_stack;
  assert(stype);
  if (!(field = calloc(1, sizeof(*field))))
    return IDL_RETCODE_NO_MEMORY;
  field->previous = stype->fields;
  field->node = node;
  stype->fields = field;
  if (fieldp)
    *fieldp = field;
  return IDL_RETCODE_OK;
}

static void pop_field(struct descriptor *descriptor)
{
  struct field *field;
  struct stack_type *stype;
  assert(descriptor);
  stype = descriptor->type_stack;
  assert(stype);
  field = stype->fields;
  assert(field);
  stype->fields = field->previous;
  free(field);
}

static idl_retcode_t push_type(
  struct descriptor *descriptor, const void *node, struct constructed_type *ctype, struct stack_type **typep)
{
  struct stack_type *stype;
  assert(descriptor);
  assert(ctype);
  assert(idl_is_struct(node) ||
         idl_is_union(node) ||
         idl_is_sequence(node) ||
         idl_is_declarator(node));
  if (!(stype = calloc(1, sizeof(*stype))))
    return IDL_RETCODE_NO_MEMORY;
  stype->previous = descriptor->type_stack;
  stype->node = node;
  stype->ctype = ctype;
  descriptor->type_stack = stype;
  if (typep)
    *typep = stype;
  return IDL_RETCODE_OK;
}

static void pop_type(struct descriptor *descriptor)
{
  struct stack_type *stype;
  assert(descriptor);
  assert(descriptor->type_stack);
  stype = descriptor->type_stack;
  descriptor->type_stack = stype->previous;
  assert(!stype->fields || (stype->previous && stype->fields == stype->previous->fields));
  free(stype);
}

static idl_retcode_t
stash_instruction(
  struct instructions *instructions, uint32_t index, const struct instruction *inst)
{
  /* make more slots available as necessary */
  if (instructions->count == instructions->size) {
    uint32_t size = instructions->size + 100;
    struct instruction *table = instructions->table;
    if (!(table = realloc(table, size * sizeof(*table))))
      return IDL_RETCODE_NO_MEMORY;
    instructions->size = size;
    instructions->table = table;
  }

  if (index >= instructions->count) {
    index = instructions->count;
  } else {
    size_t size = instructions->count - index;
    struct instruction *table = instructions->table;
    memmove(&table[index+1], &table[index], size * sizeof(*table));
    /* update element_offset base */
    for (uint32_t i = index; i < instructions->count; i++)
      if (table[i].type == ELEM_OFFSET || table[i].type == JEQ_OFFSET || table[i].type == MEMBER_OFFSET)
        table[i].data.inst_offset.addr_offs++;
  }

  instructions->table[index] = *inst;
  instructions->count++;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
stash_opcode(
  struct descriptor *descriptor, struct instructions *instructions, uint32_t index, uint32_t code, uint32_t order)
{
  uint32_t typecode = 0;
  struct instruction inst = { OPCODE, { .opcode = { .code = code, .order = order } } };
  const struct alignment *alignment = NULL;

  descriptor->n_opcodes++;
  switch ((code & (0xffu<<24))) {
    case DDS_OP_ADR:
      if (code & DDS_OP_FLAG_KEY) {
        assert(order >  0);
      } else {
        assert(order == 0);
      }
      /* fall through */
    case DDS_OP_JEQ:
      typecode = (code >> 16) & 0xffu;
      if (typecode == DDS_OP_VAL_ARR)
        typecode = (code >> 8) & 0xffu;
      break;
    default:
      return stash_instruction(instructions, index, &inst);
  }

  switch (typecode) {
    case DDS_OP_VAL_STR:
    case DDS_OP_VAL_SEQ:
      alignment = ALIGNMENT_PTR;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_BST:
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_EXT:
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_8BY:
      alignment = ALIGNMENT_8BY;
      break;
    case DDS_OP_VAL_4BY:
      alignment = ALIGNMENT_4BY;
      break;
    case DDS_OP_VAL_2BY:
      alignment = ALIGNMENT_2BY;
      break;
    case DDS_OP_VAL_1BY:
      alignment = ALIGNMENT_1BY;
      break;
    case DDS_OP_VAL_UNI:
      /* strictly speaking a topic with a union can be optimized if all
         members have the same size, and if the non-basetype members are all
         optimizable themselves, and the alignment of the discriminant is not
         less than the alignment of the members */
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE | DDS_TOPIC_CONTAINS_UNION;
      break;
    default:
      break;
  }

  descriptor->alignment = max_alignment(descriptor->alignment, alignment);
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_offset(
  struct instructions *instructions,
  uint32_t index,
  const struct field *field)
{
  size_t cnt, pos, len, levels;
  const char *ident;
  const struct field *fld;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL } } };

  if (!field)
    return stash_instruction(instructions, index, &inst);

  assert(field);

  len = 0;
  for (fld = field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else
      ident = idl_identifier(fld->node);
    len += strlen(ident);
    if (!fld->previous)
      break;
    len += strlen(".");
  }

  pos = len;
  if (!(inst.data.offset.member = malloc(len + 1)))
    goto err_member;

  inst.data.offset.member[pos] = '\0';
  for (fld=field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else
      ident = idl_identifier(fld->node);
    cnt = strlen(ident);
    assert(pos >= cnt);
    pos -= cnt;
    memcpy(inst.data.offset.member + pos, ident, cnt);
    if (!fld->previous)
      break;
    assert(pos > 1);
    pos -= 1;
    inst.data.offset.member[pos] = '.';
  }
  assert(pos == 0);

  levels = idl_is_declarator(fld->node) != 0;
  if (IDL_PRINT(&inst.data.offset.type, print_type, idl_ancestor(fld->node, levels)) < 0)
    goto err_type;

  if (stash_instruction(instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.offset.type);
err_type:
  free(inst.data.offset.member);
err_member:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_key_offset(struct instructions *instructions, uint32_t index, char *key_name, uint16_t length, uint16_t key_size, uint16_t order)
{
  struct instruction inst = { KEY_OFFSET, { .key_offset = { .len = length, .key_size = key_size, .order = order } } };
  if (!(inst.data.key_offset.key_name = idl_strdup(key_name)))
    return IDL_RETCODE_NO_MEMORY;
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_element_offset(struct instructions *instructions, uint32_t index, const idl_node_t *node, uint16_t high, uint16_t addr_offs)
{
  struct instruction inst = { ELEM_OFFSET, { .inst_offset = { .node = node, .inst.high = high, .addr_offs = addr_offs, .elem_offs = 0 } } };
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_jeq_offset(struct instructions *instructions, uint32_t index, const idl_node_t *node, uint32_t opcode, uint16_t addr_offs)
{
  struct instruction inst = { JEQ_OFFSET, { .inst_offset = { .node = node, .inst.opcode = opcode, .addr_offs = addr_offs, .elem_offs = 0 } } };
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_member_offset(struct instructions *instructions, uint32_t index, uint16_t addr_offs)
{
  struct instruction inst = { MEMBER_OFFSET, { .inst_offset = { .addr_offs = addr_offs } } };
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_size(
  struct instructions *instructions, uint32_t index, const void *node)
{
  const idl_type_spec_t *type_spec;
  struct instruction inst = { SIZE, { .size = { NULL } } };

  if (idl_is_sequence(node)) {
    type_spec = idl_type_spec(node);

    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  } else {
    const idl_type_spec_t *array = NULL;

    type_spec = idl_type_spec(node);
    while (idl_is_alias(type_spec)) {
      if (idl_is_array(type_spec))
        array = type_spec;
      type_spec = idl_type_spec(type_spec);
    }

    if (array) {
      type_spec = idl_type_spec(array);
      /* sequences are special if non-implicit, because no implicit sequence
         is generated for typedefs of a sequence with a complex declarator */
      if (idl_is_sequence(type_spec))
        type_spec = array;
    } else {
      assert(idl_is_array(node));
      type_spec = idl_type_spec(node);
    }

    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else if (idl_is_array(type_spec)) {
      char *typestr = NULL;
      size_t len, pos;
      const idl_const_expr_t *const_expr;

      if (IDL_PRINT(&typestr, print_type, type_spec) < 0)
        goto err_type;

      len = pos = strlen(typestr);
      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      assert(const_expr);
      for (; const_expr; const_expr = idl_next(const_expr), len += 3)
        /* do nothing */;

      inst.data.size.type = malloc(len + 1);
      if (inst.data.size.type)
        memcpy(inst.data.size.type, typestr, pos);
      free(typestr);
      if (!inst.data.size.type)
        goto err_type;

      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      assert(const_expr);
      for (; const_expr; const_expr = idl_next(const_expr), pos += 3)
        memmove(inst.data.size.type + pos, "[0]", 3);
      inst.data.size.type[pos] = '\0';
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  }

  if (stash_instruction(instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.size.type);
err_type:
  return IDL_RETCODE_NO_MEMORY;
}

/* used to stash case labels. no need to take into account strings etc */
static idl_retcode_t
stash_constant(
  struct instructions *instructions, uint32_t index, const idl_const_expr_t *const_expr)
{
  int cnt = 0;
  struct instruction inst = { CONSTANT, { .constant = { NULL } } };
  char **strp = &inst.data.constant.value;

  if (idl_is_enumerator(const_expr)) {
    cnt = IDL_PRINT(strp, print_type, const_expr);
  } else {
    const idl_literal_t *literal = const_expr;

    switch (idl_type(const_expr)) {
      case IDL_CHAR:
        cnt = idl_asprintf(strp, "'%c'", literal->value.chr);
        break;
      case IDL_BOOL:
        cnt = idl_asprintf(strp, "%s", literal->value.bln ? "true" : "false");
        break;
      case IDL_INT8:
        cnt = idl_asprintf(strp, "%" PRId8, literal->value.int8);
        break;
      case IDL_OCTET:
      case IDL_UINT8:
        cnt = idl_asprintf(strp, "%" PRIu8, literal->value.uint8);
        break;
      case IDL_SHORT:
      case IDL_INT16:
        cnt = idl_asprintf(strp, "%" PRId16, literal->value.int16);
        break;
      case IDL_USHORT:
      case IDL_UINT16:
        cnt = idl_asprintf(strp, "%" PRIu16, literal->value.uint16);
        break;
      case IDL_LONG:
      case IDL_INT32:
        cnt = idl_asprintf(strp, "%" PRId32, literal->value.int32);
        break;
      case IDL_ULONG:
      case IDL_UINT32:
        cnt = idl_asprintf(strp, "%" PRIu32, literal->value.uint32);
        break;
      case IDL_LLONG:
      case IDL_INT64:
        cnt = idl_asprintf(strp, "%" PRId64, literal->value.int64);
        break;
      case IDL_ULLONG:
      case IDL_UINT64:
        cnt = idl_asprintf(strp, "%" PRIu64, literal->value.uint64);
        break;
      default:
        break;
    }
  }

  if (!strp || cnt < 0)
    goto err_value;
  if (stash_instruction(instructions, index, &inst))
    goto err_stash;
  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.constant.value);
err_value:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_couple(
  struct instructions *instructions, uint32_t index, uint16_t high, uint16_t low)
{
  struct instruction inst = { COUPLE, { .couple = { high, low } } };
  return stash_instruction(instructions, index, &inst);
}

static idl_retcode_t
stash_single(
  struct instructions *instructions, uint32_t index, uint32_t single)
{
  struct instruction inst = { SINGLE, { .single = single } };
  return stash_instruction(instructions, index, &inst);
}

static uint32_t typecode(const idl_type_spec_t *type_spec, uint32_t shift, bool struct_union_ext)
{
  assert(shift == 8 || shift == 16);
  if (idl_is_array(type_spec))
    return ((uint32_t)DDS_OP_VAL_ARR << shift);
  if (idl_is_forward(type_spec))
    return ((uint32_t)DDS_OP_VAL_EXT << shift);
  type_spec = idl_unalias(type_spec, 0u);
  assert(!idl_is_typedef(type_spec));
  switch (idl_type(type_spec)) {
    case IDL_CHAR:
      return ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_BOOL:
      return ((uint32_t)DDS_OP_VAL_1BY << shift);
    case IDL_INT8:
      return ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_OCTET:
    case IDL_UINT8:
      return ((uint32_t)DDS_OP_VAL_1BY << shift);
    case IDL_SHORT:
    case IDL_INT16:
      return ((uint32_t)DDS_OP_VAL_2BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_USHORT:
    case IDL_UINT16:
      return ((uint32_t)DDS_OP_VAL_2BY << shift);
    case IDL_LONG:
    case IDL_INT32:
      return ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULONG:
    case IDL_UINT32:
      return ((uint32_t)DDS_OP_VAL_4BY << shift);
    case IDL_LLONG:
    case IDL_INT64:
      return ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULLONG:
    case IDL_UINT64:
      return ((uint32_t)DDS_OP_VAL_8BY << shift);
    case IDL_FLOAT:
      return ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_DOUBLE:
      return ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_LDOUBLE:
      /* long doubles are not supported (yet) */
      abort();
    case IDL_STRING:
      if (idl_is_bounded(type_spec))
        return ((uint32_t)DDS_OP_VAL_BST << shift);
      return ((uint32_t)DDS_OP_VAL_STR << shift);
    case IDL_SEQUENCE:
      /* bounded sequences are not supported (yet) */
      if (idl_is_bounded(type_spec))
        abort();
      return ((uint32_t)DDS_OP_VAL_SEQ << shift);
    case IDL_ENUM:
      return ((uint32_t)DDS_OP_VAL_4BY << shift);
    case IDL_UNION:
      return ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_UNI) << shift);
    case IDL_STRUCT:
      return ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_STU) << shift);
    case IDL_BITMASK:
    {
      uint16_t bit_bound = idl_bit_bound(type_spec);
      if (bit_bound <= 8)
        return ((uint32_t)DDS_OP_VAL_1BY << shift);
      else if (bit_bound <= 16)
        return ((uint32_t)DDS_OP_VAL_2BY << shift);
      else if (bit_bound > 32)
        return ((uint32_t)DDS_OP_VAL_8BY << shift);
      else
        return ((uint32_t)DDS_OP_VAL_4BY << shift);
    }
    default:
      abort ();
      break;
  }
  return 0u;
}

static struct constructed_type *
find_ctype(const struct descriptor *descriptor, const void *node)
{
  struct constructed_type *ctype = descriptor->constructed_types;
  while (ctype && ctype->node != node)
    ctype = ctype->next;
  return ctype;
}

static struct constructed_type *
find_ctype_byname(const struct descriptor *descriptor, const idl_scope_t *scope, const idl_name_t *name)
{
  struct constructed_type *ctype = descriptor->constructed_types;
  while (ctype && (strcmp(name->identifier, ctype->name->identifier) || ctype->scope != scope))
    ctype = ctype->next;
  return ctype;
}

static const struct constructed_type_fwd *
ctype_has_fwd(const struct constructed_type *ctype, const void *node)
{
  const struct constructed_type_fwd *fwd = ctype->fwd_decls;
  while (fwd && fwd->node != node)
    fwd = fwd->next;
  return fwd;
}

static struct constructed_type *
find_ctype_byfwd(const struct descriptor *descriptor, const void *node)
{
  struct constructed_type *ctype = descriptor->constructed_types;
  while (ctype && !ctype_has_fwd(ctype, node))
    ctype = ctype->next;
  return ctype;
}

static idl_retcode_t
add_ctype(struct descriptor *descriptor, const idl_scope_t *scope, const void *node, bool is_fwd_decl, struct constructed_type **ctype)
{
  struct constructed_type *ctype1;
  struct constructed_type_fwd *fwd = NULL;

  if (!(ctype1 = calloc(1, sizeof (*ctype1))))
    goto err_ctype;
  if (is_fwd_decl) {
    if (!(fwd = calloc(1, sizeof(*fwd))))
      goto err_fwd;
    fwd->node = node;
    ctype1->fwd_decls = fwd;
  } else
    ctype1->node = node;
  ctype1->name = idl_name(node);
  ctype1->scope = scope;

  struct constructed_type **tmp = &descriptor->constructed_types;
  while (*tmp)
    tmp = &(*tmp)->next;
  *tmp = ctype1;
  if (ctype)
    *ctype = ctype1;
  return IDL_RETCODE_OK;

err_fwd:
  free(ctype1);
err_ctype:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
add_ctype_fwd(struct constructed_type *ctype, const void *node, struct constructed_type_fwd **fwd)
{
  struct constructed_type_fwd *fwd1;
  if (!(fwd1 = calloc(1, sizeof(*fwd1))))
    return IDL_RETCODE_NO_MEMORY;
  fwd1->node = node;
  fwd1->next = ctype->fwd_decls;
  ctype->fwd_decls = fwd1;

  if (fwd)
    *fwd = fwd1;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_case(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;

  (void)pstate;
  (void)path;
  if (revisit) {
    /* close inline case */
    if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
      return ret;
    pop_field(descriptor);
  } else {
    enum { SIMPLE, EXTERNAL, INLINE } case_type;
    uint32_t off, cnt;
    uint32_t opcode = DDS_OP_JEQ;
    const idl_case_t *_case = node;
    const idl_case_label_t *label;
    const idl_type_spec_t *type_spec;

    type_spec = idl_unalias(idl_type_spec(node), 0u);

    /* simple elements are embedded, complex elements are not */
    if (idl_is_array(_case->declarator)) {
      opcode |= DDS_OP_TYPE_ARR;
      case_type = INLINE;
    } else {
      opcode |= typecode(type_spec, TYPE, false);
      if (idl_is_struct(type_spec) || idl_is_union(type_spec))
        case_type = EXTERNAL;
      else if (idl_is_array(type_spec) || !(idl_is_base_type(type_spec) || idl_is_string(type_spec)))
        case_type = INLINE;
      else
        case_type = SIMPLE;
    }

    if ((ret = push_field(descriptor, _case, NULL)))
      return ret;
    if ((ret = push_field(descriptor, _case->declarator, NULL)))
      return ret;

    cnt = ctype->instructions.count + (stype->labels - stype->label) * 3;
    for (label = _case->labels; label; label = idl_next(label)) {
      off = stype->offset + 2 + (stype->label * 3);
      if (case_type == SIMPLE || case_type == INLINE) {
        /* update offset to first instruction for inline non-simple cases */
        opcode &= ~0xffffu;
        if (case_type == INLINE)
          opcode |= (cnt - off);
        /* generate union case opcode */
        if ((ret = stash_opcode(descriptor, &ctype->instructions, off++, opcode, 0u)))
          return ret;
      } else {
        uint32_t addr_offs = ctype->instructions.count;
        stash_jeq_offset(&ctype->instructions, off++, type_spec, opcode, (uint16_t)addr_offs);
      }
      /* generate union case discriminator */
      if ((ret = stash_constant(&ctype->instructions, off++, label->const_expr)))
        return ret;
      /* generate union case offset */
      if ((ret = stash_offset(&ctype->instructions, off++, stype->fields)))
        return ret;
      stype->label++;
    }

    pop_field(descriptor); /* field readded by declarator for complex types */
    if (case_type == SIMPLE || case_type == EXTERNAL) {
      pop_field(descriptor); /* field readded by declarator for complex types */
      return (case_type == SIMPLE) ? IDL_VISIT_DONT_RECURSE : IDL_VISIT_RECURSE;
    }

    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  uint32_t opcode, order;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;
  struct constructed_type *ctype = descriptor->type_stack->ctype;
  struct field *field = NULL;

  (void)revisit;

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  assert(!idl_is_typedef(type_spec) && !idl_is_array(type_spec));

  if ((ret = push_field(descriptor, node, &field)))
    return ret;

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI | typecode(type_spec, SUBTYPE, false);
  if ((order = idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path)))
    opcode |= DDS_OP_FLAG_KEY;
  if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, opcode, order)))
    return ret;
  if ((ret = stash_offset(&ctype->instructions, nop, field)))
    return ret;
  pop_field(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype;
  (void)pstate;
  (void)path;
  if (revisit) {
    uint32_t cnt;
    ctype = stype->ctype;
    assert(stype->label == stype->labels);
    cnt = (ctype->instructions.count - stype->offset) + 2;
    if ((ret = stash_single(&ctype->instructions, stype->offset + 2, stype->labels)))
      return ret;
    if ((ret = stash_couple(&ctype->instructions, stype->offset + 3, (uint16_t)cnt, 4u)))
      return ret;
    pop_type(descriptor);
  } else {
    const idl_case_t *_case;
    const idl_case_label_t *label;
    bool existing = false;

    if (find_ctype(descriptor, node))
      return IDL_RETCODE_OK | IDL_VISIT_DONT_RECURSE;

    if ((ctype = find_ctype_byname(descriptor, idl_scope(node), idl_name(node))))
    {
      if (!ctype->node)
        ctype->node = node;
      else {
        assert(ctype->node == node);
        existing = true;
      }
    } else {
      if ((ret = add_ctype(descriptor, idl_scope(node), node, false, &ctype)))
        return ret;
    }

    if (!existing) {
      switch (((idl_union_t *)node)->extensibility) {
        case IDL_EXTENSIBILITY_APPENDABLE:
          stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u);
          break;
        case IDL_EXTENSIBILITY_MUTABLE:
          stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_PLC, 0u);
          break;
        case IDL_EXTENSIBILITY_FINAL:
          break;
      }
    }

    if ((ret = push_type(descriptor, node, ctype, &stype)))
      return ret;

    stype->offset = ctype->instructions.count;
    stype->labels = stype->label = 0;

    /* determine total number of case labels as opcodes for complex elements
       are stored after case label opcodes */
    _case = ((const idl_union_t *)node)->cases;
    for (; _case; _case = idl_next(_case)) {
      for (label = _case->labels; label; label = idl_next(label))
        stype->labels++;
    }

    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_forward(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct constructed_type *ctype;
  struct descriptor *descriptor = user_data;
  idl_retcode_t ret;

  (void)pstate;
  (void)revisit;
  (void)path;
  if (find_ctype_byfwd(descriptor, node))
    return IDL_RETCODE_OK | IDL_VISIT_DONT_RECURSE;
  if ((ctype = find_ctype_byname(descriptor, idl_scope(node), idl_name(node)))) {
    if (!(ret = add_ctype_fwd(ctype, node, NULL)))
      return ret;
  } else {
    if (!(ret = add_ctype(descriptor, idl_scope(node), node, true, NULL)))
      return ret;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct constructed_type *ctype;
  (void)pstate;
  (void)path;
  if (revisit) {
    ctype = find_ctype(descriptor, node);
    assert(ctype);
    /* generate return from subroutine */
    uint32_t off = is_node_mutable(node) ? ctype->pl_offset : nop;
    if ((ret = stash_opcode(descriptor, &ctype->instructions, off, DDS_OP_RTS, 0u)))
      return ret;
    pop_type(descriptor);
  } else {
    bool existing = false;

    if (find_ctype(descriptor, node))
      return IDL_RETCODE_OK | IDL_VISIT_DONT_RECURSE;

    if ((ctype = find_ctype_byname(descriptor, idl_scope(node), idl_name(node))))
    {
      if (!ctype->node)
        ctype->node = node;
      else {
        assert(ctype->node == node);
        existing = true;
      }
    } else {
      if ((ret = add_ctype(descriptor, idl_scope(node), node, false, &ctype)))
        return ret;
    }

    if (!existing) {
      switch (((idl_struct_t *)node)->extensibility) {
        case IDL_EXTENSIBILITY_APPENDABLE:
          stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u);
          break;
        case IDL_EXTENSIBILITY_MUTABLE:
          stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_PLC, 0u);
          ctype->pl_offset = ctype->instructions.count;
          break;
        case IDL_EXTENSIBILITY_FINAL:
          break;
      }
    }

    if ((ret = push_type(descriptor, node, ctype, NULL)))
      return ret;
    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;
  const idl_type_spec_t *type_spec;

  (void)pstate;
  (void)path;

  /* resolve non-array aliases */
  type_spec = idl_unalias(idl_type_spec(node), 0u);
  if (revisit) {
    uint32_t off, cnt;
    off = stype->offset;
    cnt = ctype->instructions.count;
    /* generate data field [elem-size] */
    if ((ret = stash_size(&ctype->instructions, off + 2, node)))
      return ret;
    /* generate data field [next-insn, elem-insn] */
    if (idl_is_forward(type_spec) || idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      uint32_t addr_offs = cnt - 2u; /* minus 2 for the opcode and offset ops that are already stashed for this sequence */
      if ((ret = stash_element_offset(&ctype->instructions, off + 3, type_spec, 4u, (uint16_t)addr_offs)))
        return ret;
    } else {
      if ((ret = stash_couple(&ctype->instructions, off + 3, (uint16_t)((cnt - off) + 3u), 4u)))
        return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
        return ret;
    }
    pop_type(descriptor);
  } else {
    uint32_t off;
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_SEQ;
    struct field *field = NULL;

    opcode |= typecode(type_spec, SUBTYPE, false);
    off = ctype->instructions.count;
    if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, opcode, 0u)))
      return ret;
    if (idl_is_struct(stype->node))
      field = stype->fields;
    if ((ret = stash_offset(&ctype->instructions, nop, field)))
      return ret;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) || idl_is_base_type(type_spec)) {
      if (idl_is_bounded(type_spec)) {
        if ((ret = stash_single(&ctype->instructions, nop, idl_bound(type_spec) + 1)))
          return ret;
      }
      return IDL_RETCODE_OK;
    }

    struct stack_type *seq_stype;
    if ((ret = push_type(descriptor, node, stype->ctype, &seq_stype)))
      return ret;
    seq_stype->offset = off;
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;
  const idl_type_spec_t *type_spec;
  bool simple = false;
  uint32_t dims = 1;

  if (idl_is_array(node)) {
    dims = idl_array_size(node);
    type_spec = idl_type_spec(node);
  } else {
    type_spec = idl_unalias(idl_type_spec(node), 0u);
    assert(idl_is_array(type_spec));
    dims = idl_array_size(type_spec);
    type_spec = idl_type_spec(type_spec);
  }

  /* resolve aliases, squash multi-dimensional arrays */
  for (; idl_is_alias(type_spec); type_spec = idl_type_spec(type_spec))
    if (idl_is_array(type_spec))
      dims *= idl_array_size(type_spec);

  simple = (idl_mask(type_spec) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM)) != 0;

  if (revisit) {
    uint32_t off, cnt;

    off = stype->offset;
    cnt = ctype->instructions.count;
    /* generate data field [next-insn, elem-insn] */
    if (idl_is_forward(type_spec) || idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      uint32_t addr_offs = cnt - 3; /* minus 2 for the opcode and offset ops that are already stashed for this array */
      if ((ret = stash_element_offset(&ctype->instructions, off + 3, type_spec, 5u, (uint16_t)addr_offs)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_size(&ctype->instructions, off + 4, node)))
        return ret;
    } else {
      if ((ret = stash_couple(&ctype->instructions, off + 3, (uint16_t)((cnt - off) + 3u), 5u)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_size(&ctype->instructions, off + 4, node)))
        return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
        return ret;
    }

    pop_type(descriptor);
    stype = descriptor->type_stack;
    if (!idl_is_alias(node) && idl_is_struct(stype->node))
      pop_field(descriptor);
  } else {
    uint32_t off;
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_ARR;
    uint32_t order;
    struct field *field = NULL;

    /* type definitions do not introduce a field */
    if (idl_is_alias(node))
      assert(idl_is_sequence(stype->node));
    else if (idl_is_struct(stype->node) && (ret = push_field(descriptor, node, &field)))
      return ret;

    opcode |= typecode(type_spec, SUBTYPE, false);
    if ((order = idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path)))
      opcode |= DDS_OP_FLAG_KEY;

    off = ctype->instructions.count;
    /* generate data field opcode */
    if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(&ctype->instructions, nop, field)))
      return ret;
    /* generate data field alen */
    if ((ret = stash_single(&ctype->instructions, nop, dims)))
      return ret;

    /* short-circuit on simple types */
    if (simple) {
      if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
        /* generate data field noop [next-insn, elem-insn] */
        if ((ret = stash_single(&ctype->instructions, nop, 0)))
          return ret;
        /* generate data field bound */
        if ((ret = stash_single(&ctype->instructions, nop, idl_bound(type_spec)+1)))
          return ret;
      }
      if (!idl_is_alias(node) && idl_is_struct(stype->node))
        pop_field(descriptor);
      return IDL_RETCODE_OK;
    }

    struct stack_type *array_stype;
    if ((ret = push_type(descriptor, node, stype->ctype, &array_stype)))
      return ret;
    array_stype->offset = off;
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_declarator(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  /* delegate array type specifiers or declarators */
  if (idl_is_array(node) || idl_is_array(type_spec))
    return emit_array(pstate, revisit, path, node, user_data);

  if (revisit) {
    if (!idl_is_alias(node))
      pop_field(descriptor);
    return IDL_RETCODE_OK;
  } else {
    uint32_t opcode;
    uint32_t order;
    struct field *field = NULL;
    struct constructed_type *ctype = descriptor->type_stack->ctype;

    if (!idl_is_alias(node) && (ret = push_field(descriptor, node, &field)))
      return ret;

    if (idl_is_sequence(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;

    /* inline the type spec for seq/struct/union declarators in a union */
    if (idl_is_union(ctype->node)) {
      if (idl_is_sequence(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
      else if (idl_is_union(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
      else if (idl_is_struct(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    }

    uint32_t addr_offs = ctype->instructions.count;
    opcode = DDS_OP_ADR | typecode(type_spec, TYPE, true);
    if ((order = idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path)))
      opcode |= DDS_OP_FLAG_KEY;

    /* generate data field opcode */
    if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(&ctype->instructions, nop, field)))
      return ret;
    /* generate data field bound */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      if ((ret = stash_single(&ctype->instructions, nop, idl_bound(type_spec)+1)))
        return ret;
    } else if (idl_is_forward(type_spec) || idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      if ((ret = stash_element_offset(&ctype->instructions, nop, type_spec, 3, (uint16_t)addr_offs)))
        return ret;
    }

    if (idl_is_forward(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    else if (idl_is_union(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    else if (idl_is_struct(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;

    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_member(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct constructed_type *ctype = descriptor->type_stack->ctype;

  (void)pstate;
  (void)path;
  (void)node;
  if (!is_node_mutable(ctype->node))
    return IDL_RETCODE_OK;

  assert(idl_is_member(node));

  if (revisit) {
    /* close mutable member */
    if ((ret = stash_opcode(descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
      return ret;
  } else {
    idl_member_t *member = (idl_member_t *)node;
    uint32_t addr_offs = ctype->instructions.count
        - (ctype->pl_offset - 1 /* PLC op */)
        + 2 /* skip this JEQ and member id */
        + 1 /* skip RTS */;
    if ((ret = stash_member_offset(&ctype->instructions, ctype->pl_offset++, (uint16_t)addr_offs)))
      return ret;
    stash_single(&ctype->instructions, ctype->pl_offset++, member->id.value);

    /* update offset for previous members for this ctype */
    struct instruction *table = ctype->instructions.table;
    for (uint32_t i = 1; i < ctype->pl_offset - 2; i++)
      if (table[i].type == MEMBER_OFFSET) {
        table[i].data.inst_offset.addr_offs += 2;
      }
  }
  return IDL_VISIT_REVISIT;
}

static int print_opcode(FILE *fp, const struct instruction *inst)
{
  char buf[16];
  const char *vec[10];
  size_t len = 0;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode_primary type;
  enum dds_stream_typecode_subtype subtype;

  assert(inst->type == OPCODE);

  opcode = inst->data.opcode.code & (0xffu << 24);

  switch (opcode) {
    case DDS_OP_DLC:
      vec[len++] = "DDS_OP_DLC";
      break;
    case DDS_OP_PLC:
      vec[len++] = "DDS_OP_PLC";
      break;
    case DDS_OP_RTS:
      vec[len++] = "DDS_OP_RTS";
      break;
    case DDS_OP_KOF:
      vec[len++] = "DDS_OP_KOF";
      /* lower 16 bits contains length */
      idl_snprintf(buf, sizeof(buf), " | %u", inst->data.opcode.code & 0xffff);
      vec[len++] = buf;
      break;
    case DDS_OP_JEQ:
      vec[len++] = "DDS_OP_JEQ";
      /* lower 16 bits contain offset to next instruction */
      idl_snprintf(buf, sizeof(buf), " | %u", inst->data.opcode.code & 0xffff);
      vec[len++] = buf;
      break;

    default:
      assert(opcode == DDS_OP_ADR);
      vec[len++] = "DDS_OP_ADR";
      type = inst->data.opcode.code & (0xffu << 16);
      assert(type);
      switch (type) {
        case DDS_OP_TYPE_1BY: vec[len++] = " | DDS_OP_TYPE_1BY"; break;
        case DDS_OP_TYPE_2BY: vec[len++] = " | DDS_OP_TYPE_2BY"; break;
        case DDS_OP_TYPE_4BY: vec[len++] = " | DDS_OP_TYPE_4BY"; break;
        case DDS_OP_TYPE_8BY: vec[len++] = " | DDS_OP_TYPE_8BY"; break;
        case DDS_OP_TYPE_STR: vec[len++] = " | DDS_OP_TYPE_STR"; break;
        case DDS_OP_TYPE_BST: vec[len++] = " | DDS_OP_TYPE_BST"; break;
        case DDS_OP_TYPE_BSP: vec[len++] = " | DDS_OP_TYPE_BSP"; break;
        case DDS_OP_TYPE_SEQ: vec[len++] = " | DDS_OP_TYPE_SEQ"; break;
        case DDS_OP_TYPE_ARR: vec[len++] = " | DDS_OP_TYPE_ARR"; break;
        case DDS_OP_TYPE_UNI: vec[len++] = " | DDS_OP_TYPE_UNI"; break;
        case DDS_OP_TYPE_STU: vec[len++] = " | DDS_OP_TYPE_STU"; break;
        case DDS_OP_TYPE_ENU: vec[len++] = " | DDS_OP_TYPE_ENU"; break;
        case DDS_OP_TYPE_EXT: vec[len++] = " | DDS_OP_TYPE_EXT"; break;
      }
      subtype = inst->data.opcode.code & (0xffu << 8);
      assert(( subtype &&  (type == DDS_OP_TYPE_SEQ ||
                            type == DDS_OP_TYPE_ARR ||
                            type == DDS_OP_TYPE_UNI ||
                            type == DDS_OP_TYPE_STU))
          || (!subtype && !(type == DDS_OP_TYPE_SEQ ||
                            type == DDS_OP_TYPE_ARR ||
                            type == DDS_OP_TYPE_UNI ||
                            type == DDS_OP_TYPE_STU)));
      switch (subtype) {
        case DDS_OP_SUBTYPE_1BY: vec[len++] = " | DDS_OP_SUBTYPE_1BY"; break;
        case DDS_OP_SUBTYPE_2BY: vec[len++] = " | DDS_OP_SUBTYPE_2BY"; break;
        case DDS_OP_SUBTYPE_4BY: vec[len++] = " | DDS_OP_SUBTYPE_4BY"; break;
        case DDS_OP_SUBTYPE_8BY: vec[len++] = " | DDS_OP_SUBTYPE_8BY"; break;
        case DDS_OP_SUBTYPE_STR: vec[len++] = " | DDS_OP_SUBTYPE_STR"; break;
        case DDS_OP_SUBTYPE_BST: vec[len++] = " | DDS_OP_SUBTYPE_BST"; break;
        case DDS_OP_SUBTYPE_BSP: vec[len++] = " | DDS_OP_SUBTYPE_BSP"; break;
        case DDS_OP_SUBTYPE_SEQ: vec[len++] = " | DDS_OP_SUBTYPE_SEQ"; break;
        case DDS_OP_SUBTYPE_ARR: vec[len++] = " | DDS_OP_SUBTYPE_ARR"; break;
        case DDS_OP_SUBTYPE_UNI: vec[len++] = " | DDS_OP_SUBTYPE_UNI"; break;
        case DDS_OP_SUBTYPE_STU: vec[len++] = " | DDS_OP_SUBTYPE_STU"; break;
        case DDS_OP_SUBTYPE_ENU: vec[len++] = " | DDS_OP_SUBTYPE_ENU"; break;
      }

      if (type == DDS_OP_TYPE_UNI && (inst->data.opcode.code & DDS_OP_FLAG_DEF))
        vec[len++] = " | DDS_OP_FLAG_DEF";
      else if (inst->data.opcode.code & DDS_OP_FLAG_FP)
        vec[len++] = " | DDS_OP_FLAG_FP";
      if (inst->data.opcode.code & DDS_OP_FLAG_SGN)
        vec[len++] = " | DDS_OP_FLAG_SGN";
      if (inst->data.opcode.code & DDS_OP_FLAG_KEY)
        vec[len++] = " | DDS_OP_FLAG_KEY";
      break;
  }

  for (size_t cnt=0; cnt < len; cnt++) {
    if (fputs(vec[cnt], fp) < 0)
      return -1;
  }
  return 0;
}

static int print_offset(FILE *fp, const struct instruction *inst)
{
  const char *type, *member;
  assert(inst->type == OFFSET);
  type = inst->data.offset.type;
  member = inst->data.offset.member;
  assert((!type && !member) || (type && member));
  if (!type)
    return fputs("0u", fp);
  else
    return idl_fprintf(fp, "offsetof (%s, %s)", type, member);
}

static int print_size(FILE *fp, const struct instruction *inst)
{
  const char *type;
  assert(inst->type == SIZE);
  type = inst->data.offset.type;
  return idl_fprintf(fp, "sizeof (%s)", type) < 0 ? -1 : 0;
}

static int print_constant(FILE *fp, const struct instruction *inst)
{
  const char *value;
  value = inst->data.constant.value ? inst->data.constant.value : "0";
  return fputs(value, fp);
}

static int print_couple(FILE *fp, const struct instruction *inst)
{
  uint16_t high, low;
  assert(inst->type == COUPLE);
  high = inst->data.couple.high;
  low = inst->data.couple.low;
  return idl_fprintf(fp, "(%"PRIu16"u << 16u) + %"PRIu16"u", high, low);
}

static int print_single(FILE *fp, const struct instruction *inst)
{
  assert(inst->type == SINGLE);
  return idl_fprintf(fp, "%"PRIu32"u", inst->data.single);
}

static int print_opcodes(FILE *fp, const struct descriptor *descriptor, uint32_t *kof_offs)
{
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode_primary optype;
  enum dds_stream_typecode_subtype subtype;
  char *type = NULL;
  const char *seps[] = { ", ", ",\n  " };
  const char *sep = "  ";
  uint32_t cnt = 0;

  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  if (idl_fprintf(fp, "static const uint32_t %s_ops [] =\n{\n", type) < 0)
    return -1;

  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    if (ctype != descriptor->constructed_types)
      if (fputs(",\n\n", fp) < 0)
        return -1;

    if (idl_fprintf(fp, "  /* %s */\n", idl_identifier(ctype->node)) < 0)
      return -1;
    for (size_t op = 0, brk = 0; op < ctype->instructions.count; op++) {
      inst = &ctype->instructions.table[op];
      sep = seps[op == brk];
      switch (inst->type) {
        case OPCODE:
          sep = op ? seps[1] : "  "; /* indent, always */
          /* determine when to break line */
          opcode = inst->data.opcode.code & (0xffu << 24);
          optype = inst->data.opcode.code & (0xffu << 16);
          if (opcode == DDS_OP_RTS || opcode == DDS_OP_DLC || opcode == DDS_OP_PLC)
            brk = op + 1;
          else if (opcode == DDS_OP_JEQ)
            brk = op + 3;
          else if (optype == DDS_OP_TYPE_BST || optype == DDS_OP_TYPE_EXT)
            brk = op + 3;
          else if (optype == DDS_OP_TYPE_ARR || optype == DDS_OP_TYPE_SEQ) {
            subtype = inst->data.opcode.code & (0xffu << 8);
            brk = op + (optype == DDS_OP_TYPE_SEQ ? 2 : 3);
            if (subtype > DDS_OP_SUBTYPE_8BY && subtype != DDS_OP_SUBTYPE_BST)
              brk += 2;
          } else if (optype == DDS_OP_TYPE_UNI)
            brk = op + 4;
          else
            brk = op + 2;
          if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
            return -1;
          break;
        case OFFSET:
          if (fputs(sep, fp) < 0 || print_offset(fp, inst) < 0)
            return -1;
          break;
        case SIZE:
          if (fputs(sep, fp) < 0 || print_size(fp, inst) < 0)
            return -1;
          break;
        case CONSTANT:
          if (fputs(sep, fp) < 0 || print_constant(fp, inst) < 0)
            return -1;
          break;
        case COUPLE:
          if (fputs(sep, fp) < 0 || print_couple(fp, inst) < 0)
            return -1;
          break;
        case SINGLE:
          if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
            return -1;
          break;
        case ELEM_OFFSET:
        {
          const struct instruction inst_couple = { COUPLE, { .couple = { .high = inst->data.inst_offset.inst.high & 0xffffu, .low = inst->data.inst_offset.elem_offs & 0xffffu } } };
          if (fputs(sep, fp) < 0 || print_couple(fp, &inst_couple) < 0 || idl_fprintf(fp, " /* %s */", idl_identifier(inst->data.inst_offset.node)) < 0)
            return -1;
          break;
        }
        case JEQ_OFFSET:
        {
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = (inst->data.inst_offset.inst.opcode & ~0xffffu) | inst->data.inst_offset.elem_offs, .order = 0 } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0 || idl_fprintf(fp, " /* %s */", idl_identifier(inst->data.inst_offset.node)) < 0)
            return -1;
          brk = op + 3;
          break;
        }
        case MEMBER_OFFSET:
        {
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_JEQ & ~0xffffu) | (inst->data.inst_offset.addr_offs & 0xffffu), .order = 0 } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0)
            return -1;
          brk = op + 2;
          break;
        }
        case KEY_OFFSET:
          return -1;
      }
      cnt++;
    }
  }

  if (kof_offs)
    *kof_offs = cnt;

  for (size_t op = 0, brk = 0; op < descriptor->key_offsets.count; op++) {
    inst = &descriptor->key_offsets.table[op];
    sep = seps[op == brk];
    switch (inst->type) {
      case KEY_OFFSET:
      {
        const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_KOF & ~0xffffu) | (inst->data.key_offset.len & 0xffffu), .order = 0 } } };
        if (fputs(sep, fp) < 0 || idl_fprintf(fp, "\n  /* key: %s (size: %u, order: %u) */\n  ", inst->data.key_offset.key_name, inst->data.key_offset.key_size, inst->data.key_offset.order) < 0 || print_opcode(fp, &inst_op) < 0)
          return -1;
        brk = op + 1 + inst->data.key_offset.len;
        break;
      }
      case SINGLE:
        if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
          return -1;
        break;
      case OPCODE:
        opcode = inst->data.opcode.code & (0xffu << 24);
        assert (opcode == DDS_OP_RTS);
        if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
          return -1;
        break;
      default:
        return -1;
    }
  }

  if (fputs("\n};\n\n", fp) < 0)
    return -1;
  return 0;
}

static void free_ctype_keys(struct constructed_type_key *key)
{
  struct constructed_type_key *tmp = key, *tmp1;
  while (tmp) {
    if (tmp->name)
      free(tmp->name);
    if (tmp->sub)
      free_ctype_keys(tmp->sub);
    tmp1 = tmp;
    tmp = tmp->next;
    free(tmp1);
  }
}

static idl_retcode_t get_ctype_keys(struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_key **keys)
{
  idl_retcode_t ret;
  assert(keys);
  struct constructed_type_key *ctype_keys = NULL;
  for (uint32_t i = 0; i < ctype->instructions.count; i++) {
    const struct instruction *inst = &ctype->instructions.table[i];
    uint32_t code, size = 0, dims = 1;

    if (inst->type != OPCODE)
      continue;
    code = inst->data.opcode.code;
    if ((code & (0xffu<<24)) != DDS_OP_ADR || !(code & DDS_OP_FLAG_KEY))
      continue;

    struct constructed_type_key *key = calloc (1, sizeof(*key)), *tmp;
    if (!key)
      goto err_no_memory;
    if (ctype_keys == NULL)
      ctype_keys = key;
    else {
      tmp = ctype_keys;
      while (tmp->next)
        tmp = tmp->next;
      tmp->next = key;
    }

    key->offset = i;
    assert(inst->data.opcode.order);
    key->order = inst->data.opcode.order - 1;

    const struct instruction *inst2 = &ctype->instructions.table[i + 2];
    if ((code & (0xffu << 16)) == DDS_OP_TYPE_EXT) {
      assert(inst2->type == ELEM_OFFSET);
      const idl_node_t *node = inst2->data.inst_offset.node;
      struct constructed_type *csubtype = idl_is_forward(node) ?  find_ctype_byfwd(descriptor, node) : find_ctype(descriptor, node);
      assert(csubtype);
      if ((ret = get_ctype_keys(descriptor, csubtype, &key->sub)))
        goto err;
    } else {
      descriptor->n_keys++;
      if ((code & (0xffu << 16)) == DDS_OP_TYPE_ARR) {
        assert(i + 2 < ctype->instructions.count);
        assert(inst2->type == SINGLE);
        dims = inst2->data.single;
        code >>= 8;
      } else {
        code >>= 16;
      }

      switch (code & 0xffu) {
        case DDS_OP_VAL_1BY: size = 1; break;
        case DDS_OP_VAL_2BY: size = 2; break;
        case DDS_OP_VAL_4BY: size = 4; break;
        case DDS_OP_VAL_8BY: size = 8; break;
        /* FIXME: handle bounded strings by size too? */
        default:
          size = MAX_SIZE + 1;
          break;
      }
      key->size = size * dims;
    }

    const struct instruction *inst1 = &ctype->instructions.table[i + 1];
    assert(inst1->type == OFFSET);
    assert(inst1->data.offset.type);
    assert(inst1->data.offset.member);
    if (!(key->name = idl_strdup(inst1->data.offset.member)))
      goto err_no_memory;
  }
  *keys = ctype_keys;
  return IDL_RETCODE_OK;

err_no_memory:
  ret = IDL_RETCODE_NO_MEMORY;
err:
  free_ctype_keys(ctype_keys);
  return ret;
}

static int add_key_offset(struct descriptor *descriptor, struct constructed_type_key *key, char *name, struct key_offs *offs)
{
  char *name1;
  while (key) {
    if (idl_asprintf(&name1, "%s%s%s", name ? name : "", name ? "." : "", key->name) == -1)
      goto err;
    offs->val[offs->n] = (uint16_t)key->offset;
    offs->n++;
    if (key->sub) {
      if (add_key_offset(descriptor, key->sub, name1, offs))
        goto err_stash;
    } else {
      if (stash_key_offset(&descriptor->key_offsets, nop, name1, offs->n, (uint16_t)key->size, (uint16_t)key->order) < 0)
        goto err_stash;
      for (uint32_t n = 0; n < offs->n; n++)
        if (stash_single(&descriptor->key_offsets, nop, offs->val[n]))
          goto err_stash;
    }
    offs->n--;
    free(name1);
    key = key->next;
  }
  return 0;
err_stash:
  free(name1);
err:
  return -1;
}

static int add_key_offset_list(struct descriptor *descriptor)
{
  struct constructed_type *ctype = find_ctype(descriptor, descriptor->topic);
  assert(ctype);
  struct constructed_type_key *keys;
  if (get_ctype_keys(descriptor, ctype, &keys))
    return -1;
  struct key_offs offs = { .val = { 0 }, .n = 0 };
  add_key_offset(descriptor, keys, NULL, &offs);
  free_ctype_keys(keys);
  return 0;
}

static int print_keys(FILE *fp, struct descriptor *descriptor, bool keylist, uint32_t offset)
{
  char *typestr = NULL;
  const char *fmt, *sep="";
  uint32_t sz = 0;
  struct { const char *member; uint32_t inst_offs; } *keys = NULL;

  if (descriptor->n_keys == 0)
    return 0;
  if (!(keys = calloc(descriptor->n_keys, sizeof(*keys))))
    goto err_keys;
  if (IDL_PRINT(&typestr, print_type, descriptor->topic) < 0)
    goto err_type;

  for (uint32_t i = 0, k = 0; i < descriptor->key_offsets.count; i++) {
    const struct instruction *inst = &descriptor->key_offsets.table[i];
    if (inst->type != KEY_OFFSET)
      continue;
    sz += inst->data.key_offset.key_size;
    if (keylist) {
      uint32_t order = inst->data.key_offset.order;
      assert(order < descriptor->n_keys);
      keys[order].member = inst->data.key_offset.key_name;
      keys[order].inst_offs = i;
    } else {
      keys[k].member = inst->data.key_offset.key_name;
      keys[k].inst_offs = i;
    }
    k++;
  }

  if (sz && sz <= MAX_SIZE)
    descriptor->flags |= DDS_TOPIC_FIXED_KEY;

  fmt = "static const dds_key_descriptor_t %s_keys[%"PRIu32"] =\n{\n";
  if (idl_fprintf(fp, fmt, typestr, descriptor->n_keys) < 0)
    goto err_print;
  sep = "";
  fmt = "%s  { \"%s\", %"PRIu32" }";
  for (uint32_t k=0; k < descriptor->n_keys; k++) {
    if (idl_fprintf(fp, fmt, sep, keys[k].member, offset + keys[k].inst_offs) < 0)
      goto err_print;
    sep = ",\n";
  }
  if (fputs("\n};\n\n", fp) < 0)
    goto err_print;

  free(typestr);
  free(keys);
  return 0;
err_print:
  free(typestr);
err_type:
  free(keys);
err_keys:
  return -1;
}

static int print_flags(FILE *fp, struct descriptor *descriptor)
{
  const char *fmt;
  const char *vec[4] = { NULL };
  size_t cnt, len = 0;

  if (descriptor->flags & DDS_TOPIC_NO_OPTIMIZE)
    vec[len++] = "DDS_TOPIC_NO_OPTIMIZE";
  if (descriptor->flags & DDS_TOPIC_CONTAINS_UNION)
    vec[len++] = "DDS_TOPIC_CONTAINS_UNION";
  if (descriptor->flags & DDS_TOPIC_FIXED_KEY)
    vec[len++] = "DDS_TOPIC_FIXED_KEY";

  bool fixed_size = true;
  for (uint32_t op = 0; op < descriptor->instructions.count && fixed_size; op++)
  {
    struct instruction i = descriptor->instructions.table[op];
    if (i.type != OPCODE)
      continue;

    if (((i.data.opcode.code>>16)&0xFF) == DDS_OP_VAL_STR ||
      ((i.data.opcode.code >> 16) & 0xFF) == DDS_OP_VAL_BST ||
      ((i.data.opcode.code >> 16) & 0xFF) == DDS_OP_VAL_SEQ)
      fixed_size = false;
  }

  if (fixed_size)
    vec[len++] = "DDS_TOPIC_FIXED_SIZE";

  if (!len)
    vec[len++] = "0u";

  for (cnt=0, fmt="%s"; cnt < len; cnt++, fmt=" | %s") {
    if (idl_fprintf(fp, fmt, vec[cnt]) < 0)
      return -1;
  }

  return fputs(",\n", fp) < 0 ? -1 : 0;
}

static int print_descriptor(FILE *fp, struct descriptor *descriptor)
{
  char *name, *type;
  const char *fmt;

  if (IDL_PRINTA(&name, print_scoped_name, descriptor->topic) < 0)
    return -1;
  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  fmt = "#define TID_SER (unsigned char []){ 0x12, 0x00, 0x00, 0x00, 0xf1, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e }\n"
        "#define TID_SER_SZ 22\n"
        "const dds_topic_descriptor_t %1$s_desc =\n{\n"
        "  sizeof (%1$s),\n" /* size of type */
        "  %2$s,\n  "; /* alignment */
  if (idl_fprintf(fp, fmt, type, descriptor->alignment->rendering) < 0)
    return -1;
  if (print_flags(fp, descriptor) < 0)
    return -1;
  if (descriptor->n_keys)
    fmt = "  %1$"PRIu32"u,\n" /* number of keys */
          "  \"%2$s\",\n" /* fully qualified name in IDL */
          "  %3$s_keys,\n" /* key array */
          "  %4$"PRIu32",\n" /* number of ops */
          "  %3$s_ops,\n" /* ops array */
          "  \"\",\n" /* OpenSplice metadata */
          "  { .id = { .data = TID_SER, .sz = TID_SER_SZ }, .obj = { .data = (unsigned char[]) { 1, 2, 3}, .sz = 3 }, .n_dep = 0, .dep = NULL },\n" /* minimal type identifier and type object */
          "  { .id = { .data = TID_SER, .sz = TID_SER_SZ }, .obj = { .data = (unsigned char[]) { 1, 2, 3}, .sz = 3 }, .n_dep = 0, .dep = NULL }\n" /* complete type identifier and type object */
          "};\n";
  else
    fmt = "  %1$"PRIu32"u,\n" /* number of keys */
          "  \"%2$s\",\n" /* fully qualified name in IDL */
          "  NULL,\n" /* key array */
          "  %4$"PRIu32",\n" /* number of ops */
          "  %3$s_ops,\n" /* ops array */
          "  \"\",\n" /* OpenSplice metadata */
          "  { .id = { .data = TID_SER, .sz = TID_SER_SZ }, .obj = { .data = (unsigned char[]) { 1, 2, 3}, .sz = 3 }, .n_dep = 0, .dep = NULL },\n" /* minimal type identifier and type object */
          "  { .id = { .data = TID_SER, .sz = TID_SER_SZ }, .obj = { .data = (unsigned char[]) { 1, 2, 3}, .sz = 3 }, .n_dep = 0, .dep = NULL }\n" /* complete type identifier and type object */
          "};\n";
  if (idl_fprintf(fp, fmt, descriptor->n_keys, name, type, descriptor->n_opcodes) < 0)
    return -1;
  return 0;
}

static idl_retcode_t
resolve_offsets(struct descriptor *descriptor)
{
  /* set instruction offset for each type in descriptor */
  uint32_t offs = 0;
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    /* confirm that type is complete */
    if (!ctype->node)
      return IDL_RETCODE_SEMANTIC_ERROR;
    ctype->offset = offs;
    offs += ctype->instructions.count;
  }

  /* set offset for each ELEM_OFFSET instruction */
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    for (size_t op = 0; op < ctype->instructions.count; op++) {
      if (ctype->instructions.table[op].type == ELEM_OFFSET || ctype->instructions.table[op].type == JEQ_OFFSET)
      {
        struct instruction *inst = &ctype->instructions.table[op];
        bool found = false;
        for (struct constructed_type *ctype1 = descriptor->constructed_types; ctype1; ctype1 = ctype1->next) {
          if (ctype1->node == inst->data.inst_offset.node || ctype_has_fwd(ctype1, inst->data.inst_offset.node))
          {
            inst->data.inst_offset.elem_offs = ctype1->offset - (ctype->offset + inst->data.inst_offset.addr_offs);
            ctype1->refc++;
            found = true;
            break;
          }
        }
        if (!found)
          return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
remove_unused_types(struct descriptor *descriptor)
{
  /* remove unreferenced constructed types (which are not the topic for this descriptor) */
  struct constructed_type *ctype = descriptor->constructed_types, *prev = NULL, *next;
  while (ctype) {
    next = ctype->next;
    if (ctype->refc == 0 && descriptor->topic != ctype->node) {
      if (!prev)
        descriptor->constructed_types = next;
      else
        prev->next = next;
      free(ctype);
    } else {
      prev = ctype;
    }
    ctype = next;
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t generate_descriptor(const idl_pstate_t *pstate, struct generator *generator, const idl_node_t *node);

idl_retcode_t
generate_descriptor(
  const idl_pstate_t *pstate,
  struct generator *generator,
  const idl_node_t *node)
{
  idl_retcode_t ret;
  bool keylist;
  struct descriptor descriptor;
  idl_visitor_t visitor;
  uint32_t inst_count;

  memset(&descriptor, 0, sizeof(descriptor));
  memset(&visitor, 0, sizeof(visitor));

  visitor.visit = IDL_DECLARATOR | IDL_SEQUENCE | IDL_STRUCT | IDL_UNION | IDL_SWITCH_TYPE_SPEC | IDL_CASE | IDL_FORWARD | IDL_MEMBER;
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_CASE] = &emit_case;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;
  visitor.accept[IDL_ACCEPT_FORWARD] = &emit_forward;
  visitor.accept[IDL_ACCEPT_MEMBER] = &emit_member;

  /* must be invoked for topics only, so structs and unions */
  assert(idl_is_struct(node) || idl_is_union(node));

  descriptor.topic = node;

  if ((ret = idl_visit(pstate, node, &visitor, &descriptor)))
    goto err_emit;
  if ((ret = resolve_offsets(&descriptor)) < 0)
    goto err_offset;
  if ((ret = remove_unused_types(&descriptor)) < 0)
    goto err_remove_unused;
  if ((ret = add_key_offset_list(&descriptor)) < 0)
    goto err_add_keys;
  keylist = (pstate->flags & IDL_FLAG_KEYLIST) != 0;
  if (print_opcodes(generator->source.handle, &descriptor, &inst_count) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_keys(generator->source.handle, &descriptor, keylist, inst_count) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_descriptor(generator->source.handle, &descriptor) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }

err_print:
err_emit:
err_offset:
err_remove_unused:
err_add_keys:
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 6001)
#endif
  for (struct constructed_type *ctype = descriptor.constructed_types; ctype; ctype = ctype->next) {
    for (size_t i=0; i < ctype->instructions.count; i++) {
      struct instruction *inst = &ctype->instructions.table[i];
      switch (inst->type) {
        case OFFSET:
          if (inst->data.offset.member)
            free(inst->data.offset.member);
          if (inst->data.offset.type)
            free(inst->data.offset.type);
          break;
        case SIZE:
          if (inst->data.size.type)
            free(inst->data.size.type);
          break;
        case CONSTANT:
          if (inst->data.constant.value)
            free(inst->data.constant.value);
          break;
        case KEY_OFFSET:
          if (inst->data.key_offset.key_name)
            free(inst->data.key_offset.key_name);
        default:
          break;
      }
    }
    if (ctype->instructions.table)
      free(ctype->instructions.table);
  }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  return ret;
}
