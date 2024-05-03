#include "sys/time.h"
#include <ctype.h>
#include <editline/readline.h>
#include <limits.h>
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

typedef enum { QUIET, CAPTURE, CASTLE, PROMOTION } move_type_t;
typedef enum {
  NO_FLAG,
  KNIGHT_PROMOTION,
  BISHOP_PROMOTION,
  ROOK_PROMOTION,
  QUEEN_PROMOTION,
  EN_PASSANT,
} move_flag_t;

typedef struct {
  square_t from;
  square_t to;
  move_type_t move_type;
  move_flag_t flag;
} move_t;

typedef struct {
  move_t moves[512];
  size_t count;
} move_list_t;

typedef struct {
  uint64_t hash;
  uint8_t castle_rights;
  square_t en_passant_square;
  uint8_t halfmove_clock;
  piece_t moved_piece;
  piece_t captured_piece;
} history_item_t;

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

  uint8_t castle_rights;

  square_t en_passant_square;

  uint64_t hash;

  history_item_t history[500];
  int history_length;

  int ply;
} board_t;

typedef struct {
  // uci arguments
  int time_left;
  int moves_to_go;
  int move_time;
  int depth;

  // calculated search info
  bool stopped;
  int stop_time;
  uint64_t nodes_searched;

} search_info_t;

const wchar_t PIECE_UNICODE[12] = {0x2659, 0x2658, 0x2657, 0x2656,
                                   0x2655, 0x2654, 0x265F, 0x265E,
                                   0x265D, 0x265C, 0x265B, 0x265A};

const uint64_t RANK_4_MASK = 4278190080ULL;
const uint64_t RANK_5_MASK = 1095216660480ULL;

const uint64_t NOT_A_FILE = 18374403900871474942ULL;
const uint64_t NOT_H_FILE = 9187201950435737471ULL;
const uint64_t NOT_AB_FILE = 18229723555195321596ULL;
const uint64_t NOT_GH_FILE = 4557430888798830399ULL;

uint64_t PAWN_ATTACKS[2][64];
uint64_t KNIGHT_ATTACKS[64];
uint64_t KING_ATTACKS[64];

#define IS_RANK_1(sq) (sq >= 0 && sq <= 7)
#define IS_RANK_8(sq) (sq >= 56 && sq <= 63)

const uint64_t ROOK_MAGICS[64] = {
    0xa8002c000108020ULL,  0x4440200140003000ULL, 0x8080200010011880ULL,
    0x380180080141000ULL,  0x1a00060008211044ULL, 0x410001000a0c0008ULL,
    0x9500060004008100ULL, 0x100024284a20700ULL,  0x802140008000ULL,
    0x80c01002a00840ULL,   0x402004282011020ULL,  0x9862000820420050ULL,
    0x1001448011100ULL,    0x6432800200800400ULL, 0x40100010002000cULL,
    0x2800d0010c080ULL,    0x90c0008000803042ULL, 0x4010004000200041ULL,
    0x3010010200040ULL,    0xa40828028001000ULL,  0x123010008000430ULL,
    0x24008004020080ULL,   0x60040001104802ULL,   0x582200028400d1ULL,
    0x4000802080044000ULL, 0x408208200420308ULL,  0x610038080102000ULL,
    0x3601000900100020ULL, 0x80080040180ULL,      0xc2020080040080ULL,
    0x80084400100102ULL,   0x4022408200014401ULL, 0x40052040800082ULL,
    0xb08200280804000ULL,  0x8a80a008801000ULL,   0x4000480080801000ULL,
    0x911808800801401ULL,  0x822a003002001894ULL, 0x401068091400108aULL,
    0x4a10a00004cULL,      0x2000800640008024ULL, 0x1486408102020020ULL,
    0x100a000d50041ULL,    0x810050020b0020ULL,   0x204000800808004ULL,
    0x20048100a000cULL,    0x112000831020004ULL,  0x9000040810002ULL,
    0x440490200208200ULL,  0x8910401000200040ULL, 0x6404200050008480ULL,
    0x4b824a2010010100ULL, 0x4080801810c0080ULL,  0x400802a0080ULL,
    0x8224080110026400ULL, 0x40002c4104088200ULL, 0x1002100104a0282ULL,
    0x1208400811048021ULL, 0x3201014a40d02001ULL, 0x5100019200501ULL,
    0x101000208001005ULL,  0x2008450080702ULL,    0x1002080301d00cULL,
    0x410201ce5c030092ULL,
};

const uint64_t BISHOP_MAGICS[64] = {
    0x40210414004040ULL,   0x2290100115012200ULL, 0xa240400a6004201ULL,
    0x80a0420800480ULL,    0x4022021000000061ULL, 0x31012010200000ULL,
    0x4404421051080068ULL, 0x1040882015000ULL,    0x8048c01206021210ULL,
    0x222091024088820ULL,  0x4328110102020200ULL, 0x901cc41052000d0ULL,
    0xa828c20210000200ULL, 0x308419004a004e0ULL,  0x4000840404860881ULL,
    0x800008424020680ULL,  0x28100040100204a1ULL, 0x82001002080510ULL,
    0x9008103000204010ULL, 0x141820040c00b000ULL, 0x81010090402022ULL,
    0x14400480602000ULL,   0x8a008048443c00ULL,   0x280202060220ULL,
    0x3520100860841100ULL, 0x9810083c02080100ULL, 0x41003000620c0140ULL,
    0x6100400104010a0ULL,  0x20840000802008ULL,   0x40050a010900a080ULL,
    0x818404001041602ULL,  0x8040604006010400ULL, 0x1028044001041800ULL,
    0x80b00828108200ULL,   0xc000280c04080220ULL, 0x3010020080880081ULL,
    0x10004c0400004100ULL, 0x3010020200002080ULL, 0x202304019004020aULL,
    0x4208a0000e110ULL,    0x108018410006000ULL,  0x202210120440800ULL,
    0x100850c828001000ULL, 0x1401024204800800ULL, 0x41028800402ULL,
    0x20642300480600ULL,   0x20410200800202ULL,   0xca02480845000080ULL,
    0x140c404a0080410ULL,  0x2180a40108884441ULL, 0x4410420104980302ULL,
    0x1108040046080000ULL, 0x8141029012020008ULL, 0x894081818082800ULL,
    0x40020404628000ULL,   0x804100c010c2122ULL,  0x8168210510101200ULL,
    0x1088148121080ULL,    0x204010100c11010ULL,  0x1814102013841400ULL,
    0xc00010020602ULL,     0x1045220c040820ULL,   0x12400808070840ULL,
    0x2004012a040132ULL,
};

const size_t ROOK_OFFSETS[64] = {
    0,     4096,  6144,  8192,  10240, 12288, 14336, 16384, 20480, 22528, 23552,
    24576, 25600, 26624, 27648, 28672, 30720, 32768, 33792, 34816, 35840, 36864,
    37888, 38912, 40960, 43008, 44032, 45056, 46080, 47104, 48128, 49152, 51200,
    53248, 54272, 55296, 56320, 57344, 58368, 59392, 61440, 63488, 64512, 65536,
    66560, 67584, 68608, 69632, 71680, 73728, 74752, 75776, 76800, 77824, 78848,
    79872, 81920, 86016, 88064, 90112, 92160, 94208, 96256, 98304,
};

const size_t BISHOP_OFFSETS[64] = {
    0,    64,   96,   128,  160,  192,  224,  256,  320,  352,  384,
    416,  448,  480,  512,  544,  576,  608,  640,  768,  896,  1024,
    1152, 1184, 1216, 1248, 1280, 1408, 1920, 2432, 2560, 2592, 2624,
    2656, 2688, 2816, 3328, 3840, 3968, 4000, 4032, 4064, 4096, 4224,
    4352, 4480, 4608, 4640, 4672, 4704, 4736, 4768, 4800, 4832, 4864,
    4896, 4928, 4992, 5024, 5056, 5088, 5120, 5152, 5184,
};

const uint8_t ROOK_RELEVANT_BITS[64] = {
    12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12,
};

const uint8_t BISHOP_RELEVANT_BITS[64] = {
    6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7,
    5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7,
    7, 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6,
};

uint64_t ROOK_ATTACK_TABLE[102400];
uint64_t BISHOP_ATTACK_TABLE[5248];

// clang-format off
const int CASTLE_PERMISSIONS[64] = {
  13, 15, 15, 15, 12, 15, 15, 14,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  07, 15, 15, 15, 03, 15, 15, 11
};
// clang-format on

const char FLAG_TO_ALGEBRAIC_NOTATION[6] = {'-', 'n', 'b', 'r', 'q', '-'};

const int PIECE_VALUES[13] = {100, 300, 300, 500, 900,   10000, 100,
                              300, 300, 500, 900, 10000, 0};

const int MAX_SEARCH_DEPTH = 64;
const int INFINITE_SEARCH_TIME = -1;

typedef struct {
  uint64_t state;
} prng_t;

prng_t prng_new(uint64_t seed) {
  prng_t prng = {.state = seed};
  return prng;
}

// https://en.wikipedia.org/wiki/Xorshift
uint64_t prng_generate_random(prng_t *prng) {
  uint64_t result = prng->state;

  result ^= result >> 12;
  result ^= result << 25;
  result ^= result >> 27;

  prng->state = result;
  return result * UINT64_C(2685821657736338717);
}

// random numbers to be used for zobrist hashing
// 12 * 64 = 768 for the pieces
// + 1 for current side to move being black
// + 16 for castling rights
// + 8 for en passant files (only need the file because column is unique
// depending on white or black to move)
// = 793 numbers total
// https://www.chessprogramming.org/Zobrist_Hashing
uint64_t ZOBRIST_HASH_NUMBERS[793];

// clang-format off
const uint8_t ZOBRIST_EP_FILES[65] = {
    8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8,
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8,
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8,
    8
};
// clang-format on

void init_zobrist_hash() {
  prng_t prng = prng_new(123);

  for (int i = 0; i < 793; i++) {
    ZOBRIST_HASH_NUMBERS[i] = prng_generate_random(&prng);
  }
}

uint64_t zobrist_piece(int square, piece_t piece) {
  return ZOBRIST_HASH_NUMBERS[(square * 12) + piece];
}

uint64_t zobrist_current_side() { return ZOBRIST_HASH_NUMBERS[768]; }

uint64_t zobrist_castle(uint8_t castle_rights) {
  return ZOBRIST_HASH_NUMBERS[769 + castle_rights];
}

uint64_t zobrist_en_passant_file(int en_passant_square) {
  return ZOBRIST_HASH_NUMBERS[769 + 16 + ZOBRIST_EP_FILES[en_passant_square]];
}

uint64_t zobrist_remove_piece(board_t *board, int square) {
  piece_t piece = board->pieces[square];

  uint64_t hash = zobrist_piece(square, piece);

  uint64_t clear_bitmask = ~(1ULL << square);

  switch (piece) {
  case WHITE_PAWN:
    board->white_pawns &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case WHITE_KNIGHT:
    board->white_knights &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case WHITE_BISHOP:
    board->white_bishops &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case WHITE_ROOK:
    board->white_rooks &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case WHITE_QUEEN:
    board->white_queens &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case WHITE_KING:
    board->white_king &= clear_bitmask;
    board->occupancies[WHITE] &= clear_bitmask;
    break;
  case BLACK_PAWN:
    board->black_pawns &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  case BLACK_KNIGHT:
    board->black_knights &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  case BLACK_BISHOP:
    board->black_bishops &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  case BLACK_ROOK:
    board->black_rooks &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  case BLACK_QUEEN:
    board->black_queens &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  case BLACK_KING:
    board->black_king &= clear_bitmask;
    board->occupancies[BLACK] &= clear_bitmask;
    break;
  default:
    break;
  }

  board->pieces[square] = EMPTY;

  return hash;
}

uint64_t zobrist_add_piece(board_t *board, int square, piece_t piece) {
  board->pieces[square] = piece;

  uint64_t set_bitmask = 1ULL << square;

  switch (piece) {
  case WHITE_PAWN:
    board->white_pawns |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case WHITE_KNIGHT:
    board->white_knights |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case WHITE_BISHOP:
    board->white_bishops |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case WHITE_ROOK:
    board->white_rooks |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case WHITE_QUEEN:
    board->white_queens |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case WHITE_KING:
    board->white_king |= set_bitmask;
    board->occupancies[WHITE] |= set_bitmask;
    break;
  case BLACK_PAWN:
    board->black_pawns |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  case BLACK_KNIGHT:
    board->black_knights |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  case BLACK_BISHOP:
    board->black_bishops |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  case BLACK_ROOK:
    board->black_rooks |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  case BLACK_QUEEN:
    board->black_queens |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  case BLACK_KING:
    board->black_king |= set_bitmask;
    board->occupancies[BLACK] |= set_bitmask;
    break;
  default:
    printf("Tried to move piece from empty square!\n");
    printf("FROM: %s\n", SQUARE_TO_READABLE[square]);
    exit(EXIT_FAILURE);
  }

  return zobrist_piece(square, piece);
}

uint64_t generate_hash(const board_t *board) {
  uint64_t hash = 0ULL;

  for (int square = 0; square < 64; square++) {
    piece_t piece = board->pieces[square];

    if (piece != EMPTY) {
      hash ^= zobrist_piece(square, piece);
    }
  }

  if (board->side == BLACK) {
    hash ^= zobrist_current_side();
  }

  hash ^= zobrist_castle(board->castle_rights);

  if (board->en_passant_square != NO_SQUARE) {
    hash ^= zobrist_en_passant_file(board->en_passant_square);
  }

  return hash;
}

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
  char files[8] = "abcdefgh";
  for (int i = 0; i < 8; i++) {
    printf("%3c", files[i]);
  }
  printf("\nRaw value: %lu\n\n", bitboard);
};

void board_reset(board_t *board) {
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
  board->hash = 0ULL;

  for (int i = 0; i < 64; i++) {
    board->pieces[i] = EMPTY;
  }

  board->occupancies[WHITE] = 0ULL;
  board->occupancies[BLACK] = 0ULL;

  board->history_length = 0;
  board->ply = 0;
}

board_t *board_new() {
  board_t *board = malloc(sizeof(board_t));
  board_reset(board);
  return board;
}

void board_print(const board_t *board) {
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
  printf("Hash: 0x%luULL\n", board->hash);
  printf("History length: %d\n", board->history_length);
}

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define START_E4_FEN                                                           \
  "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"
#define OPERA_GAME_FEN "1n1Rkb1r/p4ppp/4q3/4p1B1/4P3/8/PPP2PPP/2K5 b k - 1 17"
#define HIGH_HALFMOVE_FEN                                                      \
  "r1bq1rk1/ppp2pbp/2np1np1/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 20 11"
#define PAWN_CAPTURES_WHITE_FEN                                                \
  "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define PAWN_CAPTURES_BLACK_FEN                                                \
  "rnbqkbnr/p1p1p3/3p3p/1p1p4/2P1Pp2/8/PP1P1PpP/RNBQKB1R b - e3 0 1"
#define KNIGHT_MOVES_FEN "5k2/1n6/4n3/6N1/8/3N4/8/5K2 b - - 0 1"
#define BISHOP_MOVES_FEN "6k1/1b6/4n2P/8/1n4B1/1B3N2/1N6/2b2K1 b - - 0 1"
#define ROOK_MOVES_FEN "6k1/8/5r1p/8/1nR5/5N2/8/6K1 b - - 0 1"
#define QUEEN_MOVES_FEN "6k1/7P/4nq2/8/1nQ5/5N2/1N6/6K1 b - - 0 1"
#define CASTLING_BASIC_FEN "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"
#define CASTLING_NO_KINGSIDE_FEN "r3k2r/8/8/2b5/2B5/8/8/R3K2R w KQkq - 0 1"
#define CASTLING_NO_QUEENSIDE_FEN "r3k2r/8/8/5B2/5b2/8/8/R3K2R w KQkq - 0 1"

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

  char halfmoves[6] = {'\0'};
  int halfmove_length = 0;

  while (isdigit(*fen)) {
    halfmoves[halfmove_length] = *fen;
    halfmove_length++;
    fen++;
  }

  if (halfmove_length == 0) {
    printf("Invalid FEN. Expected a halfmove count");
    return false;
  }

  board->halfmove_clock = strtol(halfmoves, NULL, 10);

  board->hash = generate_hash(board);
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

move_t move_new(square_t from, square_t to, move_type_t move_type,
                move_flag_t flag) {
  move_t move = {.from = from, .to = to, .move_type = move_type, .flag = flag};
  return move;
}

move_list_t *move_list_new() {
  move_list_t *move_list = malloc(sizeof(move_list_t));
  move_list->count = 0;
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

    printf("From: %s, to: %s, capture: %s, promotion: %s, en passant: %s, "
           "castling: %s\n",
           SQUARE_TO_READABLE[move.from], SQUARE_TO_READABLE[move.to],
           move.move_type == CAPTURE ? "yes" : "no",
           move.move_type == PROMOTION ? "yes" : "no",
           move.flag == EN_PASSANT ? "yes" : "no",
           move.move_type == CASTLE ? "yes" : "no");
  }
  printf("\nTotal moves: %zu\n", move_list->count);
}

bool is_promotion(square_t destination, side_t side) {
  return (side == WHITE && IS_RANK_8(destination)) ||
         (side == BLACK && IS_RANK_1(destination));
}

void generate_pawn_moves(const board_t *board, move_list_t *move_list) {
  // each occupied square is set to `1`
  uint64_t empty = ~(board->occupancies[WHITE] | board->occupancies[BLACK]);

  if (board->side == WHITE) {
    uint64_t pawns = board->white_pawns;

    while (pawns != 0) {
      int from_square = bitboard_pop_bit(&pawns);

      uint64_t potential_single_push = 1ULL << (from_square + 8);

      if ((potential_single_push & empty) != 0) {
        if (is_promotion(from_square + 8, WHITE)) {
          move_list_push(move_list, move_new(from_square, from_square + 8,
                                             PROMOTION, QUEEN_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square + 8,
                                             PROMOTION, ROOK_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square + 8,
                                             PROMOTION, BISHOP_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square + 8,
                                             PROMOTION, KNIGHT_PROMOTION));
        } else {
          move_list_push(move_list, move_new(from_square, from_square + 8,
                                             QUIET, NO_FLAG));
        }

        uint64_t potential_double_push = 1ULL << (from_square + 16);

        if ((potential_double_push & RANK_4_MASK & empty) != 0) {
          move_list_push(move_list, move_new(from_square, from_square + 16,
                                             QUIET, NO_FLAG));
        }
      }

      uint64_t en_passant_bitboard = board->en_passant_square != NO_SQUARE
                                         ? (1ULL << board->en_passant_square)
                                         : 0ULL;

      uint64_t enemy = board->occupancies[BLACK] | en_passant_bitboard;
      uint64_t attacks = (PAWN_ATTACKS[WHITE][from_square]) & enemy;

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);

        if (is_promotion(attacked_square, WHITE)) {
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, QUEEN_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, ROOK_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, BISHOP_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, KNIGHT_PROMOTION));
        } else {
          move_list_push(move_list,
                         move_new(from_square, attacked_square, CAPTURE,
                                  attacked_square == board->en_passant_square
                                      ? EN_PASSANT
                                      : NO_FLAG));
        }
      }
    }

  } else {
    uint64_t pawns = board->black_pawns;

    while (pawns != 0) {
      int from_square = bitboard_pop_bit(&pawns);

      uint64_t potential_single_push = 1ULL << (from_square - 8);

      if ((potential_single_push & empty) != 0) {
        if (is_promotion(from_square - 8, BLACK)) {
          move_list_push(move_list, move_new(from_square, from_square - 8,
                                             PROMOTION, QUEEN_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square - 8,
                                             PROMOTION, ROOK_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square - 8,
                                             PROMOTION, BISHOP_PROMOTION));
          move_list_push(move_list, move_new(from_square, from_square - 8,
                                             PROMOTION, KNIGHT_PROMOTION));
        } else {
          move_list_push(move_list, move_new(from_square, from_square - 8,
                                             QUIET, NO_FLAG));
        }

        uint64_t potential_double_push = 1ULL << (from_square - 16);

        if ((potential_double_push & RANK_5_MASK & empty) != 0) {
          move_list_push(move_list, move_new(from_square, from_square - 16,
                                             QUIET, NO_FLAG));
        }
      }

      uint64_t en_passant_bitboard = board->en_passant_square != NO_SQUARE
                                         ? (1ULL << board->en_passant_square)
                                         : 0ULL;

      uint64_t enemy = board->occupancies[WHITE] | en_passant_bitboard;

      uint64_t attacks = (PAWN_ATTACKS[BLACK][from_square]) & enemy;

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);

        if (is_promotion(attacked_square, BLACK)) {
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, QUEEN_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, ROOK_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, BISHOP_PROMOTION));
          move_list_push(move_list, move_new(from_square, attacked_square,
                                             PROMOTION, KNIGHT_PROMOTION));
        } else {
          move_list_push(move_list,
                         move_new(from_square, attacked_square, CAPTURE,
                                  attacked_square == board->en_passant_square
                                      ? EN_PASSANT
                                      : NO_FLAG));
        }
      }
    }
  }
}

void generate_knight_moves(const board_t *board, move_list_t *move_list) {
  uint64_t knights =
      board->side == WHITE ? board->white_knights : board->black_knights;

  side_t current_side = board->side;

  uint64_t current_side_occupancy = board->occupancies[current_side];
  uint64_t enemy_occupancy = board->occupancies[current_side ^ 1];

  while (knights != 0) {
    square_t from_square = bitboard_pop_bit(&knights);

    uint64_t knight_moves =
        (KNIGHT_ATTACKS[from_square]) & ~current_side_occupancy;

    while (knight_moves != 0) {
      square_t to_square = bitboard_pop_bit(&knight_moves);

      bool is_capture = ((1ULL << to_square) & enemy_occupancy) != 0;

      if (is_capture) {
        move_list_push(move_list,
                       move_new(from_square, to_square, CAPTURE, NO_FLAG));
      } else {
        move_list_push(move_list,
                       move_new(from_square, to_square, QUIET, NO_FLAG));
      }
    }
  }
}

uint64_t generate_pawn_attack_mask(square_t square, side_t side) {
  uint64_t mask = 0ULL;
  uint64_t bitboard = 1ULL << square;

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

uint64_t generate_knight_attack_mask(square_t square) {
  uint64_t mask = 0ULL;
  uint64_t bitboard = 1ULL << square;

  uint64_t north_north_east = (bitboard << 17) & NOT_A_FILE;
  mask |= north_north_east;

  uint64_t north_east_east = (bitboard << 10) & NOT_AB_FILE;
  mask |= north_east_east;

  uint64_t south_east_east = (bitboard >> 6) & NOT_AB_FILE;
  mask |= south_east_east;

  uint64_t south_south_east = (bitboard >> 15) & NOT_A_FILE;
  mask |= south_south_east;

  uint64_t north_north_west = (bitboard << 15) & NOT_H_FILE;
  mask |= north_north_west;

  uint64_t north_west_west = (bitboard << 6) & NOT_GH_FILE;
  mask |= north_west_west;

  uint64_t south_west_west = (bitboard >> 10) & NOT_GH_FILE;
  mask |= south_west_west;

  uint64_t south_south_west = (bitboard >> 17) & NOT_H_FILE;
  mask |= south_south_west;

  return mask;
}

uint64_t east_one(uint64_t bits) { return (bits << 1) & NOT_A_FILE; }
uint64_t west_one(uint64_t bits) { return (bits >> 1) & NOT_H_FILE; }
uint64_t north_one(uint64_t bits) { return bits << 8; }
uint64_t south_one(uint64_t bits) { return bits >> 8; }

uint64_t generate_king_attack_mask(int square) {
  uint64_t king_bitboard = 1ULL << square;
  uint64_t attacks = east_one(king_bitboard) | west_one(king_bitboard);
  king_bitboard |= attacks;
  attacks |= north_one(king_bitboard) | south_one(king_bitboard);
  return attacks;
}

uint64_t generate_rook_blocker_mask(square_t square) {
  uint64_t mask = 0ULL;

  int start_rank = square / 8;
  int start_file = square % 8;

  for (int rank = start_rank + 1; rank < 7; rank++) {
    mask |= 1ULL << ((rank * 8) + start_file);
  }

  for (int rank = start_rank - 1; rank > 0; rank--) {
    mask |= 1ULL << ((rank * 8) + start_file);
  }

  for (int file = start_file + 1; file < 7; file++) {
    mask |= 1ULL << ((start_rank * 8) + file);
  }

  for (int file = start_file - 1; file > 0; file--) {
    mask |= 1ULL << ((start_rank * 8) + file);
  }

  return mask;
}

uint64_t generate_bishop_blocker_mask(square_t square) {
  uint64_t mask = 0ULL;

  int start_rank = square / 8;
  int start_file = square % 8;

  int rank;
  int file;

  for (rank = start_rank + 1, file = start_file + 1; rank < 7 && file < 7;
       rank++, file++) {
    mask |= 1ULL << ((rank * 8) + file);
  }

  for (rank = start_rank + 1, file = start_file - 1; rank < 7 && file > 0;
       rank++, file--) {
    mask |= 1ULL << ((rank * 8) + file);
  }

  for (rank = start_rank - 1, file = start_file + 1; rank > 0 && file < 7;
       rank--, file++) {
    mask |= 1ULL << ((rank * 8) + file);
  }

  for (rank = start_rank - 1, file = start_file - 1; rank > 0 && file > 0;
       rank--, file--) {
    mask |= 1ULL << ((rank * 8) + file);
  }

  return mask;
}

uint64_t generate_rook_attack_mask(int square, uint64_t blockers) {
  uint64_t mask = 0ULL;

  int start_rank = square / 8;
  int start_file = square % 8;

  for (int rank = start_rank + 1; rank < 8; rank++) {
    mask |= 1ULL << ((rank * 8) + start_file);
    if (blockers & (1ULL << ((rank * 8) + start_file))) {
      break;
    }
  }

  for (int rank = start_rank - 1; rank >= 0; rank--) {
    mask |= 1ULL << ((rank * 8) + start_file);
    if (blockers & (1ULL << ((rank * 8) + start_file))) {
      break;
    }
  }

  for (int file = start_file + 1; file < 8; file++) {
    mask |= 1ULL << ((start_rank * 8) + file);
    if (blockers & (1ULL << ((start_rank * 8) + file))) {
      break;
    }
  }

  for (int file = start_file - 1; file >= 0; file--) {
    mask |= 1ULL << ((start_rank * 8) + file);
    if (blockers & (1ULL << ((start_rank * 8) + file))) {
      break;
    }
  }

  return mask;
}

uint64_t generate_bishop_attack_mask(int square, uint64_t blockers) {
  uint64_t mask = 0ULL;

  int start_rank = square / 8;
  int start_file = square % 8;

  int rank;
  int file;

  for (rank = start_rank + 1, file = start_file + 1; rank < 8 && file < 8;
       rank++, file++) {
    mask |= 1ULL << ((rank * 8) + file);
    if (blockers & (1ULL << ((rank * 8) + file))) {
      break;
    }
  }

  for (rank = start_rank + 1, file = start_file - 1; rank < 8 && file >= 0;
       rank++, file--) {
    mask |= 1ULL << ((rank * 8) + file);
    if (blockers & (1ULL << ((rank * 8) + file))) {
      break;
    }
  }

  for (rank = start_rank - 1, file = start_file + 1; rank >= 0 && file < 8;
       rank--, file++) {
    mask |= 1ULL << ((rank * 8) + file);
    if (blockers & (1ULL << ((rank * 8) + file))) {
      break;
    }
  }

  for (rank = start_rank - 1, file = start_file - 1; rank >= 0 && file >= 0;
       rank--, file--) {
    mask |= 1ULL << ((rank * 8) + file);
    if (blockers & (1ULL << ((rank * 8) + file))) {
      break;
    }
  }

  return mask;
}

size_t get_magic_index(uint64_t magic, uint64_t mask, uint64_t current_blockers,
                       uint8_t shift, size_t offset) {
  uint64_t blockers = current_blockers & mask;
  uint64_t hash = magic * blockers;
  return (hash >> shift) + offset;
}

void add_rook_attack_table_entries(int square) {
  uint64_t mask = generate_rook_blocker_mask(square);
  uint64_t magic = ROOK_MAGICS[square];
  uint64_t blockers = 0ULL;

  while (true) {
    uint64_t moves = generate_rook_attack_mask(square, blockers);
    size_t magic_index =
        get_magic_index(magic, mask, blockers, 64 - ROOK_RELEVANT_BITS[square],
                        ROOK_OFFSETS[square]);
    ROOK_ATTACK_TABLE[magic_index] = moves;

    blockers = (blockers - mask) & mask;
    if (blockers == 0) {
      break;
    }
  }
}

void add_bishop_attack_table_entries(int square) {
  uint64_t mask = generate_bishop_blocker_mask(square);
  uint64_t magic = BISHOP_MAGICS[square];
  uint64_t blockers = 0ULL;

  while (true) {
    uint64_t moves = generate_bishop_attack_mask(square, blockers);
    size_t magic_index = get_magic_index(magic, mask, blockers,
                                         64 - BISHOP_RELEVANT_BITS[square],
                                         BISHOP_OFFSETS[square]);
    BISHOP_ATTACK_TABLE[magic_index] = moves;

    blockers = (blockers - mask) & mask;
    if (blockers == 0) {
      break;
    }
  }
}

void init_attack_masks() {
  for (int square = 0; square < 64; square++) {
    PAWN_ATTACKS[WHITE][square] = generate_pawn_attack_mask(square, WHITE);
    PAWN_ATTACKS[BLACK][square] = generate_pawn_attack_mask(square, BLACK);
    KNIGHT_ATTACKS[square] = generate_knight_attack_mask(square);
    KING_ATTACKS[square] = generate_king_attack_mask(square);

    add_rook_attack_table_entries(square);
    add_bishop_attack_table_entries(square);
  }
}

uint64_t get_bishop_attacks(int square, uint64_t blockers) {
  size_t magic_index = get_magic_index(
      BISHOP_MAGICS[square], generate_bishop_blocker_mask(square), blockers,
      64 - BISHOP_RELEVANT_BITS[square], BISHOP_OFFSETS[square]);

  return BISHOP_ATTACK_TABLE[magic_index];
}

uint64_t get_rook_attacks(int square, uint64_t blockers) {
  size_t magic_index = get_magic_index(
      ROOK_MAGICS[square], generate_rook_blocker_mask(square), blockers,
      64 - ROOK_RELEVANT_BITS[square], ROOK_OFFSETS[square]);

  return ROOK_ATTACK_TABLE[magic_index];
}

uint64_t get_queen_attacks(int square, uint64_t blockers) {
  size_t rook_magic_index = get_magic_index(
      ROOK_MAGICS[square], generate_rook_blocker_mask(square), blockers,
      64 - ROOK_RELEVANT_BITS[square], ROOK_OFFSETS[square]);

  size_t bishop_magic_index = get_magic_index(
      BISHOP_MAGICS[square], generate_bishop_blocker_mask(square), blockers,
      64 - BISHOP_RELEVANT_BITS[square], BISHOP_OFFSETS[square]);

  return ROOK_ATTACK_TABLE[rook_magic_index] |
         BISHOP_ATTACK_TABLE[bishop_magic_index];
}

bool is_square_attacked(int square, const board_t *board,
                        side_t attacker_side) {
  uint64_t pawns =
      attacker_side == WHITE ? board->white_pawns : board->black_pawns;

  if (PAWN_ATTACKS[attacker_side ^ 1][square] & pawns) {
    return true;
  }

  uint64_t king =
      attacker_side == WHITE ? board->white_king : board->black_king;

  if (KING_ATTACKS[square] & king) {
    return true;
  }

  uint64_t knights =
      attacker_side == WHITE ? board->white_knights : board->black_knights;

  if (KNIGHT_ATTACKS[square] & knights) {
    return true;
  }

  uint64_t bishops =
      attacker_side == WHITE ? board->white_bishops : board->black_bishops;

  if (get_bishop_attacks(square, board->occupancies[WHITE] |
                                     board->occupancies[BLACK]) &
      bishops) {
    return true;
  }

  uint64_t rooks =
      attacker_side == WHITE ? board->white_rooks : board->black_rooks;

  if (get_rook_attacks(square,
                       board->occupancies[WHITE] | board->occupancies[BLACK]) &
      rooks) {
    return true;
  }

  uint64_t queens =
      attacker_side == WHITE ? board->white_queens : board->black_queens;

  if (get_queen_attacks(square,
                        board->occupancies[WHITE] | board->occupancies[BLACK]) &
      queens) {
    return true;
  }

  return false;
}

void generate_bishop_moves(const board_t *board, move_list_t *move_list) {
  uint64_t bishops =
      board->side == WHITE ? board->white_bishops : board->black_bishops;

  side_t current_side = board->side;

  uint64_t current_side_occupancy = board->occupancies[current_side];
  uint64_t enemy_occupancy = board->occupancies[current_side ^ 1];

  while (bishops != 0) {
    square_t from_square = bitboard_pop_bit(&bishops);

    uint64_t bishop_moves =
        (get_bishop_attacks(from_square,
                            current_side_occupancy | enemy_occupancy)) &
        ~current_side_occupancy;

    while (bishop_moves != 0) {
      square_t to_square = bitboard_pop_bit(&bishop_moves);

      bool is_capture = ((1ULL << to_square) & enemy_occupancy) != 0;

      if (is_capture) {
        move_list_push(move_list,
                       move_new(from_square, to_square, CAPTURE, NO_FLAG));
      } else {
        move_list_push(move_list,
                       move_new(from_square, to_square, QUIET, NO_FLAG));
      }
    }
  }
}

void generate_rook_moves(const board_t *board, move_list_t *move_list) {
  uint64_t rooks =
      board->side == WHITE ? board->white_rooks : board->black_rooks;

  side_t current_side = board->side;

  uint64_t current_side_occupancy = board->occupancies[current_side];
  uint64_t enemy_occupancy = board->occupancies[current_side ^ 1];

  while (rooks != 0) {
    square_t from_square = bitboard_pop_bit(&rooks);

    uint64_t rook_moves =
        (get_rook_attacks(from_square,
                          current_side_occupancy | enemy_occupancy)) &
        ~current_side_occupancy;

    while (rook_moves != 0) {
      square_t to_square = bitboard_pop_bit(&rook_moves);

      bool is_capture = ((1ULL << to_square) & enemy_occupancy) != 0;

      move_t move;

      if (is_capture) {
        move = move_new(from_square, to_square, CAPTURE, NO_FLAG);
      } else {
        move = move_new(from_square, to_square, QUIET, NO_FLAG);
      }

      move_list_push(move_list, move);
    }
  }
}

void generate_queen_moves(const board_t *board, move_list_t *move_list) {
  uint64_t queens =
      board->side == WHITE ? board->white_queens : board->black_queens;

  side_t current_side = board->side;

  uint64_t current_side_occupancy = board->occupancies[current_side];
  uint64_t enemy_occupancy = board->occupancies[current_side ^ 1];

  while (queens != 0) {
    square_t from_square = bitboard_pop_bit(&queens);

    uint64_t queen_moves =
        (get_queen_attacks(from_square,
                           current_side_occupancy | enemy_occupancy)) &
        ~current_side_occupancy;

    while (queen_moves != 0) {
      square_t to_square = bitboard_pop_bit(&queen_moves);

      bool is_capture = ((1ULL << to_square) & enemy_occupancy) != 0;

      if (is_capture) {
        move_list_push(move_list,
                       move_new(from_square, to_square, CAPTURE, NO_FLAG));
      } else {
        move_list_push(move_list,
                       move_new(from_square, to_square, QUIET, NO_FLAG));
      }
    }
  }
}

void generate_king_moves(const board_t *board, move_list_t *move_list) {
  uint64_t king = board->side == WHITE ? board->white_king : board->black_king;

  side_t current_side = board->side;

  uint64_t current_side_occupancy = board->occupancies[current_side];
  uint64_t enemy_occupancy = board->occupancies[current_side ^ 1];

  square_t from_square = bitboard_pop_bit(&king);

  uint64_t king_moves = KING_ATTACKS[from_square] & ~current_side_occupancy;

  while (king_moves != 0) {
    square_t to_square = bitboard_pop_bit(&king_moves);

    bool is_capture = ((1ULL << to_square) & enemy_occupancy) != 0;

    if (is_capture) {
      move_list_push(move_list,
                     move_new(from_square, to_square, CAPTURE, NO_FLAG));
    } else {
      move_list_push(move_list,
                     move_new(from_square, to_square, QUIET, NO_FLAG));
    }
  }
}

void generate_castling_moves(const board_t *board, move_list_t *move_list) {
  uint64_t occupied = board->occupancies[WHITE] | board->occupancies[BLACK];

  if (board->side == WHITE) {
    if (board->castle_rights & WHITE_KING_CASTLE) {
      if ((occupied & (1ULL << F1)) == 0 && (occupied & (1ULL << G1)) == 0 &&
          !is_square_attacked(E1, board, BLACK) &&
          !is_square_attacked(F1, board, BLACK) &&
          !is_square_attacked(G1, board, BLACK)) {
        move_list_push(move_list, move_new(E1, G1, CASTLE, NO_FLAG));
      }
    }

    if (board->castle_rights & WHITE_QUEEN_CASTLE) {
      if ((occupied & (1ULL << D1)) == 0 && (occupied & (1ULL << C1)) == 0 &&
          (occupied & (1ULL << B1)) == 0 &&
          !is_square_attacked(E1, board, BLACK) &&
          !is_square_attacked(D1, board, BLACK) &&
          !is_square_attacked(C1, board, BLACK)) {
        move_list_push(move_list, move_new(E1, C1, CASTLE, NO_FLAG));
      }
    }
  } else {
    if (board->castle_rights & BLACK_KING_CASTLE) {
      if ((occupied & (1ULL << F8)) == 0 && (occupied & (1ULL << G8)) == 0 &&
          !is_square_attacked(E8, board, WHITE) &&
          !is_square_attacked(F8, board, WHITE) &&
          !is_square_attacked(G8, board, WHITE)) {
        move_list_push(move_list, move_new(E8, G8, CASTLE, NO_FLAG));
      }
    }

    if (board->castle_rights & BLACK_QUEEN_CASTLE) {
      if ((occupied & (1ULL << D8)) == 0 && (occupied & (1ULL << C8)) == 0 &&
          (occupied & (1ULL << B8)) == 0 &&
          !is_square_attacked(E8, board, WHITE) &&
          !is_square_attacked(D8, board, WHITE) &&
          !is_square_attacked(C8, board, WHITE)) {
        move_list_push(move_list, move_new(E8, C8, CASTLE, NO_FLAG));
      }
    }
  }
}

void generate_all_moves(const board_t *board, move_list_t *move_list) {
  generate_pawn_moves(board, move_list);
  generate_knight_moves(board, move_list);
  generate_bishop_moves(board, move_list);
  generate_rook_moves(board, move_list);
  generate_queen_moves(board, move_list);
  generate_king_moves(board, move_list);
  generate_castling_moves(board, move_list);
}

bool is_in_check(board_t *board, side_t side) {
  uint64_t king_bitboard =
      side == WHITE ? board->white_king : board->black_king;

  square_t king_position = __builtin_ctzll(get_lsb(king_bitboard));

  return is_square_attacked(king_position, board, board->side);
}

bool make_move(board_t *board, move_t move) {
  history_item_t irreversible_state = {
      .hash = board->hash,
      .castle_rights = board->castle_rights,
      .en_passant_square = board->en_passant_square,
      .halfmove_clock = board->halfmove_clock,
      .moved_piece = board->pieces[move.from],
      .captured_piece = board->pieces[move.to]};

  board->halfmove_clock++;

  // TODO: could this be lower down?
  // clear en passant
  board->hash ^= zobrist_en_passant_file(board->en_passant_square);
  board->en_passant_square = NO_SQUARE;

  board->hash ^= zobrist_remove_piece(board, move.from);

  switch (move.move_type) {
  case QUIET:
    board->hash ^=
        zobrist_add_piece(board, move.to, irreversible_state.moved_piece);
    break;
  case CAPTURE: {
    if (move.flag == EN_PASSANT) {
      square_t captured_square =
          board->side == WHITE ? (move.to - 8) : (move.to + 8);
      irreversible_state.captured_piece = board->pieces[captured_square];

      board->hash ^= zobrist_remove_piece(board, captured_square);
      board->hash ^=
          zobrist_add_piece(board, move.to, irreversible_state.moved_piece);
    } else {
      board->hash ^= zobrist_remove_piece(board, move.to);
      board->hash ^=
          zobrist_add_piece(board, move.to, irreversible_state.moved_piece);
    }
    break;
  }
  case CASTLE: {
    square_t rook_from_square;
    square_t rook_to_square;

    if (move.to == G1) {
      rook_from_square = H1;
      rook_to_square = F1;
    } else if (move.to == C1) {
      rook_from_square = A1;
      rook_to_square = D1;
    } else if (move.to == G8) {
      rook_from_square = H8;
      rook_to_square = F8;
    } else if (move.to == C8) {
      rook_from_square = A8;
      rook_to_square = D8;
    } else {
      printf("Invalid rook move\n");
      exit(EXIT_FAILURE);
    }

    // move the king
    board->hash ^=
        zobrist_add_piece(board, move.to, irreversible_state.moved_piece);

    // move the rook
    board->hash ^= zobrist_add_piece(
        board, rook_to_square, board->side == WHITE ? WHITE_ROOK : BLACK_ROOK);
    board->hash ^= zobrist_remove_piece(board, rook_from_square);

    break;
  }
  case PROMOTION: {
    if (irreversible_state.captured_piece != EMPTY) {
      board->hash ^= zobrist_remove_piece(board, move.to);
    }

    piece_t promotion_piece = EMPTY;

    if (move.flag == KNIGHT_PROMOTION) {
      promotion_piece = board->side == WHITE ? WHITE_KNIGHT : BLACK_KNIGHT;
    }

    if (move.flag == BISHOP_PROMOTION) {
      promotion_piece = board->side == WHITE ? WHITE_BISHOP : BLACK_BISHOP;
    }

    if (move.flag == ROOK_PROMOTION) {
      promotion_piece = board->side == WHITE ? WHITE_ROOK : BLACK_ROOK;
    }

    if (move.flag == QUEEN_PROMOTION) {
      promotion_piece = board->side == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
    }

    if (promotion_piece == EMPTY) {
      printf("Expected promotion flag\n");
      exit(EXIT_FAILURE);
    }

    board->hash ^= zobrist_add_piece(board, move.to, promotion_piece);
    break;
  }
  }

  if (irreversible_state.moved_piece == WHITE_PAWN ||
      irreversible_state.moved_piece == BLACK_PAWN) {
    // pawn moves reset fifty-move rule clock
    board->halfmove_clock = 0;

    // double pawn push
    if (abs((int8_t)(move.from) - (int8_t)(move.to)) == 16) {
      board->en_passant_square =
          board->side == WHITE ? (move.from + 8) : (move.from - 8);

      // we only set en passant square if a pawn can actually capture, as per
      // https://github.com/fsmosca/PGN-Standard/blob/61a82dab3ff62d79dea82c15a8cc773f80f3a91e/PGN-Standard.txt#L2231-L2242
      uint64_t enemy_pawns =
          board->side == WHITE ? board->black_pawns : board->white_pawns;
      if ((PAWN_ATTACKS[board->side][board->en_passant_square] & enemy_pawns) ==
          0) {
        board->en_passant_square = NO_SQUARE;
      }
    }
  }

  board->hash ^= zobrist_castle(board->castle_rights);
  board->castle_rights = board->castle_rights & CASTLE_PERMISSIONS[move.from] &
                         CASTLE_PERMISSIONS[move.to];
  board->hash ^= zobrist_castle(board->castle_rights);

  if (board->en_passant_square != NO_SQUARE) {
    board->hash ^= zobrist_en_passant_file(board->en_passant_square);
  }

  board->side ^= 1;
  board->hash ^= zobrist_current_side();

  board->history[board->history_length] = irreversible_state;
  board->history_length++;

  board->ply++;

  return !is_in_check(board, board->side ^ 1);
}

void unmake_move(board_t *board, move_t move) {
  board->history_length--;
  history_item_t move_state = board->history[board->history_length];

  board->ply--;

  board->hash = move_state.hash;
  board->castle_rights = move_state.castle_rights;
  board->en_passant_square = move_state.en_passant_square;
  board->halfmove_clock = move_state.halfmove_clock;

  board->side ^= 1;

  zobrist_add_piece(board, move.from, move_state.moved_piece);

  switch (move.move_type) {
  case QUIET:
    zobrist_remove_piece(board, move.to);
    break;
  case CAPTURE: {
    if (move.flag == EN_PASSANT) {
      square_t captured_square =
          board->side == WHITE ? (move.to - 8) : (move.to + 8);

      zobrist_remove_piece(board, move.to);
      zobrist_add_piece(board, captured_square, move_state.captured_piece);
    } else {
      zobrist_remove_piece(board, move.to);
      zobrist_add_piece(board, move.to, move_state.captured_piece);
    }
    break;
  }
  case CASTLE: {
    square_t rook_from_square;
    square_t rook_to_square;

    if (move.to == G1) {
      rook_from_square = H1;
      rook_to_square = F1;
    } else if (move.to == C1) {
      rook_from_square = A1;
      rook_to_square = D1;
    } else if (move.to == G8) {
      rook_from_square = H8;
      rook_to_square = F8;
    } else if (move.to == C8) {
      rook_from_square = A8;
      rook_to_square = D8;
    } else {
      printf("Invalid rook move\n");
      exit(EXIT_FAILURE);
    }

    // remove the king
    zobrist_remove_piece(board, move.to);

    // move the rook back
    zobrist_remove_piece(board, rook_to_square);
    zobrist_add_piece(board, rook_from_square,
                      board->side == WHITE ? WHITE_ROOK : BLACK_ROOK);
    break;
  }
  case PROMOTION: {
    zobrist_remove_piece(board, move.to);
    if (move_state.captured_piece != EMPTY) {
      zobrist_add_piece(board, move.to, move_state.captured_piece);
    }
    break;
  }
  }
}

void init_all() {
  init_attack_masks();
  init_zobrist_hash();
}

typedef struct {
  uint64_t hash;
  uint64_t nodes;
  int depth;
} transposition_table_entry_t;

typedef struct {
  transposition_table_entry_t *entries;
  size_t size;
} transposition_table_t;

transposition_table_t *transposition_table_new(int size_in_mb) {
  transposition_table_t *table = malloc(sizeof(transposition_table_t));

  size_t size = size_in_mb * 1024 * 1024 / sizeof(transposition_table_entry_t);

  table->size = size;
  table->entries = malloc(size * sizeof(transposition_table_entry_t));

  return table;
}

transposition_table_entry_t *
transposition_table_probe(transposition_table_t *table, uint64_t hash) {
  size_t index = hash % table->size;
  return &table->entries[index];
}

void transposition_table_store(transposition_table_t *table, uint64_t hash,
                               uint64_t nodes, int depth) {
  size_t index = hash % table->size;
  table->entries[index].nodes = nodes;
  table->entries[index].hash = hash;
  table->entries[index].depth = depth;
}

uint64_t perft(board_t *board, int depth, transposition_table_t *table) {
  if (depth == 0) {
    return 1;
  }

  transposition_table_entry_t *entry =
      transposition_table_probe(table, board->hash);

  if (entry->hash == board->hash && entry->depth == depth) {
    return entry->nodes;
  }

  uint64_t nodes = 0;

  move_list_t *move_list = move_list_new();
  generate_all_moves(board, move_list);

  move_t move;

  for (int i = 0; i < move_list->count; i++) {
    move = move_list->moves[i];
    if (make_move(board, move)) {
      nodes += perft(board, depth - 1, table);
    }

    unmake_move(board, move);
  }

  transposition_table_store(table, board->hash, nodes, depth);

  free(move_list);

  return nodes;
}

int get_time_ms() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec * 1000 + t.tv_usec / 1000;
}

// void perft_test(board_t *board, int depth, transposition_table_t *table) {
//   printf("\nStarting Test To Depth:%d\n", depth);
//   int start = get_time_ms();
//
//   uint64_t nodes = 0;
//   int move_num = 0;
//
//   move_list_t *move_list = move_list_new();
//
//   generate_all_moves(board, move_list);
//
//   move_t move;
//
//   for (int i = 0; i < move_list->count; i++) {
//     move = move_list->moves[i];
//     if (make_move(board, move)) {
//       move_num++;
//       uint64_t old_nodes = nodes;
//       nodes += perft(board, depth - 1, table);
//       printf("move %d : %s%s : %ld\n", move_num,
//       SQUARE_TO_READABLE[move.from],
//              SQUARE_TO_READABLE[move.to], nodes - old_nodes);
//     }
//
//     unmake_move(board, move);
//   }
//
//   free(move_list);
//
//   printf("\nTest Complete : %ld nodes visited in %dms\n", nodes,
//          get_time_ms() - start);
//
//   return;
// }

void run_perft_suite() {
  FILE *perft_file = fopen("perft.epd", "r");

  char line[256];
  char fens[256][100];
  int depths[256];
  uint64_t nodes[256];
  size_t line_count = 0;

  transposition_table_t *table = transposition_table_new(64);

  int start = get_time_ms();

  while (fgets(line, sizeof(line), perft_file)) {
    char fen[100];
    size_t fen_size = 0;

    for (int i = 0; i < sizeof(line); i++) {
      if (line[i + 1] == ';') {
        break;
      }

      fen[fen_size] = line[i];
      fen_size++;
    }
    fen[fen_size] = '\0';

    // +2 to eat the space and first semicolon
    char *token = strtok(line + fen_size + 2, ";");

    int depth = 0;
    uint64_t expected_nodes = 0ULL;

    while (token != NULL) {
      int current_depth;
      uint64_t current_expected_nodes;
      sscanf(token, "D%d %lu", &current_depth, &current_expected_nodes);
      token = strtok(NULL, ";");

      if (!depth || current_depth > depth) {
        depth = current_depth;
        expected_nodes = current_expected_nodes;
      }
    }

    strcpy(fens[line_count], fen);
    depths[line_count] = depth;
    nodes[line_count] = expected_nodes;

    line_count++;
  }

  for (size_t i = 0; i < line_count; i++) {
    board_t *board = board_new();
    board_parse_FEN(board, fens[i]);
    uint64_t expected_nodes = nodes[i];
    int depth = depths[i];
    printf("\033[36m[Perft test %zu/%zu]\033[0m expected nodes: %lu, depth: "
           "%d",
           i + 1, line_count, expected_nodes, depth);
    fflush(stdout);
    uint64_t result = perft(board, depths[i], table);
    printf("\r\033[36m[Perft test %zu/%zu]\033[0m expected nodes: %lu, depth: "
           "%d",
           i + 1, line_count, expected_nodes, depth);
    if (expected_nodes == result) {
      printf("\033[32m Passed\033[0m\n");
    } else {
      printf("\033[31m Failed (got %lu)\033[0m\n", result);
      exit(EXIT_FAILURE);
    }
    free(board);
  }

  int end = get_time_ms() - start;
  int minutes = end / (1000 * 60);
  int seconds = (end / 1000) % 60;
  int milliseconds = end % 1000;

  float precise_seconds = seconds + ((float)milliseconds / 1000);

  printf("\n\033[32mAll tests passed in %dm %.1fs\033[0m\n", minutes,
         precise_seconds);
}

int evaluate_position(board_t *board) {
  int multiplier = board->side == WHITE ? 1 : -1;

  int white_score = 0;
  int black_score = 0;

  for (int square = 0; square < 64; square++) {
    piece_t piece = board->pieces[square];
    if (piece == EMPTY) {
      continue;
    }

    if (piece >= WHITE_PAWN && piece <= WHITE_KING) {
      white_score += PIECE_VALUES[piece];
    }

    if (piece >= BLACK_PAWN && piece <= BLACK_KING) {
      black_score += PIECE_VALUES[piece];
    }
  }

  return (white_score - black_score) * multiplier;
}

void check_search_time(search_info_t *info) {
  if (info->time_left == INFINITE_SEARCH_TIME &&
      info->move_time == INFINITE_SEARCH_TIME) {
    return;
  }

  if (get_time_ms() > info->stop_time) {
    info->stopped = true;
  }
}

int negamax(board_t *board, int depth, move_t *best_move,
            search_info_t *search_info) {
  search_info->nodes_searched++;

  if (depth == 0) {
    return evaluate_position(board);
  }

  // check move time expiry every 2048 nodes
  if ((search_info->nodes_searched & 2047) == 0) {
    check_search_time(search_info);
  }

  if (search_info->stopped) {
    return 0;
  }

  int max = -INT_MAX;
  move_list_t *move_list = move_list_new();
  generate_all_moves(board, move_list);

  size_t legal_move_count = 0;

  for (size_t i = 0; i < move_list->count; i++) {
    if (!make_move(board, move_list->moves[i])) {
      unmake_move(board, move_list->moves[i]);
      continue;
    }

    int score = -negamax(board, depth - 1, best_move, search_info);
    unmake_move(board, move_list->moves[i]);
    if (score > max) {
      max = score;

      if (board->ply == 0) {
        *best_move = move_list->moves[i];
      }
    }

    legal_move_count++;
  }

  // checkmate or stalemate
  if (legal_move_count == 0) {
    if (is_in_check(board, board->side)) {
      return (-INT_MAX + board->ply);
    } else {
      return 0;
    }
  }

  free(move_list);
  return max;
}

// TODO: this is bad
// can get rid when change moves to be integers
const move_t NULL_MOVE = {
    .from = 64, .to = 64, .flag = NO_FLAG, .move_type = QUIET};

void search_position(board_t *board, search_info_t *search_info) {
  board->ply = 0;

  move_t best_move = NULL_MOVE;
  uint64_t total_time = 0ULL;

  for (int depth = 1; depth <= search_info->depth; depth++) {
    move_t current_best_move;
    int start_time = get_time_ms();
    int score = negamax(board, depth, &current_best_move, search_info);
    int end_time = get_time_ms() - start_time;

    if (search_info->stopped) {
      if (depth == 1) {
        best_move = current_best_move;
      }
      break;
    }

    best_move = current_best_move;

    total_time += end_time;

    printf("info depth %d score %d nodes %lu time %lu\n", depth, score,
           search_info->nodes_searched, total_time);
  }

  printf("bestmove %s%s", SQUARE_TO_READABLE[best_move.from],
         SQUARE_TO_READABLE[best_move.to]);
  if (best_move.move_type == PROMOTION) {
    printf("%c", FLAG_TO_ALGEBRAIC_NOTATION[best_move.flag]);
  }
  printf("\n");
}

move_t uci_parse_move(board_t *board, char *move_string) {
  while (isspace(*move_string)) {
    move_string++;
  }

  if (move_string[0] < 'a' || move_string[0] > 'h') {
    printf("Invalid move provided");
    exit(EXIT_FAILURE);
  }

  if (move_string[2] < 'a' || move_string[2] > 'h') {
    printf("Invalid move provided");
    exit(EXIT_FAILURE);
  }

  if (move_string[1] < '1' || move_string[1] > '8') {
    printf("Invalid move provided");
    exit(EXIT_FAILURE);
  }

  if (move_string[3] < '1' || move_string[3] > '8') {
    printf("Invalid move provided");
    exit(EXIT_FAILURE);
  }

  int from = (move_string[0] - 'a') + (move_string[1] - '1') * 8;
  int to = (move_string[2] - 'a') + (move_string[3] - '1') * 8;

  move_list_t *move_list = move_list_new();
  generate_all_moves(board, move_list);

  for (size_t i = 0; i < move_list->count; i++) {
    move_t move = move_list->moves[i];

    if (move.from == from && move.to == to) {
      if (move.move_type == PROMOTION) {
        if (move.flag == KNIGHT_PROMOTION && move_string[4] == 'n') {
          return move;
        }

        if (move.flag == BISHOP_PROMOTION && move_string[4] == 'b') {
          return move;
        }

        if (move.flag == ROOK_PROMOTION && move_string[4] == 'r') {
          return move;
        }

        if (move.flag == QUEEN_PROMOTION && move_string[4] == 'q') {
          return move;
        }
      } else {
        return move;
      }
    }
  }

  printf("Provided move was valid but illegal in this position\n");
  exit(EXIT_FAILURE);
}

void uci_parse_position(board_t *board, char *position) {
  board_reset(board);

  char *current = position + 9;

  while (isspace(*current)) {
    current++;
  }

  if (strncmp(current, "startpos", 8) == 0) {
    board_parse_FEN(board, START_FEN);
  } else {
    current = strstr(current, "fen");
    if (current == NULL) {
      printf("invalid position string. need either `startpos` or `fen`");
      exit(EXIT_FAILURE);
    } else {
      current += 4;
      while (isspace(*current)) {
        current++;
      }
      board_parse_FEN(board, current);
    }
  }

  current = strstr(current, "moves");
  if (current != NULL) {
    current += 6;

    move_t move;

    while (isspace(*current)) {
      current++;
    }

    while (*current) {
      move = uci_parse_move(board, current);
      make_move(board, move);

      while (isalnum(*current)) {
        current++;
      }

      while (isspace(*current)) {
        current++;
      }
    }

    // the moves won't be undone, so no point storing them in history
    board->history_length = 0;
  }
}

void start_search_timer(search_info_t *info) {
  info->stopped = false;
  info->nodes_searched = 0;

  int start_time = get_time_ms();

  if (info->move_time != -1) {
    info->stop_time = start_time + info->move_time;
    return;
  }

  info->stop_time = start_time + (info->time_left / 30);
}

search_info_t search_info_new() {
  search_info_t search_info;

  // uci arguments
  search_info.time_left = INFINITE_SEARCH_TIME;
  search_info.moves_to_go = -1;
  search_info.move_time = INFINITE_SEARCH_TIME;
  search_info.depth = MAX_SEARCH_DEPTH;

  // calculated search info
  search_info.stopped = false;
  search_info.stop_time = -1;
  search_info.nodes_searched = 0ULL;

  return search_info;
}

void uci_parse_go(board_t *board, char *move_string) {
  search_info_t search_info = search_info_new();
  char *current = NULL;

  current = strstr(move_string, "depth");
  if (current) {
    search_info.depth = atoi(current + 6);
  } else {
    search_info.depth = MAX_SEARCH_DEPTH;
  }

  current = strstr(move_string, "wtime");
  if (current && board->side == WHITE) {
    search_info.time_left = atoi(current + 6);
  }

  current = strstr(move_string, "btime");
  if (current && board->side == BLACK) {
    search_info.time_left = atoi(current + 6);
  }

  current = strstr(move_string, "movetime");
  if (current && board->side == BLACK) {
    search_info.move_time = atoi(current + 9);
  }

  start_search_timer(&search_info);
  search_position(board, &search_info);
}

void uci_loop() {
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  init_all();

  board_t *board = board_new();

  printf("id name Billy's Engine v1.0\n");
  printf("id author Billy Levin\n");
  printf("uciok\n");

  while (1) {
    char *input = readline(NULL);
    add_history(input);

    if (strncmp(input, "uci", 3) == 0) {
      printf("id name Billy's Engine v1.0\n");
      printf("id author Billy Levin\n");
      printf("uciok\n");
    } else if (strncmp(input, "isready", 7) == 0) {
      printf("readyok\n");
    } else if (strncmp(input, "position", 8) == 0) {
      uci_parse_position(board, input);
    } else if (strncmp(input, "go", 2) == 0) {
      uci_parse_go(board, input);
    }
  }
}

void main_loop() {
  while (1) {
    char *input = readline(NULL);
    add_history(input);

    if (strncmp(input, "uci", 3) == 0) {
      uci_loop();
      break;
    }
  }
}

int main() {
  main_loop();
  // init_all();
  // run_perft_suite();
  return EXIT_SUCCESS;
}
