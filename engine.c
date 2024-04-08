#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
  square_t from;
  square_t to;
  bool is_capture;
} move_t;

typedef struct {
  move_t moves[512];
  size_t count;
} move_list_t;

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

  uint8_t halfmove_clock;

  piece_t pieces[64];

  uint64_t occupancies[2];

  side_t side;

  int castle_rights;

  square_t en_passant_square;
} board_t;

const wchar_t PIECE_UNICODE[12] = {0x2659, 0x2658, 0x2657, 0x2656,
                                   0x2655, 0x2654, 0x265F, 0x265E,
                                   0x265D, 0x265C, 0x265B, 0x265A};

const uint64_t RANK_4_MASK = 4278190080ULL;
const uint64_t RANK_5_MASK = 1095216660480ULL;

const uint64_t NOT_A_FILE = 18374403900871474942ULL;
const uint64_t NOT_H_FILE = 9187201950435737471ULL;

uint64_t PAWN_ATTACKS[2][64];

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
  printf("\nRaw value: %lu\n\n", bitboard);
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

  board->halfmove_clock = 0;
  board->side = WHITE;
  board->castle_rights = 0;
  board->en_passant_square = NO_SQUARE;

  for (int i = 0; i < 64; i++) {
    board->pieces[i] = EMPTY;
  }

  board->occupancies[WHITE] = 0ULL;
  board->occupancies[BLACK] = 0ULL;

  return board;
}

void board_print(board_t *board) {
  printf("\n");

  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      if (file == 0) {
        printf("%3d", rank + 1);
      }

      int square = (rank * 8) + file;

      piece_t piece = board->pieces[square];

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

  printf("\n\nHalfmove clock count: %d\n", board->halfmove_clock);
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
#define HIGH_HALFMOVE_FEN                                                      \
  "r1bq1rk1/ppp2pbp/2np1np1/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 20 11"
#define PAWN_CAPTURES_FEN                                                      \
  "rn1qkb1r/ppp1pppp/5n2/3p1b2/2B1P3/2N5/PPPP1PPP/R1BQK1NR b KQkq - 4 4"

void board_insert_piece(board_t *board, const piece_t piece, const int square) {
  switch (piece) {
  case WHITE_PAWN:
    board->white_pawns |= 1ULL << square;
    board->pieces[square] = WHITE_PAWN;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case WHITE_KNIGHT:
    board->white_knights |= 1ULL << square;
    board->pieces[square] = WHITE_KNIGHT;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case WHITE_BISHOP:
    board->white_bishops |= 1ULL << square;
    board->pieces[square] = WHITE_BISHOP;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case WHITE_ROOK:
    board->white_rooks |= 1ULL << square;
    board->pieces[square] = WHITE_ROOK;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case WHITE_QUEEN:
    board->white_queens |= 1ULL << square;
    board->pieces[square] = WHITE_QUEEN;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case WHITE_KING:
    board->white_king |= 1ULL << square;
    board->pieces[square] = WHITE_KING;
    board->occupancies[WHITE] |= 1ULL << square;
    break;
  case BLACK_PAWN:
    board->black_pawns |= 1ULL << square;
    board->pieces[square] = BLACK_PAWN;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case BLACK_KNIGHT:
    board->black_knights |= 1ULL << square;
    board->pieces[square] = BLACK_KNIGHT;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case BLACK_BISHOP:
    board->black_bishops |= 1ULL << square;
    board->pieces[square] = BLACK_BISHOP;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case BLACK_ROOK:
    board->black_rooks |= 1ULL << square;
    board->pieces[square] = BLACK_ROOK;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case BLACK_QUEEN:
    board->black_queens |= 1ULL << square;
    board->pieces[square] = BLACK_QUEEN;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case BLACK_KING:
    board->black_king |= 1ULL << square;
    board->pieces[square] = BLACK_KING;
    board->occupancies[BLACK] |= 1ULL << square;
    break;
  case EMPTY:
    board->pieces[square] = EMPTY;
    break;
  }
}

bool board_parse_FEN(board_t *board, char *fen) {
  int rank = 7;
  int file = 0;

  while (rank >= 0) {
    int square = (rank * 8) + file;

    if (isalpha(*fen)) {
      switch (*fen) {
      case 'p':
        board_insert_piece(board, BLACK_PAWN, square);
        break;
      case 'n':
        board_insert_piece(board, BLACK_KNIGHT, square);
        break;
      case 'b':
        board_insert_piece(board, BLACK_BISHOP, square);
        break;
      case 'r':
        board_insert_piece(board, BLACK_ROOK, square);
        break;
      case 'q':
        board_insert_piece(board, BLACK_QUEEN, square);
        break;
      case 'k':
        board_insert_piece(board, BLACK_KING, square);
        break;
      case 'P':
        board_insert_piece(board, WHITE_PAWN, square);
        break;
      case 'N':
        board_insert_piece(board, WHITE_KNIGHT, square);
        break;
      case 'B':
        board_insert_piece(board, WHITE_BISHOP, square);
        break;
      case 'R':
        board_insert_piece(board, WHITE_ROOK, square);
        break;
      case 'Q':
        board_insert_piece(board, WHITE_QUEEN, square);
        break;
      case 'K':
        board_insert_piece(board, WHITE_KING, square);
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
    case '-':
      board->castle_rights = 0;
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

  if (*fen != ' ') {
    printf("Invalid FEN. Expected a space after en passant square notation");
    return false;
  }

  fen++;

  char halfmoves[6];
  int halfmove_length = 0;

  while (isdigit(*fen)) {
    halfmoves[halfmove_length] = *fen;
    halfmove_length++;
    fen++;
  }

  halfmoves[halfmove_length] = '\0';

  if (halfmove_length == 0) {
    printf("Invalid FEN. Expected a halfmove count");
    return false;
  }

  board->halfmove_clock = strtol(halfmoves, NULL, 10);
  return true;
}

uint64_t get_lsb(const uint64_t bitboard) {
  if (bitboard == 0) {
    return 0;
  }

  return bitboard & -bitboard;
}

int bitboard_pop_bit(uint64_t *bitboard) {
  uint64_t lsb = get_lsb(*bitboard);
  int square = __builtin_ctzll(lsb);
  *bitboard ^= (1ULL << square);
  return square;
}

move_t move_new(square_t from, square_t to, bool is_capture) {
  move_t move = {.from = from, .to = to, .is_capture = is_capture};
  return move;
}

move_list_t *move_list_new() {
  move_list_t *move_list = malloc(sizeof(move_list_t));
  return move_list;
}

void move_list_push(move_list_t *move_list, move_t move) {
  move_list->moves[move_list->count] = move;
  move_list->count++;
}

void move_list_print(move_list_t *move_list) {
  printf("Generated Moves:\n");
  for (size_t i = 0; i < move_list->count; i++) {
    move_t move = move_list->moves[i];

    printf("From: %s, to: %s, capture: %s\n", SQUARE_TO_READABLE[move.from],
           SQUARE_TO_READABLE[move.to], move.is_capture ? "true" : "false");
  }
  printf("\nTotal moves: %zu\n", move_list->count);
}

void generate_pawn_moves(const board_t *board, move_list_t *move_list) {
  // each occupied square is set to `1`
  uint64_t empty = ~(board->occupancies[WHITE] | board->occupancies[BLACK]);

  if (board->side == WHITE) {
    uint64_t push_destinations = (board->white_pawns << 8) & empty;

    while (push_destinations != 0) {
      int square = bitboard_pop_bit(&push_destinations);

      move_list_push(move_list, move_new(square - 8, square, false));

      uint64_t potential_double_push = 1ULL << (square + 8);

      if ((potential_double_push & RANK_4_MASK & empty) != 0) {
        move_list_push(move_list, move_new(square - 8, square + 8, false));
      }

      uint64_t attacks =
          (PAWN_ATTACKS[WHITE][square - 8]) & (board->occupancies[BLACK]);

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);
        move_list_push(move_list, move_new(square - 8, attacked_square, true));
      }
    }

  } else {
    uint64_t push_destinations = (board->black_pawns >> 8) & empty;

    while (push_destinations != 0) {
      int square = bitboard_pop_bit(&push_destinations);

      move_list_push(move_list, move_new(square + 8, square, false));

      uint64_t potential_double_push = 1ULL << (square - 8);

      if ((potential_double_push & RANK_5_MASK & empty) != 0) {
        move_list_push(move_list, move_new(square + 8, square - 8, false));
      }

      uint64_t attacks =
          (PAWN_ATTACKS[BLACK][square + 8]) & (board->occupancies[WHITE]);

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);
        move_list_push(move_list, move_new(square + 8, attacked_square, true));
      }
    }
  }
}

uint64_t generate_pawn_attack_mask(square_t square, side_t side) {
  uint64_t mask = 0ULL;
  uint64_t bitboard = 0ULL;

  bitboard |= (1ULL << square);

  if (side == WHITE) {
    uint64_t east_attacks = (bitboard << 9) & NOT_A_FILE;
    uint64_t west_attacks = (bitboard << 7) & NOT_H_FILE;
    mask |= east_attacks;
    mask |= west_attacks;
  } else {
    uint64_t east_attacks = (bitboard >> 7) & NOT_A_FILE;
    uint64_t west_attacks = (bitboard >> 9) & NOT_H_FILE;
    mask |= east_attacks;
    mask |= west_attacks;
  }

  return mask;
}

void init_pawn_attack_masks() {
  for (int i = 0; i < 64; i++) {
    PAWN_ATTACKS[WHITE][i] = generate_pawn_attack_mask(i, WHITE);
    PAWN_ATTACKS[BLACK][i] = generate_pawn_attack_mask(i, BLACK);
  }
}

int main() {
  init_pawn_attack_masks();

  board_t *board = board_new();

  board_parse_FEN(board, PAWN_CAPTURES_FEN);
  board_print(board);

  move_list_t *move_list = move_list_new();

  generate_pawn_moves(board, move_list);

  move_list_print(move_list);

  return EXIT_SUCCESS;
}
