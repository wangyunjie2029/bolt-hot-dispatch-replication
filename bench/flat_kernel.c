// Negative control: a straight-line, vectorisable kernel with no switch and no
// indirect jumps.  BOLT's dispatch replication must not find anything to
// replicate here, therefore the optimised binaries have to stay identical (and
// the runtime must not regress).
//
// Build: clang -O2 -g -fno-pie -no-pie -fcf-protection=none -Wl,-q

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline)) static double kernel(const double *A, const double *B,
                                               int N) {
  double S0 = 0, S1 = 0, S2 = 0, S3 = 0;
  for (int I = 0; I < N; I += 4) {
    S0 += A[I] * B[I];
    S1 += A[I + 1] * B[I + 1];
    S2 += A[I + 2] * B[I + 2];
    S3 += A[I + 3] * B[I + 3];
  }
  return S0 + S1 + S2 + S3;
}

int main(int argc, char **argv) {
  const int N = 1 << 16;
  const int Reps = argc > 1 ? atoi(argv[1]) : 200;
  double *A = aligned_alloc(64, N * sizeof(double));
  double *B = aligned_alloc(64, N * sizeof(double));
  for (int I = 0; I < N; ++I) {
    A[I] = (double)(I % 97) / 7.0;
    B[I] = (double)(I % 31) / 3.0;
  }
  double Acc = 0;
  for (int R = 0; R < Reps; ++R)
    Acc += kernel(A + (R & 8), B + (R & 4), N - 16);
  printf("%.6f\n", Acc);
  free(A);
  free(B);
  return 0;
}
