/*---------------------------------------------------------------------------
 *                        OPERA - Optimized Serena
 *---------------------------------------------------------------------------
 * Authors: Ichrak Amdouni, Cedric Adjih
 * Copyright 2010-2011 Inria.
 *
 * This file is part of the OPERA.
 *
 * The OPERA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * The OPERA is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the OPERA; see the file LICENSE.LGPLv3.  If not, see
 * http://www.gnu.org/licenses/ or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. 
 *---------------------------------------------------------------------------*/

#ifndef _HIPSENS_OSERENA_H
#define _HIPSENS_OSERENA_H

/*---------------------------------------------------------------------------*/

#define HIPSENS_MSG_COLOR 'C' 
#define HIPSENS_MSG_MAX_COLOR 'M'
#define HIPSENS_MSG_DESCENDANT_COUNT 'D'

#define MAX_PRIO1_SIZE 4
#define MAX_PRIO2_SIZE 3
#define MAX_PRIO3_SIZE 1

#ifndef NB_COLOR_MAX
#warning NB_COLOR_MAX was not defined using default value
#define NB_COLOR_MAX 128
#endif

#define COLOR_NONE 0xffu
#define PRIORITY_NONE 0

#define IMPLICIT_COLORING_DETECTION

/* this is NO_COLOR in OCARI */
#define OCARI_NO_COLOR 0

/*--------------------------------------------------*/

typedef eond_state_t serena_nd_state_t;

/*--------------------------------------------------
 * Bitmaps 
 * (for sets of color)
 *--------------------------------------------------*/

#define BYTES_PER_BITMAP ((NB_COLOR_MAX + BITS_PER_BYTE-1)/ BITS_PER_BYTE)

typedef struct s_bitmap_t {
  byte content[BYTES_PER_BITMAP];
} bitmap_t;

/*--------------------------------------------------
 * Priority
 *--------------------------------------------------*/

// XXX: remove: changed to a struct during refactoring only to be sure
// to have updated all code
#define GET_PRIORITY(priority) ((priority).value)
#define IS_PRIORITY_NONE(priority) ((priority).value == PRIORITY_NONE)
#define SET_PRIORITY(priority,newvalue) (priority).value = (newvalue)


#ifdef WITH_LONG_PRIORITY

#define FMT_PRIORITY "%lu"
typedef struct s_priority_t {
  unsigned long value;
} priority_t;

#define buffer_get_PRIORITY(buffer,target) \
  BEGIN_MACRO SET_PRIORITY(target, buffer_get_u32(buffer)); END_MACRO

#define buffer_put_PRIORITY(buffer,priority) \
  BEGIN_MACRO buffer_put_u32(buffer, GET_PRIORITY(priority)); END_MACRO

#else /* WITH_LONG_PRIORITY */

#define FMT_PRIORITY "%d"
typedef struct s_priority_t {
  hipsens_u8 value;
} priority_t;

#define buffer_get_PRIORITY(buffer,target) \
  BEGIN_MACRO SET_PRIORITY(target, buffer_get_u8(buffer)); END_MACRO

#define buffer_put_PRIORITY(buffer,priority) \
  BEGIN_MACRO buffer_put_u8(buffer, GET_PRIORITY(priority)); END_MACRO

#endif /* WITH_LONG_PRIORITY */

/*--------------------------------------------------
 * A structure with address and priority
 * (this forms the 'full priority')
 *--------------------------------------------------*/

typedef struct s_addr_priority_t {
  address_t address;
  priority_t priority;
} addr_priority_t;

/*--------------------------------------------------
 * Information w.r.t a neighbor for SERENA
 *--------------------------------------------------*/

typedef struct s_serena_neighbor_t {
  address_t address; /**< externally set */
  byte color;     /**< the color of the neighbor */
  priority_t priority; /**< coloring priority of the node w.r.t other nodes */
  /** MAX_PRIO1_SIZE most prioritary 1-hop neighbors */
  addr_priority_t max2_prio1[MAX_PRIO1_SIZE]; 
  /** MAX_PRIO2_SIZE most prioritary 2-hop neighbors */
  addr_priority_t max2_prio2[MAX_PRIO2_SIZE]; 

  byte has_prio:1;  /**< did it send already a Color message (w/ priority)? */
  byte has_prio1:1; /**< did it send a non-empty Max2Prio1? */
  byte has_prio2:1; /**< did it send a non-empty Max2Prio2? */

  /* when a tree is colored this information is used */
  /* this has been added */
  byte is_child:1; /**< externally set */
  byte is_parent:1; /**< externally set */
  byte has_sent_max_color:1; 
  byte child_max_color; /*< XXX */
  /* hipsens_u16 child_seq_num; **< XXX remove ? */
} serena_neighbor_t;


/*--------------------------------------------------
 * Configuration of the SERENA protocol
 *--------------------------------------------------*/

typedef enum {
  Priority_mode_tree = 0,
  Priority_mode_fixed = 1,
  Priority_mode_2hop = 2
#ifdef WITH_SIMUL
  , Priority_mode_3hop = 3
#endif
} priority_mode_t;

typedef struct s_serena_config_t {
  /* configuration */
  hipsens_time_t max_jitter_time;
  hipsens_time_t msg_color_interval;
  priority_mode_t priority_mode; 
  priority_t fixed_priority; /* only w/`priority_mode' == Priority_mode_fixed */
  hipsens_time_t recoloring_delay; /* XXX: should be in opera_config */
} serena_config_t;

/*--------------------------------------------------
 * The full state of an instance of the SERENA protocol
 *--------------------------------------------------*/

struct s_serena_state_t;
typedef void (callback_coloring_finished_t)(struct s_serena_state_t* state);

typedef struct s_serena_state_t {
  base_state_t* base; /**< the base state */
  eond_state_t* nd_state; /**< the state of the neighbor discovery XXX:remove*/
  serena_config_t* config;

  byte is_started:1; /**< is coloring started? */
  byte is_topology_set:1; /**< has the topology been set? */
  byte is_finished:1; /**< is coloring finished? */
  hipsens_u16 color_seq_num;
  hipsens_time_t next_msg_color_time; /**< time for next color message */

  byte color;    /**< the color of the node */
  priority_t priority; /**< the priority of the node */
  
  bitmap_t color_bitmap1;
  bitmap_t color_bitmap2;
  bitmap_t color_bitmap3;

  int nb_neighbor;
  serena_neighbor_t neighbor_table[MAX_NEIGHBOR];

  /* information about the tree */
  address_t root_address;
  hipsens_u16 tree_seq_num;

  /* dbg/simul extra */
  addr_priority_t max2_prio1[MAX_PRIO1_SIZE];
  addr_priority_t max2_prio2[MAX_PRIO2_SIZE];
#ifdef DBG_SERENA
  addr_priority_t dbg_max_prio3;
  addr_priority_t dbg_max_addr_priority;
  hipsens_time_t coloring_time;
#endif /* DBG_SERENA */
  byte last_nb_color; /* last_nb_color sent */

#ifdef IMPLICIT_COLORING_DETECTION
#define MAX_IMPLICIT_COLORED 10
  int last_implicit_colored_index;
  addr_priority_t last_implicit_colored[MAX_IMPLICIT_COLORED];
#endif /* IMPLICIT_COLORING_DETECTION */

#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
  /* XXX: is this no longer used?: */
  byte is_stopped;
  int last_round_used; /*the number of round, the node has last sent its color message*/
#endif
#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
  int nb_sent_msg;
  int first_empty_msg;
  int size_sent_msg;
#endif
  callback_coloring_finished_t* callback_coloring_finished;

  /* final color information passed as-is */
  byte final_nb_neighbor_color; /* if != 0 then the node is colored */
  byte final_neighbor_color_list[MAX_NEIGHBOR];
  byte final_node_color; 
} serena_state_t;

/*---------------------------------------------------------------------------*/

void serena_config_init_default(serena_config_t* config);

void new__serena_state_init(serena_state_t* state, base_state_t* base,
			    serena_config_t* config);
void new__serena_start(serena_state_t* state);

void serena_process_message(serena_state_t* state,
			    byte* packet, int max_packet_size);
int serena_generate_message(serena_state_t* state,
			    byte* packet, int max_packet_size);

void serena_get_next_wakeup_condition(serena_state_t* state,
				      hipsens_wakeup_condition_t* condition);
int serena_notify_wakeup(serena_state_t* state, 
			 void* packet, int max_packet_size);

int serena_find_neighbor_index(serena_state_t* state, address_t address); //added-ridha
void serena_remove_neighbor(serena_state_t* state, int neighbor_index); //added-ridha
int hipsens_seqnum_cmp(hipsens_u16 s1, hipsens_u16 s2);//added-ridha


int serena_get_nb_color(serena_state_t* state); //added-ridha

#ifdef WITH_PRINTF
void serena_dump(serena_state_t* state);
void serena_pydump(serena_state_t* state);
#endif /* WITH_PRINTF */

/* exported only for hipsens-opera-coloring.c: */
void bitmap_init(bitmap_t* bitmap);
void bitmap_set_bit(bitmap_t* bitmap, int pos, serena_state_t* state);

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_OSERENA_H */
