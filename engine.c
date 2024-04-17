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
  piece_t promotion;
  bool is_en_passant;
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

const char PIECE_LETTER[12][2] = {"P", "N", "B", "R", "Q", "K",
                                  "p", "n", "b", "r", "q", "k"};

const uint64_t RANK_4_MASK = 4278190080ULL;
const uint64_t RANK_5_MASK = 1095216660480ULL;

const uint64_t NOT_A_FILE = 18374403900871474942ULL;
const uint64_t NOT_H_FILE = 9187201950435737471ULL;
const uint64_t NOT_AB_FILE = 18229723555195321596ULL;
const uint64_t NOT_GH_FILE = 4557430888798830399ULL;

uint64_t PAWN_ATTACKS[2][64];
uint64_t KNIGHT_ATTACKS[64];

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
#define PAWN_CAPTURES_WHITE_FEN                                                \
  "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define PAWN_CAPTURES_BLACK_FEN                                                \
  "rnbqkbnr/p1p1p3/3p3p/1p1p4/2P1Pp2/8/PP1P1PpP/RNBQKB1R b - e3 0 1"
#define KNIGHT_MOVES_FEN "5k2/1n6/4n3/6N1/8/3N4/8/5K2 b - - 0 1"
#define BISHOP_MOVES_FEN "6k1/1b6/4n2P/8/1n4B1/1B3N2/1N6/2b2K1 b - - 0 1"
#define ROOK_MOVES_FEN "6k1/8/5r2/8/1nR5/5N2/8/6K1 b - - 0 1"
#define QUEEN_MOVES_FEN "6k1/8/4nq2/8/1nQ5/5N2/1N6/6k1 b - - 0 1"

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

move_t move_new(square_t from, square_t to, bool is_capture, piece_t promotion,
                bool is_en_passant) {
  move_t move = {.from = from,
                 .to = to,
                 .is_capture = is_capture,
                 .promotion = promotion,
                 .is_en_passant = is_en_passant};
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

    printf("From: %s, to: %s, capture: %s, promotion: %s, en passant: %s\n",
           SQUARE_TO_READABLE[move.from], SQUARE_TO_READABLE[move.to],
           move.is_capture ? "true" : "false",
           move.promotion == EMPTY ? "none" : PIECE_LETTER[move.promotion],
           move.is_en_passant ? "true" : "false");
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
    uint64_t push_destinations = (board->white_pawns << 8) & empty;

    while (push_destinations != 0) {
      int square = bitboard_pop_bit(&push_destinations);

      if (is_promotion(square, WHITE)) {
        move_list_push(move_list,
                       move_new(square - 8, square, false, WHITE_QUEEN, false));
        move_list_push(move_list,
                       move_new(square - 8, square, false, WHITE_ROOK, false));
        move_list_push(move_list, move_new(square - 8, square, false,
                                           WHITE_BISHOP, false));
        move_list_push(move_list, move_new(square - 8, square, false,
                                           WHITE_KNIGHT, false));
      } else {
        move_list_push(move_list,
                       move_new(square - 8, square, false, EMPTY, false));
      }

      uint64_t potential_double_push = 1ULL << (square + 8);

      if ((potential_double_push & RANK_4_MASK & empty) != 0) {
        move_list_push(move_list,
                       move_new(square - 8, square + 8, false, EMPTY, false));
      }

      uint64_t en_passant_bitboard = board->en_passant_square != NO_SQUARE
                                         ? (1ULL << board->en_passant_square)
                                         : 0ULL;

      uint64_t enemy = board->occupancies[BLACK] | en_passant_bitboard;
      uint64_t attacks = (PAWN_ATTACKS[WHITE][square - 8]) & enemy;

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);

        if (is_promotion(attacked_square, WHITE)) {
          move_list_push(move_list, move_new(square - 8, attacked_square, true,
                                             WHITE_QUEEN, false));
          move_list_push(move_list, move_new(square - 8, attacked_square, true,
                                             WHITE_ROOK, false));
          move_list_push(move_list, move_new(square - 8, attacked_square, true,
                                             WHITE_BISHOP, false));
          move_list_push(move_list, move_new(square - 8, attacked_square, true,
                                             WHITE_KNIGHT, false));
        } else {
          move_list_push(move_list,
                         move_new(square - 8, attacked_square, true, EMPTY,
                                  attacked_square == board->en_passant_square));
        }
      }
    }

  } else {
    uint64_t push_destinations = (board->black_pawns >> 8) & empty;

    while (push_destinations != 0) {
      int square = bitboard_pop_bit(&push_destinations);

      if (is_promotion(square, BLACK)) {
        move_list_push(move_list,
                       move_new(square + 8, square, false, BLACK_QUEEN, false));
        move_list_push(move_list,
                       move_new(square + 8, square, false, BLACK_ROOK, false));
        move_list_push(move_list, move_new(square + 8, square, false,
                                           BLACK_BISHOP, false));
        move_list_push(move_list, move_new(square + 8, square, false,
                                           BLACK_KNIGHT, false));
      } else {
        move_list_push(move_list,
                       move_new(square + 8, square, false, EMPTY, false));
      }

      uint64_t potential_double_push = 1ULL << (square - 8);

      if ((potential_double_push & RANK_5_MASK & empty) != 0) {
        move_list_push(move_list,
                       move_new(square + 8, square - 8, false, EMPTY, false));
      }

      uint64_t en_passant_bitboard = board->en_passant_square != NO_SQUARE
                                         ? (1ULL << board->en_passant_square)
                                         : 0ULL;

      uint64_t enemy = board->occupancies[WHITE] | en_passant_bitboard;

      uint64_t attacks = (PAWN_ATTACKS[BLACK][square + 8]) & enemy;

      while (attacks != 0) {
        int attacked_square = bitboard_pop_bit(&attacks);

        if (is_promotion(attacked_square, BLACK)) {
          move_list_push(move_list, move_new(square + 8, attacked_square, true,
                                             BLACK_QUEEN, false));
          move_list_push(move_list, move_new(square + 8, attacked_square, true,
                                             BLACK_ROOK, false));
          move_list_push(move_list, move_new(square + 8, attacked_square, true,
                                             BLACK_BISHOP, false));
          move_list_push(move_list, move_new(square + 8, attacked_square, true,
                                             BLACK_KNIGHT, false));
        } else {
          move_list_push(move_list,
                         move_new(square + 8, attacked_square, true, EMPTY,
                                  attacked_square == board->en_passant_square));
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

      move_list_push(move_list, move_new(from_square, to_square, is_capture,
                                         EMPTY, false));
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

      move_list_push(move_list, move_new(from_square, to_square, is_capture,
                                         EMPTY, false));
    }
  }
}

int main() {
  init_attack_masks();

  board_t *board = board_new();

  board_parse_FEN(board, BISHOP_MOVES_FEN);
  board_print(board);

  move_list_t *move_list = move_list_new();

  generate_pawn_moves(board, move_list);
  generate_knight_moves(board, move_list);
  generate_bishop_moves(board, move_list);

  move_list_print(move_list);

  return EXIT_SUCCESS;
}
