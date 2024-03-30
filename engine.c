#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

typedef enum { WHITE, BLACK } side_t;

enum {
  WHITE_KING_CASTLE = 1,
  WHITE_QUEEN_CASTLE = 2,
  BLACK_KING_CASTLE = 4,
  BLACK_QUEEN_CASTLE = 8
};

// clang-format off
typedef enum {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8, NO_SQUARE
} square_t;

const char SQUARE_TO_READABLE[][3] = {
  "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
  "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
  "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
  "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
  "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
  "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
  "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
  "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};
// clang-format on

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

  side_t side;

  int castle_rights;

  square_t en_passant_square;
} board_t;

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
  board->side = WHITE;
  board->castle_rights = 0;
  board->en_passant_square = NO_SQUARE;

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
  printf("Side to play: %s\n", board->side == WHITE ? "White" : "Black");
  printf("Castling rights:\n");
  printf("  - White kingside: %s\n",
         board->castle_rights & WHITE_KING_CASTLE ? "yes" : "no");
  printf("  - White queenside: %s\n",
         board->castle_rights & WHITE_QUEEN_CASTLE ? "yes" : "no");
  printf("  - Black kingside: %s\n",
         board->castle_rights & BLACK_KING_CASTLE ? "yes" : "no");
  printf("  - Black queenside: %s\n",
         board->castle_rights & BLACK_QUEEN_CASTLE ? "yes" : "no");
  printf("En passant square: %s\n",
         board->en_passant_square == NO_SQUARE
             ? "-"
             : SQUARE_TO_READABLE[board->en_passant_square]);
}

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define START_E4_FEN                                                           \
  "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"
#define OPERA_GAME_FEN "1n1Rkb1r/p4ppp/4q3/4p1B1/4P3/8/PPP2PPP/2K5 b k - 1 17"

bool board_parse_FEN(board_t *board, char *fen) {
  int rank = 7;
  int file = 0;

  while (rank >= 0) {
    int square = (rank * 8) + file;

    if (isalpha(*fen)) {
      switch (*fen) {
      case 'p':
        board->black_pawns |= 1ULL << square;
        break;
      case 'n':
        board->black_knights |= 1ULL << square;
        break;
      case 'b':
        board->black_bishops |= 1ULL << square;
        break;
      case 'r':
        board->black_rooks |= 1ULL << square;
        break;
      case 'q':
        board->black_queens |= 1ULL << square;
        break;
      case 'k':
        board->black_king |= 1ULL << square;
        break;
      case 'P':
        board->white_pawns |= 1ULL << square;
        break;
      case 'N':
        board->white_knights |= 1ULL << square;
        break;
      case 'B':
        board->white_bishops |= 1ULL << square;
        break;
      case 'R':
        board->white_rooks |= 1ULL << square;
        break;
      case 'Q':
        board->white_queens |= 1ULL << square;
        break;
      case 'K':
        board->white_king |= 1ULL << square;
        break;
      default:
        printf("Invalid FEN. Character `%c` is not a valid piece notation",
               *fen);
        return false;
      }
      file++;
      fen++;
    } else if (isdigit(*fen)) {
      int increment = *fen - '0';
      file += increment;
      fen++;
    } else if (*fen == '/') {
      rank--;
      file = 0;
      fen++;
    } else if (*fen == ' ') {
      fen++;
      break;
    }
  }

  switch (*fen) {
  case 'w':
    board->side = WHITE;
    break;
  case 'b':
    board->side = BLACK;
    break;
  default:
    printf("Invalid FEN. Character `%c` is not a valid color notation", *fen);
    return false;
  }

  fen++;
  if (*fen != ' ') {
    printf("Invalid FEN. Expected a space after current color notation");
    return false;
  }

  fen++;

  int castle_check_count = 0;

  while (*fen != ' ' && castle_check_count < 4) {
    switch (*fen) {
    case 'K':
      board->castle_rights |= WHITE_KING_CASTLE;
      break;
    case 'Q':
      board->castle_rights |= WHITE_QUEEN_CASTLE;
      break;
    case 'k':
      board->castle_rights |= BLACK_KING_CASTLE;
      break;
    case 'q':
      board->castle_rights |= BLACK_QUEEN_CASTLE;
      break;
    default:
      printf(
          "Invalid FEN. Character `%c` is not a valid castling rights notation",
          *fen);
      return false;
    }

    fen++;
    castle_check_count++;
  }

  if (*fen != ' ') {
    printf("Invalid FEN. Expected a space after castling rights notation");
    return false;
  }

  fen++;

  if (*fen == '-') {
    board->en_passant_square = NO_SQUARE;
    fen++;
  } else {
    if (!(fen[0] >= 'a' && fen[0] <= 'h')) {
      printf("Invalid FEN. En passant file should be a lowercase letter "
             "between a and h");
      return false;
    }

    if (!(fen[1] >= '1' && fen[1] <= '8')) {
      printf("Invalid FEN. En passant rank should be a number between 1 and 8");
      return false;
    }

    int en_passant_file = fen[0] - 'a';
    int en_passant_rank = (fen[1] - '0') - 1;

    board->en_passant_square = (en_passant_rank * 8) + en_passant_file;
    fen += 2;
  }

  return true;
}

int main() {
  board_t *board = board_new();

  board_parse_FEN(board, START_E4_FEN);
  board_print(board);

  return EXIT_SUCCESS;
}
