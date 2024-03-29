#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint64_t white_pawns;
  uint64_t white_knights;
  uint64_t white_bishops;
  uint64_t white_rooks;
  uint64_t white_queens;
  uint64_t white_king;

  uint64_t black_pawns;
  uint64_t black_knights;
  uint64_t black_bishops;
  uint64_t black_rooks;
  uint64_t black_queens;
  uint64_t black_king;
} board_t;

board_t *board_new() {
  board_t *board = malloc(sizeof(board_t));
  return board;
}

int main() { return EXIT_SUCCESS; }
