#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#define SCORE_MASK_OPP (0x1fULL | (0x1fULL << 10) | (0x1fULL << 20) |\
                        (0x1fULL << 30) | (0x1fULL << 40) | (0x1fULL << 50))
#define SCORE_MASK_ME SCORE_MASK_OPP << 5
#define SCORE_INDEX_MAX 8

#define NO_SCORE INT64_MIN /* INT64_MIN is one smaller than -INT64_MAX */
#define MAX_HASH_COLLISIONS 32
#define HASH_TABLE_SIZE (1024 * 1024 + MAX_HASH_COLLISIONS)

#define FILLED_CELL 128

#define MAX_AB_DEPTH 10
#define AB_ITEMS 32
#define AB_MOVE_IS_THREAT 128
#define AB_MOVE_IS_BLOCK 64
#define AB_MOVE_IS_LAST 32

#define INFINITY INT64_MAX /* Used for beta pruning -INFINITY for alpha */
#define WIN (INFINITY - 1)
#define LOSE -WIN


struct cell {
    uint64_t zobrist_hash,
			 *adjacent,
             *score,
             score_me,
             score_opp;
    uint8_t  *threat,
			 threat_level_me[5],
			 threat_level_opp[5],
			 threat_me,
			 threat_opp;
};

struct bucket {
	uint64_t hashpart_depth_round;
	long long value;
};

struct field {
	struct timeval tv;
	double game_time,
		   move_time;
	long long best_ab_score;
    uint64_t *lookup,
			 *zobrist;
	struct bucket *hash_table;
    uint8_t *nr_adjacent,
            *index_adjacent;
    struct cell *cell;
	uint8_t round,
            start_player,
			next_move,
			min_ab_depth,
			ab_items,
			start_move[3],
			*score_sort,
			*threat_space,
			*ab_moves_depth[MAX_AB_DEPTH];
    int count;
	char buffer[6];
};

struct field *init_field();
void make_lookup(uint64_t *lookup);
uint64_t calc_score(const uint8_t me, const uint8_t opponent);
void init_zobrist(uint64_t *zobrist);
void init_cell(struct field *const field);
uint8_t add_index_adjacent(const uint8_t index, const uint8_t *adj_index_shift,
                          uint64_t *const adjacent, uint8_t *index_adjacent);
long long find_hash(const uint64_t zobrist_hash, const struct field *const field);
void add_hash(const long long value, const uint64_t zobrist_hash, struct field *const field);
uint8_t calc_threat_level(const uint64_t score);
long long toggle_move(const uint8_t index, const uint8_t is_opp_move,
                 struct cell *const cell, const struct field *const field);
uint8_t get_input(char *buffer);
void swap_players(struct field *field);
uint8_t coord_to_index(char *coord);
int play_first_moves(struct field *field);
int play_normal_moves(struct field *field);
uint8_t make_move(struct field *field);
long long ab_prune(uint8_t index, int depth, long long alpha, long long beta, struct field *field);
void copy_threat_space(uint8_t *dest, const uint8_t *src);
void update_sort_move(const uint8_t index, const int depth, const int player, struct field *const field);
void set_score_index_start(const uint8_t start, uint8_t *array);
void pop_score_index(const uint8_t index, uint8_t *array);
void push_score_index(const uint8_t index, uint8_t *array, const uint8_t score);
void copy_sort_score(uint8_t *base);
uint8_t get_next_score_index(const uint8_t index, uint8_t *array);

int main() /* int argc, char **argv */
{
    struct field *field;
    struct timeval tv;

/*    int i, j, round;
    uint8_t player, index, prime;*/
	int play = 0;

    gettimeofday(&tv, NULL);
    #ifdef DEBUG
        if (!freopen("log.txt","w",stderr) ||
            !freopen("input.txt","r",stdin)) {
            printf("Debug input/output file error");
            return 1;
        }
    #endif

	fprintf(stderr, "Start..");
	fflush(stderr);

    if (!(field = init_field())) {
        return 1;
    }

    field->tv.tv_usec = tv.tv_usec;
    field->tv.tv_sec = tv.tv_sec;

    make_lookup(field->lookup);
	init_zobrist(field->zobrist);
    init_cell(field);

	gettimeofday(&tv, NULL);
	fprintf(stderr, "Init: %.3f\n",
				(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
				(double)(tv.tv_sec - field->tv.tv_sec));
	fflush(stderr);

	while(play >= 0) {
		if (play) {
			play = play_normal_moves(field);
		} else {
			play = play_first_moves(field);
			fprintf(stderr, "\n");
			fflush(stderr);
		}
		if (field->game_time > 4.0) {
			field->min_ab_depth = MAX_AB_DEPTH - 2;
		}
		if (field->game_time > 4.5) {
			field->min_ab_depth = MAX_AB_DEPTH - 4;
		}
	}
    return 0;
}

struct field *init_field()
{
    struct field *field = NULL;
    struct cell *cell = NULL;
	struct bucket *hash_table = NULL;
    uint8_t *a = NULL,
			*t = NULL;
    uint64_t *lookup = NULL,
             *c = NULL;
	int i;

    if (!(field = calloc(1, sizeof(*field)))) {
        goto err;
    }
    if (!(lookup = malloc(65536 * sizeof(*lookup) + 2 * 256 * sizeof(*field->zobrist)))) {
        goto err;
    }
    field->lookup = lookup;
	field->zobrist = lookup + 65536;

	//if (!(hash_table = calloc(HASH_TABLE_SIZE, sizeof(*hash_table)))) {
	if (!(hash_table = malloc(HASH_TABLE_SIZE * sizeof(*hash_table)))) {
        goto err;
    }
	field->hash_table = hash_table;

	if (!(a = calloc(256 + /* #Cells */
			256 * 32 * 3 + /* Cell adjacent info */
			(MAX_AB_DEPTH + 2) * 32 + /* Threat space search */
			MAX_AB_DEPTH * (SCORE_INDEX_MAX + 3 * 256) + /* Score sort */
			MAX_AB_DEPTH * (MAX_AB_DEPTH + 1) /* AB moves info */, sizeof(*a)))) {
        goto err;
    }
    field->nr_adjacent = a;
    field->index_adjacent = field->nr_adjacent + 256;
    field->threat_space = field->index_adjacent + 256 * 32 * 3;
    field->score_sort = field->threat_space + (MAX_AB_DEPTH + 2) * 32;
    field->ab_moves_depth[0] = field->score_sort + MAX_AB_DEPTH * (SCORE_INDEX_MAX + 3 * 256);

    if (!(cell = calloc(1, sizeof(*cell)))) {
        goto err;
    }
    if (!(c = calloc(2 * 256, sizeof(*c)))) {
        goto err;
    }
    if (!(t = calloc(256, sizeof(*t)))) {
        goto err;
    }
	field->min_ab_depth = MAX_AB_DEPTH;
	field->ab_items = AB_ITEMS;
    field->cell = cell;
    cell->adjacent = c;
    cell->score = c + 256;
	cell->threat = t;

	t = field->ab_moves_depth[0];
	for (i = 0; i < MAX_AB_DEPTH; i++) {
		field->ab_moves_depth[i] = t;
		t += (MAX_AB_DEPTH - i) * 2;
	}

    return field;

err:
    if (t) {
        free(t);
    }
    if (c) {
        free(c);
    }
    if (cell) {
        free(cell);
    }
    if (a) {
        free(a);
    }
    if (hash_table) {
        free(hash_table);
    }
    if (lookup) {
        free(lookup);
    }
    if (field) {
        free(field);
    }
    return NULL;
}

/*
 * Table with scores for all possible positions seen from a cell
 *
 * Each cell has 8 adjecent cells for a direction (hor, vert, diags) to make 5 in a row
 * hor: 4 left + 4 right, vert: 4 up + 4 down, etc
 * These can be empty, filled with your stones, with opponents stones or outside the field
 * With 16 bits (8 bits me, 8 bits opponent) we cover all filled possibilities
 * Empty cells is a 0 for both players, outside the field is a 1 for both players
 *
 */
void make_lookup(uint64_t *lookup)
{
    uint64_t *t;
    int i;

    /* score me */
    t = lookup;
    for (i = 0; i < 65536; i++) {
        *t++ = calc_score((uint8_t)(i >> 8), (uint8_t)(i & 0xff));
    }
    /* score opponent */
    t = lookup;
    for (i = 0; i < 65536; i++) {
        lookup[((i << 8) & 0xff00) + (i >> 8)] |= (*t++ & SCORE_MASK_ME) >> 5;
    }

    return;
}

/*
 * calculate scores for every posible position in 1 direction (hor, vert, diags)
 *
 * score: the most amount of stones present for a 5 in a row
 *
 * Example:
 * Wo.x(X).x.o  (X): cell to calculate score for
 * 8765   4321    W: Wall (outside playing field if cell is close to the left border)
 *
 * function is called in 2x 8bit pieces of adjacent cells
 *   Me      Opponent
 * 87654321  87654321
 * W..x.x..  Wo.....o
 * 10010100  11000001
 *
 * Use a sliding window of 4 bits on both players
 * - If there is a 1 in the Opponents part
		it is blocked by opponent or outside the playing field, move window 1 bit (both parts)
 * - If not
 * 	 - count how many bits are set in the Me part
 *     If the (X) was inserted in it's position it would give the full 5 positiona, but we don't need that
 *   - Also check if a 5 bit window fits in the opponent part, this will make ik an open sides score
 *
 *    ----       ----   4 bit window, 2 bits set
 * 10010100   11000001
 *              -----   5 bit window fits: open sides
 *
 * Soooooo, this cell can make a 2 + (X) = 3of5 open sides
 * Since this has the most stones, this is the score for this position
 *
 * Score bits will be set in a 64bit value (see below)
 * It holds this position score for both players
 * if (X) is replaced with (O) you get opponents score (which is 0, since x always blocks somewhere)
 */
uint64_t calc_score(const uint8_t me, const uint8_t opponent)
{
	/*
	 *					Score bits when playing this move
	 *				me						      opponent
	 * both sides open  side closed     both sides open  side closed
	 *	  bit 60: 5of5, WIN                bit 55: 5of5, WIN
	 *		  56: 4of5  46: 4of5               51: 4of5  41: 4of5
	 *		  36: 3of5  26: 3of5               31: 3of5  21: 3of5
	 *		  16: 2of5   6: 2of5               11: 2of5   1: 2of5
	 *
	 *
	 * This is a 5-bit Parallel adder attempt
	 * A cell has 4 directions (hor, vert, diags) so you can just add scores (5bit parallel adder)
	 & To calculate the overall field score: put a player mask over a cell score so only one of the players bits remain
	 * Then all 256 cell scores could be added (10bits should not overflow)
	 */
    uint64_t score, v;
    int i, c, t;
    uint8_t m,
            mask = 0xf,
            side_mask = 0x11;

    score = 0;
    t = 0;
    for (i = 0; i < 5; i++) {
        if (opponent & mask) { /* blocked by opponent, cannot make 5 in a row */
            mask <<= 1;
            side_mask <<= 1;
            continue;
        }
		v = 0;
        c = 0;
        m = me & mask;
        while (m) { /* count set bits */
            c += (m & 0x01);
            m >>= 1;
        }
        if (c == 4) {
            v = 0x1ULL << 59;
        }
        if (t < c) {
            /* Keep best score only, ie: x.xXx.. = closed 4of5, don't count open 3of5 */
            t = c;
            score = 0;
        }
        if (c && c != 4) {
            v = 0x20ULL << ((c - 1) * 20);
            /* check for score with both side open */
            if (c == t && mask != 0xf0 && !((opponent | me) & side_mask)) {
                v <<= 10;
            }
        }
        score |= v;
        side_mask <<= 1;
        mask <<= 1;
    }

    return score;
}

void init_zobrist(uint64_t *zobrist)
{
	int i;

	for (i = 0; i < 2 * 256; i++) {
		*zobrist++ = (((uint64_t) rand() << 0) & 0x000000000000ffffULL) |
						(((uint64_t) rand() << 16) & 0x00000000ffff0000ULL) |
						(((uint64_t) rand() << 32) & 0x0000ffff00000000ULL) |
						(((uint64_t) rand() << 48) & 0xffff000000000000ULL);
	}
	/*
	 * Excerpts from: https://www.gnu.org/software/libc/manual/html_node/ISO-Random.html
	 * The rand function returns the next pseudo-random number in the series. The value ranges from 0 to RAND_MAX.
	 * If you call rand before a seed has been established with srand, it uses the value 1 as a default seed.
	 * RAND_MAX: In the GNU C Library, it is 2147483647, which is the largest signed integer representable in 32 bits.
	 * In other libraries, it may be as low as 32767.
	 */
	return;
}

void init_cell(struct field *const field)
{
	/* Initial thought: cache friendly */
	/*
    uint8_t adj_index_shift[8*4*2] = {
        0xcc, 23,                               0xc0, 39,                               0xc4,  7,
                  0xbb, 22,                     0xb0, 38,                     0xb3,  6,
                            0xaa, 21,           0xa0, 37,           0xa2,  5,
                                      0x99, 20, 0x90, 36, 0x91,  4,
        0x0c, 55, 0x0b, 54, 0x0a, 53, 0x09, 52,           0x01, 51, 0x02, 50, 0x03, 49, 0x04, 48,
                                      0x19,  3, 0x10, 35, 0x11, 19,
                            0x2a,  2,           0x20, 34,           0x22, 18,
                  0x3b,  1,                     0x30, 33,                     0x33, 17,
        0x4c,  0,                               0x40, 32,                               0x44, 16
    };*/
	/*
	 * Idea for last test competition (2019-12-28):
	 * From outside to inside (the RADIX-ish sort is an unstable LIFO sort)
	 * so cells with same score closer to last move will be searched first in next move
	 */
    uint8_t adj_index_shift[8*4*2] = {
        0xcc, 23,                               0xc0, 39,                               0xc4,  7,
        0x0c, 55,                                                                       0x04, 48,
        0x4c,  0,                               0x40, 32,                               0x44, 16,
                  0xbb, 22,                     0xb0, 38,                     0xb3,  6,
                  0x0b, 54,                                                   0x03, 49,
                  0x3b,  1,                     0x30, 33,                     0x33, 17,
                            0xaa, 21,           0xa0, 37,           0xa2,  5,
                            0x0a, 53,                               0x02, 50,
                            0x2a,  2,           0x20, 34,           0x22, 18,
                                      0x99, 20, 0x90, 36, 0x91,  4,
                                      0x09, 52,           0x01, 51,
                                      0x19,  3, 0x10, 35, 0x11, 19


    };
    uint8_t *nr_adjacent, *index_adjacent;
    uint64_t *adjacent;
    int i;

    nr_adjacent = field->nr_adjacent;
    index_adjacent = field->index_adjacent;
    adjacent = field->cell->adjacent;
    for (i = 0; i < 256; i++) {
        *nr_adjacent++ = add_index_adjacent(i, adj_index_shift, adjacent, index_adjacent);
        index_adjacent += 3 * 32;
        adjacent++;
    }
    return;
}

uint8_t add_index_adjacent(const uint8_t index, const uint8_t *adj_index_shift,
                          uint64_t *const adjacent, uint8_t *index_adjacent)
{
    uint8_t c, v;
    int i, x, y;

    c = 0;
    for (i = 0; i < 32; i++) {
        v = *adj_index_shift++;
        x = v & 0x7; /* x distance */
        if (v & 0x8) {
            x = -x;
        }
        x += index & 0x0f; /* -4 <= x <= 20 */
        y = (v & 0x70) >> 4; /* y distance */
        if (v & 0x80) {
            y = -y;
        }
        y += index >> 4; /* -4 <= y <= 20 */
        v = *adj_index_shift++; /* shift to index */
        if (x < 0 || x > 15 || y < 0 || y > 15) {
            *adjacent |= 0x101ULL << v;  /* not in field, set wall bits (me and opponent) */
            continue;
        }
        *index_adjacent++ = v;
        *index_adjacent++ = x | (y << 4); /* adjacent index */
        *index_adjacent++ = (v & 0x30) | (0x7 - (v & 0x7)); /* shift cross reference */
        c++;
    }
    return c;
}

long long find_hash(const uint64_t zobrist_hash, const struct field *const field)
{
	/*
	 * index: 20-LSB of zobrist_hash
	 * bucket
	 * 		hashpart_depth_round: bit64-13: 52-MSB zobrist_hash, bit12-9: 4 bits search depth, bit8-1: round
	 *		value: alpha beta prune value
	 * if same zobrist_hash is found, larger search depth is considered a new position)
	 */
	struct bucket *bucket = field->hash_table + (zobrist_hash & 0xfffff);
	uint64_t key = bucket->hashpart_depth_round;
	int c = 0;
	const uint8_t round = field->round;

	while (c < MAX_HASH_COLLISIONS && round == (key & 0xff) && (key ^ zobrist_hash) >> 12) {
		bucket++;
		key = bucket->hashpart_depth_round;
		c++;
	}
	if (c == MAX_HASH_COLLISIONS || round != (key & 0xff) ||
		/* if new search depth is larger than value depth part, treat as new (will be replaced) */
				field->min_ab_depth > (int)((key >> 8) & 0xf)) {
		return NO_SCORE;
	}
	return bucket->value;
}

void add_hash(const long long value, const uint64_t zobrist_hash, struct field *const field)
{
	/*
	 * add hash, append after collisions with same round
	 * if same zobrist_hash exists: replace
	 * does NOT check if search tree depth is longer (should already be checked in find_hash function)
	 *
	 * index: 20-LSB of zobrist_hash
	 * bucket
	 * 		hashpart_depth_round: bit64-13: 52-MSB zobrist_hash, bit12-9: 4 bits search depth, bit8-1: round
	 *		value: alpha beta prune value
	 */
//assert(value != NO_SCORE);
	struct bucket *bucket = field->hash_table + (zobrist_hash & 0xfffff);
	uint64_t key = bucket->hashpart_depth_round;
	int c = 0;
	const uint8_t round = field->round;

	while (c < MAX_HASH_COLLISIONS && round == (key & 0xff) && (key ^ zobrist_hash) >> 12) {
		bucket++;
		key = bucket->hashpart_depth_round;
		c++;
	}
	if (c < MAX_HASH_COLLISIONS) {
		bucket->hashpart_depth_round = (zobrist_hash & 0xfffffffffffff000ULL) | (field->min_ab_depth << 8) | round;
		bucket->value = value;
	}
	return;
}

inline uint8_t calc_threat_level(const uint64_t score)
{
	uint8_t threat = 0;

	if (score & (0x1ULL << 54)) { /* bit 55: WIN */
		threat = 1;
	} else if (score & (0xfULL << 50) /* bit 54-51: at least 1x 4of5 open sides */
				|| score & (0xfULL << 41)) { /* bit 45-42: at least 2x 4of5 closed side */
		threat = 2;
	} else if (score & (0x1ULL << 40) && score & (0x1fULL << 30)) { /* bit 41 + 35-31 */
				/* 1x 4of5 closed side and at least 1x 3of5 open sides */
		threat = 3;
	} else if (score & (0xfULL << 31)) { /* bit 35-32: at least 2x 3of5 open sides */
		threat = 4;
	}
	/* Threat ME */
	if (score & (0x1ULL << 59)) { /* bit 60: WIN */
		threat += 1 << 4;
	} else if (score & (0xfULL << 55) /* bit 59-56: at least 1x 4of5 open sides */
				|| score & (0xfULL << 46)) { /* bit 50-47: at least 2x 4of5 closed side */
		threat += 2 << 4;
	} else if (score & (0x1ULL << 45) && score & (0x1fULL << 35)) { /* bit 46 + 40-36 */
				/* 1x 4of5 closed side and at least 1x 3of5 open sides */
		threat += 3 << 4;
	} else if (score & (0xfULL << 36)) { /* bit 40-37: at least 2x 3of5 open sides */
		threat += 4 << 4;
	}

	return threat;
}

/*
 * Toggle_move
 *
 * This is the most important function in this program
 * - sets or unsets a cell
 * - updates zobrish hash
 * - if position is already visited with same or lesser search depth (transposition table)
 *		- returns score value
 * - else (new position or same position with deeper search depth)
 * 		- for all adjacent cells (precalculed indexes)
 * 			- update scores (score lookup table)
 * 			- update threat level (score bit mask fiddling)
 * - update overall players scores and threats
 */
long long toggle_move(const uint8_t index, const uint8_t is_opp_move,
                 struct cell *const cell, const struct field *const field)
{
    uint64_t curr, *p, s, *lookup, score;
	long long value;
    int i;
    uint8_t *index_adjacent, t, shift, next, threat, me_t3, opp_t3, b4of5;
    /*
	 * Score bits:
	 *
     *          6         5         4         3         2         1
     *  4321098765432109876543210987654321098765432109876543210987654321
     *      Wme4o     me_4c     me_3p     me_3c     me_2o     me_2c
     *      12345     12345     12345     12345     12345     12345
     *           12345     12345     12345     12345     12345     12345
     *           Wop4o     opp4c     opp3o     opp3c     opp2o     opp2c
	 *
	 */
	s = cell->zobrist_hash ^ field->zobrist[(index << 1) + is_opp_move];
	score = cell->score[index];
	t = cell->threat[index];
	if (t & FILLED_CELL) {
		/* cell will be unset, add scores and threat */
		cell->score_me <<= 5;
        cell->score_me += score & SCORE_MASK_ME;
        cell->score_opp += score & SCORE_MASK_OPP;
		cell->threat_level_me[(t >> 4) & 0x07]++;
		cell->threat_level_opp[t & 0x07]++;
	} else {
		/* cell will be set, check if zobrist hash already exists */
		value = find_hash(s, field);
		if (value != NO_SCORE) {
			return value; /* Assume same position  */
		}
		/* new position, subtract scores and threat */
		cell->score_me <<= 5;
        cell->score_me -= score & SCORE_MASK_ME;
        cell->score_opp -= score & SCORE_MASK_OPP;
		cell->threat_level_me[t >> 4]--;
		cell->threat_level_opp[t & 0x07]--;
	}
	cell->zobrist_hash = s;
	cell->threat[index] ^= FILLED_CELL; /* toggle cell */

	me_t3 = 0;
    opp_t3 = 0;
    b4of5 = 0;

    index_adjacent = field->index_adjacent + 3 * 32 * index;
    lookup = field->lookup;
    curr = cell->adjacent[index];
    s = is_opp_move ? 0x1ULL : 0x100ULL;
    t = field->nr_adjacent[index];
    for (i = 0; i < t; i++) {
        if (curr & (0x101ULL << *index_adjacent)) {
            index_adjacent += 3;
            continue; /* Skip filled adjacent cells */
        }
        next = *(index_adjacent + 1);
		score = cell->score[next];
		/* subtract old score */
        cell->score_me -= score & SCORE_MASK_ME;
        cell->score_opp -= score & SCORE_MASK_OPP;
        /* update adjacent cell score and toggle reference bit to current */
        p = cell->adjacent + next;
        shift = *(index_adjacent + 2);
        score -= lookup[(*p >> (shift & 0x30)) & 0xffff];
		 *p ^= s << shift;
        score += lookup[(*p >> (shift & 0x30)) & 0xffff];
        cell->score[next] = score;
		/* add new score */
        cell->score_me += score & SCORE_MASK_ME;
        cell->score_opp += score & SCORE_MASK_OPP;
		/* Update threats */
		threat = cell->threat[next];
		cell->threat_level_me[threat >> 4]--;
		cell->threat_level_opp[threat & 0x07]--;
		threat = calc_threat_level(score);
		cell->threat[next] = threat;
		cell->threat_level_me[threat >> 4]++;
		cell->threat_level_opp[threat & 0x07]++;

		/*
		 * This bot assumes a player will always lose if it cannot outperform or oancel the other players threat
		 * But in some cases it is totally blind for a defensive move
		 *    -
		 *    .B .  <-- the bot assumes it will win after the ( B ) move because of the new created threat ( + )
		 *     .b       Opponent has to block ( - ) first, so bot can always make the ( + ) "endgame" move
		 *     +ob      Therefore it will call this a win right after the ( B ) move
		 *   . b ob
		 *  b  b  o		in this case however, o makes a 4of5 with it's last block move and regains the initiative
		 * .
		 */
		me_t3 |= (((threat >> 4) & 0x7) == 3);
		opp_t3 |= ((threat & 0x7) == 3);
		b4of5 |= (score & (0x1fULL << 40)) && (score & (0x1fULL << 45));

        index_adjacent += 3;
    }
    cell->score_me >>= 5;

	/* Set overall highest threat level */
	cell->threat_me = 0;
	if (cell->threat_level_me[1]) {
		cell->threat_me = 6;
	} else if (cell->threat_level_me[2]) {
		cell->threat_me = 5;
	} else if (cell->threat_level_me[3] - (me_t3 && b4of5)) {
//	} else if (cell->threat_level_me[3] > 1 || (me_t3 && !b4of5)) {
		cell->threat_me = 5;
	} else if (cell->threat_level_me[4]) {
		cell->threat_me = 3;
	}
    cell->threat_opp = 0;
	if (cell->threat_level_opp[1]) {
		cell->threat_opp = 6;
	} else if (cell->threat_level_opp[2]) {
		cell->threat_opp = 5;
	} else if (cell->threat_level_opp[3] - (opp_t3 && b4of5)) {
//	} else if (cell->threat_level_opp[3] > 1 || (opp_t3 && !b4of5)) {
		cell->threat_opp = 5;
	} else if (cell->threat_level_opp[4]) {
		cell->threat_opp = 3;
	}

    return NO_SCORE;
}


uint8_t get_input(char *buffer) {
	if (!scanf("%5s", buffer)) {
		return 0;
	}
	if (*buffer <= 'P') {
		return ( ((buffer[0] - 'A') << 4) + (buffer[1] - 'a') );
	}
	return 0;
}

void swap_players(struct field *field) {
	int i,
		player = field->start_player;

	for (i = 0; i < 3; i++) {
		/* Undo moves */
		toggle_move(field->start_move[2 - i], player, field->cell, field);
		player ^= 0x1;
	}
	//field->start_player = player;
	for (i = 0; i < 3; i++) {
		/* Do moves with other player */
		toggle_move(field->start_move[i], player, field->cell, field);
		player ^= 0x1;
	}
	//toggle_move(field->start_move[2], player, field->cell, field);

	return;
}

uint8_t coord_to_index(char *coord) {
    return (((*coord - 'A')<<4) + (coord[1] - 'a'));
}

int play_first_moves(struct field *field) {
	uint8_t index;
	char *buffer = field->buffer;
	int i;
	struct cell *cell = field->cell;
    struct timeval tv;

	index = get_input(buffer);

    gettimeofday(&tv, NULL);
	field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec);
    fprintf(stderr, "S: %.3f, ",
                (double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
                (double)(tv.tv_sec - field->tv.tv_sec));
	fflush(stderr);

	if (!*buffer || *buffer == 'Q') { /* Error or Quit */
		return -1;
	}
	if (*buffer == 'S') { /* Start, make 3 moves */
		field->start_player = 0;
		field->start_move[0] = coord_to_index("Hh");
		field->start_move[1] = coord_to_index("Hi");
		field->start_move[2] = coord_to_index("Ii");

		set_score_index_start(*field->start_move, field->score_sort);
		toggle_move(field->start_move[0], 0, cell, field);
		update_sort_move(field->start_move[0], MAX_AB_DEPTH, 0, field);
		toggle_move(field->start_move[1], 1, cell, field);
		update_sort_move(field->start_move[1], MAX_AB_DEPTH, 1, field);
		toggle_move(field->start_move[2], 0, cell, field);
		update_sort_move(field->start_move[2], MAX_AB_DEPTH, 0, field);
		copy_threat_space(field->threat_space, field->threat_space + 32);
		field->score_sort[SCORE_INDEX_MAX + *field->start_move * 3 + 2] = SCORE_INDEX_MAX - 1;
		field->round = 3;

		gettimeofday(&tv, NULL);
		fprintf(stderr, "C: Hh, Hi, Ii, E: %.3f\n",
					(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
					(double)(tv.tv_sec - field->tv.tv_sec));
		fflush(stderr);
		for (i = 0; i < 3; i++) {
			fprintf(stdout, "%c%c\n",
				'A' + (field->start_move[i] >> 4),
				'a' + (field->start_move[i] & 0xf));
		}
		fflush(stdout);
		return 0;
	}
	if (*buffer == 'Z') { /* Opponent refusal move */
		swap_players(field);
		//field->round = 3;
		field->round++;
		/* Best pre calculated move (saves time) */
		index = coord_to_index("Jj");
		field->round++;
		toggle_move(index, 0, cell, field);
		/* Best pre calculated move (saves time) */

		//index = make_move(field);
		update_sort_move(index, MAX_AB_DEPTH, 0, field);
		copy_threat_space(field->threat_space, field->threat_space + 32);
		gettimeofday(&tv, NULL);
		fprintf(stderr, "C: Jj, E: %.3f\n",
					(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
					(double)(tv.tv_sec - field->tv.tv_sec));
		fflush(stderr);
		fprintf(stdout, "%c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
		fflush(stdout);

		return 1;
	}

	if (field->round) {
		/* Opponent agreed start moves */
		toggle_move(index, 1, field->cell, field);
		update_sort_move(index, MAX_AB_DEPTH, 1, field);
		field->round++;
		/* My move */
        index = make_move(field);
        update_sort_move(index, MAX_AB_DEPTH, 0, field);
		copy_threat_space(field->threat_space, field->threat_space + 32);
		gettimeofday(&tv, NULL);
		field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec) - field->move_time + 0.002;
		field->game_time += field->move_time;
		fprintf(stderr, "T: %.3f, G: %.3f, M: %.3f\n",
					(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
					(double)(tv.tv_sec - field->tv.tv_sec), field->game_time, field->move_time);
		fflush(stderr);
		fprintf(stdout, "%c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
		fflush(stdout);

		return 1;
	}

	/* Opponent starts (first 3 moves) */
	field->start_player = 1;
	field->start_move[0] = index;
	field->start_move[1] = get_input(buffer);
	field->start_move[2] = get_input(buffer);

	set_score_index_start(*field->start_move, field->score_sort);
	toggle_move(field->start_move[0], 1, field->cell, field);
	update_sort_move(field->start_move[0], MAX_AB_DEPTH, 1, field);
	toggle_move(field->start_move[1], 0, field->cell, field);
	update_sort_move(field->start_move[1], MAX_AB_DEPTH, 0, field);
	toggle_move(field->start_move[2], 1, field->cell, field);
	update_sort_move(field->start_move[2], MAX_AB_DEPTH, 1, field);
	field->score_sort[SCORE_INDEX_MAX + *field->start_move * 3 + 2] = SCORE_INDEX_MAX - 1;
	field->round = 3;

	/* make best move */
	index = make_move(field);
	if (field->best_ab_score < -(0x1LL<<60) + (0x5LL << 30)) {
	/* 2020-01-18 some gut feeling score value based on test competitions */
	//if (field->cell->score_me < field->cell->score_opp) {
		/* Refuse opponents start moves, rewind our move */
		toggle_move(index, 0, field->cell, field);
		//field->round--;
		swap_players(field);
		copy_threat_space(field->threat_space, field->threat_space + 32);
		gettimeofday(&tv, NULL);
		field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec) - field->move_time + 0.002;
		field->game_time += field->move_time;
		fprintf(stderr, "C: Zz\nT: %.3f, G: %.3f, M: %.3f\n",
					(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
					(double)(tv.tv_sec - field->tv.tv_sec), field->game_time, field->move_time);
		fflush(stderr);
		fprintf(stdout, "Zz\n");
		fflush(stdout);
	} else {
		update_sort_move(index, MAX_AB_DEPTH, 0, field);
		copy_threat_space(field->threat_space, field->threat_space + 32);
		gettimeofday(&tv, NULL);
		field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec) - field->move_time + 0.002;
		field->game_time += field->move_time;
		fprintf(stderr, "T: %.3f, G: %.3f, M: %.3f\n",
					(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
					(double)(tv.tv_sec - field->tv.tv_sec), field->game_time, field->move_time);
		fflush(stderr);
		fprintf(stdout, "%c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
		fflush(stdout);
	}

	return 1;
}


int play_normal_moves(struct field *field) {
	uint8_t index;
	char *buffer = field->buffer;
    struct timeval tv;

	index = get_input(buffer);
	if (!*buffer || *buffer == 'Q') { /* Error or Quit */
		return -1;
	}
    gettimeofday(&tv, NULL);
 	field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec);
    fprintf(stderr, "S: %.3f, ",
                (double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
                (double)(tv.tv_sec - field->tv.tv_sec));
	fflush(stderr);
/* Keep this assert in case of stupid test input mistakes */
assert(!(field->cell->threat[index] & FILLED_CELL));

	toggle_move(index, 1, field->cell, field);
	update_sort_move(index, MAX_AB_DEPTH, 1, field);
	field->round++; /* new opponents move */

	index = make_move(field);
	update_sort_move(index, MAX_AB_DEPTH, 0, field);
	copy_threat_space(field->threat_space, field->threat_space + 32);
	gettimeofday(&tv, NULL);
	field->move_time = (double)(tv.tv_usec) / 1000000 + (double)(tv.tv_sec) - field->move_time + 0.002;
	field->game_time += field->move_time;
	fprintf(stderr, "T: %.3f, G: %.3f, M: %.3f\n\n",
				(double)(tv.tv_usec - field->tv.tv_usec) / 1000000 +
				(double)(tv.tv_sec - field->tv.tv_sec), field->game_time, field->move_time);
	fflush(stderr);
	fprintf(stdout, "%c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
	fflush(stdout);

	return 1;
}

#ifdef DEBUG

	/* only for debug purposes, (print ab path) */
	void add_ab_move(uint8_t *ab_moves_depth, const uint8_t index, const uint8_t count, const uint8_t type)
	{
		*ab_moves_depth++ = index;
		*ab_moves_depth = count | type;
		return;
	}

	/* only for debug purposes, (print ab path) */
	void copy_ab_depth(uint8_t *ab_moves_depth_dest, const uint8_t *ab_moves_depth_src, const uint8_t depth)
	{
		int i;

		ab_moves_depth_dest += 2; /* Keep first move in this depth */
		for (i = 0; i < depth * 2; i++) {
			*ab_moves_depth_dest++ = *ab_moves_depth_src++;
		}
		return;
	}

	/* only for debug purposes, (print ab path) */
	void print_ab_moves(uint8_t *ab_moves_depth)
	{
		int i;
		uint8_t t;

		for (i = 0; i < MAX_AB_DEPTH; i++) {
			t = *ab_moves_depth++;
			fprintf(stderr, "D:%02d, Cell: %c%c", MAX_AB_DEPTH - i, 'A' + (t >> 4), 'a' + (t & 0xf));
			t = *ab_moves_depth;
			fprintf(stderr, ", #%02d", t & 0x1f); /* cell element number */
			if (t & AB_MOVE_IS_THREAT) {
				fprintf(stderr, " (Attack)");
			} else if (t & AB_MOVE_IS_BLOCK) {
				fprintf(stderr, " (Defend)");
			}
			if (t & AB_MOVE_IS_LAST) {
				fprintf(stderr, " (Last)\n");
				fflush(stderr);
				break;
			}
			fprintf(stderr, "\n");
			ab_moves_depth++;
		}
		return;
	}

#endif

uint8_t make_move(struct field *field) {
	struct cell *cell = field->cell;
	uint8_t index, start; /*, i;

        fprintf(stderr, "Round %d, Threat:\n", field->round + 1);
		for (i = 0; i < 5; i++) {
			fprintf(stderr, "%d: %d/%d, ", i, field->cell->threat_level_me[i], field->cell->threat_level_opp[i]);
        }
        fprintf(stderr, "\nElements:\n");
        i = get_next_score_index(*field->start_move, field->score_sort);
        while (i != *field->start_move) {
            fprintf(stderr, "%c%c (%d), ", 'A' + (i >> 4), 'a' + (i & 0xf), field->score_sort[SCORE_INDEX_MAX + i * 3 + 2]);
            i = get_next_score_index(i, field->score_sort);
        }
        fprintf(stderr, "\n");
        fflush(stderr);*/

	start = *field->start_move;
	index = get_next_score_index(start, field->score_sort);
	/* If a draw is inevitable, take the first move available */
	if (field->score_sort[SCORE_INDEX_MAX + index * 3 + 2]) {
		if (cell->threat_me == 6) { /* I can make 5of5 -> WIN */
			while(index != start && !(((cell->threat[index] >> 4) & 7) == 1)) {
				index = get_next_score_index(index, field->score_sort);
			}
			fprintf(stderr, "R: %d\n", field->round + 1);
            fprintf(stderr, "C: %c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
			fflush(stderr);
		} else if (cell->threat_opp == 6) { /* Opponent can make 5of5 -> BLOCK */
			while(index != start && !((cell->threat[index] & 7) == 1)) {
				index = get_next_score_index(index, field->score_sort);
			}
			fprintf(stderr, "R: %d\n", field->round + 1);
            fprintf(stderr, "C: %c%c\n", 'A' + (index >> 4), 'a' + (index & 0xf));
			fflush(stderr);
		} else {
			field->count = 0;
			field->next_move = index;
			field->best_ab_score = ab_prune(0, MAX_AB_DEPTH, -INFINITY, INFINITY, field);
			index = field->next_move;
			fprintf(stderr, "N: %d, ", field->count);
			fflush(stderr);
		}
	}

#ifdef DEBUG
	if (field->buffer[2] == '-') {
		/*
		 * Test game up to certain position
		 * If there's a minus after opponents move, play move after that sign
		 * USE THIS FOR NORMAL MOVES ONLY OR WHEN OPPONENT STARTS
		 */
		if (field->buffer[3] == 'Z') {
			field->best_ab_score = LOSE; /* Refuse opponents start move (Zz), pick worst score */
			index = *field->score_sort;
		} else {
			field->best_ab_score = WIN; /* always accept test move */
			index = coord_to_index(field->buffer + 3);
		}
	}
#endif

	field->round++; /* Early round increase to prevent hash collision best move */
	toggle_move(index, 0, cell, field);

//assert(index != start);

	return index;
}

long long ab_prune(uint8_t index, int depth, long long alpha, long long beta, struct field *field) {
/* TODO
	- hashmap, transposition table and/or visited / heatmap: DONE (zobrist hash)
	- abprune upgrade? (see wikipedia https://en.wikipedia.org/wiki/MTD-f)
	- rewrite once this mess runs stable
*/
    struct cell *cell = field->cell;
	uint8_t *danger_p1 = &cell->threat_me,
            *danger_p2 = &cell->threat_opp,
			*base = field->score_sort + (SCORE_INDEX_MAX + 3 * 256) * (MAX_AB_DEPTH - depth),
			*threat_space = field->threat_space + (MAX_AB_DEPTH - depth) * 32,
			s, ab_move_type = 0;
    const uint8_t start = *field->start_move;
	long long value = NO_SCORE;
	int t, player = (field->start_player ^ (field->round + MAX_AB_DEPTH - depth)) & 0x1;
	#ifdef DEBUG
        int i;
	#endif

	if (player) { /* opponent */
		/* swap pointers */
		danger_p1 = &cell->threat_opp;
		danger_p2 = &cell->threat_me;
	}
	/* Terminal node states */
	if (*danger_p1 == 6) { /* Last move from other player cannot clear highest threat */
		/*
		 * 2019-12-20: Ok, this is getting REALLY annoying
		 * I cannot seem to calculate a reliable score that always plays the endgame moves
		 * Sometimes it just won't play the killer moves it found 2 rounds before
		 * because all kinds of other opportunities make a better score
		 */
		if (player) { /* Opponent */
			// 2010-01-11: AAAGGHHH, ok, try to count 2x open 3of5 only if created in this move
			value = LOSE + 512 - depth * 32 -
						(cell->threat_level_opp[1] > 1) * 100 - // 2x 5of5: LOSE
						cell->threat_level_opp[1] * 11 - // single 5of5
						cell->threat_level_opp[2] * 6 -  // open 4of5 or 2x 4of5
						cell->threat_level_opp[3] * - // 4of5 + open 3of5
						cell->threat_level_opp[4] * (cell->threat_level_opp[2] < 2); // 2x open 3of5
		} else {
			value = WIN - 512 + depth * 32 +
						(cell->threat_level_me[1] > 1) * 100 + // 2x 5of5: WIN
						cell->threat_level_me[1] * 11 + // single 5of5
						cell->threat_level_me[2] * 6 + // open 4of5 or 2x 4of5
						cell->threat_level_me[3] * 2 + // 4of5 + open 3of5
						cell->threat_level_me[4] * (cell->threat_level_me[2] < 2); // 2x open 3of5
		}
		field->ab_moves_depth[MAX_AB_DEPTH - depth - 1][1] |= AB_MOVE_IS_LAST;

		return value;
	}
	if (depth <= MAX_AB_DEPTH - field->min_ab_depth || field->round + MAX_AB_DEPTH - depth == 256) {
		/* calculate some score (always own score) */
		value = ((long long)cell->score_me) | ((0x7LL & cell->threat_me) << (60 - player));
		value -= ((long long)cell->score_opp) | ((0x7LL & cell->threat_opp) << (60 - !player));
		/* 2020-01-01: lets try some uncertainty penalty in the finals */
		value -= (0x1LL << 60) * (MAX_AB_DEPTH - field->min_ab_depth > 2);
		field->ab_moves_depth[MAX_AB_DEPTH - depth - 1][1] |= AB_MOVE_IS_LAST;
		return value;
	}
	/* Copy current score state if not root */
	if (depth < MAX_AB_DEPTH) {
//assert((cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
		copy_sort_score(base - SCORE_INDEX_MAX - 3 * 256);
		update_sort_move(index, depth, !player, field); /* Player from last move */
//assert((cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
	} else {
        fprintf(stderr, "R: %d\n", field->round + 1);
		fflush(stderr);
	}

	t = 0;
	index = get_next_score_index(start, base);
	if (*danger_p1 < *danger_p2) {
		/* Last turn player can make 5of5 faster than this turn player */
		while (index != start && base[SCORE_INDEX_MAX + index * 3 + 2] >= *danger_p2 /*&& t < AB_ITEMS*/) {
//assert(!(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
			value = toggle_move(index, player, cell, field);
			ab_move_type = AB_MOVE_IS_BLOCK;
			field->count++;
//assert((field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
			/* Check if this move blocks other player's threat, or a better counter move */
			if (value == NO_SCORE) {
				if (*danger_p2 && *danger_p1 <= *danger_p2) {
					toggle_move(index, player, cell, field); /* not good enough */
	//assert(!(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
					index = get_next_score_index(index, base);
					if (t || (index != start && base[SCORE_INDEX_MAX + index * 3 + 2] >= *danger_p2)) {
						continue;
					}
					/* No good block or counter move found, this branch ends */
					if (player) {
						value = WIN - 512 + depth * 32 +
									(cell->threat_level_me[1] > 1) * 100 + // 2x 5of5: WIN
									cell->threat_level_me[1] * 11 + // single 5of5
									cell->threat_level_me[2] * 6 + // open 4of5 or 2x 4of5
									cell->threat_level_me[3] * 2 + // 4of5 + open 3of5
									cell->threat_level_me[4] * (cell->threat_level_me[2] < 2); // 2x open 3of5
					} else {
						value = LOSE + 512 - depth * 32 -
									(cell->threat_level_opp[1] > 1) * 100 - // 2x 5of5: LOSE
									cell->threat_level_opp[1] * 11 - // single 5of5
									cell->threat_level_opp[2] * 6 -  // open 4of5 or 2x 4of5
									cell->threat_level_opp[3] * - // 4of5 + open 3of5
									cell->threat_level_opp[4] * (cell->threat_level_opp[2] < 2); // 2x open 3of5
					}
					ab_move_type |= AB_MOVE_IS_LAST;
					if (depth < MAX_AB_DEPTH) {
						field->ab_moves_depth[MAX_AB_DEPTH - depth - 1][1] |= AB_MOVE_IS_LAST;
					} else {
						field->ab_moves_depth[0][1] |= AB_MOVE_IS_LAST;
					}
				} else {

	//assert((field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));

					value = ab_prune(index, depth - 1, alpha, beta, field);
#ifdef DEBUG
                    if (value == NO_SCORE) {
                        fprintf(stderr, "NO SCORE error after abprune");
                        fflush(stderr);
                    }
#endif
                    add_hash(value, cell->zobrist_hash, field);
	//assert(value != INFINITY && value != -INFINITY);
					toggle_move(index, player, cell, field);
	//assert(!(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
				}
			} else {
				ab_move_type |= AB_MOVE_IS_LAST;
			}
			t++;
#ifdef DEBUG
			if (value == NO_SCORE) {
                fprintf(stderr, "NO SCORE error");
                fflush(stderr);
			}
#endif
			if (!player) { /* maximizing */
				if (value > alpha) {
					alpha = value;
#ifdef DEBUG
					add_ab_move(field->ab_moves_depth[MAX_AB_DEPTH - depth], index, t, ab_move_type);
					if (depth > 1) {
						copy_ab_depth(field->ab_moves_depth[MAX_AB_DEPTH - depth],
										field->ab_moves_depth[MAX_AB_DEPTH - depth + 1], depth - 1);
					}
#endif
                    if (depth == MAX_AB_DEPTH) {
                        fprintf(stderr, "C: %c%c, #: %d, A: %08lx%08lx\n",
                            'A' + (index >> 4), 'a' + (index & 0xf),
                            field->count,
                            (unsigned long int)(alpha >> 32) & 0xffffffff,
                            (unsigned long int)alpha & 0xffffffff);
#ifdef DEBUG
                        fprintf(stderr, "Threat: ");
                        for (i = 0; i < 5; i++) {
                            fprintf(stderr, "%d: %d/%d, ", i,
                                field->cell->threat_level_me[i],
                                field->cell->threat_level_opp[i]);
                        }
                        fprintf(stderr, "\nCells: ");
                        for (i = 0; i < 256; i++) {
                            if (field->cell->threat[i] && field->cell->threat[i] < FILLED_CELL) {
                                fprintf(stderr, "%c%c: %d/%d, ",
                                    'A' + (i >> 4), 'a' + (i & 0xf),
                                    field->cell->threat[i] & 3, field->cell->threat[i] >> 4);
                            }
                        }
                        fprintf(stderr, "\n");
                        print_ab_moves(*field->ab_moves_depth);
#endif
						fflush(stderr);
                    }
					if (depth == MAX_AB_DEPTH && index != start) {
						field->next_move = index;
					}
				}
			} else { /* minimizing */
				if (value < beta) {
					beta = value;
#ifdef DEBUG
					add_ab_move(field->ab_moves_depth[MAX_AB_DEPTH - depth], index, t, ab_move_type);
					if (depth > 1) {
						copy_ab_depth(field->ab_moves_depth[MAX_AB_DEPTH - depth],
										field->ab_moves_depth[MAX_AB_DEPTH - depth + 1], depth - 1);
					}
#endif
				}
			}
			if (alpha >= beta) {
				field->ab_moves_depth[MAX_AB_DEPTH - depth][1] |= AB_MOVE_IS_LAST;
				return value; /* cut off */
			}
			if (index != start) {
				index = get_next_score_index(index, base);
			}
		}
	} else {
		/* take the best moves */
		s = *threat_space;
		/*
		 * 2019-12-20: Try less "best" moves if there are threat space moves as well
		 * I hope this will shrink the search tree considerably, without weakening it (too much)
		 * Still fighting time out issues, but don't want to do shallower searches
		 */
		while (index != start && (s || t < (field->ab_items /* >> (*threat_space > 0) */ ))) {
			ab_move_type = 0;
			/* Start Threat space search */
			if (t < s) {
				ab_move_type = AB_MOVE_IS_THREAT;
				index = threat_space[t + 1];
				if (cell->threat[index] & FILLED_CELL || base[SCORE_INDEX_MAX + index * 3 + 2] < 4) {
					/* Move last turn was this cell, or nullified it's threat */
					t++;
					continue;
				}
//assert(!(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
			}
			if (s && s == t) {
				index = get_next_score_index(start, base);
				s = 0;
				t = 0;
				continue;
			}
			/* End Threat space search */
			value = toggle_move(index, player, cell, field);
            field->count++;
			if (value == NO_SCORE) {
				if (!s) {
					field->min_ab_depth--;
					/* Some value to get more aggressive in deeper searches (smaller tree search) */
					field->ab_items -= 3;
				}
                value = ab_prune(index, depth - 1, alpha, beta, field);
#ifdef DEBUG
                if (value == NO_SCORE) {
                    fprintf(stderr, "NO SCORE error after abprune");
                    fflush(stderr);
                }
#endif
                add_hash(value, cell->zobrist_hash, field);
	//assert(value != INFINITY && value != -INFINITY);
				if (!s) {
					field->min_ab_depth++;
					field->ab_items += 3; /* Some value */
				}
				toggle_move(index, player, cell, field);
			} else {
				ab_move_type |= AB_MOVE_IS_LAST;
			}
//assert(!(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f))));
#ifdef DEBUG
			if (value == NO_SCORE) {
                fprintf(stderr, "NO SCORE error");
                fflush(stderr);
			}
#endif
			if (!player) { /* maximizing */
				if (value > alpha) {
					alpha = value;
#ifdef DEBUG
					add_ab_move(field->ab_moves_depth[MAX_AB_DEPTH - depth], index, t, ab_move_type);
					if (depth > 1) {
						copy_ab_depth(field->ab_moves_depth[MAX_AB_DEPTH - depth],
										field->ab_moves_depth[MAX_AB_DEPTH - depth + 1], depth  - 1);
					}
#endif
                    if (depth == MAX_AB_DEPTH) {
                        fprintf(stderr, "C: %c%c, #: %d, A: %08lx%08lx\n",
                            'A' + (index >> 4), 'a' + (index & 0xf),
                            field->count,
                            (unsigned long int)(alpha >> 32) & 0xffffffff,
                            (unsigned long int)alpha & 0xffffffff);
#ifdef DEBUG
                        fprintf(stderr, "Threat: ");
                        for (i = 0; i < 5; i++) {
                            fprintf(stderr, "%d: %d/%d, ", i,
                                field->cell->threat_level_me[i],
                                field->cell->threat_level_opp[i]);
                        }
                        fprintf(stderr, "\nCells: ");
                        for (i = 0; i < 256; i++) {
                            if (field->cell->threat[i] && field->cell->threat[i] < FILLED_CELL) {
                                fprintf(stderr, "%c%c: %d/%d, ",
                                    'A' + (i >> 4), 'a' + (i & 0xf),
                                    field->cell->threat[i] & 3, field->cell->threat[i] >> 4);
                            }
                        }
                        fprintf(stderr, "\n");
                        print_ab_moves(*field->ab_moves_depth);
#endif
						fflush(stderr);
                    }
					if (depth == MAX_AB_DEPTH && index != start) {
						field->next_move = index;
					}
				}
			} else { /* minimizing */
				if (value < beta) {
					beta = value;
#ifdef DEBUG
					add_ab_move(field->ab_moves_depth[MAX_AB_DEPTH - depth], index, t, ab_move_type);
					if (depth > 1) {
						copy_ab_depth(field->ab_moves_depth[MAX_AB_DEPTH - depth],
										field->ab_moves_depth[MAX_AB_DEPTH - depth + 1], depth - 1);
					}
#endif
				}
			}
			if (alpha >= beta) {
				field->ab_moves_depth[MAX_AB_DEPTH - depth][1] |= AB_MOVE_IS_LAST;
				return value; /* cut off */
			}
			if (!s) {
				index = get_next_score_index(index, base);
			}
			t++;
		}
	}
#ifdef DEBUG
    if ((player && beta == INFINITY) || (!player && alpha == -INFINITY)) {
        fprintf(stderr, "INFINITY ERROR\n");
        fflush(stderr);
    }
#endif

    return (player ? beta : alpha);
}

void copy_threat_space(uint8_t *dest, const uint8_t *src) {
	memcpy(dest, src, 32 * sizeof(*src));

	return;
}

/*
 * Cell score sort inspired by RADIX sort
 * takes highest bit of updated cell score and moves it to score queue(0..6] higher is better
 * unstabel sort (LIFO)
 */
void update_sort_move(const uint8_t index, const int depth, const int player, struct field *const field)
{
    uint64_t curr, score;
	const uint64_t mask = player ? SCORE_MASK_OPP : SCORE_MASK_ME;
    int i;//, j;
    uint8_t t, c, s, next, *index_adjacent, *score_sort, *threat_space;
    struct cell *cell = field->cell;

	score_sort = field->score_sort + (SCORE_INDEX_MAX + 3 * 256) * (MAX_AB_DEPTH - depth);
	threat_space = field->threat_space + 32 * (MAX_AB_DEPTH - depth + 1);
//assert(field->cell->map[index>>6] & (0x1ULL << (index & 0x3f)));
	pop_score_index(index, score_sort);

	s = 0;
	index_adjacent = field->index_adjacent + 3 * 32 * index;
    curr = cell->adjacent[index];
    t = field->nr_adjacent[index];
    for (i = 0; i < t; i++) {
        next = *(index_adjacent + 1);
        if (curr & (0x101ULL << *index_adjacent)) {
//assert(field->cell->map[*(index_adjacent + 1)>>6] & (0x1ULL << (*(index_adjacent + 1) & 0x3f)));
//assert(field->cell->map[next>>6] & (0x1ULL << (next & 0x3f)));
            index_adjacent += 3;
            continue; /* Skip filled adjacent cells */
        }
		score = cell->score[next];
//assert(!(field->cell->map[next>>6] & (0x1ULL << (next & 0x3f))));
		if (!score) {
			pop_score_index(next, score_sort);
            push_score_index(next, score_sort, 0);
            index_adjacent += 3;
            continue;
		}
		/* Check for threat space */
		if ((score & mask) >= (0x1ULL << 30)) {
			/* Next move can make a new threat thanks to this move (new 3of5 open sides, or 4of5) */
			s++;
			threat_space[s] = next;
		}
		/* Update score sort */
        c = SCORE_INDEX_MAX - 2 - (__builtin_clzll(score) - 4) / 10;
		if (c == score_sort[SCORE_INDEX_MAX + next * 3 + 2]) {
            index_adjacent += 3;
            continue; /* Same score */
		}
		pop_score_index(next, score_sort);
		push_score_index(next, score_sort, c);

		index_adjacent += 3;
    }
	*threat_space = s;
	/*
	 * 2020-01-04: Try new optimization idea
	 * Put threats in front of score queues, so other player will search them early
	 */
	i = 0;
	while (i < s) {
		i++;
		t = threat_space[i];
		c = score_sort[SCORE_INDEX_MAX + t * 3 + 2];
		pop_score_index(t, score_sort);
		push_score_index(t, score_sort, c);
	}

    return;
}

void set_score_index_start(const uint8_t start, uint8_t *array)
{
	int i;

	/* Score array:
	 * 	   0 .. SCORE_INDEX_MAX - 2: score (0 - 6) first element index
	 *     SCORE_INDEX_MAX-1: start cell index
	 * Elements (cells) linked list:
	 *     SCORE_INDEX_MAX .. SCORE_INDEX_MAX + 3 * 256
	 * element:
	 * 	   index to previous element with same score (start = first in queue)
	 *     index to next element with same score (start = last in queue)
	 *     score (same as score array index it belongs to)
	 */

	array[0] = 0; /* 0 score queue starts with cell Aa (Even if this is the start cell) */
	for(i = 1; i < SCORE_INDEX_MAX; i++) {
		array[i] = start; /* other score queues are empty */
	}
	/* init score index linked list, except first previous and last next */
	for (i = 1; i < 256; i++) {
		array[SCORE_INDEX_MAX + (i - 1) * 3 + 1] = (uint8_t)i; /* element next "pointer" index */
		array[SCORE_INDEX_MAX + i * 3] = (uint8_t)(i - 1); /* element previous "pointer" index */
	}
	array[SCORE_INDEX_MAX] = start; /* index of first element previous "pointer" */
	array[SCORE_INDEX_MAX + 3 * 255 + 1] = start; /* index of last elements next "pointer" */
}


void pop_score_index(const uint8_t index, uint8_t *array)
{
	/* DO NOT double pop a cell, it will corrupt the score queues */
	uint8_t *p = array + SCORE_INDEX_MAX + index * 3;
	const uint8_t previous = *p,
				  next = *(p + 1),
				  score = *(p + 2),
				  start = array[SCORE_INDEX_MAX - 1];

	if (previous == start) {
		array[score] = next; /* next cell is first in score queue */
	} else {
		array[SCORE_INDEX_MAX + previous * 3 + 1] = next; /* set previous element next pointer */
		//*p = start;
	}
    //if (next != start) {
	/* set next element previous pointer (even if the popped one was last) */
        array[SCORE_INDEX_MAX + next * 3] = previous;
    //}
	//*(p + 1) = start;

	return;
}

void push_score_index(const uint8_t index, uint8_t *array, const uint8_t score)
{
	/* DO NOT push the start cell, it will corrupt the score queues */
	const uint8_t first = array[score],
				  start = array[SCORE_INDEX_MAX - 1];
	uint8_t *p = array + SCORE_INDEX_MAX + index * 3;

	*p++ = start; /* Previous: element will be first in score index queue, so no previous */
	*p++ = first; /* Next: is current first element of score index (start == was empty) */
	*p = score;
	//if (first != start) {
	/* set 2nd element previous pointer (even if there is no 2nd) */
		array[SCORE_INDEX_MAX + first * 3] = index;
	//}
	array[score] = index;

	return;
}

void copy_sort_score(uint8_t *base) {
	memcpy(base + SCORE_INDEX_MAX + 3 * 256, base, (SCORE_INDEX_MAX + 3 * 256) * sizeof(*base));

	return;
}

uint8_t get_next_score_index(const uint8_t index, uint8_t *array)
{
	/* ONLY use this after pop start cell and start score set to SCORE_INDEX_MAX - 1 */
	const uint8_t start = array[SCORE_INDEX_MAX - 1];
	uint8_t s, r = start,
			*p = array + SCORE_INDEX_MAX + index * 3;

	if (index != start) {
		r = *(p + 1); /* Element next "pointer" in score queue */
		if (r != start) {
			return r;
		}
	}
	/* No next element with this score */
	//if (index != start) {
		s = *(p + 2); /* Get elements score */
	//} else {
		//s = SCORE_INDEX_MAX - 1; /* Start at highest score */
	//}
	while (r == start && s) {
		s--; /* decrrease score */
		r = array[s]; /* score queue first element index */
	}
	return r;
}
