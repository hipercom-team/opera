/*---------------------------------------------------------------------------
 *                         OPERA - Optimized Serena
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

#include <string.h>

#include "hipsens-all.h"

/*---------------------------------------------------------------------------*/

#ifdef WITH_SIMUL
#define WITH_START_ON_COLOR_MESSAGE
#endif

#define DEFAULT_MSG_COLOR_INTERVAL_SEC 2 /* seconds */

/*---------------------------------------------------------------------------*/

// -- logging information
// STDBG(condition, for(i=-0) ...  );  -- only debugging code
// STLOG(condition, "%d %d\n", 1, 3);
// STWARN("warning:%d\n", 123);
// STFATAL("error:%d\n", 123);
// STLOGA(condition, "%d %d\n", 1, 3); -- log with time and address

/*---------------------------------------------------------------------------*/

#if defined(WITH_SIMUL) && defined(WITHOUT_LOSS)
int new__stop_color_generation(serena_state_t *state);
#endif /* WITH_SIMUL */

/*---------------------------------------------------------------------------*/

void bitmap_init(bitmap_t* bitmap)
{ memset(bitmap, 0, sizeof(*bitmap)); }

static void addr_priority_init(addr_priority_t* addr_priority)
{
  SET_PRIORITY(addr_priority->priority, PRIORITY_NONE);
  memset(addr_priority->address, 0, ADDRESS_SIZE); //XXX! null address
}


void serena_config_init_default(serena_config_t* config)
{
#ifdef WITH_FRAME_TIME
  config->max_jitter_time = 0;
  config->msg_color_interval = SEC_TO_FRAME_TIME
    (DEFAULT_MSG_COLOR_INTERVAL_SEC);
#else
  config->max_jitter_time = MILLISEC_TO_HIPSENS_TIME
    (DEFAULT_JITTER_MILLISEC);
  config->msg_color_interval = SEC_TO_HIPSENS_TIME
    (DEFAULT_MSG_COLOR_INTERVAL_SEC);
#endif /* WITH_FRAME_TIME */
  config->priority_mode = Priority_mode_tree;
  /* note: fixed_priority is not used when Priority_mode_tree (set anyway): */
  config->recoloring_delay = 0; /* 0 = do not recolor */
  SET_PRIORITY(config->fixed_priority,1); 
}


/* minimum initialisation for serena */
void new__serena_state_init(serena_state_t* state, base_state_t* base,
			    serena_config_t* config)
{
  state->base = base;
  state->config = config;
  state->nd_state = NULL;
  state->is_started = HIPSENS_FALSE;
  state->is_topology_set = HIPSENS_FALSE;
  state->is_finished = HIPSENS_FALSE;
  state->next_msg_color_time = undefined_time;
  state->callback_coloring_finished = NULL;
#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
  state->is_stopped = HIPSENS_FALSE;
  state->nb_sent_msg = 0;
  state->size_sent_msg = 0;
  state->first_empty_msg = HIPSENS_FALSE;
#endif //defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
}

/* this is NO_COLOR in OCARI */
#define OCARI_NO_COLOR 0

/* 
   The topology must have been set by the caller before calling this method,
   that is:
   - nb_neighbor must be set and neighbor_table must be filled
        (the fields: address, is_child, is_parent)
   - is_topology_set must be set to TRUE=1 (and is_started set to FALSE=0)
   - priority, root_address and the tree_seq_num must be set
*/
void new__serena_start(serena_state_t* state)
{
  if (!state->is_topology_set || state->is_started)
    return;
  state->is_started = HIPSENS_TRUE;
  state->is_finished = HIPSENS_FALSE;

  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    serena_neighbor_t* current_neigh = &(state->neighbor_table[i]);
    current_neigh->has_prio  = HIPSENS_FALSE;
    SET_PRIORITY(current_neigh->priority, PRIORITY_NONE); // XXX:used even if above is false
    current_neigh->has_prio1 = HIPSENS_FALSE;
    current_neigh->has_prio2 = HIPSENS_FALSE;
    current_neigh->color = COLOR_NONE;
    
    current_neigh->has_sent_max_color = HIPSENS_FALSE;
    current_neigh->child_max_color = COLOR_NONE;

    /* clean previous max prio */
    int j;
    for (j=0;j<MAX_PRIO1_SIZE;j++)
      addr_priority_init(&(current_neigh->max2_prio1[j]));
    
    for (j=0;j<MAX_PRIO2_SIZE;j++)
      addr_priority_init(&(current_neigh->max2_prio2[j]));
  }

  state->color = COLOR_NONE;
  // XXX: not set: state->priority = PRIORITY_NONE;   
#if defined(DBG_SERENA)
  state->coloring_time = undefined_time;
#endif

#ifdef IMPLICIT_COLORING_DETECTION
  state->last_implicit_colored_index = 0;
  for (i=0; i<MAX_IMPLICIT_COLORED; i++)
    addr_priority_init(&state->last_implicit_colored[i]);
#endif /* IMPLICIT_COLORING_DETECTION */

  state->last_nb_color = 0;

  bitmap_init(&(state->color_bitmap1));
  bitmap_init(&(state->color_bitmap2));
  bitmap_init(&(state->color_bitmap3));

  state->next_msg_color_time = base_state_time_after_delay_jitter
    (state->base, state->config->msg_color_interval,
     state->config->max_jitter_time);
  state->is_started = HIPSENS_TRUE;

  /* external information */
  state->final_nb_neighbor_color = 0;
  state->final_node_color = OCARI_NO_COLOR;
}

int serena_find_neighbor_index(serena_state_t* state, address_t address)
{
  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    serena_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (!memcmp(neighbor->address, address, ADDRESS_SIZE))
      return i;
  }
  return -1;
}

void serena_remove_neighbor(serena_state_t* state, int neighbor_index)
{
  ASSERT( neighbor_index < state->nb_neighbor );
  if (neighbor_index < state->nb_neighbor-1) {
    memcpy( &state->neighbor_table[neighbor_index], 
	    &state->neighbor_table[state->nb_neighbor-1],
	    sizeof(serena_neighbor_t) );
  }
  state->nb_neighbor --;
}

/* return -1 if p1<p2, 0 if p1 == p2, and +1 if p1 > p2 */
static int cmp_addr_priority(addr_priority_t* p1, addr_priority_t* p2) 
{
  /* XXX: warning: PRIORITY_NONE should be lower than any real priority */
  if (GET_PRIORITY(p1->priority) < GET_PRIORITY(p2->priority)) return -1;
  else if (GET_PRIORITY(p1->priority) > GET_PRIORITY(p2->priority)) return +1;

  return -memcmp(p1->address, p2->address, ADDRESS_SIZE);
}


static void update_max2_prio(int max_prio_size, addr_priority_t addr_priority[],
			     addr_priority_t* new_addr_priority)
{

  int i, cmp, j;
#ifdef DEBUG
  FPRINTF(stdout," update ");
  addr_priority_pywrite(stdout, addr_priority, MAX_PRIO2_SIZE);
  FPRINTF (stdout, " with ");
  addr_priority_pywrite(stdout, new_addr_priority, 1);  
#endif

  for (i=0;i<max_prio_size;i++) {
    cmp = cmp_addr_priority(new_addr_priority, &(addr_priority[i]));
    if (cmp == 0) {
#ifdef DEBUG
      FPRINTF (stdout," and result is ");
      addr_priority_pywrite(stdout, addr_priority, MAX_PRIO2_SIZE);
      FPRINTF (stdout,"\n");
#endif
      return;
    }
    if (cmp > 0){
      for (j=max_prio_size-2;j>=i;j--){
	if (&(addr_priority[j]).priority != PRIORITY_NONE)
	  memcpy(&(addr_priority[j+1]), &(addr_priority[j]), sizeof(addr_priority_t));
      }
      memcpy(&(addr_priority[i]), new_addr_priority, sizeof(addr_priority_t));
#ifdef DEBUG
      FPRINTF (stdout," and result is ");
      addr_priority_pywrite(stdout, addr_priority, MAX_PRIO2_SIZE);
      FPRINTF (stdout,"\n");
#endif
      return;
    } 
  }
}

static void update_max_prio(addr_priority_t* addr_priority, 
			    addr_priority_t* new_addr_priority)
{
  int cmp0 = cmp_addr_priority(new_addr_priority, addr_priority);
  if (cmp0 == 0) return;
  if (cmp0 > 0)
    memcpy(addr_priority, new_addr_priority, sizeof(addr_priority_t));
}

//not used
/*static void remove_max2_prio(addr_priority_t addr_priority[],
			     addr_priority_t* new_addr_priority)
{
  if (cmp_addr_priority(new_addr_priority, &(addr_priority[0]))==0) {
    memcpy(&(addr_priority[0]), &(addr_priority[1]), sizeof(addr_priority_t));
    addr_priority_init(&(addr_priority[1]));
  } else if (cmp_addr_priority(new_addr_priority, &(addr_priority[1]))==0) {
    addr_priority_init(&(addr_priority[1]));
  }
}*/

//not used
/*static void remove_max_prio(addr_priority_t* addr_priority,
		     addr_priority_t* new_addr_priority)
{
   if (cmp_addr_priority(new_addr_priority, addr_priority)==0) {
     addr_priority_init(addr_priority);
   }
}*/

static int count_max2_prio(int max_prio_size, addr_priority_t addr_priority[])
{
  int i;
  for (i=0;i<max_prio_size;i++) {
    if (IS_PRIORITY_NONE(addr_priority[i].priority)) return i;  
  }
  return max_prio_size;  
}

static int serena_has_all_neigh_prio(serena_state_t* state)
{
  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    if (!state->neighbor_table[i].has_prio)
      return HIPSENS_FALSE;
  }
  return HIPSENS_TRUE;
}

static int serena_has_all_neigh_prio1(serena_state_t* state)
{
  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    if (!state->neighbor_table[i].has_prio1)
      return HIPSENS_FALSE;
  }
  return HIPSENS_TRUE;
}

static int serena_has_all_neigh_prio2(serena_state_t* state)
{
  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    if (!state->neighbor_table[i].has_prio2)
      return HIPSENS_FALSE;
  }
  return HIPSENS_TRUE;
}

/*---------------------------------------------------------------------------*/

/* note: result, bitmap1, and/or bitmap2 can point to same bitmap */
static void bitmap_difference(bitmap_t* result, bitmap_t* bitmap1, bitmap_t* bitmap2)
{ 
  int i;
  for (i=0; i<BYTES_PER_BITMAP; i++)
    result->content[i] = bitmap1->content[i] & ~(bitmap2->content[i]);
}

/* note: result, bitmap1, and/or bitmap2 can point to same bitmap */
static void bitmap_union(bitmap_t* result, bitmap_t* bitmap1, bitmap_t* bitmap2)
{
  int i;
  for (i=0; i<BYTES_PER_BITMAP; i++)
    result->content[i] = bitmap1->content[i] | bitmap2->content[i];
}


void bitmap_set_bit(bitmap_t* bitmap, int pos, serena_state_t* state)
{
  int byteIndex = BYTE_OF_BIT(pos);
  if (byteIndex < NB_COLOR_MAX) {
    bitmap->content[BYTE_OF_BIT(pos)] |= MASK_OF_INDEX(INDEX_OF_BIT(pos));
  } else {
    DBG( hipsens_fatal(NULL/*XXX*/, "bit position too high for bitmap") );
  }
}

static void bitmap_clear_bit(bitmap_t* bitmap, int pos, serena_state_t* state)
{
  int byteIndex = BYTE_OF_BIT(pos);
  if (byteIndex < NB_COLOR_MAX) {
    bitmap->content[BYTE_OF_BIT(pos)] 
      &= ~(unsigned int)MASK_OF_INDEX(INDEX_OF_BIT(pos));
  } else {
    DBG( hipsens_fatal(NULL/*XXX*/, "bit position too high for bitmap") );
  }
}

#if 0
/* XXX: no longer used */
static int bitmap_get_first_empty_bit(bitmap_t* bitmap)
{
  int i,j;
  for (i=0; i<BYTES_PER_BITMAP; i++) {
    if (bitmap->content[i] == 0xffu)
      continue;
    for (j=0; j<BITS_PER_BYTE;j++)
      if ((bitmap->content[i]&(1<<j)) == 0)
	return i*BITS_PER_BYTE + j;
  }
  return -1;
}
#endif

static int bitmap_get_first_empty_bit_greater_than(bitmap_t* bitmap, 
						   int min_value)
{
  int i,j;
  for (i=0; i<BYTES_PER_BITMAP; i++) {
    if (bitmap->content[i] == 0xffu)
      continue;
    for (j=0; j<BITS_PER_BYTE;j++)
      if ((bitmap->content[i]&(1<<j))==0 && (i*BITS_PER_BYTE + j)>=min_value)
	return i*BITS_PER_BYTE + j;
  }
  return -1;
}

static int bitmap_get_size(bitmap_t* bitmap) 
{
  int result = BYTES_PER_BITMAP;
  while (result>0  && (bitmap->content[result-1])==0)
    result--;
  return result;
}

static void buffer_get_bitmap(serena_state_t* state, 
			      buffer_t* buffer, bitmap_t* bitmap)
{
  int size = buffer_get_byte(buffer);
  bitmap_init(bitmap);
  if (size > BYTES_PER_BITMAP) {
    DBG( hipsens_fatal(NULL/*XXX*/, "received bitmap too large") );
    /* XXX: consumme size bytes */
    return;
  }
  buffer_get_data(buffer, bitmap->content, size);
}

static void buffer_put_bitmap(buffer_t* buffer, bitmap_t* bitmap)
{
  int size = bitmap_get_size(bitmap);
  buffer_put_byte(buffer, size);
  buffer_put_data(buffer, bitmap->content, size);
}

/*---------------------------------------------------------------------------*/


static int serena_get_color(serena_state_t* state, address_t* address)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  if (address_equal(&my_address, address)) {
    /* this is the node address */
    return state->color;
  } else {
    /* this is the address of another node, look in neighbor table */
    int neigh_index = serena_find_neighbor_index(state, *address);
    if (neigh_index == -1)
      return COLOR_NONE;
    serena_neighbor_t* neighbor = &(state->neighbor_table[neigh_index]);
    return neighbor->color;    
  }
}


static hipsens_bool serena_has_color(serena_state_t* state, address_t* address)
{
  hipsens_bool has_color = (serena_get_color(state, address) != COLOR_NONE);
  if (has_color)
    return HIPSENS_TRUE;

#ifdef IMPLICIT_COLORING_DETECTION
  int i;
  for (i=0;i<MAX_IMPLICIT_COLORED; i++)
    if (!IS_PRIORITY_NONE(state->last_implicit_colored[i].priority)
	&& hipsens_address_equal(state->last_implicit_colored[i].address,
				 *address)){
    	// XXX: address
    	
    	return HIPSENS_TRUE;
    }
      
#endif /* IMPLICIT_COLORING_DETECTION */
  return HIPSENS_FALSE;
}

#ifdef IMPLICIT_COLORING_DETECTION
// XXX: this relies on null address (0) different from everything else
static void notify_implicit_node_coloring(serena_state_t* state,
					  addr_priority_t* addr_priority)
{
  int i;
  for (i=0; i <MAX_IMPLICIT_COLORED; i++)
    if (cmp_addr_priority(addr_priority, &state->last_implicit_colored[i]) == 0)
      return;
  memcpy(&state->last_implicit_colored[state->last_implicit_colored_index],
	 addr_priority, sizeof(addr_priority_t));
  state->last_implicit_colored_index 
    = (  state->last_implicit_colored_index + 1) % MAX_IMPLICIT_COLORED;

}
#endif /* IMPLICIT_COLORING_DETECTION */

static void notify_update_neighbor_prio(serena_state_t* state,
					int max_prio_size,
					addr_priority_t old_max2_prio[],
					addr_priority_t new_max2_prio[])
{

#ifdef debugfast	
  FPRINTF(stdout, "\n comparing: OLD:");
  addr_priority_pywrite(stdout, old_max2_prio, max_prio_size);
  FPRINTF(stdout, " To NEW:");
  addr_priority_pywrite(stdout, new_max2_prio, max_prio_size);
  printf ("\n");
#endif
  
  int i,j;
  for (i=0; i< max_prio_size; i++) {//XXX
    if (!IS_PRIORITY_NONE(old_max2_prio[i].priority)) {
      hipsens_bool old_is_greater = HIPSENS_FALSE;
      for (j=0; j<1; j++){
	if (cmp_addr_priority(&old_max2_prio[i], &new_max2_prio[j]) > 0) {
	  old_is_greater = HIPSENS_TRUE;
	}
      }  
      if (old_is_greater) {
#ifdef debugfast	
	FPRINTF(stdout, " so to notify i=:%d",i);
	addr_priority_pywrite(stdout, old_max2_prio, 1);
#endif
#ifdef IMPLICIT_COLORING_DETECTION
	notify_implicit_node_coloring(state, &old_max2_prio[i]); 
#endif /* IMPLICIT_COLORING_DETECTION */
      }
      
    }
  }
}

#define SHORT_HEADER_SIZE 2
static void serena_internal_process_message(serena_state_t* state,
					    buffer_t* buffer)
{
  int i;
  address_t originator;

  STLOGA(DBGsrn || DBGmsg || DBGmsgdat, "process-msg-color ");

  if (buffer_get_byte(buffer) != HIPSENS_MSG_COLOR) {
    STWARN("bad message header, expected HIPSENS_MSG_COLOR");
    return;
  }
  int message_size = buffer_get_u8(buffer);
  if (message_size != buffer_remaining(buffer)) {
    STWARN("bad message size: size@header=%d actual-size=%d\n",
	   message_size, buffer_remaining(buffer));
  }
  buffer_get_data(buffer, originator, ADDRESS_SIZE);

  address_t root_address;
  buffer_get_ADDRESS(buffer, root_address);
  hipsens_u16 tree_seq_num = buffer_get_u16(buffer);
  hipsens_u8 nb_color = buffer_get_u8(buffer);

  STWRITE(DBGnd || DBGmsgdat, address_write, originator);
  //STLOG(DBGsrn || DBGmsgdat, " [");
  //STWRITE(DBGsrn || DBGmsgdat, data_write, buffer->data, buffer->size);
  //STLOG(DBGsrn || DBGmsgdat, "]");
  STLOG(DBGsrn || DBGmsg || DBGmsgdat, "\n");

  if (!state->is_started) {
#ifndef WITH_START_ON_COLOR_MESSAGE
    return;
#endif    
    
    if (!state->is_topology_set) {
      /* Note: we don't know the topology, will be handled by EOSTC */
      STWARN("Color message received, but node is not started, "
	     "and has no topology.\n");
      
      return;
    }
  }

  if (!hipsens_address_equal(root_address, state->root_address))
    return; /* ignore color messages from other trees */

  if (hipsens_seqnum_cmp(tree_seq_num, state->tree_seq_num) < 0)
    return; /* ignore color messages from old trees */

  if (tree_seq_num != state->tree_seq_num) {
    /* here, the seqnum in the message was greater: another tree is colored */
#if 0
    state->is_started = HIPSENS_TRUE;
    state->is_topology_set = HIPSENS_TRUE;
#endif
    state->is_finished = HIPSENS_TRUE;
    /* XXX: note: topology is not available */
    //STWARN("XXX: send UNSTABLE_OUT_OF_TREE");
    return;
  }

  if (!state->is_started) {
    new__serena_start(state);
    if (!state->is_started) {
      STWARN("Color message received, but node failed to start.\n");
      return;
    }
  }
  ASSERT( state->is_started );

  int neigh_index = serena_find_neighbor_index(state, originator);
  if (neigh_index == -1) {
    static hipsens_bool has_warned = HIPSENS_FALSE;
    if (!has_warned) {
      STWARN("XXX: not implemented: receive message with unknown neighbor:");
#ifdef WITH_PRINTF
      address_pywrite(stdout, originator); 
#endif
      STWARN("\n");
    
      has_warned = HIPSENS_TRUE;
    }
    return;
  }
  serena_neighbor_t* neighbor = &(state->neighbor_table[neigh_index]);
  
  byte neigh_color = buffer_get_byte(buffer);
  //byte neigh_priority = buffer_get_byte(buffer);
  priority_t neigh_priority;
  buffer_get_PRIORITY(buffer, neigh_priority);

  /*--- Update whenever a neighbor has already a color ---*/
  neighbor->color = neigh_color;
  hipsens_bool has_color = (neigh_color != COLOR_NONE);
  if (has_color)
    bitmap_set_bit(&(state->color_bitmap1), neigh_color,  state);

  /*--- Update all the priority information ---*/
  neighbor->has_prio = HIPSENS_TRUE;
  neighbor->priority = neigh_priority; /* address is already there */

  /* update Max2Prio1 */
  addr_priority_t neigh_addr_priority;
  neigh_addr_priority.priority = neigh_priority;
  address_copy(&neigh_addr_priority.address, &originator);

  /* update neighbor->max2_prio1 for Max2Prio2 */
  byte nb_max2_prio1 = buffer_get_byte(buffer);
  neighbor->has_prio1 |= (nb_max2_prio1 > 0) || has_color; /* XXX: hack */
  
  addr_priority_t previous_max2_prio1[MAX_PRIO1_SIZE];
  memcpy(&previous_max2_prio1, neighbor->max2_prio1, 
	 sizeof(previous_max2_prio1));
  for (i=0; i<nb_max2_prio1; i++) {
    addr_priority_t neigh_max2_prio1;
    buffer_get_PRIORITY(buffer, neigh_max2_prio1.priority);
    buffer_get_data(buffer, neigh_max2_prio1.address, ADDRESS_SIZE);
    memcpy( &(neighbor->max2_prio1[i]), & neigh_max2_prio1, 
	    sizeof(addr_priority_t));
  }
  
  
  for (i=nb_max2_prio1;  i<MAX_PRIO1_SIZE; i++)
	addr_priority_init(&(neighbor->max2_prio1[i]));
  
  notify_update_neighbor_prio(state, MAX_PRIO1_SIZE, previous_max2_prio1, neighbor->max2_prio1);

  /* update neighbor->max2_prio2 for Max2Prio3 */
  byte nb_max2_prio2 = buffer_get_byte(buffer);
  neighbor->has_prio2 |= (nb_max2_prio2 > 0) || has_color; /* XXX: hack */
 
  addr_priority_t previous_max2_prio2[MAX_PRIO2_SIZE];
  memcpy(&previous_max2_prio2, neighbor->max2_prio2, sizeof(previous_max2_prio2));
  for (i=0; i<nb_max2_prio2; i++) {
    addr_priority_t neigh_max2_prio2;
    buffer_get_PRIORITY(buffer, neigh_max2_prio2.priority);
    buffer_get_data(buffer, neigh_max2_prio2.address, ADDRESS_SIZE);
    //update_neighbor_prio(state, &(neighbor->max2_prio1[i]), &neigh_max2_prio1);
    memcpy( &(neighbor->max2_prio2[i]), & neigh_max2_prio2, 
        sizeof(addr_priority_t));
  }
  
  for (i=nb_max2_prio2;  i<MAX_PRIO2_SIZE; i++)
    addr_priority_init(&(neighbor->max2_prio2[i]));
  
  notify_update_neighbor_prio(state, MAX_PRIO2_SIZE, previous_max2_prio2, neighbor->max2_prio2);

  /*--- Update all the color information ---*/

  bitmap_t neigh_color_bitmap1;
  buffer_get_bitmap(state, buffer, &neigh_color_bitmap1);
  bitmap_union(&(state->color_bitmap2), /*=*/
	       &(state->color_bitmap2), /*|*/ &neigh_color_bitmap1);
  bitmap_difference(&(state->color_bitmap2), /*=*/ &(state->color_bitmap2),
		    /*-*/ &(state->color_bitmap1));
  if (state->color != COLOR_NONE)
    bitmap_clear_bit(&(state->color_bitmap2), state->color,  state);

  bitmap_t neigh_color_bitmap2;
  buffer_get_bitmap(state, buffer, &neigh_color_bitmap2);
  bitmap_union(&(state->color_bitmap3), /*=*/
	       &(state->color_bitmap3), /*|*/ &neigh_color_bitmap2);  

  /* update MAX_COLOR information */
  if (nb_color > 0) {
    neighbor->has_sent_max_color = HIPSENS_TRUE; /* XXX:reset in case of tree change */
    neighbor->child_max_color = nb_color - 1;
  }

  STLOG(DBGsrn, "\n");
}

static int serena_update_color(serena_state_t* state,
			       addr_priority_t* max_addr_priority)
{
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  if (state->color != COLOR_NONE)
    return HIPSENS_FALSE;

  if (!serena_has_all_neigh_prio2(state)) 
    return HIPSENS_FALSE;
  
  if (!IS_PRIORITY_NONE(max_addr_priority->priority)
      && hipsens_address_equal(max_addr_priority->address, my_address)) {
    bitmap_t all_color_bitmap;
    bitmap_union(&all_color_bitmap, &(state->color_bitmap1),
		 &(state->color_bitmap2));
    bitmap_union(&all_color_bitmap, &all_color_bitmap, &(state->color_bitmap3));
    
    int min_color = 0;

    /* if there is a parent and it has a color, min_color is set to this */
    if (state->config->priority_mode == Priority_mode_tree) {
      int i;
      for (i=0; i<state->nb_neighbor; i++)
	if (state->neighbor_table[i].is_parent 
	    && state->neighbor_table[i].color != COLOR_NONE)
	  min_color = state->neighbor_table[i].color;
    }
      
    int indexBit = bitmap_get_first_empty_bit_greater_than(&all_color_bitmap,
							   min_color);
    state->color = indexBit;
    return HIPSENS_TRUE;
  } else return HIPSENS_FALSE;
}


//not used
/*
static byte received_max_prio_is_two_hop_neighbor(serena_state_t* state,
			     addr_priority_t received_max2_prio1[2]){
  int i,j;
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  for (j=0;j<2;j++) {
	  byte is_twohop = HIPSENS_FALSE;
	  for (i=0; i<state->nb_neighbor; i++) {
	 	  serena_neighbor_t* neighbor = &(state->neighbor_table[i]);	  
	 		  if (memcmp(neighbor->address, &received_max2_prio1[j].address, ADDRESS_SIZE) != 0)
	 			   if (memcmp(my_address, &received_max2_prio1[j].address, ADDRESS_SIZE) != 0)
	 					is_twohop = HIPSENS_TRUE;  
	  }
	  
	  if (is_twohop)
		  return HIPSENS_TRUE;
  }

  return HIPSENS_FALSE;
 }
*/

//not used
/*
static byte should_update_max_prio(serena_state_t* state) {
	
	int i;
	byte update = HIPSENS_FALSE;
	
	for (i=0; i<state->nb_neighbor; i++) {
		 	  serena_neighbor_t* neighbor = &(state->neighbor_table[i]);
		 	  addr_priority_t received_max2_prio1[2];
		 	  update = received_max_prio_is_two_hop_neighbor (state, &neighbor->max2_prio1);
	}
	return update;
}
*/


static void serena_compute_max_prio
(serena_state_t* state,
 addr_priority_t local_max2_prio1[MAX_PRIO1_SIZE],
 addr_priority_t local_max2_prio2[MAX_PRIO2_SIZE],
 addr_priority_t* local_max_prio3)
{
  int i,j;
 
  /* compute the max_prio */
  for (i=0; i<MAX_PRIO1_SIZE; i++) 
    addr_priority_init(&(local_max2_prio1[i]));
  
  for (i=0;i<MAX_PRIO2_SIZE;i++)
    addr_priority_init(&(local_max2_prio2[i]));
  
  addr_priority_init(local_max_prio3);
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  for (i=0; i<state->nb_neighbor;i++) {
    //byte removed_node_prio2 = HIPSENS_FALSE;
    //byte removed_node_prio3 = HIPSENS_FALSE;
    serena_neighbor_t* neighbor = &(state->neighbor_table[i]);
    addr_priority_t neigh_addr_priority;
    neigh_addr_priority.priority = neighbor->priority;
    address_copy(&neigh_addr_priority.address, &(neighbor->address));
    
    //1. update Max2Prio1
    if (!serena_has_color(state, &(neigh_addr_priority.address)))
      update_max2_prio(MAX_PRIO1_SIZE, local_max2_prio1, &neigh_addr_priority);
    
    /* XXX: check if ok when j>actual max2_prio... size */
    //2. update Max2Prio2
    for (j=0; j< MAX_PRIO1_SIZE; j++) {
      switch (j){
      case 0:
	if (!serena_has_color(state, &(neighbor->max2_prio1[j].address)))
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	
	break;
      case 1:
#ifdef OLD_IMPLICIT_COLORING
	if (!serena_has_color(state, &(neighbor->max2_prio1[j].address)))
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	else 
	  if (
	      (&(neighbor->max2_prio1[j+1]).priority == PRIORITY_NONE) ||
	      
	      ( (&(neighbor->max2_prio1[j+1]).priority != PRIORITY_NONE) && (&(neighbor->max2_prio1[j+1]).priority == PRIORITY_NONE))
	      )
	    update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
#endif /* OLD_IMPLICIT_COLORING */
	if (!serena_has_color(state, &(neighbor->max2_prio1[j].address)))
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	else if (&(neighbor->max2_prio1[MAX_PRIO1_SIZE-1]).priority != PRIORITY_NONE)
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	break;
      case 2:
	if (&(neighbor->max2_prio1[j]).priority != PRIORITY_NONE)
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	break;
      case 3:
	if (&(neighbor->max2_prio1[j]).priority != PRIORITY_NONE)
	  update_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2, &(neighbor->max2_prio1[j]));
	break;
      default:
	STWARN("Max2Prio2 update: Generalization is not implemented yet!");
      }		  
    }
    
    
    //3. update MaxPrio3
    for (j=0; j< MAX_PRIO2_SIZE; j++) {
#if 0
      // We can remove all colored nodes, except the last one
      // The reason: maybe there is another node with intermediary
      // priority 
      if (!serena_has_color(state, &(neighbor->max2_prio2[0].address)) 
	  || j == MAX_PRIO2_SIZE-1)
	update_max_prio(local_max_prio3, &(neighbor->max2_prio2[0]));
#endif

      switch(j){
      case 0:
	if (!serena_has_color(state, &(neighbor->max2_prio2[0].address)))
	  update_max_prio(local_max_prio3, &(neighbor->max2_prio2[0]));
	break;
      case 1:
	//
	if (!serena_has_color(state, &(neighbor->max2_prio2[j].address)))
	  update_max_prio(local_max_prio3, &(neighbor->max2_prio2[j]));
#ifdef OLD_IMPLICIT_COLORING
	else 	// the node is colored. XXX:maybe can remove in all cases
	  if (
	      (&(neighbor->max2_prio2[j+1]).priority == PRIORITY_NONE)
	      )
	    update_max_prio(local_max_prio3, &(neighbor->max2_prio2[j]));
#endif /* OLD_IMPLICIT_COLORING */
	break;
      case 2:  
	if (&(neighbor->max2_prio2[j]).priority != PRIORITY_NONE)
	  update_max_prio(local_max_prio3, &(neighbor->max2_prio2[j]));
	break;
	
      default:
	STWARN("MaxPrio3 update: Generalization is not implemented yet");
	break;
      }
    }
    
  }
  
}




/** return the number of colors (MAX_COLOR+1) necessary to color the 
    children of this node and this node itself.

    return 0, if the number is not yet known.

    MODIFIED:
    return 0 also if some of the colors of the neighbors are not known.
*/
int serena_get_nb_color(serena_state_t* state)
{
  int nb_color = 0;

  if (state->color != COLOR_NONE) {
    nb_color = state->color + 1;
    int i;
    for (i=0; i<state->nb_neighbor; i++) {
      if (state->neighbor_table[i].is_child) {
	serena_neighbor_t* neighbor = &state->neighbor_table[i];
	if (neighbor->has_sent_max_color) {
	  if (neighbor->child_max_color+1 > nb_color)
	    nb_color = neighbor->child_max_color+1;
	} else {
	  nb_color = 0;
	  break;
	}
      } else {
	/* we now store the neighbor colors before receiving the
	   confirmation from the root that the tree is colored.
	   hence we now wait for all neighbors to be colored 
	   XXX: can be changed */
	serena_neighbor_t* neighbor = &state->neighbor_table[i];
	if (neighbor->color == COLOR_NONE)
	  nb_color = 0;
      }
    }
  }
  return nb_color;
}

/*---------------------------------------------------------------------------*/

static hipsens_bool serena_get_color_info(serena_state_t* state, 
					  color_info_t* result)
{
  if (!state->is_started){
    STWARN("Serena not started yet:");
    if (!state->is_topology_set)
      STWARN(" topology not set.\n");
    return HIPSENS_FALSE;
  }

  if (state->color == COLOR_NONE || state->last_nb_color == 0) {
    STWARN("inconsistent state: serena finished but without (max_?)color.\n");
    return HIPSENS_FALSE;
  }
  
  result->nb_color = serena_get_nb_color(state);
  if (result->nb_color == 0) {
    STWARN("inconsistent state: serena finished but without MAX_COLOR.\n");
    return HIPSENS_FALSE;
  }

  result->node_color = state->color + 1;
  bitmap_t* neighbor_color_bitmap = &result->neighbor_color_bitmap;
  bitmap_init(neighbor_color_bitmap);
  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    serena_neighbor_t* neighbor  = &state->neighbor_table[i];
    if (neighbor->color != COLOR_NONE) {
      bitmap_set_bit(neighbor_color_bitmap, neighbor->color, state);
    } else {
      STWARN("unexpected neighbor without color.\n");
    }
  }
  return HIPSENS_TRUE;
}

static hipsens_bool serena_check_color_in_bitmap(serena_state_t* state,
						 bitmap_t* color_bitmap, 
						 hipsens_u8 color)
{
  if (color == 0) {
#ifdef WITH_SIMUL
    STFATAL("wrong color passed as argument.\n");
#endif
    return HIPSENS_FALSE;
  }
  int pos = color - 1;
  int byteIndex = BYTE_OF_BIT( pos );
  if (byteIndex < NB_COLOR_MAX) {
    return (color_bitmap->content[BYTE_OF_BIT(pos)] 
	    & MASK_OF_INDEX(INDEX_OF_BIT(pos))) != 0;
  } else {
#ifdef WITH_SIMUL
    STFATAL("bit position too high for bitmap");
#endif
    return HIPSENS_FALSE;
  }
  /* return HIPSENS_TRUE; XXX: unreachable */
}


static void serena_set_final_color(serena_state_t* state)
{
  /* XXX: maybe avoid use of old api w/ bitmap (but then: need a sort) */
  color_info_t color_info;

  state->final_nb_neighbor_color = 0;
  if (!serena_get_color_info(state, &color_info)) {
    STWARN("error while getting color information.\n");
    /* will set default color: none */
  } else {

      int i;
      for (i=0;i<NB_COLOR_MAX;i++)
	if (serena_check_color_in_bitmap
	    (state, &color_info.neighbor_color_bitmap, i+1)) {
	  if (state->final_nb_neighbor_color >= MAX_NEIGHBOR) {
	    STFATAL("too many neighbor colors.\n");
	    return;
	  }
	  state->final_neighbor_color_list[state->final_nb_neighbor_color] =i+1;
	  state->final_nb_neighbor_color++;
	}
      state->final_node_color = color_info.node_color;
    }
}

/*---------------------------------------------------------------------------*/

static int serena_internal_generate_message(serena_state_t* state,
					    buffer_t* buffer)
{
	
#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
//  int end = stop_color_generation(state);
//  if (end == 1)
	  //return 0;
#endif // defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
  
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  int i;
  STLOGA(DBGsrn, "serena-generate-msg-color ");
  addr_priority_t local_max2_prio1[MAX_PRIO1_SIZE];
  addr_priority_t local_max2_prio2[MAX_PRIO2_SIZE];
  addr_priority_t local_max_prio3;

  serena_compute_max_prio(state, local_max2_prio1,
			  local_max2_prio2, &local_max_prio3);
  
  
  addr_priority_t max_addr_priority;
  addr_priority_init(&max_addr_priority);
  update_max_prio(&max_addr_priority, &(local_max2_prio1[0]));
  update_max_prio(&max_addr_priority, &(local_max2_prio2[0]));  
  update_max_prio(&max_addr_priority, &(local_max_prio3));


  /* check if the node should be colored */
  hipsens_bool is_just_colored = serena_update_color(state, &max_addr_priority);
    
  if (is_just_colored == HIPSENS_TRUE) {
    serena_compute_max_prio(state, local_max2_prio1,
			    local_max2_prio2, &local_max_prio3);
#if defined(DBG_SERENA)
    state->coloring_time = state->base->current_time;
#endif
  }
//#define DBG_SERENA
#ifdef DBG_SERENA 

  addr_priority_init(&max_addr_priority);
  update_max_prio(&max_addr_priority, &(local_max2_prio1[0]));
  update_max_prio(&max_addr_priority, &(local_max2_prio2[0]));  
  update_max_prio(&max_addr_priority, &(local_max_prio3));


  memcpy(&(state->dbg_max_prio3), &local_max_prio3,
	 sizeof(addr_priority_t));
  
  memcpy(&(state->max2_prio1), &local_max2_prio1,
  	 sizeof(local_max2_prio1));
  
  memcpy(&(state->max2_prio2), &local_max2_prio2,
  	 sizeof(local_max2_prio2));

  memcpy(&(state->dbg_max_addr_priority), &max_addr_priority,
	 sizeof(addr_priority_t));
#endif

  /* generate message */
  buffer_put_u8(buffer, HIPSENS_MSG_COLOR);
  int msg_size_pos = buffer->pos;
  buffer_put_u8(buffer, 0); /* size, filled later */
  int msg_content_start_pos = buffer->pos;
  buffer_put_ADDRESS(buffer, my_address); /* originator */

  buffer_put_ADDRESS(buffer, state->root_address); /* root address */
  buffer_put_u16(buffer, state->tree_seq_num);


  int nb_color = serena_get_nb_color(state);


  if (state->last_nb_color != nb_color && state->last_nb_color != 0) {
    STWARN("unexpected change of nb_color");
  }

  if (state->last_nb_color != nb_color && state->last_nb_color == 0) {
    state->last_nb_color = nb_color;
    serena_set_final_color(state);
  }

  if (nb_color>0 && state->callback_coloring_finished != NULL) {
    (*state->callback_coloring_finished)(state);
    state->callback_coloring_finished = NULL;
  }

  buffer_put_u8(buffer, nb_color);
  buffer_put_u8(buffer, state->color);
  buffer_put_PRIORITY(buffer, state->priority);

  /* msg: priority information */
  int nb_max2_prio1 = 0;
  if (serena_has_all_neigh_prio(state))
    nb_max2_prio1 = count_max2_prio(MAX_PRIO1_SIZE, local_max2_prio1);

  buffer_put_u8(buffer, nb_max2_prio1);
  for (i=0; i<nb_max2_prio1; i++) {
    buffer_put_PRIORITY(buffer, local_max2_prio1[i].priority);
    buffer_put_data(buffer, local_max2_prio1[i].address, ADDRESS_SIZE);
  }

  int nb_max2_prio2 = 0;
  if (serena_has_all_neigh_prio1(state))
    nb_max2_prio2 = count_max2_prio(MAX_PRIO2_SIZE, local_max2_prio2);
  buffer_put_byte(buffer, nb_max2_prio2);
  for (i=0; i<nb_max2_prio2; i++) {
    buffer_put_PRIORITY(buffer, local_max2_prio2[i].priority);
    buffer_put_data(buffer, local_max2_prio2[i].address, ADDRESS_SIZE);
  }

  /* msg: color information */
  buffer_put_bitmap(buffer, &(state->color_bitmap1));
  buffer_put_bitmap(buffer, &(state->color_bitmap2));

  /* check buffer ok */
  if (buffer->status != HIPSENS_TRUE) {
    WARN(state->base, "packet buffer too small\n");
    return -1; /* buffer overflow, assuming it is the only possible error */
  }

  /* update back message size */
  int result = buffer->pos;
  ASSERT(result > 0);
  buffer->pos = msg_size_pos;
  buffer_put_u8(buffer, result - msg_content_start_pos);
  buffer->pos = result; /* restore buffer size */

  STLOG(DBGsrn, "msg-size=%d\n", result);
  
  return result;
}

/*---------------------------------------------------------------------------*/
/* the functions below are copied from nd_process_message */

void serena_process_message(serena_state_t* state,
			    byte* packet, int max_packet_size)
{
  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);
  serena_internal_process_message(state, &buffer);
}

int serena_generate_message(serena_state_t* state,
			    byte* packet, int max_packet_size)
{ 
  
  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);

  if (state->is_finished)
    return -1;

  if (!state->is_started) { /* XXX: clean up */
    /* NOTE: untested. If `is_started' false, one should call serena_start(...),
     not this function */
    new__serena_start(state);
    if (!state->is_started) {
      /* XXX: possible reasons? */
      STWARN("failed to start SERENA.\n");
      return -1;
    }
  } else {
    /* reschedule */
    state->next_msg_color_time = base_state_time_after_delay_jitter
      (state->base, state->config->msg_color_interval, 
       state->config->max_jitter_time);
  }

  int result = serena_internal_generate_message(state, &buffer);
  return result;
}

void serena_get_next_wakeup_condition(serena_state_t* state,
				      hipsens_wakeup_condition_t* condition)
{
  condition->wakeup_time = undefined_time;
  if (state->is_started && !state->is_finished)
    condition->wakeup_time_buffer = state->next_msg_color_time;
  else condition->wakeup_time_buffer = undefined_time;
}

int serena_notify_wakeup(serena_state_t* state, 
			 void* packet, int max_packet_size)
{
  /* Serena only handles packet generation */
  if (!state->is_started ||state->is_finished || packet == NULL)
    return 0;
  if (HIPSENS_TIME_COMPARE_LARGE_UNDEF
      (state->base->current_time, <, state->next_msg_color_time))
    return 0;

  /* generate a color message if time has come */
  int packet_size = serena_generate_message(state, (byte*)packet,
					    max_packet_size);
  if (packet_size <= 0) {
    STWARN("serena_notify_wakeup: msg color generation failed\n"); 
    return 0;
  } else {
#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
  if (new__stop_color_generation(state) != HIPSENS_TRUE) {
	  state->nb_sent_msg++;
	  state->size_sent_msg+=packet_size;
  }
#endif
	  return packet_size;
  }
}
  
//#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)
//}
//else{
//	  STWARN("serena_notify_wakeup: msg color generation on a stopped node, should return nothing\n"); 
//	  return 0;
//  }	 
//#endif
//}

/*---------------------------------------------------------------------------*/

#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL)

static int all_conflicting_nodes_colored
(serena_state_t* state,
 addr_priority_t local_max2_prio1[MAX_PRIO1_SIZE],
 addr_priority_t local_max2_prio2[MAX_PRIO2_SIZE],
 addr_priority_t* local_max_prio3)
{
  int i;
  for (i =0; i<MAX_PRIO1_SIZE; i++){
    if (!IS_PRIORITY_NONE(local_max2_prio1[i].priority)) {
      return HIPSENS_FALSE;
    }
  }
  
  for (i =0; i<MAX_PRIO2_SIZE; i++){
    if (!IS_PRIORITY_NONE(local_max2_prio2[i].priority))
      return HIPSENS_FALSE;
  }
  if (!IS_PRIORITY_NONE(local_max_prio3->priority))
	  return HIPSENS_FALSE;
  return HIPSENS_TRUE;
}

//#ifdef SWIG
static int should_send_msg(serena_state_t* state,
			   addr_priority_t new_max2_prio1[MAX_PRIO1_SIZE],
			   addr_priority_t new_max2_prio2[MAX_PRIO2_SIZE],
			   // addr_priority_t* new_max_prio3,
			   byte new_color)
{
  if(!state->is_started) 
    return HIPSENS_FALSE;
  if (new_color != state->color) 
    return HIPSENS_FALSE;
  if (!cmp_addr_priority(new_max2_prio1, state->max2_prio1))
    return HIPSENS_FALSE;
  if (!cmp_addr_priority(new_max2_prio2, state->max2_prio2))
    return HIPSENS_FALSE;
  
  return HIPSENS_TRUE;
}
//#endif

int stop_color_generation(serena_state_t *state)
{
  addr_priority_t local_max2_prio1[MAX_PRIO1_SIZE];
  addr_priority_t local_max2_prio2[MAX_PRIO2_SIZE];
  addr_priority_t local_max_prio3;
  
  if (!state->is_started) {
    STLOGA(DBGsrn, "serena-start (from stop color generation)\n");
    HIPSENS_FATAL("XXX: obsolete__serena_start(state); is no longer available");
  }
  
  if (state->nb_neighbor == 0){
    state->is_stopped = HIPSENS_TRUE; //a disconnected node should not generate color message
    if (state->color == COLOR_NONE){
      STLOGA(DBGsrn, "serena-select a color 0 (from stop color generation)\n"); 
      state->color = 0; 
    }
    return state->is_stopped; 
  }
  
  if (state->color == COLOR_NONE) {
    state->is_stopped = HIPSENS_FALSE;
    return state->is_stopped;
  }
  
  
  serena_compute_max_prio(state, local_max2_prio1,
			  local_max2_prio2, &local_max_prio3);
  
  
  hipsens_bool result = all_conflicting_nodes_colored (state, local_max2_prio1,
					local_max2_prio2, &local_max_prio3); // in an ideal environment, no need to generate a color message if 
  //all conflicting nodes have computed their colors
  if (result) 
	  state->is_stopped = HIPSENS_TRUE;
  
  return result;
}

int new__stop_color_generation(serena_state_t *state){
	//the >=second color message with empty tables can be not sent

	if (state->color == COLOR_NONE)
		return HIPSENS_FALSE;
	int i;
	//addr_priority_t local_max2_prio1[MAX_PRIO1_SIZE];
	//addr_priority_t local_max2_prio2[MAX_PRIO2_SIZE];
	//addr_priority_t local_max_prio3;
	//serena_compute_max_prio(state, local_max2_prio1,		  local_max2_prio2, &local_max_prio3);
	
	for (i = 0;i<MAX_PRIO1_SIZE;i++){
		if (!IS_PRIORITY_NONE(state->max2_prio1[i].priority))
			return HIPSENS_FALSE;
	}
	
	for (i = 0;i<MAX_PRIO2_SIZE;i++){
		if (!IS_PRIORITY_NONE(state->max2_prio2[i].priority))
			return HIPSENS_FALSE;
	}
	
#ifdef DBG_SERENA //XXX should be always true
	if (!IS_PRIORITY_NONE(state->dbg_max_prio3.priority))
		return HIPSENS_FALSE;
#endif
	
	if (state->first_empty_msg == HIPSENS_FALSE){
		state->first_empty_msg = HIPSENS_TRUE;
		return  HIPSENS_FALSE;
		}
	return HIPSENS_TRUE;
}
#endif /* defined(WITHOUT_LOSS) && defined(WITH_SIMUL)*/


/*---------------------------------------------------------------------------*/

#ifdef WITH_PRINTF

void dump_addr_priority(addr_priority_t* addr_priority_table, int nb)
{
  int j;
  for (j=0; j<nb; j++)
    if (!IS_PRIORITY_NONE(addr_priority_table[j].priority)) {
      if (j>0) printf(",");
      printf_address(&(addr_priority_table[j].address));
      printf(":" FMT_PRIORITY, GET_PRIORITY(addr_priority_table[j].priority));
    } else return;
}

void addr_priority_pywrite(outstream_t out,
			   addr_priority_t* addr_priority_table, int nb)
{
  int j;
  byte isFirst = 1;
  if (nb==1 && IS_PRIORITY_NONE(addr_priority_table[0].priority)) {
    FPRINTF(out, "None");
    return;
  }

  if (nb>1) FPRINTF(out, "[");
  for (j=0; j<nb; j++)
    if (!IS_PRIORITY_NONE(addr_priority_table[j].priority)) {
      if (isFirst) isFirst = 0;
      else FPRINTF(out, ",");
      FPRINTF(out, "(");
      address_pywrite(out, addr_priority_table[j].address);
      FPRINTF(out, "," FMT_PRIORITY ")", 
	      GET_PRIORITY(addr_priority_table[j].priority));
    } else break;
  if (nb>1) FPRINTF(out, "]");
}

void pydump_addr_priority(addr_priority_t* addr_priority_table, int nb)
{
  int j;
  byte isFirst = 1;
  if (nb==1 && IS_PRIORITY_NONE(addr_priority_table[0].priority)) {
    printf("None");
    return;
  }

  if (nb>1) printf("[");
  for (j=0; j<nb; j++)
    if (!IS_PRIORITY_NONE(addr_priority_table[j].priority)) {
      if (isFirst) isFirst = 0;
      else printf(",");
      printf("(");
      pyprintf_address(&(addr_priority_table[j].address));
      printf(","FMT_PRIORITY")", 
	     GET_PRIORITY(addr_priority_table[j].priority));
    } else break;
  if (nb>1) printf("]");
}

void dump_bitmap(bitmap_t* bitmap)
{
  int i;
  byte first = 1;
  for (i=0;i<NB_COLOR_MAX;i++)
    if (bitmap->content[BYTE_OF_BIT(i)] 
	& (unsigned int)MASK_OF_INDEX(INDEX_OF_BIT(i))) {
      if (first) first=0;
      else printf(",");
      printf("%d",i);
    }
}

/* XXX: remove */
void pydump_bitmap(bitmap_t* bitmap)
{
  printf("[");
  dump_bitmap(bitmap);
  printf("]");
}

void serena_dump(serena_state_t* state)
{

  printf("Serena ");
  if (!state->is_started) {
    printf("(not started)\n");
    return;
  } else printf(" (started)\n");
  printf("  color: ");
  if (state->color == COLOR_NONE)
    printf("none\n");
  else printf("%d\n", state->color);

  printf("  priority: " FMT_PRIORITY "\n", GET_PRIORITY(state->priority));
  printf("  neighbors (nb=%d):\n", state->nb_neighbor);

  printf("  bitmap1: ");
  dump_bitmap(&(state->color_bitmap1));
  printf("\n");

  printf("  bitmap2: ");
  dump_bitmap(&(state->color_bitmap2));
  printf("\n");

  printf("  bitmap3: ");
  dump_bitmap(&(state->color_bitmap3));
  printf("\n");


#ifdef DBG_SERENA
  printf("  max2_prio1: ");
  dump_addr_priority(state->max2_prio1, MAX_PRIO1_SIZE);
  printf("\n");

  printf("  max2_prio2: ");
  dump_addr_priority(state->max2_prio2, MAX_PRIO2_SIZE);
  printf("\n");

  printf("  dbg_max_prio3: ");
  dump_addr_priority(&(state->dbg_max_prio3), 1);
  printf("\n");

  printf("  dbg_max_addr_priority: ");
  dump_addr_priority(&(state->dbg_max_addr_priority), 1);
  printf("\n");
#endif

  int i;
  for (i=0; i<state->nb_neighbor; i++) {
    serena_neighbor_t* neighbor = &(state->neighbor_table[i]);
    printf("    ");
    printf_address(&(neighbor->address));
    if (neighbor->has_prio) {
      printf(" prio=" FMT_PRIORITY, GET_PRIORITY(neighbor->priority));
    }

    if (neighbor->has_prio1) {
      printf(" prio1=(");
      dump_addr_priority(neighbor->max2_prio1, MAX_PRIO1_SIZE);
      printf(")");      
    }

    if (neighbor->has_prio2) {
      printf(" prio2=(");
      dump_addr_priority(neighbor->max2_prio2, MAX_PRIO2_SIZE);
      printf(")");      
    }

    printf(" color=");
    if (neighbor->color == COLOR_NONE)
      printf("none\n");
    else printf("%d\n", neighbor->color); 
  }
}

#define bitmap_PYWRITE(out,bitmap) \
  BEGIN_MACRO bitset_pywrite(out, (bitmap).content, NB_COLOR_MAX); END_MACRO

void serena_pywrite(outstream_t out, serena_state_t* state)
{
  FPRINTF(out, "{ 'type':'serena',\n  'running':");
  if (!state->is_started) {
    FPRINTF(out, "False }");
    return;
  } else FPRINTF(out, "True");
  FPRINTF(out, ",\n 'priority choice': ");
  FPRINTF (out, "%d", state->config->priority_mode);
  FPRINTF(out, ",\n  'color':");
  if (state->color == COLOR_NONE)
    FPRINTF(out, "None");
  else FPRINTF(out, "%d", state->color);
  
  
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  
  FPRINTF(out, ",\n  'address':");
  address_pywrite(out, my_address);
      
  FPRINTF(out, ",\n  'priority':"FMT_PRIORITY, GET_PRIORITY(state->priority));
  FPRINTF(out, ",\n  'nbNeighbor':%d", state->nb_neighbor);

#if defined(WITHOUT_LOSS) && defined(WITH_SIMUL) 
  FPRINTF(out, ",\n  'nbSentMsgColor':%d", state->nb_sent_msg);
  FPRINTF(out, ",\n  'sizeSentMsgColor':%d", state->size_sent_msg);
#endif
  FPRINTF(out, ",\n  'bitmap1':");
  bitmap_PYWRITE(out, state->color_bitmap1);

  FPRINTF(out, ",\n  'bitmap2':");
  bitmap_PYWRITE(out, state->color_bitmap2);

  FPRINTF(out, ",\n  'bitmap3':");
  bitmap_PYWRITE(out, state->color_bitmap3);

#ifdef DBG_SERENA
  FPRINTF(out, ",\n  'max2_prio1':");
  addr_priority_pywrite(out, state->max2_prio1, MAX_PRIO1_SIZE);

  FPRINTF(out, ",\n  'max2_prio2':");
  addr_priority_pywrite(out, state->max2_prio2, MAX_PRIO2_SIZE);

  FPRINTF(out, ",\n  'dbg_max_prio3':");
  addr_priority_pywrite(out, &state->dbg_max_prio3, 1);

  FPRINTF(out, ",\n  'dbg_max_addr_priority':");
  addr_priority_pywrite(out, &state->dbg_max_addr_priority, 1);
#endif
  byte isFirst = HIPSENS_TRUE;
#ifdef IMPLICIT_COLORING_DETECTION
  FPRINTF (out, ",\n  'implicitColored': ");
  addr_priority_pywrite(out, state->last_implicit_colored, 
			MAX_IMPLICIT_COLORED);
  /*
  for (j=0;j<MAX_IMPLICIT_COLORED; j++) {
    if (isFirst) isFirst = HIPSENS_FALSE;
    else FPRINTF(out, ",");
    addr_priority_pywrite (out, &state->last_implicit_colored[j], 1);  
  }	 
  FPRINTF (out,"]");
  */
#endif /* IMPLICIT_COLORING_DETECTION */
#if defined(DBG_SERENA)
  FPRINTF(out, ",\n  'coloringTime': " FMT_HST, state->coloring_time);
#endif
  FPRINTF(out, ",\n 'lastNbColor':%d", state->last_nb_color);

  FPRINTF(out, ",\n  'neighborTable':[");
  int i;
  isFirst = 1;
  for (i=0; i<state->nb_neighbor; i++) {
    serena_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (isFirst) isFirst = 0;
    else FPRINTF(out, ",");
    FPRINTF(out, "\n    { 'address':");
    address_pywrite(out, neighbor->address);
    if (neighbor->has_prio) {
      FPRINTF(out, ", 'prio':"FMT_PRIORITY, GET_PRIORITY(neighbor->priority));
    } else FPRINTF(out, ", 'prio': None");

    if (neighbor->has_prio1) {
      FPRINTF(out, ", 'prio1':");
      addr_priority_pywrite(out, neighbor->max2_prio1, MAX_PRIO1_SIZE);
    } else FPRINTF(out, ", 'prio1': None");

    if (neighbor->has_prio2) {
      FPRINTF(out, ", 'prio2':");
      addr_priority_pywrite(out, neighbor->max2_prio2, MAX_PRIO2_SIZE);
    } else FPRINTF(out, ", 'prio2': None");

    FPRINTF(out, ", 'isChild':%d", neighbor->is_child);
    FPRINTF(out, ", 'isParent':%d", neighbor->is_parent);
    
    if (neighbor->has_sent_max_color) {
      FPRINTF(out, ", 'maxColor':%d", neighbor->child_max_color);
    } else FPRINTF(out, ", 'maxColor':None");

    FPRINTF(out, ", 'color':");
    if (neighbor->color == COLOR_NONE)
      FPRINTF(out, "None");
    else FPRINTF(out, "%d", neighbor->color); 
    FPRINTF(out, "}");
  }
  FPRINTF(out, "\n    ]\n  }\n");
}

void serena_pydump(serena_state_t* state)
{ 
#ifdef WITH_FILE_IO
  serena_pywrite(stdout, state); 
#else
  serena_pywrite(/*unused*/0, state); 
#endif /* WITH_FILE_IO */
}

#endif /* WITH_PRINTF */
