#include <common.h>
#include <monitor/monitor.h>
#include <stdlib.h>
#include <isa.h>
#include <monitor/difftest.h>
#include "tran.h"

#define BUF_SIZE 13000 //8192

uint32_t trans_buffer[BUF_SIZE] = {};
int trans_buffer_index = 0;
int tran_next_pc = NEXT_PC_SEQ;

static void clear_trans_buffer() { trans_buffer_index = 0; }
void asm_print(vaddr_t ori_pc, int instr_len, bool print_flag);
vaddr_t rv64_exec_trans_buffer(void *buf, int nr_instr, int npc_type);
void guest_getregs(CPU_state *cpu);

typedef struct TB {
  vaddr_t pc;
  int npc_type;
  vaddr_t npc;
  void *code;
  uint32_t nr_instr;
  uint32_t hit_time;
  struct TB *next;
} TB;

static TB head = { .next = NULL };

static TB* find_tb(vaddr_t pc) {
  TB *tb;
  for (tb = head.next; tb != NULL; tb = tb->next) {
    if (tb->pc == pc) return tb;
  }
  return NULL;
}

void write_ins(uint32_t ins) {
  assert(trans_buffer_index < BUF_SIZE);
  trans_buffer[trans_buffer_index++]=ins;
}

void mainloop() {
  nemu_state.state = NEMU_RUNNING;
  uint64_t total_instr = 0;
  while (1) {
    vaddr_t tb_start = cpu.pc;
    TB *tb = find_tb(tb_start);
    if (tb == NULL) {
      clear_trans_buffer();
      tran_next_pc = NEXT_PC_SEQ;
      while (1) {
        __attribute__((unused)) vaddr_t ori_pc = cpu.pc;
        __attribute__((unused)) vaddr_t seq_pc = isa_exec_once();

        if (nemu_state.state != NEMU_RUNNING) tran_next_pc = NEXT_PC_END;

#ifdef DEBUG
        asm_print(ori_pc, seq_pc - ori_pc, true);
#endif
#ifdef DIFF_TEST
        if (true)
#else
        if (tran_next_pc != NEXT_PC_SEQ)
#endif
        {
          tb = malloc(sizeof(TB));
          tb->pc = tb_start;
          tb->nr_instr = trans_buffer_index;
          tb->code = malloc(tb->nr_instr * 4);
          memcpy(tb->code, trans_buffer, tb->nr_instr * 4);
          tb->npc_type = tran_next_pc;
          tb->npc = cpu.pc;
          tb->hit_time = 0;
          tb->next = head.next;
          head.next = tb;
          break;
        }
      }
    }

    //Log("enter tb with pc = %x, nr_instr = %d", tb->pc, tb->nr_instr);
    vaddr_t next_pc = rv64_exec_trans_buffer(tb->code, tb->nr_instr, tb->npc_type);
    total_instr += tb->nr_instr;
    tb->hit_time ++;

    if (tb->npc_type == NEXT_PC_END) {
      // get cpu.eax and interpret `nemu_trap` again
      guest_getregs(&cpu);
      cpu.pc = tb_start;
      nemu_state.state = NEMU_RUNNING;
      while (nemu_state.state == NEMU_RUNNING) {
        isa_exec_once();
      }
      break;
    }

    if (tb->npc_type != NEXT_PC_SEQ) cpu.pc = next_pc;
    else cpu.pc = tb->npc;

#ifdef DIFF_TEST
    guest_getregs(&cpu);
    difftest_step(tb_start, cpu.pc);
    if (nemu_state.state == NEMU_ABORT) break;
#endif
  }

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s\33[0m at pc = " FMT_WORD "\n\n",
          (nemu_state.state == NEMU_ABORT ? "\33[1;31mABORT" :
           (nemu_state.halt_ret == 0 ? "\33[1;32mHIT GOOD TRAP" : "\33[1;31mHIT BAD TRAP")),
          nemu_state.halt_pc);
      if (nemu_state.state == NEMU_ABORT) abort();
  }

  Log("#(rv64 instr) = %ld\n", total_instr);
}
