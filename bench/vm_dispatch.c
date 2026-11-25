// A small register-based bytecode VM used as a dispatch-loop microbenchmark.
//
// The loop below is the classic interpreter shape: one indirect jump
// (switch -> jump table) per executed instruction, and every opcode body
// jumps back to the dispatch.  It is the same shape as SQLite's VDBE or a
// vectorised engine's expression switch, only smaller and deterministic.
//
// Build: gcc -O2 -g -fno-omit-frame-pointer -o vm_dispatch vm_dispatch.c

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Opcode {
  OP_NOP = 0,
  OP_LOADK,
  OP_MOV,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_SHL,
  OP_SHR,
  OP_LT,
  OP_LE,
  OP_EQ,
  OP_NE,
  OP_JMP,
  OP_JZ,
  OP_JNZ,
  OP_MAX,
  OP_MIN,
  OP_NEG,
  OP_NOT,
  OP_INC,
  OP_DEC,
  OP_ADDI,
  OP_MULI,
  OP_SELECT,
  OP_SWAP,
  OP_HALT
};

typedef struct {
  uint8_t Op;
  int32_t A;
  int32_t B;
  int32_t C;
} Insn;

#define NREGS 64

__attribute__((noinline)) static int64_t run(const Insn *Code, int64_t Count) {
  int64_t R[NREGS];
  memset(R, 0, sizeof(R));
  int64_t Acc = 0;
  int64_t PC = 0;
  for (int64_t Iter = 0; Iter < Count; ++Iter) {
    PC = 0;
    for (;;) {
      const Insn *I = &Code[PC];
      switch (I->Op) {
      case OP_NOP:
        ++PC;
        break;
      case OP_LOADK:
        R[I->A] = I->B;
        ++PC;
        break;
      case OP_MOV:
        R[I->A] = R[I->B];
        ++PC;
        break;
      case OP_ADD:
        R[I->A] = R[I->B] + R[I->C];
        ++PC;
        break;
      case OP_SUB:
        R[I->A] = R[I->B] - R[I->C];
        ++PC;
        break;
      case OP_MUL:
        R[I->A] = R[I->B] * R[I->C];
        ++PC;
        break;
      case OP_AND:
        R[I->A] = R[I->B] & R[I->C];
        ++PC;
        break;
      case OP_OR:
        R[I->A] = R[I->B] | R[I->C];
        ++PC;
        break;
      case OP_XOR:
        R[I->A] = R[I->B] ^ R[I->C];
        ++PC;
        break;
      case OP_SHL:
        R[I->A] = R[I->B] << (R[I->C] & 31);
        ++PC;
        break;
      case OP_SHR:
        R[I->A] = R[I->B] >> (R[I->C] & 31);
        ++PC;
        break;
      case OP_LT:
        R[I->A] = R[I->B] < R[I->C];
        ++PC;
        break;
      case OP_LE:
        R[I->A] = R[I->B] <= R[I->C];
        ++PC;
        break;
      case OP_EQ:
        R[I->A] = R[I->B] == R[I->C];
        ++PC;
        break;
      case OP_NE:
        R[I->A] = R[I->B] != R[I->C];
        ++PC;
        break;
      case OP_JMP:
        PC += I->A;
        break;
      case OP_JZ:
        PC = R[I->B] == 0 ? PC + I->A : PC + 1;
        break;
      case OP_JNZ:
        PC = R[I->B] != 0 ? PC + I->A : PC + 1;
        break;
      case OP_MAX:
        R[I->A] = R[I->B] > R[I->C] ? R[I->B] : R[I->C];
        ++PC;
        break;
      case OP_MIN:
        R[I->A] = R[I->B] < R[I->C] ? R[I->B] : R[I->C];
        ++PC;
        break;
      case OP_NEG:
        R[I->A] = -R[I->B];
        ++PC;
        break;
      case OP_NOT:
        R[I->A] = ~R[I->B];
        ++PC;
        break;
      case OP_INC:
        R[I->A] = R[I->B] + 1;
        ++PC;
        break;
      case OP_DEC:
        R[I->A] = R[I->B] - 1;
        ++PC;
        break;
      case OP_ADDI:
        R[I->A] = R[I->B] + I->C;
        ++PC;
        break;
      case OP_MULI:
        R[I->A] = R[I->B] * I->C;
        ++PC;
        break;
      case OP_SELECT:
        R[I->A] = R[I->B] != 0 ? R[I->C] : R[I->B];
        ++PC;
        break;
      case OP_SWAP:
        R[I->A] = R[I->B];
        R[I->B] = R[I->A];
        ++PC;
        break;
      case OP_HALT:
        Acc += R[1] + R[2] + R[3];
        goto next_iteration;
      default:
        fprintf(stderr, "bad opcode %d\n", I->Op);
        exit(1);
      }
    }
  next_iteration:;
  }
  return Acc + R[0];
}

// Build a deterministic program: a small loop that walks a table of values,
// applies arithmetic and accumulates.  ~60 instructions, executed many times.
static int build_program(Insn *Code) {
  int N = 0;
  const int32_t K[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
  // r0 = 0 (accumulator), r1..r10 = constants
  Code[N++] = (Insn){OP_LOADK, 0, 0, 0};
  for (int i = 0; i < 10; ++i)
    Code[N++] = (Insn){OP_LOADK, (int32_t)(i + 1), K[i], 0};
  // r20 is the loop counter; it is reset on every outer iteration because the
  // program always starts at PC == 0.
  Code[N++] = (Insn){OP_LOADK, 20, 0, 0};

  const int LoopStart = N;
  // r11 = r1 + r2 ; r12 = r11 * r3 ; r13 = r12 ^ r4
  Code[N++] = (Insn){OP_ADD, 11, 1, 2};
  Code[N++] = (Insn){OP_MUL, 12, 11, 3};
  Code[N++] = (Insn){OP_XOR, 13, 12, 4};
  Code[N++] = (Insn){OP_ADD, 14, 13, 5};
  Code[N++] = (Insn){OP_SUB, 15, 14, 6};
  Code[N++] = (Insn){OP_MAX, 16, 15, 7};
  Code[N++] = (Insn){OP_MIN, 17, 16, 8};
  Code[N++] = (Insn){OP_SHL, 18, 17, 9};
  Code[N++] = (Insn){OP_SHR, 19, 18, 10};
  Code[N++] = (Insn){OP_ADD, 0, 0, 19};
  Code[N++] = (Insn){OP_INC, 20, 20, 0};
  Code[N++] = (Insn){OP_LT, 21, 20, 9};
  {
    const int JumpIndex = N;
    Code[N++] = (Insn){OP_JNZ, LoopStart - JumpIndex, 21, 0};
  }
  Code[N++] = (Insn){OP_HALT, 0, 0, 0};
  return N;
}

int main(int argc, char **argv) {
  int64_t Count = argc > 1 ? atoll(argv[1]) : 200000;
  Insn *Code = calloc(256, sizeof(Insn));
  int N = build_program(Code);
  int64_t Acc = 0;
  for (int Rep = 0; Rep < 5; ++Rep)
    Acc += run(Code, Count);
  printf("insns=%d iterations=%lld acc=%lld\n", N, (long long)Count,
         (long long)Acc);
  free(Code);
  return 0;
}
