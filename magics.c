#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint64_t magic;
  size_t table_length;
} found_magic_t;

typedef struct {
  uint64_t magic;
  uint64_t mask;
  uint8_t bits_in_mask;
} magic_candidate_t;

typedef enum { ROOK, BISHOP } piece_t;

uint64_t generate_rook_blocker_mask(int square) {
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

uint64_t generate_bishop_blocker_mask(int square) {
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

// rng stuff taken from
// https://www.chessprogramming.org/Looking_for_Magics#Feeding_in_Randoms
uint64_t random_uint64() {
  uint64_t u1, u2, u3, u4;
  u1 = (uint64_t)(rand()) & 0xFFFF;
  u2 = (uint64_t)(rand()) & 0xFFFF;
  u3 = (uint64_t)(rand()) & 0xFFFF;
  u4 = (uint64_t)(rand()) & 0xFFFF;
  return u1 | (u2 << 16) | (u3 << 32) | (u4 << 48);
}

uint64_t random_uint64_fewbits() {
  return random_uint64() & random_uint64() & random_uint64();
}

uint8_t count_bits(uint64_t number) {
  uint8_t count = 0;
  while (number) {
    count += number & 1;
    number >>= 1;
  }
  return count;
}

size_t get_magic_index(magic_candidate_t *candidate,
                       uint64_t current_blockers) {
  uint64_t blockers = current_blockers & candidate->mask;
  uint64_t hash = candidate->magic * blockers;
  uint8_t shift = 64 - candidate->bits_in_mask;
  return (size_t)(hash >> shift);
}

size_t check_magic(magic_candidate_t *candidate, int square, piece_t piece) {
  uint64_t attack_table[1 << candidate->bits_in_mask];
  size_t attack_table_length = 0;

  for (size_t i = 0; i < (1 << candidate->bits_in_mask); i++) {
    attack_table[i] = 0ULL;
  }

  uint64_t blockers = 0ULL;

  while (1) {
    uint64_t moves = piece == ROOK
                         ? generate_rook_attack_mask(square, blockers)
                         : generate_bishop_attack_mask(square, blockers);

    size_t index = get_magic_index(candidate, blockers);

    if (attack_table[index] == 0) {
      attack_table[index] = moves;
      attack_table_length++;
    } else if (attack_table[index] != moves) {
      return 0;
    }

    blockers = (blockers - candidate->mask) & candidate->mask;
    if (blockers == 0) {
      break;
    }
  }

  return attack_table_length;
}

found_magic_t find_magic(int square, piece_t piece) {
  uint64_t mask = piece == ROOK ? generate_rook_blocker_mask(square)
                                : generate_bishop_blocker_mask(square);

  uint8_t bits_in_mask = count_bits(mask);

  magic_candidate_t magic_candidate = {.magic = 0ULL, mask, bits_in_mask};

  while (1) {
    magic_candidate.magic = random_uint64_fewbits();
    size_t table_length = check_magic(&magic_candidate, square, piece);

    if (table_length > 0) {
      found_magic_t found = {.magic = magic_candidate.magic,
                             .table_length = table_length};
      return found;
    }
  }
}

void print_magics(piece_t piece) {
  size_t total_size = 0;
  for (int square = 0; square < 64; square++) {
    found_magic_t found_magic = find_magic(square, piece);
    printf("Magic: 0x%lx, table_size: %zu\n", found_magic.magic,
           found_magic.table_length);
    total_size += found_magic.table_length;
  }

  printf("TOTAL SIZE: %zu\n", total_size);
}

int main() {
  print_magics(ROOK);
  print_magics(BISHOP);

  return EXIT_SUCCESS;
}
