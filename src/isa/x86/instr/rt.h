#ifndef __RT_H__
#define __RT_H__

static inline void rt_decode_imm(Decode *s, Operand *op, bool load, int width) {
  assert(load);
  IFNDEF(CONFIG_ENGINE_INTERPRETER, rtl_li(s, op->preg, op->val));
}

static inline void rt_decode_reg(Decode *s, Operand *op, bool load, int width) {
  if (load && (width == 1 || width == 2)) { rtl_lr(s, op->preg, op->reg, width); }
}

static inline void rt_decode_mem(Decode *s, Operand *op, bool load, int width) {
#if 0
  if (((s->opcode == 0x80 || s->opcode == 0x81 || s->opcode == 0x83) && s->isa.ext_opcode == 7) ||
      (s->opcode == 0x1ba && s->isa.ext_opcode == 4)) {
    // fix with cmp and bt, since they do not write memory
    IFDEF(CONFIG_DIFFTEST_REF_KVM, IFNDEF(CONFIG_PA, cpu.lock = 0));
  }
#endif

  rtl_mv(s, &s->extraInfo->isa.mbr, s->extraInfo->isa.mbase);
  if (s->extraInfo->isa.midx != rz) {
    rtl_slli(s, s1, s->extraInfo->isa.midx, s->extraInfo->isa.mscale);
    rtl_add(s, &s->extraInfo->isa.mbr, &s->extraInfo->isa.mbr, s1);
  }
  if (ISNDEF(CONFIG_PA) && s->extraInfo->isa.sreg_base != NULL) {
    rtl_add(s, &s->extraInfo->isa.mbr, &s->extraInfo->isa.mbr, s->extraInfo->isa.sreg_base);
  }

  if (load) rtl_lm(s, &op->val, &s->extraInfo->isa.mbr, s->extraInfo->isa.moff, width, MMU_DYNAMIC);
}

static inline void rt_decode(Decode *s, Operand *op, bool load, int width) {
  switch (op->type) {
    case OP_TYPE_IMM: rt_decode_imm(s, op, load, width); break;
    case OP_TYPE_REG: rt_decode_reg(s, op, load, width); break;
    case OP_TYPE_MEM: rt_decode_mem(s, op, load, width); break;
  }
}

static inline def_rtl(decode_unary, bool load) {
  rt_decode(s, id_dest, load, s->extraInfo->isa.width);
}

static inline def_rtl(decode_binary, bool load_dest, bool load_src1) {
  rt_decode(s, id_dest, load_dest, s->extraInfo->isa.width);
  rt_decode(s, id_src1, load_src1, s->extraInfo->isa.width);
}



static inline def_rtl(wb_r, rtlreg_t *src) {
  rtl_sr(s, id_dest->reg, src, s->extraInfo->isa.width);
}

static inline def_rtl(wb_m, rtlreg_t *src) {
  rtl_sm(s, src, &s->extraInfo->isa.mbr, s->extraInfo->isa.moff, s->extraInfo->isa.width, MMU_DYNAMIC);
}

static inline def_rtl(wb, rtlreg_t *src) {
  if      (id_dest->type == OP_TYPE_REG) rtl_wb_r(s, src);
  else if (id_dest->type == OP_TYPE_MEM) rtl_wb_m(s, src);
  else assert(0);
}

#include "cc.h"

#endif
