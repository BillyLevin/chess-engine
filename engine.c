#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

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

  uint8_t fifty_move_rule_count;
} board_t;

// clang-format off
typedef enum {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8
} square_t;
// clang-format on

typedef enum {
  WHITE_PAWN,
  WHITE_KNIGHT,
  WHITE_BISHOP,
  WHITE_ROOK,
  WHITE_QUEEN,
  WHITE_KING,
  BLACK_PAWN,
  BLACK_KNIGHT,
  BLACK_BISHOP,
  BLACK_ROOK,
  BLACK_QUEEN,
  BLACK_KING,
  EMPTY,
} piece_t;

const wchar_t PIECE_UNICODE[12] = {0x2659, 0x2658, 0x2657, 0x2656,
                                   0x2655, 0x2654, 0x265F, 0x265E,
                                   0x265D, 0x265C, 0x265B, 0x265A};

void piece_print(piece_t piece) {
  setlocale(LC_CTYPE, "");
  printf("  %lc", PIECE_UNICODE[piece]);
}

void bitboard_print(uint64_t bitboard, piece_t piece) {
  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      if (file == 0) {
        printf("%3d", rank + 1);
      }

      // https://www.chessprogramming.org/Square_Mapping_Considerations#Deduction_on_Files_and_Ranks
      int square = (rank * 8) + file;
      uint64_t occupation = (bitboard & (1ULL << square));

      if (occupation > 0) {
        piece_print(piece);
      } else {
        printf("%3d", 0);
      }
    }

    printf("\n");
  }

  printf("   ");
  char ranks[8] = "abcdefgh";
  for (int i = 0; i < 8; i++) {
    printf("%3c", ranks[i]);
  }
  printf("\n\n");
};

board_t *board_new() {
  board_t *board = malloc(sizeof(board_t));

  board->white_pawns = 0ULL;
  board->white_knights = 0ULL;
  board->white_bishops = 0ULL;
  board->white_rooks = 0ULL;
  board->white_queens = 0ULL;
  board->white_king = 0ULL;

  board->black_pawns = 0ULL;
  board->black_knights = 0ULL;
  board->black_bishops = 0ULL;
  board->black_rooks = 0ULL;
  board->black_queens = 0ULL;
  board->black_king = 0ULL;

  board->fifty_move_rule_count = 0;

  return board;
}

void board_print(board_t *board) {
  uint64_t bitboards[12] = {
      board->white_pawns, board->white_knights, board->white_bishops,
      board->white_rooks, board->white_queens,  board->white_king,
      board->black_pawns, board->black_knights, board->black_bishops,
      board->black_rooks, board->black_queens,  board->black_king,
  };

  printf("\n");

  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      if (file == 0) {
        printf("%3d", rank + 1);
      }

      // https://www.chessprogramming.org/Square_Mapping_Considerations#Deduction_on_Files_and_Ranks
      int square = (rank * 8) + file;

      piece_t piece = EMPTY;

      for (int i = 0; i < 12; i++) {
        uint64_t occupation = (bitboards[i] & (1ULL << square));

        if (occupation > 0) {
          piece = i;
          break;
        }
      }

      if (piece != EMPTY) {
        piece_print(piece);
      } else {
        printf("%3d", 0);
      }
    }

    printf("\n");
  }

  printf("   ");
  char ranks[8] = "ABCDEFGH";
  for (int i = 0; i < 8; i++) {
    printf("%3c", ranks[i]);
  }
  printf("\n\nFifty move rule count: %d\n", board->fifty_move_rule_count);
}

void board_set_starting_position(board_t *board) {
  board->white_pawns |= 1ULL << A2;
  board->white_pawns |= 1ULL << B2;
  board->white_pawns |= 1ULL << C2;
  board->white_pawns |= 1ULL << D2;
  board->white_pawns |= 1ULL << E2;
  board->white_pawns |= 1ULL << F2;
  board->white_pawns |= 1ULL << G2;
  board->white_pawns |= 1ULL << H2;

  board->white_knights |= 1ULL << B1;
  board->white_knights |= 1ULL << G1;

  board->white_bishops |= 1ULL << C1;
  board->white_bishops |= 1ULL << F1;

  board->white_rooks |= 1ULL << A1;
  board->white_rooks |= 1ULL << H1;

  board->white_queens |= 1ULL << D1;
  board->white_king |= 1ULL << E1;

  board->black_knights |= 1ULL << B8;
  board->black_knights |= 1ULL << G8;

  board->black_pawns |= 1ULL << A7;
  board->black_pawns |= 1ULL << B7;
  board->black_pawns |= 1ULL << C7;
  board->black_pawns |= 1ULL << D7;
  board->black_pawns |= 1ULL << E7;
  board->black_pawns |= 1ULL << F7;
  board->black_pawns |= 1ULL << G7;
  board->black_pawns |= 1ULL << H7;

  board->black_knights |= 1ULL << B8;
  board->black_knights |= 1ULL << G8;

  board->black_bishops |= 1ULL << C8;
  board->black_bishops |= 1ULL << F8;

  board->black_rooks |= 1ULL << A8;
  board->black_rooks |= 1ULL << H8;

  board->black_queens |= 1ULL << D8;
  board->black_king |= 1ULL << E8;
}

int main() {
  board_t *board = board_new();

  board_set_starting_position(board);
  board_print(board);

  return EXIT_SUCCESS;
}
