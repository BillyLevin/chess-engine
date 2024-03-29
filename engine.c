#include <stdint.h>
#include <stdio.h>
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

void bitboard_print(uint64_t bitboard) {
  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      if (file == 0) {
        printf("%3d", 8 - rank);
      }

      // https://www.chessprogramming.org/Square_Mapping_Considerations#Deduction_on_Files_and_Ranks
      int square = (rank * 8) + file;
      int occupation = (bitboard & (1ULL < square)) ? 1 : 0;

      printf("%3d", occupation);
    }

    printf("\n");
  }

  printf("   ");
  char ranks[8] = "abcdefgh";
  for (int i = 0; i < 8; i++) {
    printf("%3c", ranks[i]);
  }
  printf("\n");
};

board_t *board_new() {
  board_t *board = malloc(sizeof(board_t));

  board->white_pawns = 0;
  board->white_knights = 0;
  board->white_bishops = 0;
  board->white_rooks = 0;
  board->white_queens = 0;
  board->white_king = 0;

  board->black_pawns = 0;
  board->black_knights = 0;
  board->black_bishops = 0;
  board->black_rooks = 0;
  board->black_queens = 0;
  board->black_king = 0;

  return board;
}

int main() {
  board_t *board = board_new();

  bitboard_print(board->white_pawns);

  return EXIT_SUCCESS;
}
