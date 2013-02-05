/*---------------------------------------------------------------------------
 *                OPERA - EOLSR Strategic Tree Construction
 *---------------------------------------------------------------------------
 * Authors: Ichrak Amdouni, Cedric Adjih
 * Copyright 2011 Inria.
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

#ifndef _HIPSENS_EOSTC_H
#define _HIPSENS_EOSTC_H

/*---------------------------------------------------------------------------*/

#define HIPSENS_MSG_STC 'S'
#define HIPSENS_MSG_TREE_STATUS 'T'

#ifndef MAX_STC_TREE
#define MAX_STC_TREE 10
#endif

#ifndef MAX_STC_SERENA_TREE
#define MAX_STC_SERENA_TREE 3
#endif

//#define EOSTC_MY_TREE 0

typedef struct s_eostc_config_t {
  hipsens_time_t max_jitter_time; /**< for both gen. and retransmit. */
  hipsens_time_t stc_interval; /**< STCMax = STCMin */
  hipsens_time_t tree_hold_time; 
  hipsens_time_t stability_time;  
} eostc_config_t;

typedef enum {
  EOSTC_None = 0,
  EOSTC_HasParent = 1,
  EOSTC_IsRoot = 2
} tree_status_t;

typedef enum {
  SerenaTree_None = 0
} serena_tree_status_t;

typedef enum {
  Child_None = 0,
  Child_Unstable = 1,
  Child_Stable = 2
} child_status_t;


typedef struct s_eostc_child_t {
  address_t address;            /**< C_child_addr */
  hipsens_u16 nb_descendant;    /**< C_descendants_number */
  child_status_t status;        /**< C_tree_status */
  hipsens_time_t validity_time; /**< C_validity_time */
} eostc_child_t;

struct s_eostc_tree_t;

typedef struct s_eostc_serena_tree_t {
  struct s_eostc_tree_t* tree;
  hipsens_u8 flags:7;
  hipsens_bool should_generate_tree_status:1; /**< set to 1 until ack. */
  hipsens_bool limit_tree_status:1; /**< only send one per seqnum */
  hipsens_bool is_subtree_stable:1; /**< S_stable */
  hipsens_bool is_neighborhood_stable:1; /* neighbors, parent and children */
  hipsens_bool has_sent_stable:1;
  hipsens_time_t stability_time; /**<  */
  hipsens_u16 tree_seq_num; /**< S_tree_seqnum */
  eostc_child_t child[MAX_NEIGHBOR]; /**< S_childrenset */
} eostc_serena_tree_t;

/* invariants: 
   - all the active tree have a valid parent_address 
*/
typedef struct s_eostc_tree_t {
  tree_status_t status;

  address_t root_address; /**< S_strategic_addr */
  hipsens_u16 stc_seq_num; /**< S_seqnum */
  address_t parent_address; /**< S_parent_addr */
  hipsens_u16 current_cost; /**< S_cost */
  
  hipsens_time_t validity_time; /**< S_validity_time */

  /* received information */
  hipsens_u8 received_vtime; /** for regeneration */
  hipsens_u8 ttl_if_generate; /**< 0 if no generation, else ttl */

  eostc_serena_tree_t* serena_info; /** NULL iff S_color == false */
} eostc_tree_t ;

#define IS_FOR_SERENA(tree) ((tree).serena_info != NULL)

#define STC_SERENA_TREE_MINE 0
#define STC_TREE_MINE MAX_STC_SERENA_TREE

typedef struct s_eostc_state_t {
  base_state_t* base;
  eostc_config_t* config;
  eond_state_t* eond_state;

  eostc_tree_t* my_tree; /**< points to one of among the following */
  eostc_tree_t tree[MAX_STC_TREE]; /* Strategic Tree Table */
  eostc_serena_tree_t serena_info[MAX_STC_SERENA_TREE];
  
  hipsens_time_t next_msg_stc_time; /**< time for next stc message */

#ifdef WITH_STAT
  /* statistics */
  hipsens_u32 stat_child_disappear;
  hipsens_u32 stat_parent_disappear;
#endif
} eostc_state_t;


typedef struct s_eostc_message_t {
  address_t    sender_address;
  hipsens_u16  stc_seq_num;
  address_t    tree_root_address;
  hipsens_u16  cost;
  address_t    parent_address;
  hipsens_u8   vtime;
  hipsens_u8   ttl; 
  hipsens_bool flag_colored:1;
  hipsens_u8   flags:7;
  hipsens_u16  tree_seq_num;
} eostc_message_t;

/* definintion of the bits in flags message */
#define EOSTC_FLAG_COLORED_BIT         7 /* specific handling */
#define EOSTC_FLAG_STABLE_BIT          6 /* specific handling */
#define EOSTC_FLAG_BECAME_UNSTABLE_BIT 5
//#define EOSTC_FLAG_STOP_SERENA_BIT     4

/* XXX: check */
//#define EOSTC_FLAG_COLORED_BIT            7
//#define EOSTC_FLAG_STABLE_BIT             6
//#define EOSTC_FLAG_BECAME_UNSTABLE_BIT    5
#define EOSTC_FLAG_INCONSISTENT_STABILITY 4

#define EOSTC_FLAG_TREE_CHANGE            2
#define EOSTC_FLAG_COLOR_CONFLICT         1
#define EOSTC_FLAG_NEW_NEIGHBOR           0
#define EOSTC_NO_FLAG                   (-1)

void eostc_state_init(eostc_state_t* state, eond_state_t* nd_state,
		      eostc_config_t* config);

/**
 * Initialize an OPERA configuration with default values
 */
void eostc_config_init_default(eostc_config_t* config);

hipsens_bool eostc_start(eostc_state_t* state, hipsens_bool is_serena_tree);

void eostc_state_reset(eostc_state_t* state);

void eostc_close(eostc_state_t* state);

int eostc_process_stc_message(eostc_state_t* state, 
			      byte* packet, int packet_size);

int  eostc_generate_stc_message(eostc_state_t* state, 
				byte* packet, int max_packet_size);

void eostc_get_next_wakeup_condition(eostc_state_t* state,
				     hipsens_wakeup_condition_t* condition);
int eostc_notify_wakeup(eostc_state_t* state, 
			byte* packet, int max_packet_size);

#ifdef WITH_PRINTF
void eostc_pywrite(outstream_t out, eostc_state_t* state);
#endif

/*---------------------------------------------------------------------------*/

/* External API: this function must be defined by the caller of module */
void hipsens_eostc_event_my_tree_flags_change(eostc_state_t* state, 
					      eostc_tree_t* tree,
					      hipsens_u8 new_flags);

/* External API: this function must be defined by the caller of module */
void hipsens_eostc_event_tree_stability(eostc_state_t* state, 
					eostc_tree_t* tree);

hipsens_bool hipsens_is_tree_being_colored(eostc_state_t* state, 
					   eostc_tree_t* tree);

void hipsens_notify_neighbor_disappeared(base_state_t* base, address_t address);

void hipsens_notify_tree_change(base_state_t* base, address_t address,
				eostc_tree_t* tree,
				hipsens_tristate is_parent, 
				hipsens_tristate is_child);
eostc_tree_t* eond_find_tree_by_address
(eostc_state_t* state, address_t address, hipsens_bool is_for_serena);
int eostc_count_descendant(eostc_state_t* state, eostc_tree_t* tree);//added-ridha
int eostc_process_tree_status_message(eostc_state_t* state, 
				      byte* packet, int packet_size);//added-ridha

int eostc_generate_tree_status_message(eostc_state_t* state,
				       eostc_tree_t* tree,
				       byte* packet, int max_packet_size); //added-ridha

void clear_serena_tree(eostc_serena_tree_t* serena_tree);

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_EOSTC_H */
