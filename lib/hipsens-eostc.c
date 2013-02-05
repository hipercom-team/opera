/*---------------------------------------------------------------------------
 *                OPERA - EOLSR Strategic Tree Construction
 *---------------------------------------------------------------------------
 * Author: Cedric Adjih
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

#include "hipsens-all.h"

/*---------------------------------------------------------------------------*/

/*
  STC Format

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Message Type  |  Message Size |         Sender Address        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |   Message Sequence Number     |   Strategic Node Address      |  
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |   Cost                        |   Parent Address              |  
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |   Vtime       |  Time To Live | Flags     |C| Tree Sequence ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |... Number     | XXX: changed     
      +-+-+-+-+-+-+-+-+


 Tree Status Format

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Message Type  |  Message Size |    Sender Address             |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |     Message Sequence Number   |    Strategic Node Address     |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |     Tree Sequence Number      |    Descendant Number          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |    Flags    |0| XXX: changed
      +-+-+-+-+-+-+-+-+

*/

/* XXX: crashes when STC_TREE = STC_SERENA_TREE = 0  and started */

/* XXX: check stability -> unstability -> stability */


#define DEFAULT_MSG_STC_INTERVAL_SEC 5 /* seconds */

/** HOLD_TIME IS SET TO (3+1/2) times STC_INTERVAL */
#define DEFAULT_TREE_HOLD_TIME_SEC  \
  ((DEFAULT_MSG_STC_INTERVAL_SEC*7)/2) /* seconds */

#define DEFAULT_STABILITY_DELAY_SEC 20 /* seconds */


#ifndef DEFAULT_STC_TTL
#define DEFAULT_STC_TTL 200
#endif

#define STNOTICE(...) do { } while(0)



/*---------------------------------------------------------------------------*/

#if STC_TREE_MINE > MAX_STC_TREE
#error "Bad configuration, MAX_STC_TREE < MAX_STC_SERENA_TREE"
#endif

/*---------------------------------------------------------------------------*/

// XXX!! remove when beacons are used
void opera_ensure_serena_stopped(eostc_state_t* state);


void eostc_config_init_default(eostc_config_t* config)
{
#ifdef WITH_FRAME_TIME
  /* default configuration */
  config->max_jitter_time = 0;
  config->stc_interval = SEC_TO_FRAME_TIME(DEFAULT_MSG_STC_INTERVAL_SEC);
  config->tree_hold_time = SEC_TO_FRAME_TIME(DEFAULT_TREE_HOLD_TIME_SEC);
  config->stability_time = SEC_TO_FRAME_TIME(DEFAULT_STABILITY_DELAY_SEC);
#else /* WITH_FRAME_TIME */
  /* default configuration */
  config->max_jitter_time = MILLISEC_TO_HIPSENS_TIME(DEFAULT_JITTER_MILLISEC);
  config->stc_interval = SEC_TO_HIPSENS_TIME(DEFAULT_MSG_STC_INTERVAL_SEC);
  config->tree_hold_time = SEC_TO_HIPSENS_TIME(DEFAULT_TREE_HOLD_TIME_SEC);
  config->stability_time = SEC_TO_HIPSENS_TIME(DEFAULT_STABILITY_DELAY_SEC);
#endif /* WITH_FRAME_TIME */
}


/*
  This is called when:
  - a child disappears (as a neighbor)

  - a new tree is detected
  - a child has expired (STC)
  - a new child has been detected
  - the parent has changed  
 */
static void eostc_event_topology_change(eostc_state_t* state, 
					eostc_tree_t* tree,
					int flag_bit_to_set)
{
  if (IS_FOR_SERENA(*tree)) { /* XXX: if root ?? */
    eostc_serena_tree_t* serena_tree = tree->serena_info;
    if (serena_tree->is_subtree_stable) {
      if (flag_bit_to_set >= 0) {
	serena_tree->should_generate_tree_status = HIPSENS_TRUE;
	serena_tree->flags |= (1<<flag_bit_to_set);
      }
    }
    serena_tree->is_subtree_stable = HIPSENS_FALSE;
    serena_tree->is_neighborhood_stable = HIPSENS_FALSE;
    serena_tree->stability_time = HIPSENS_TIME_ADD
      (state->base->current_time, state->config->stability_time);
  }
}

static void eostc_handle_neighbor_change
(void* data, address_t neighbor_address,
 eond_neighbor_state_t old_state, eond_neighbor_state_t new_state,
 int neighbor_index)
{
  eostc_state_t* state = (eostc_state_t*)data;
  
  if (old_state == EOND_Sym) {
    ASSERT( new_state != EOND_Sym );
    /* symetric neighbor has become asym or has disappeared */
    
    hipsens_bool is_relative = HIPSENS_FALSE; /* relative = parent or child */
    /* XXX:note: for now is_relative is not used */

    int i,j;

    for (i=0; i<MAX_STC_TREE; i++) {
      eostc_tree_t* tree = &state->tree[i];
      if (tree->status == EOSTC_HasParent) {
	if (hipsens_is_tree_being_colored(state, tree))
	  hipsens_notify_neighbor_disappeared(state->base, neighbor_address);
	eostc_event_topology_change(state, &state->tree[i], EOSTC_NO_FLAG);
      }
    }

    /* remove all trees where the node is parent */
    for (i=0; i<MAX_STC_TREE; i++) {
      eostc_tree_t* tree = &state->tree[i];
      if (tree->status == EOSTC_HasParent 
	  && hipsens_address_equal(tree->parent_address, neighbor_address)) {
	tree->status = EOSTC_None;
	IFSTAT( state->stat_parent_disappear++ );
	/* we cannot send a Tree Status, because we have removed the tree
	   XXX: possibly do additional processing for SERENA */
	is_relative = HIPSENS_TRUE;
      }
    }

    /* remove it also as child of all trees */
    for (i=0; i<MAX_STC_TREE; i++)
      if (state->tree[i].status != EOSTC_None 
	  && state->tree[i].serena_info != NULL) {
	eostc_serena_tree_t* serena_tree = state->tree[i].serena_info;
	for (j=0; j<MAX_NEIGHBOR; j++) {
	  eostc_child_t* child = &serena_tree->child[j];
	  if (child->status != Child_None
	      && hipsens_address_equal(child->address, neighbor_address)) {
	    child->status = Child_None;
	    IFSTAT( state->stat_child_disappear++ );
	    is_relative = HIPSENS_TRUE;
	  }
	}
      }
  } else if (new_state == EOND_Sym) {
    int i;
    for (i=0; i<MAX_STC_TREE; i++) {
      eostc_tree_t* tree = &state->tree[i];
      if (tree->status == EOSTC_HasParent) {
	if (hipsens_is_tree_being_colored(state, tree))
	  hipsens_notify_neighbor_disappeared(state->base, neighbor_address);
	eostc_event_topology_change(state, &state->tree[i],
				    EOSTC_FLAG_NEW_NEIGHBOR);
      }
    }

  }
}

static void eostc_update_next_stc_time(eostc_state_t* state)
{
  state->next_msg_stc_time = base_state_time_after_delay_jitter
    (state->base, state->config->stc_interval, state->config->max_jitter_time);
}

static int buffer_put_stc_message(buffer_t* buffer, eostc_message_t* message)
{
  /* --- generate the header */
  buffer_put_u8(buffer, HIPSENS_MSG_STC);
  int msg_size_pos = buffer->pos;
  buffer_put_u8(buffer, 0); /* size, filed later */
  int msg_content_start_pos = buffer->pos;
  buffer_put_ADDRESS(buffer, message->sender_address); /* sender address */
  buffer_put_u16(buffer, message->stc_seq_num);  /* stc sequence number */
  buffer_put_ADDRESS(buffer, message->tree_root_address); /*strategic node */
  buffer_put_u16(buffer, message->cost); /* cost taking into account ourself */
  buffer_put_ADDRESS(buffer, message->parent_address); /* parent address */
  buffer_put_u8(buffer, message->vtime);
  buffer_put_u8(buffer, message->ttl);

  hipsens_u8 flag_field = ((message->flag_colored << EOSTC_FLAG_COLORED_BIT)
			   | message->flags);
  buffer_put_u8(buffer, flag_field);
  buffer_put_u16(buffer, message->tree_seq_num);

  /* --- update size field */
  if (buffer->status != HIPSENS_TRUE) {
    return -1;
  }

  int result = buffer->pos;
  ASSERT(result > 0);
  buffer->pos = msg_size_pos;
  buffer_put_u8(buffer, result - msg_content_start_pos);
  return result;
}

#define EOSTC_BAD_SIZE_PARSE (-1)
#define EOSTC_BAD_MSG_TYPE (-2)
#define EOSTC_BAD_SIZE_GENERATE (-3)

static int buffer_get_stc_message(buffer_t* buffer, eostc_message_t* message)
{
  /* --- parse the header */
  hipsens_u8 message_type = buffer_get_u8(buffer);
  hipsens_u8 message_size = buffer_get_u8(buffer);
  
  if (message_type != HIPSENS_MSG_STC)
    return EOSTC_BAD_MSG_TYPE;
  if (message_size > buffer->size - buffer->pos) 
    return EOSTC_BAD_SIZE_PARSE;

  int result = buffer->pos + message_size;
  buffer->size = result;

  buffer_get_ADDRESS(buffer, message->sender_address);
  message->stc_seq_num = buffer_get_u16(buffer);
  buffer_get_ADDRESS(buffer, message->tree_root_address);
  
  message->cost = buffer_get_u16(buffer);
  buffer_get_ADDRESS(buffer, message->parent_address);

  message->vtime = buffer_get_u8(buffer);
  message->ttl = buffer_get_u8(buffer);

  hipsens_u8 flag_field = buffer_get_u8(buffer);
  message->flag_colored = (flag_field >> EOSTC_FLAG_COLORED_BIT) & 1;
  message->flags = flag_field & ~(1 << EOSTC_FLAG_COLORED_BIT);
  
  message->tree_seq_num = buffer_get_u16(buffer);

  if (buffer->status != HIPSENS_TRUE)
    return EOSTC_BAD_SIZE_GENERATE;
  else return result;
}

/*
  Current allocation strategy:
  - colored trees and non-colored trees are strictly separated
    in state->tree
*/
static eostc_tree_t* eostc_find_free_tree(eostc_state_t* state,
					  hipsens_bool is_for_serena)
{
  int i;

  if (!is_for_serena) {
    for (i=0; i<MAX_STC_TREE; i++)
      if (i != STC_TREE_MINE && i != STC_SERENA_TREE_MINE
	  && state->tree[i].serena_info == NULL
	  && state->tree[i].status == EOSTC_None)
	return &state->tree[i];
//    return NULL; // XXX: we no longer use colored trees in this case

    //if (!is_for_serena && state->tree[STC_TREE_MINE].status == EOSTC_None)
    //return &state->tree[STC_TREE_MINE];
  }

  for (i=0; i<MAX_STC_TREE; i++)
    if (i != STC_TREE_MINE && i != STC_SERENA_TREE_MINE
	&& state->tree[i].serena_info != NULL 
	&& state->tree[i].status == EOSTC_None)
      return &state->tree[i];

  if (MAX_STC_SERENA_TREE > 0
      && state->tree[STC_SERENA_TREE_MINE].status == EOSTC_None)
    return &state->tree[STC_SERENA_TREE_MINE];

  return NULL;
}

/** return the tree with strategic node (root) with the given address,
    and the exact same value for `is_for_serena'
*/
eostc_tree_t* eond_find_tree_by_address
(eostc_state_t* state, address_t address, hipsens_bool is_for_serena)
{
  int i;
  for (i=0;i<MAX_STC_TREE; i++) {
    eostc_tree_t* tree = &state->tree[i];
    if (tree->status != EOSTC_None) {
      if (hipsens_address_equal(tree->root_address, address)
	  && IS_FOR_SERENA(*tree) == is_for_serena)
	return tree;
    } 
  }
  return NULL;
}

void clear_serena_tree(eostc_serena_tree_t* serena_tree)
{
  int i;
  for (i=0; i<MAX_NEIGHBOR; i++)
    serena_tree->child[i].status = Child_None;
  serena_tree->flags = 0;
  serena_tree->should_generate_tree_status = HIPSENS_FALSE;
  serena_tree->limit_tree_status = HIPSENS_FALSE;
  serena_tree->is_subtree_stable = HIPSENS_FALSE;
  serena_tree->is_neighborhood_stable = HIPSENS_FALSE;
  serena_tree->has_sent_stable = HIPSENS_FALSE;

  // XXX: check this new addition
  // This should have been put here:
  //  serena_tree->stability_time = HIPSENS_TIME_ADD
  //(state->base->current_time, state->config->stability_time);
}

void eostc_state_reset(eostc_state_t* state)
{
  state->next_msg_stc_time = undefined_time;

  /* empty tables */
  int i;
  for (i = 0; i<MAX_STC_TREE; i++) {
    eostc_tree_t* tree = &state->tree[i];
    tree->status = EOSTC_None;
    tree->serena_info = NULL;
  }
  for (i = 0; i<MAX_STC_SERENA_TREE; i++) {
    if (i < MAX_STC_TREE) {
      state->serena_info[i].tree = &state->tree[i];
      state->tree[i].serena_info = &state->serena_info[i];
      /* XXX: redundant */
      //clear_serena_tree(&state->serena_info[i]);
    } else state->serena_info[i].tree = NULL; /* unused */
  }
  state->my_tree  = NULL;

#ifdef WITH_STAT
  state->stat_child_disappear = 0;
  state->stat_parent_disappear = 0;
#endif
}

void eostc_close(eostc_state_t* state)
{
}

void eostc_state_init(eostc_state_t* state, eond_state_t* eond_state,
		      eostc_config_t* config)
{
  state->base = eond_state->base;
  STLOGA(DBGstc, "eostc-init\n");
  state->eond_state = eond_state;
  state->config = config;

  if (state->eond_state->observer_func != NULL) {
    STWARN("EOND had already an observer_func\n");
  }
  state->eond_state->observer_data = (void*) state;
  state->eond_state->observer_func = eostc_handle_neighbor_change;
  eostc_state_reset(state);
}

/**
 * Can fail iff (is_for_serena is true and MAX_STC_SERENA_TREE == 0)
 */
hipsens_bool eostc_start(eostc_state_t* state, hipsens_bool is_for_serena)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);

  if (is_for_serena && MAX_STC_SERENA_TREE == 0)
    return HIPSENS_FALSE;
  if (state->my_tree == NULL) {
    if (is_for_serena)
      state->my_tree = &state->tree[STC_SERENA_TREE_MINE];
    else state->my_tree = &state->tree[STC_TREE_MINE];
  }

  eostc_tree_t* tree = state->my_tree;

  tree->status = EOSTC_IsRoot;  
  hipsens_address_copy(tree->root_address, my_address);
  ASSERT( IS_FOR_SERENA(*tree) == is_for_serena );
  tree->stc_seq_num = 0;
  /*XXX:changed hipsens_address_copy(tree->parent_address, undefined_address);*/
  hipsens_address_copy(tree->parent_address, my_address);
  tree->current_cost = 0;
  tree->validity_time = undefined_time;
  tree->ttl_if_generate = 0; /* no message repetition */
  if (tree->serena_info != NULL) {
    clear_serena_tree(tree->serena_info);
    tree->serena_info->tree_seq_num = 0;
    tree->serena_info->stability_time = HIPSENS_TIME_ADD
      (state->base->current_time, state->config->stability_time);
  }
  eostc_update_next_stc_time(state);
  return HIPSENS_TRUE;
}

static hipsens_u16 eostc_get_forwarding_cost(eostc_state_t* state)
{
#ifdef WITH_ENERGY
  hipsens_u16 result = eond_estimate_reception_energy_cost(state->eond_state);
  result += state->base->cfg_energy_coef
    [ENERGY_TRANSMISSION][state->base->energy_class];
#else
  hipsens_u16 result = 1;
#endif /* WITH_ENERGY */
  return result; 
}

#define SEQNUM_U16_MAX_DIV_2 ((hipsens_u16)(1u<<15u))
//#define SEQNUM_U16_IS_GREATER(s1,s2) ((s1>s2) && (s1-s2) <= SEQNUM_U16_MAX/2)

/* returns as for memcmp */
int hipsens_seqnum_cmp(hipsens_u16 s1, hipsens_u16 s2)
{
  /* see RFC 3626 sect. 19 */
  if (s1 == s2) return 0; 
  if ( ((s1 > s2) && ( ((hipsens_u16)(s1-s2)) <= SEQNUM_U16_MAX_DIV_2))
       || ((s2 > s1) && (((hipsens_u16)(s2-s1)) > SEQNUM_U16_MAX_DIV_2)))
    return 1;
  else return -1;
}


/*
  This function updates a tree when a EOSTC message has been received 
  (from a parent) and the tree is already known.
 */
static void eostc_refresh_tree(eostc_state_t* state,
			       eostc_tree_t* tree,
			       eostc_message_t* message,
			       hipsens_bool reset_serena_children,
			       hipsens_bool regenerate_message)
{
  ASSERT( IS_FOR_SERENA(*tree) == message->flag_colored );

  HIPSENS_GET_MY_ADDRESS(state->base, my_address);

  hipsens_u16 old_stc_seq_num = tree->stc_seq_num;

  tree->validity_time = HIPSENS_TIME_ADD
    (state->base->current_time, vtime_to_hipsens_time(message->vtime));
  /* XXX: no check of overflow  - XXX! change of spec.: XXX:reverted*/
  tree->received_vtime = message->vtime;
  tree->current_cost = message->cost; //XXX:+ eostc_get_forwarding_cost(state);
  tree->stc_seq_num = message->stc_seq_num;

  eostc_serena_tree_t* serena_tree = tree->serena_info;
  if (serena_tree != NULL) {
    if (reset_serena_children || old_stc_seq_num != tree->stc_seq_num)
      serena_tree->limit_tree_status = HIPSENS_FALSE;
    if (reset_serena_children || hipsens_seqnum_cmp
	(serena_tree->tree_seq_num, message->tree_seq_num) < 0) {/*XXX <= to <*/
#ifdef WITH_OPERA_SYSTEM_INFO
          // XXX:move
        state->base->sys_info = 0;
        state->base->sys_info_color = 0;
#endif /* WITH_OPERA_SYSTEM_INFO */
      clear_serena_tree(serena_tree);
      // XXX: double check these additions
      opera_ensure_serena_stopped(state);
      tree->serena_info->stability_time = HIPSENS_TIME_ADD
	(state->base->current_time, state->config->stability_time);

    } else {
      if (serena_tree->tree_seq_num == message->tree_seq_num) {
	if ( (message->flags & (1<<EOSTC_FLAG_STABLE_BIT))
	     && !(serena_tree->flags & (1<<EOSTC_FLAG_STABLE_BIT))) {
	  /* root is stable but we are not stable */
	  //serena_tree->should_generate_tree_status = HIPSENS_TRUE;
	  serena_tree->flags |= (1<<EOSTC_FLAG_INCONSISTENT_STABILITY);
	}
	serena_tree->flags |= message->flags & ~(1<<EOSTC_FLAG_STABLE_BIT);
	serena_tree->should_generate_tree_status 
	  = (serena_tree->flags & ~message->flags) != 0;
      }
    }
    serena_tree->tree_seq_num = message->tree_seq_num;
  }

  tree->ttl_if_generate = 0;
  if (regenerate_message) {
    if (message->ttl == 0 || message->ttl == 1) {
      STWARN("message with ttl=0.\n");
    } else {
      tree->ttl_if_generate = message->ttl - 1; /* will regenerate a STC */
    }
  }
}

static void eostc_new_tree_discovered(eostc_state_t* state,
				      eostc_message_t* message)
{
  eostc_tree_t* tree = eostc_find_free_tree(state, message->flag_colored);
  if (tree == NULL) {
    STWARN("tree table full\n");
    STLOG(DBGstc," tree-table-full\n");
    return;
  }
  tree->status = EOSTC_HasParent; /* XXX! status missing in spec. */
  hipsens_address_copy(tree->root_address, message->tree_root_address);
  hipsens_address_copy(tree->parent_address, message->sender_address);
  ASSERT( IS_FOR_SERENA(*tree) == message->flag_colored );

  eostc_refresh_tree(state, tree, message, HIPSENS_TRUE, HIPSENS_TRUE);
  eostc_event_topology_change(state, tree, EOSTC_FLAG_TREE_CHANGE /*not used*/);
  hipsens_notify_tree_change(state->base, tree->parent_address, tree,
			     HIPSENS_TRUE, HIPSENS_UNDEF);
}

static eostc_child_t* get_serena_tree_child(eostc_serena_tree_t* serena_tree,
					    address_t child_address,
					    hipsens_time_t current_time)
{
  int i;
  int free_index = -1;
  for (i=0; i<MAX_NEIGHBOR;i++) {
    eostc_child_t* child = &serena_tree->child[i];
    if (child->status == Child_None) {
      if (free_index < 0)
	free_index = i;
    } else {
      if (hipsens_address_equal(child_address, child->address))
	return child;
    }
  }

  if (free_index >= 0) {
    eostc_child_t* child = &serena_tree->child[free_index];
    return child;
  } else return NULL; /* table is full */
}

/* - this is called when:
     . a STC message is emitted (repeated)
     . a child has just become stable (from a Tree Status)
   - this is the only place where child expiration is checked 
*/
static void update_tree_stability_status(eostc_state_t* state,
					 eostc_tree_t* tree)
{
  ASSERT( IS_FOR_SERENA(*tree) );
  eostc_serena_tree_t* serena_tree = tree->serena_info;
  if (serena_tree->is_subtree_stable)
    return; /* state was already updated */
  if (!serena_tree->is_neighborhood_stable) {
    if (HIPSENS_TIME_COMPARE_LARGE_UNDEF
	(serena_tree->stability_time,>,state->base->current_time))
      return;
    serena_tree->is_neighborhood_stable = HIPSENS_TRUE;
  }
  hipsens_bool stable = HIPSENS_TRUE;
  int i;

  /* check expiration + stability of the children */
  for (i=0;i<MAX_NEIGHBOR;i++) {
    eostc_child_t* child = &serena_tree->child[i];
    
    if (child->status != Child_None) {
      if (HIPSENS_TIME_COMPARE_NO_UNDEF
	  (child->validity_time,<,state->base->current_time)) {
	child->status = Child_None;
	IFSTAT( state->stat_child_disappear++ );
	eostc_event_topology_change(state, &state->tree[i], EOSTC_NO_FLAG);
	hipsens_notify_tree_change(state->base, tree->parent_address, tree,
				   HIPSENS_FALSE, HIPSENS_FALSE);
	stable = HIPSENS_FALSE;
      } else if (child->status != Child_Stable) 
	stable = HIPSENS_FALSE;
    }
  }

  if (stable) {
    serena_tree->is_subtree_stable = stable;
    serena_tree->flags |= (1<<EOSTC_FLAG_STABLE_BIT);
    if (tree->status != EOSTC_IsRoot) {
      serena_tree->has_sent_stable = HIPSENS_TRUE;
      serena_tree->should_generate_tree_status = HIPSENS_TRUE;
      hipsens_eostc_event_tree_stability(state, tree);
    } else {
      hipsens_eostc_event_my_tree_flags_change
	(state, tree, (1<<EOSTC_FLAG_STABLE_BIT) ); /* XXX: more general */
    }
  }
}

#define EOSTC_MASK_REMOVE_STABLE_BIT (~(hipsens_u16)(1u<<EOSTC_FLAG_STABLE_BIT))


static eostc_child_t* eostc_find_child_in_tree(eostc_serena_tree_t* serena_tree,
					       address_t sender_address)
{
  int i;
  eostc_child_t* child = NULL;
  for (i=0; i<MAX_NEIGHBOR; i++)
    if (serena_tree->child[i].status != Child_None
	&& hipsens_address_equal(serena_tree->child[i].address, sender_address))
      child = &serena_tree->child[i];
  return child;
}

int eostc_process_tree_status_message(eostc_state_t* state, 
				      byte* packet, int packet_size)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  buffer_t buffer;
  buffer_init(&buffer, packet, packet_size);

  /* --- parse the header */
  hipsens_u8 message_type = buffer_get_u8(&buffer);
  hipsens_u8 message_size = buffer_get_u8(&buffer);
  
  if (message_type != HIPSENS_MSG_TREE_STATUS)
    return EOSTC_BAD_MSG_TYPE;
  if (message_size > buffer.size - buffer.pos) 
    return EOSTC_BAD_SIZE_PARSE;

  int result = buffer.pos + message_size;
  buffer.size = result;
  
  address_t sender_address;
  buffer_get_ADDRESS(&buffer, sender_address); /* sender address */

  hipsens_u16 stc_seq_num =  buffer_get_u16(&buffer); /* stc sequence number */
  UNUSED(stc_seq_num);
  address_t root_address;
  buffer_get_ADDRESS(&buffer, root_address); /*strategic node */

  hipsens_u16 tree_seq_num = buffer_get_u16(&buffer); /* tree seqnum */
  hipsens_u16 nb_descendant = buffer_get_u16(&buffer); /* number of desc. */
  hipsens_u8 flags = buffer_get_u8(&buffer);

  /* --- update size field */
  if (buffer.status != HIPSENS_TRUE) {
    return EOSTC_BAD_SIZE_GENERATE;
  }

  /* --- accept message only from symmetric neighbors */
  eond_neighbor_t* neighbor = eond_find_neighbor_by_address
    (state->eond_state, sender_address);
  if (neighbor == NULL || neighbor->state != EOND_Sym) {
    STNOTICE("tree status for unknown neighbor.\n");
    return result; /* not from a symmetric neighbor: ignore message */
  }

  /* --- accept message only for known trees */
  eostc_tree_t* tree = eond_find_tree_by_address(state, root_address, 
						 HIPSENS_TRUE);
  if (tree == NULL) {
    STNOTICE("tree status for unknown tree.\n");
    return result;
  }
  ASSERT( IS_FOR_SERENA(*tree) );
  eostc_serena_tree_t* serena_tree = tree->serena_info;

  /* ================== XXX: change of spec in the following: */

  /* --- accept messages only for known versions of the stc seq. num */
  int seqnum_cmp = hipsens_seqnum_cmp(tree_seq_num, serena_tree->tree_seq_num);
  if (seqnum_cmp < 0) 
    return result; /* old tree do nothing */
  if (seqnum_cmp > 0) {
    /* new tree, not expected to occur in normal processing */
    if (tree->status != EOSTC_IsRoot) {
      ASSERT( !hipsens_address_equal(my_address, root_address) );
      clear_serena_tree(serena_tree);
      // XXX: double check these additions
      opera_ensure_serena_stopped(state);
      tree->serena_info->stability_time = HIPSENS_TIME_ADD
	(state->base->current_time, state->config->stability_time);

      serena_tree->flags = flags & EOSTC_MASK_REMOVE_STABLE_BIT;
      return result;
    } else { /* this case should never occur */ }
  }

  /* here we know the tree_seq_num is the same - XXX stc_seq_num is ignored */

  /* --- update the flags */
  hipsens_u8 old_flags = serena_tree->flags;
  serena_tree->flags |= flags & EOSTC_MASK_REMOVE_STABLE_BIT;

  /* - if from the parent, consider as an acknowledgement of some flags */
  if (tree->status != EOSTC_IsRoot /* <- just to be safe */
      && hipsens_address_equal(tree->parent_address, sender_address)) {
    if ((serena_tree->flags & ~flags) == 0)
      serena_tree->should_generate_tree_status = HIPSENS_FALSE;
    /* XXX:note: this works even if we change parent, because
     'should_send_tree_status' is reset upon STC receiving */
    return result; /* no more processing to do */
  }

  eostc_child_t* child = eostc_find_child_in_tree(serena_tree, sender_address);

  if (child != NULL) {
    /* - child found, update it */
    child->nb_descendant = nb_descendant;
    /* XXX:note: we don't update child->validity_time */
    if ( (flags&(1<<EOSTC_FLAG_STABLE_BIT)) && child->status != Child_Stable) {
      child->status = Child_Stable;
      /* sender became stable, attempt to update stability */
      update_tree_stability_status(state, tree); /* updates flags when stable */
    }
  }

  /* --- if flags have changed, Tree Status should be generated (until ack.) */
  if ( (serena_tree->flags & ~old_flags) != 0 ) {
    //if (!hipsens_address_equal(tree->parent_address, my_address))
    if (tree->status != EOSTC_IsRoot)
      serena_tree->should_generate_tree_status = HIPSENS_TRUE;
#if 0
    // XXX:remove: this is now done inside update_tree_stability_status(...):
    else hipsens_eostc_event_my_tree_flags_change
	   (state, tree, (serena_tree->flags & ~old_flags));
#endif
  } 
  
  return result;
}


static void process_stc_message_from_child(eostc_state_t* state,
					   eostc_message_t* message,
					   eostc_tree_t* tree,
					   hipsens_s8 seqnum_cmp)
{
  /* message coming from a child */
  if (tree->serena_info != NULL) {
    if (seqnum_cmp <= 0)  {
      /* ensure the child is in the children list */
      eostc_child_t* child = get_serena_tree_child
	(tree->serena_info, message->sender_address, state->base->current_time);
      if (child != NULL) {
	if (child->status == Child_None) {
	  /* new children */
	  hipsens_address_copy(child->address, message->sender_address);
	  child->status = Child_Unstable;
	  child->nb_descendant = 0;
	  eostc_event_topology_change(state, tree, EOSTC_FLAG_TREE_CHANGE);
	  /* XXX: test that parent and child are different ?! */
	  hipsens_notify_tree_change(state->base, child->address, tree,
				     HIPSENS_UNDEF, HIPSENS_TRUE);
	}
	child->validity_time = HIPSENS_TIME_ADD
	  (state->base->current_time,vtime_to_hipsens_time(message->vtime));
      } else {
	STWARN("children table full.\n");
      }
    } else {
      /* this should not happen in normal functionning of the protocol */
      STWARN("children sent STC with higher seq-num.\n");
    }
  }
}

int eostc_process_stc_message(eostc_state_t* state, 
			      byte* packet, int packet_size)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  buffer_t buffer;
  buffer_init(&buffer, packet, packet_size);
  
  eostc_message_t message;
  int result = buffer_get_stc_message(&buffer, &message);

  if (result == EOSTC_BAD_MSG_TYPE) {
    STWARN("bad message type, stc message expected");
    return -1;
  } else if (result == EOSTC_BAD_SIZE_GENERATE) {
    STWARN("bad message size");
    return -1;
  } else if (result < 0) {
    STWARN("parse error in stc message content\n");
    return -1;
  }

  if (hipsens_address_equal(message.sender_address, my_address)) {
    STLOG(DBGstc, "- ignoring packet from myself\n");
    return result;
  }


  eond_neighbor_t* neighbor = eond_find_neighbor_by_address
    (state->eond_state, message.sender_address);
  if (neighbor == NULL || neighbor->state != EOND_Sym)
    return result; /* not from a symmetric neighbor: ignore message */

  eostc_tree_t* tree = eond_find_tree_by_address
    (state, message.tree_root_address, message.flag_colored);
  /* note: 
     - the all trees (returned by eond_find_tree_by_address), have parents
     which are still considered as symmetric by EOND - because when
     the status is changed (expiration), the STC tree table is cleaned
  */


  /* XXX change in specification, added the following processing:
      tree for which this node is (supposed to be) root*/
  if (hipsens_address_equal(message.tree_root_address, my_address)) {
    if (hipsens_address_equal(message.parent_address, my_address) 
	&& tree != NULL) {
      int seqnum_cmp = hipsens_seqnum_cmp(message.stc_seq_num, 
					  tree->stc_seq_num);
      process_stc_message_from_child(state, &message, tree, seqnum_cmp);
    }
    return result;
  }

  
  if (tree == NULL) {
    /* new tree discovered */
    eostc_new_tree_discovered(state, &message); // *
    return result;

  } else {
    /* existing tree */
    int seqnum_cmp = hipsens_seqnum_cmp(message.stc_seq_num, tree->stc_seq_num);

    /* XXX: note the implicit expiration of trees */
    if (HIPSENS_TIME_COMPARE_NO_UNDEF
	(state->base->current_time,<=,tree->validity_time)) {
      /* the parent of tree is still symmetric in EOND, see note above */

      if (hipsens_address_equal(tree->parent_address, message.sender_address)) {
	/* message coming from parent */
	
	if (seqnum_cmp >= 0) {
	  /* update the tree */
	  eostc_refresh_tree(state, tree,&message, HIPSENS_FALSE, HIPSENS_TRUE);
	} else {
	  /* ignore message */
	  STWARN("Parent sending lower seq-num\n");
	}
      } else if (hipsens_address_equal(message.parent_address, my_address)) {

	process_stc_message_from_child(state, &message, tree, seqnum_cmp);

      } else {
	/* message coming neither from parent nor from child */

	if (seqnum_cmp >= 0 && IS_FOR_SERENA(*tree)) {
	  /* check if it is a former child */
	  eostc_serena_tree_t* serena_tree = tree->serena_info;
	  ASSERT( tree->serena_info != NULL );
	  eostc_child_t* child = eostc_find_child_in_tree
	    (serena_tree, message.sender_address);
	  if (child != NULL) {
#warning "[CA] not updating stability status/timers on detection of child parent change"
	    child->status = Child_None; /* remove the child */
	    STWARN("XXX: child has selected a new parent ("FMT_HST")\n",
		   state->base->current_time);
	  }
	}

	if (message.cost < tree->current_cost && seqnum_cmp >= 0) {
	  /* parent change */
	  eostc_event_topology_change(state, tree, EOSTC_FLAG_TREE_CHANGE);
	  hipsens_address_copy(tree->parent_address, message.sender_address);
	  eostc_refresh_tree(state, tree, &message, HIPSENS_TRUE, HIPSENS_TRUE);
	  hipsens_notify_tree_change(state->base, tree->parent_address, tree,
				     HIPSENS_TRUE, HIPSENS_UNDEF);
	}
      }

    } else {
      /* the parent is no longer valid */
      /* XXX:note: maybe the parent has expired but we receive messages again */
      eostc_event_topology_change(state, tree, EOSTC_NO_FLAG);
      int seqcmp = hipsens_seqnum_cmp(message.stc_seq_num, tree->stc_seq_num);
      if (seqcmp >= 0) {
	/* greater message seq num: if it does not come from a child,
	   select the sender as parent */
	if (!hipsens_address_equal(message.parent_address, my_address)) {
	  hipsens_address_copy(tree->parent_address, message.sender_address);
	  eostc_refresh_tree(state, tree, &message, 
			     HIPSENS_TRUE, HIPSENS_UNDEF);
	} else {
	  if (seqcmp < 0) {
	    STWARN("children sent STC with higher seq-num.\n");
	  }
	}
      } else {
	/* ignore message */
      }
    }
    
  }
  
  return result;
}  


static int eostc_repeat_stc_message(eostc_state_t* state,
				    eostc_tree_t* tree,
				    byte* packet, int max_packet_size)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  STLOGA(DBGstc, "repeat-stc\n");
  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);
  hipsens_u16 cost = tree->current_cost + eostc_get_forwarding_cost(state);

  eostc_message_t message;
  hipsens_address_copy(message.sender_address, my_address);
  message.stc_seq_num = tree->stc_seq_num;

  hipsens_address_copy(message.tree_root_address, tree->root_address);
  message.cost = cost;
  hipsens_address_copy(message.parent_address, tree->parent_address);
  message.vtime = tree->received_vtime;
  message.ttl = tree->ttl_if_generate;
  if (tree->serena_info != NULL)
    message.flag_colored = 1;
  else message.flag_colored = 0;

  if (!IS_FOR_SERENA(*tree)) {
    message.tree_seq_num = 0; 
    message.flags = 0;
  } else {
    message.tree_seq_num = tree->serena_info->tree_seq_num;
    update_tree_stability_status(state, tree); // XXX: check here?
    message.flags = tree->serena_info->flags;
  } 

  int result =  buffer_put_stc_message(&buffer, &message);
  tree->ttl_if_generate = 0; /* default: don't re-generate STC */
  
  if (result < 0) {
    WARN(state->base, "packet buffer too small\n");
    return -1; /* buffer overflow */
  }

  return result;
}

static eostc_tree_t* eostc_get_pending_stc_tree(eostc_state_t* state)
{
  int i=0;
  for (i=0; i<MAX_STC_TREE; i++) 
    if (state->tree[i].status != EOSTC_None 
	&& state->tree[i].ttl_if_generate > 0)
      return &state->tree[i];
  return NULL;
}

static eostc_tree_t* eostc_get_pending_tree_status_tree(eostc_state_t* state)
{
  int i=0;
  for (i=0; i<MAX_STC_TREE; i++) 
    if (state->tree[i].status != EOSTC_None 
	&& IS_FOR_SERENA(state->tree[i]) 
	&& state->tree[i].serena_info->should_generate_tree_status
	&& !state->tree[i].serena_info->limit_tree_status)
      return &state->tree[i];
  return NULL;
}

int  eostc_generate_stc_message(eostc_state_t* state,
				byte* packet, int max_packet_size)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  STLOGA(DBGstc || DBGmsg, "eostc-generate-stc\n");
  eostc_update_next_stc_time(state);

  eostc_tree_t* tree = state->my_tree; //&state->tree[STC_TREE_MINE];
  if (tree == NULL) {
    STFATAL("generate-stc-message but tree is NULL");
    // XXX: don't say we want to generate a message
    return -1;
  }

  if (tree->status != EOSTC_IsRoot) {
    STWARN("generate stc, but tree is in not in state 'IsRoot': %d\n",
	   tree->status);
    return -1;
  }

  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);
  hipsens_u16 cost = 0;

  eostc_message_t message;
  hipsens_address_copy(message.sender_address, my_address);
  tree->stc_seq_num ++; /* moved because children will send with this seqnum */
  message.stc_seq_num = tree->stc_seq_num;
  hipsens_address_copy(message.tree_root_address, my_address);
  message.cost = cost;
  /* XXX: modified:
     hipsens_address_copy(message.parent_address, undefined_address); */
  hipsens_address_copy(message.parent_address, my_address);
  message.vtime = hipsens_time_to_vtime(state->config->tree_hold_time);
  message.ttl = DEFAULT_STC_TTL;

  if (!IS_FOR_SERENA(*tree)) {
    message.flag_colored = 0;
    message.flags = 0;
    message.tree_seq_num = 0;
  } else {
    update_tree_stability_status(state, tree); // XXX:check if ok here
    message.flag_colored = 1;
    message.flags = tree->serena_info->flags;
    message.tree_seq_num = tree->serena_info->tree_seq_num;
  }

  int result =  buffer_put_stc_message(&buffer, &message);

  if (result < 0) {
    WARN(state->base, "packet buffer too small.\n");
    return -1; /* buffer overflow */
  }

  return result;
}

int eostc_count_descendant(eostc_state_t* state, eostc_tree_t* tree)
{
  ASSERT( IS_FOR_SERENA(*tree) );
  eostc_serena_tree_t* serena_tree = tree->serena_info;
  int nb_descendant = 1 /* the node itself */;
  int i;
  for (i=0;i<MAX_NEIGHBOR;i++)
    if (serena_tree->child[i].status == Child_Stable)
      nb_descendant += serena_tree->child[i].nb_descendant;
  return nb_descendant;
}

int eostc_generate_tree_status_message(eostc_state_t* state,
				       eostc_tree_t* tree,
				       byte* packet, int max_packet_size)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  ASSERT( IS_FOR_SERENA(*tree) );
  eostc_serena_tree_t* serena_tree = tree->serena_info;
  ASSERT( serena_tree->should_generate_tree_status );
  STLOGA(DBGstc || DBGmsg, "eostc-generate-tree-status\n");

  serena_tree->limit_tree_status = HIPSENS_TRUE;

  hipsens_u16 nb_descendant = eostc_count_descendant(state, tree);

  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);

  buffer_put_u8(&buffer, HIPSENS_MSG_TREE_STATUS);
  int msg_size_pos = buffer.pos;
  buffer_put_u8(&buffer, 0); /* size, filed later */
  int msg_content_start_pos = buffer.pos;
  buffer_put_ADDRESS(&buffer, my_address); /* sender address */
  buffer_put_u16(&buffer, tree->stc_seq_num);  /* stc sequence number */
  buffer_put_ADDRESS(&buffer, tree->root_address); /*strategic node */

  buffer_put_u16(&buffer, serena_tree->tree_seq_num); /* tree seqnum */
  buffer_put_u16(&buffer, nb_descendant); /* number of descendants */
  buffer_put_u8(&buffer, serena_tree->flags);

  /* --- update size field */
  if (buffer.status != HIPSENS_TRUE) {
    return -1;
  }

  int result = buffer.pos;
  ASSERT(result > 0);
  buffer.pos = msg_size_pos;
  buffer_put_u8(&buffer, result - msg_content_start_pos);
  return result;
}

/*---------------------------------------------------------------------------*/

void eostc_get_next_wakeup_condition(eostc_state_t* state,
				     hipsens_wakeup_condition_t* condition)
{
  condition->wakeup_time = undefined_time;
  condition->wakeup_time_buffer = state->next_msg_stc_time;

  if (eostc_get_pending_stc_tree(state) != NULL 
      || eostc_get_pending_tree_status_tree(state) != NULL)
    condition->wakeup_time_buffer = state->base->current_time;
}

int eostc_notify_wakeup(eostc_state_t* state, byte* packet, int max_packet_size)
{
  if (packet == NULL)
    return 0;
  
  int packet_size = 0;

  if (HIPSENS_TIME_COMPARE_LARGE_UNDEF
      (state->base->current_time, <, state->next_msg_stc_time))  {
    /* no STC to generate, try to repeat STCs */
    eostc_tree_t* tree = eostc_get_pending_stc_tree(state);
    if (tree != NULL) {
      packet_size = eostc_repeat_stc_message(state, tree, packet, 
					     max_packet_size);
    }
  } else {
    packet_size = eostc_generate_stc_message(state, packet, max_packet_size);
  }

  if (packet_size < 0) {
    STWARN("eond_notify_wakeup: eostc message generation/forwarding failed\n"); 
    return 0;
  } 

  if (packet_size > 0)
    return packet_size; /* no concatenation */

  packet += packet_size;
  max_packet_size -= packet_size;

  eostc_tree_t* tree = eostc_get_pending_tree_status_tree(state);
  if (tree != NULL) {
    packet_size = eostc_generate_tree_status_message(state, tree, packet,
						     max_packet_size);
    if (packet_size <= 0) {
      STWARN("eond_notify_wakeup: tree status message generation failed\n"); 
      return 0;
    }
  }
  return packet_size;
}

/*---------------------------------------------------------------------------*/

#ifdef WITH_PRINTF

static const char* tree_status_as_string(tree_status_t status) 
{
  switch(status) {
  case EOSTC_None: return "None";
  case EOSTC_HasParent: return "'has-parent'";
  case EOSTC_IsRoot: return "'is-root'";
  default: return "'unknown'";
  }
}

static const char* child_status_as_string(child_status_t status) 
{
  switch(status) {
  case Child_None: return "None";
  case Child_Unstable: return "'unstable'";
  case Child_Stable: return "'stable'";
  default: return "'unknown'";
  }
}

void eostc_pywrite(outstream_t out, eostc_state_t* state)
{
  FPRINTF(out, "{ 'type':'eostc-state',\n  'nextMsgTime':" FMT_HST,
	  state->next_msg_stc_time);
  FPRINTF(out, ",\n  'treeTable':[");
  int i;
  hipsens_bool isFirst = HIPSENS_TRUE;
  for (i=0; i<MAX_STC_TREE; i++)
    if (state->tree[i].status != EOSTC_None) {
      if (isFirst) isFirst = HIPSENS_FALSE;
      else FPRINTF(out, ",");

      eostc_tree_t* tree = &(state->tree[i]);

      FPRINTF(out, "\n  {'status':%s", tree_status_as_string(tree->status));
      FPRINTF(out, ", 'rootAddress':");
      address_pywrite(out, tree->root_address);
      FPRINTF(out, ", 'msgSeqNum':%d", tree->stc_seq_num);
      FPRINTF(out, ", 'parentAddress':");
      if (!hipsens_address_equal(tree->parent_address, undefined_address))
	address_pywrite(out, tree->parent_address);
      else FPRINTF(out, "None");
      FPRINTF(out, ", 'cost':%d", tree->current_cost);
      FPRINTF(out, ", 'validityTime':" FMT_HST, tree->validity_time);
      if (tree->ttl_if_generate == 0) {
	FPRINTF(out, ", 'pendingSTC': None");
      } else {
	FPRINTF(out, ", 'pendingSTC': {'vtime': %d, 'ttl': %d }",
		tree->received_vtime, tree->ttl_if_generate);
      }
      FPRINTF(out, ", 'isForSerena': %d", IS_FOR_SERENA(*tree));

      if (IS_FOR_SERENA(*tree)) {
	eostc_serena_tree_t* serena_tree = tree->serena_info;
	FPRINTF(out, ", 'pendingTreeStatus': %d",
		serena_tree->should_generate_tree_status);
	FPRINTF(out, ", 'limitTreeStatus': %d",
		serena_tree->limit_tree_status);
	FPRINTF(out, ", 'hasSentStable': %d", serena_tree->has_sent_stable);
	FPRINTF(out, ", 'subtreeStable': %d", serena_tree->is_subtree_stable);
	FPRINTF(out, ", 'neighborhoodStable': %d",
		serena_tree->is_neighborhood_stable);
	if (!serena_tree->is_neighborhood_stable)
	  FPRINTF(out, ", 'stabilityTime': " FMT_HST, 
		  serena_tree->stability_time);
	else FPRINTF(out, ", 'stabilityTime': None");
	FPRINTF(out, ", 'treeSeqNum': %d", serena_tree->tree_seq_num);
	FPRINTF(out, ", 'flags': %d", serena_tree->flags);
	FPRINTF(out, ", 'childTable':[");
	int j;
	hipsens_bool isFirstChild = HIPSENS_TRUE;
	for (j=0;j<MAX_NEIGHBOR;j++)
	  if (tree->serena_info->child[j].status != Child_None) {
	    eostc_child_t* child = &tree->serena_info->child[j];
	    if (isFirstChild) isFirstChild = HIPSENS_FALSE;
	    else FPRINTF(out, ",");
	    FPRINTF(out, "{'address':");
	    address_pywrite(out, child->address);
	    FPRINTF(out, ", 'nbDescendant':% d", child->nb_descendant);
	    FPRINTF(out, ", 'status': %s", 
		    child_status_as_string(child->status));
	    FPRINTF(out, ", 'validityTime': " FMT_HST, child->validity_time);
	    FPRINTF(out, "}");
	  }
	FPRINTF(out, "]");
      } 
      FPRINTF(out, "}");
    }
  FPRINTF(out, "]}\n");
}

#endif /* WITH_PRINTF */

/*---------------------------------------------------------------------------*/
