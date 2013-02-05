/*---------------------------------------------------------------------------
 *                    OPERA - EOLSR Neighbor Discovery
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

#include "hipsens-base.h"
#include "hipsens-macro.h"
#include "hipsens-external-api.h"
#include "hipsens-eond.h"


/*

EOLSR Neighbor Discovery, Hello message:

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Message Type  |  Message Size |       Originator Address      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Sequence Number      |    Vtime      | Energy Class  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Link Code     |Link Mess. Size| Neighbor Interface Address    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Neighbor Interface Address    | Neighbor Interface Address    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Neighbor Interface Address    | Neighbor Interface Address    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

<Message Size> does not include the field 'Message Type' nor 'Message Size'
<Link Mess. Size> does not include the field 'Link Code' nor 'Link Mess. Size'

 */

/*---------------------------------------------------------------------------*/

#define DEFAULT_HELLO_INTERVAL_SEC 2 /* seconds */
/** HOLD_TIME IS SET TO (3+1/2) times HELLO_INTERVAL */
#define DEFAULT_NEIGH_HOLD_TIME_SEC  \
  ((DEFAULT_HELLO_INTERVAL_SEC*7)/2) /* seconds */
#define MAX_HELLO_MSG_SIZE (+ 1 /* Message Type */			\
			    + 1 /* Message Size*/			\
			    + ADDRESS_SIZE /* Originator Address */	\
			    + 2 /* Message Sequence Number */		\
			    + 1 /* Vtime */				\
			    + 1 /* Energy Class */			\
			    + 2*1 /* 2xLink Code */			\
			    + 2*1 /* 2xLink Message Size */		\
			    + MAX_NEIGHBOR * ADDRESS_SIZE		\
			    /* n x (Neighbor Interface Address) */ )
// XXX: the above is not correct for simulations

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void eond_config_init_default(eond_config_t* config)
{
  config->link_quality_pwr_low  = 0;
  config->link_quality_pwr_high = 0;
#ifdef WITH_FRAME_TIME
  config->hello_interval = SEC_TO_FRAME_TIME(DEFAULT_HELLO_INTERVAL_SEC);
  config->neigh_hold_time = SEC_TO_FRAME_TIME(DEFAULT_NEIGH_HOLD_TIME_SEC);
  config->max_jitter_time = 0;
#else
  config->hello_interval = SEC_TO_HIPSENS_TIME(DEFAULT_HELLO_INTERVAL_SEC);
  config->neigh_hold_time = SEC_TO_HIPSENS_TIME(DEFAULT_NEIGH_HOLD_TIME_SEC);
  config->max_jitter_time = MILLISEC_TO_HIPSENS_TIME(DEFAULT_JITTER_MILLISEC);
#endif
}

void eond_state_reset(eond_state_t* state)
{
  state->hello_seq_num = 0;
  int i;
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    state->neighbor_table[i].state = EOND_None;
  }

  state->has_neighborhood_changed = HIPSENS_FALSE;
  
  IFSTAT( state->rejected_pwr_low_count = 0; );
  IFSTAT( state->rejected_pwr_high_count = 0; );

  state->next_msg_hello_time = undefined_time;
}

void eond_state_init(eond_state_t* state, base_state_t* base_state,
		     eond_config_t* config)
{
  state->base = base_state;
  state->config = config;
  STLOGA(DBGnd, "eond-init\n");
  state->observer_func = NULL;
  state->observer_data = NULL;

#ifdef WITH_NEIGH_OPT
  state->current_max_neighbor = 0;
  state->max_neighbor_limit = 2;
  state->neighbor_table = (eond_neighbor_t*)malloc(state->max_neighbor_limit
   						   * sizeof(eond_neighbor_t));
  memset(state->neighbor_table, 0, state->max_neighbor_limit
	 * sizeof(eond_neighbor_t));
#endif
  eond_state_reset(state);
}

int eond_estimate_nb_2hop(eond_state_t* state)
{
  int i, result = 0;

  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Sym)
      result += neighbor->nb_1hop;
  }
  return result;
}

#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
int eond_estimate_nb_3hop(eond_state_t* state)
{
  int i, result = 0;

  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Sym)
      result += neighbor->nb_2hop;
  }
  return result;
}
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */

hipsens_u16 eond_estimate_reception_energy_cost(eond_state_t* state)
{
  hipsens_u16 result = 0;
#ifdef WITH_ENERGY
  int i;
  ASSERT( state->base->energy_class < ENERGY_CLASS_NB );
  hipsens_u16* energy_coef = state->base->cfg_energy_coef[ENERGY_RECEPTION];

  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Sym) {
      ASSERT( neighbor->energy_class < ENERGY_CLASS_NB );
      result += energy_coef[neighbor->energy_class];
    }
  }
#endif /* WITH_ENERGY */
  return result;
}


static eond_neighbor_state_t compute_neighbor_state
(hipsens_time_t current_time, eond_neighbor_t* neighbor) 
{
  if (HIPSENS_TIME_COMPARE_NO_UNDEF(neighbor->sym_time,>=,current_time))
    return EOND_Sym;
  else if (HIPSENS_TIME_COMPARE_NO_UNDEF(neighbor->asym_time,>=,current_time))
    return EOND_Asym;
  else return EOND_None;
}

void eond_check_expiration(eond_state_t* state)
{
  int i=0;
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    eond_neighbor_state_t old_state = neighbor->state;
    if (old_state != EOND_None) {
      eond_neighbor_state_t new_state = 
	compute_neighbor_state(state->base->current_time, neighbor);
      if (new_state != old_state) {
	if (state->observer_func != NULL)
	  state->observer_func(state->observer_data, neighbor->address,
			       old_state, new_state, i);
	neighbor->state = new_state;
	state->has_neighborhood_changed = HIPSENS_TRUE;
      }
    }
  }
}

/*--------------------------------------------------*/

#if 0
/* There is a tension between doing an automated update of time in
   each submodule, and doing it manually in the caller.
   In the current state of EOLSR+SERENA, the caller (a general submodule),
   has better knowledge, and will call manually the updates, so this is
   not used
*/
void eond_update_time(eond_state_t* state, hipsens_time_t new_time)
{
  state->base->current_time = new_time;
  eond_check_expiration(state);
}
#endif 

/*---------------------------------------------------------------------------*/

eond_neighbor_t* eond_find_neighbor_by_address(eond_state_t*state,
					       address_t address) 
{
  int i;
  /* find the corresponding entry */
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state != EOND_None
	&& hipsens_address_equal(neighbor->address, address))
      return neighbor;
  }
  return NULL;
}

static void eond_process_hello_update_neighbor
(eond_state_t* state, address_t neighbor_address, hipsens_u8 power, 
 hipsens_u16 seq_num, hipsens_time_t validity_time,
 eond_neighbor_state_t my_state, hipsens_u8 nb_1hop, 
 hipsens_u8 nb_2hop_optional, hipsens_u8 energy_class)
{
  int i, entry_index = -1, free_entry_index = -1;
  hipsens_time_t current_time = state->base->current_time;

  int max_neighbor = EOND_MAX_NEIGHBOR(state);
#ifdef WITH_NEIGH_OPT
  if (max_neighbor < MAX_NEIGHBOR) {
    if (max_neighbor == state->max_neighbor_limit) {
      state->max_neighbor_limit *= 2;
      state->neighbor_table = (eond_neighbor_t*)realloc
	(state->neighbor_table, 
	 state->max_neighbor_limit* sizeof(eond_neighbor_t));
    }
    ASSERT( max_neighbor < state->max_neighbor_limit );
    state->neighbor_table[max_neighbor].state = EOND_None;
    max_neighbor++;
  }
#endif

  /* find the corresponding entry */
  for (i=0; i<max_neighbor; i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state != EOND_None
	&& hipsens_address_equal(neighbor->address, neighbor_address)) {
      entry_index = i;
      break;
    } else if (free_entry_index < 0 && neighbor->state == EOND_None) {
      free_entry_index = i;
    }
  }

  STLOGA(DBGnd, "update-neighbor-entry ");
  STWRITE(DBGnd, address_write, neighbor_address);
  STLOG(DBGnd, " power=%d seq-num=%d vtime=" FMT_HST 
	" my-state=%d nb_1hop=%d entry=%d",
	power, seq_num, validity_time, my_state, nb_1hop, entry_index);

  eond_neighbor_t* neighbor = NULL;
  if (entry_index < 0) {
    /* no entry existing */

    /* power greater than high threshold is required to accept a new neigh. */
    if (power < state->config->link_quality_pwr_high) {
      IFSTAT( state->rejected_pwr_high_count++; );
      STLOG(DBGnd, " power-lower-high\n");
      return;
    }

    if (free_entry_index < 0) {
      /* no room left in neighbor table, can't add new neighbor */
#ifndef IS_EMBEDDED
      STWARN("neighbor table full [%d]\n",MAX_NEIGHBOR);
#endif
      STLOG(DBGnd," neighbor-table-full\n");
      return;
    }

    entry_index = free_entry_index;
#ifdef WITH_NEIGH_OPT
    if (state->current_max_neighbor <= entry_index) {
      state->current_max_neighbor = entry_index+1;
      ASSERT( state->current_max_neighbor <= state->max_neighbor_limit );
    }
#endif
    STLOG(DBGnd, " new-entry=%d", entry_index);
    neighbor = &(state->neighbor_table[entry_index]);
    hipsens_address_copy(neighbor->address, neighbor_address);
    neighbor->sym_time = HIPSENS_TIME_EXPIRED(current_time);
    neighbor->asym_time = HIPSENS_TIME_EXPIRED(current_time);
#ifdef WITH_STAT
    neighbor->recv_count = 0;
#endif
#ifdef WITH_INPACKET_LINK_STAT
    neighbor->link_stat = 0;
#endif /* WITH_INPACKET_LINK_STAT */
  } else {
    neighbor = &(state->neighbor_table[entry_index]);
  }

#ifdef WITH_STAT
  if (neighbor->recv_count < 0xff)
    neighbor->recv_count++;
  neighbor->last_power = power;
#endif

  if (power < state->config->link_quality_pwr_low) {
    /* the power is lower than the low threshold = removed link */
    IFSTAT( state->rejected_pwr_low_count++; );
    STLOG(DBGnd, " power-lower-low\n");
    neighbor->sym_time = HIPSENS_TIME_EXPIRED(current_time);
    neighbor->asym_time = HIPSENS_TIME_EXPIRED(current_time);
#ifndef WITH_DELAYED_STATE_UPDATE
    if (state->observer_func != NULL)
      state->observer_func(state->observer_data, neighbor->address,
			   neighbor->state, EOND_Nonde, entry_index);
    neighbor->state = EOND_None;
    state->has_neighborhood_changed = HIPSENS_TRUE;
#endif
    return;
  }

  neighbor->asym_time = HIPSENS_TIME_ADD(state->base->current_time,
					 validity_time);
  if (my_state == EOND_Asym || my_state == EOND_Sym)
    neighbor->sym_time = HIPSENS_TIME_ADD(state->base->current_time,
					  validity_time);
  neighbor->nb_1hop = nb_1hop;
#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
  neighbor->nb_2hop = nb_2hop_optional;
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */

#ifdef WITH_ENERGY
  if (energy_class >= ENERGY_CLASS_NB) {
    STWARN("hello energy-class is too high: %d\n", energy_class);
    energy_class = ENERGY_CLASS_NB-1;
  }
  neighbor->energy_class = energy_class;
#endif

  eond_neighbor_state_t old_state = neighbor->state;
  neighbor->state = compute_neighbor_state(state->base->current_time, neighbor);
#ifdef WITH_DELAYED_STATE_UPDATE
  if (old_state > neighbor->state)
    neighbor->state = old_state; /* revert state */
#endif
  if (neighbor->state != old_state) {
    STLOG(DBGnd, " state-changed:%d->%d\n", old_state, neighbor->state);
    if (state->observer_func != NULL)
      state->observer_func(state->observer_data, neighbor->address,
			   old_state, neighbor->state, entry_index);

    state->has_neighborhood_changed = HIPSENS_TRUE;
  } else {
    STLOG(DBGnd, "\n");
  }
}


int eond_process_hello_message(eond_state_t* state, 
			       byte* packet, int packet_size, hipsens_u8 power)
{
  buffer_t buffer;
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  STLOGA(DBGnd, "eond-process-hello ");
  buffer_init(&buffer, packet, packet_size);

  /* --- parse the header */
  hipsens_u8 message_type = buffer_get_u8(&buffer);
  hipsens_u8 message_size = buffer_get_u8(&buffer);

  if (message_type != HIPSENS_MSG_HELLO) {
    STWARN("bad message type, hello expected, type=%d\n", message_type);
    return -1;
  }
  if (message_size > buffer.size - buffer.pos) {
    STWARN("bad message size, buffer remain.=%d, msg size=%d\n", 
	   buffer.size-buffer.pos, message_size);
    return -1;
  }
  int result = buffer.pos + message_size;
  buffer.size = result;

  address_t neighbor_address;
  buffer_get_ADDRESS(&buffer, neighbor_address);
  hipsens_u16 seq_num = buffer_get_u16(&buffer);
  hipsens_time_t validity_time = vtime_to_hipsens_time(buffer_get_u8(&buffer));
  hipsens_u8 energy_class = buffer_get_u8(&buffer);

  STWRITE(DBGnd, address_write, neighbor_address);
  //STLOG(DBGnd || DBGmsgdat, " [");
  //STWRITE(DBGnd || DBGmsgdat, data_write, packet, packet_size);
  //STLOG(DBGnd || DBGmsgdat, "]");
  STLOG(DBGnd, "\n");

  if (hipsens_address_equal(neighbor_address, my_address)) {
    STLOG(DBGnd, "- ignoring packet from myself\n");
    return result;
  }

  hipsens_u8 nb_2hop_maybe = 0;
#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
  nb_2hop_maybe = buffer_get_u8(&buffer);
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */

  /* --- parse the links */
  eond_neighbor_state_t my_status = EOND_None;
  hipsens_u8 nb_1hop_sym = 0;

  while (buffer.pos != buffer.size) {
    hipsens_u8 link_code = buffer_get_u8(&buffer);
    hipsens_u8 link_message_size = buffer_get_u8(&buffer);
    if (link_message_size > buffer.size - buffer.pos) {
      STWARN("bad link message size, buffer remain.=%d, msg size=%d\n", 
	     buffer.size-buffer.pos, message_size);
      return -1;
    }
    if (link_code != EOND_Sym && link_code != EOND_Asym) {
      buffer.pos += link_message_size;
      continue;
    }
    buffer_t link_buffer;
    buffer_init(&link_buffer, buffer.data + buffer.pos, link_message_size);
    buffer.pos += link_message_size;

    hipsens_u8 nb_1hop_block = 0;
    while (link_buffer.pos != link_buffer.size) {
      address_t two_hop_address;
      buffer_get_ADDRESS(&link_buffer, two_hop_address);
      nb_1hop_block++;
      if (hipsens_address_equal(my_address, two_hop_address)) {
	ASSERT( my_status == EOND_None );
	my_status = (eond_neighbor_state_t) link_code;
      }
    }

    if ((eond_neighbor_state_t) link_code == EOND_Sym)
      nb_1hop_sym = nb_1hop_block;

    if (link_buffer.status != HIPSENS_TRUE) {
      STWARN("parse error in link-hello message content\n");
      return -1;
    }
  }

  if (buffer.status != HIPSENS_TRUE) {
    STWARN("parse error in hello message content\n");
    return -1;
  }    
  STLOG(DBGnd, "\n");
  
  eond_process_hello_update_neighbor(state, neighbor_address, power, seq_num,
				     validity_time, my_status,
				     nb_1hop_sym, nb_2hop_maybe, energy_class);

  return result;
}
  
/*---------------------------------------------------------------------------*/

static void eond_update_next_hello_time(eond_state_t* state)
{
  state->next_msg_hello_time = base_state_time_after_delay_jitter
    (state->base, state->config->hello_interval, 
     state->config->max_jitter_time);
}

void eond_start(eond_state_t* state)
{ eond_update_next_hello_time(state); }

#ifndef WITH_SIMUL
//extern char gBroadcastTableOverflow; // XXX: remove this is a hack
char gBroadcastTableOverflow = 0; // XXX: remove this is a hack
#endif

int  eond_generate_hello_message(eond_state_t* state, 
				 byte* packet, int max_packet_size)
{  
  STLOGA(DBGnd || DBGmsg, "eond-generate-hello\n");
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  eond_update_next_hello_time(state);
  eond_check_expiration(state);

  buffer_t buffer;
  buffer_init(&buffer, packet, max_packet_size);

  /* --- generate the header */
  buffer_put_u8(&buffer, HIPSENS_MSG_HELLO);
  int msg_size_pos = buffer.pos;
  buffer_put_u8(&buffer, 0); /* size, filed later */
  int msg_content_start_pos = buffer.pos;
  buffer_put_ADDRESS(&buffer, my_address);
  buffer_put_u16(&buffer, state->hello_seq_num);
  state->hello_seq_num ++;
  buffer_put_u8(&buffer, hipsens_time_to_vtime(state->config->neigh_hold_time));
#ifdef WITH_ENERGY
  buffer_put_u8(&buffer, state->base->energy_class);
#else
  buffer_put_u8(&buffer, 0);
#endif /* WITH_ENERGY */

#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
  buffer_put_u8(&buffer, eond_estimate_nb_2hop(state));
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */

  
  /* --- generate the link message content */
  int i;
  /* count sym/asym neighbors */
  int count_sym = 0;
  int count_asym = 0;
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Sym)
      count_sym++;
    else if (neighbor->state == EOND_Asym)
      count_asym++;
  }
  int count_total = count_sym + count_asym;

  /* put sym neighbors */
  if (count_sym > 0) {
    buffer_put_u8(&buffer, EOND_Sym);
    buffer_put_u8(&buffer, count_sym*ADDRESS_SIZE);
    for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
      eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
      if (neighbor->state == EOND_Sym) {
	buffer_put_ADDRESS(&buffer, neighbor->address);
	DBG(count_sym--);
      }
    }
  }

  /* put asym neighbors */
  if (count_asym > 0) {
    buffer_put_u8(&buffer, EOND_Asym);
    buffer_put_u8(&buffer, count_asym*ADDRESS_SIZE);
    for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
      eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
      if (neighbor->state == EOND_Asym) {
	buffer_put_ADDRESS(&buffer, neighbor->address);
	DBG(count_asym--);
      }
    }
  }
  DBG( ASSERT(count_sym == 0 && count_asym == 0) );

#ifdef WITH_OPERA_SYSTEM_INFO
  /* put state information in Hello message */
  buffer_put_u8(&buffer, EOND_SYSTEM_INFO);
  buffer_put_u8(&buffer, 4);
  if (state->base->error_count > 0)
    state->base->sys_info |= OPERA_SYSTEM_INFO_HAS_ERROR;
  if (state->base->warning_count > 0)
    state->base->sys_info |= OPERA_SYSTEM_INFO_HAS_WARNING;
#ifndef WITH_SIMUL
  if (gBroadcastTableOverflow) 
    state->base->sys_info |= OPERA_SYSTEM_INFO_HAS_BROADCAST_OVERFLOW;
  else state->base->sys_info &= ~OPERA_SYSTEM_INFO_HAS_BROADCAST_OVERFLOW;
#endif /* ndef WITH_SIMUL */  
  buffer_put_u16(&buffer, state->base->sys_info);
  buffer_put_u8(&buffer, state->base->sys_info_stability);
  buffer_put_u8(&buffer, state->base->sys_info_color);
  
#ifdef WITH_OPERA_INPACKET_MSG
  if (state->base->int_info != 0) {
    buffer_put_u8(&buffer, EOND_INPACKET_MSG);
    buffer_put_u8(&buffer, 2+sizeof(state->base->str_info));
    buffer_put_u16(&buffer, state->base->int_info);
    buffer_put_data(&buffer, (byte*)(state->base->str_info), 
                    sizeof(state->base->str_info));
  }
#endif /* WITH_OPERA_INPACKET_MSG */  
#endif /* WITH_OPERA_SYSTEM_INFO */

#ifdef WITH_INPACKET_LINK_STAT
  buffer_put_u8(&buffer, EOND_INPACKET_LINK_STAT);
  buffer_put_u8(&buffer, count_total*2); /* sizeof link_stat */
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Sym) {
      buffer_put_u16(&buffer, neighbor->link_stat);
      DBG(count_total--);
    }
  }
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++) {
    eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
    if (neighbor->state == EOND_Asym) {
      buffer_put_u16(&buffer, neighbor->link_stat);
      DBG(count_total--);
    }
  }
  DBG( ASSERT(count_total == 0) );
#endif /* WITH_INPACKET_LINK_STAT */
  
  /* --- update size field */
  if (buffer.status != HIPSENS_TRUE) {
    WARN(state->base, "packet buffer too small\n");
    return -1; /* buffer overflow */
  }

  int result = buffer.pos;
  ASSERT(result > 0);
  buffer.pos = msg_size_pos;
  buffer_put_u8(&buffer, result - msg_content_start_pos);

  return result;
}

/*---------------------------------------------------------------------------*/

void eond_get_next_wakeup_condition(eond_state_t* state,
				    hipsens_wakeup_condition_t* condition)
{
  condition->wakeup_time = undefined_time;
  condition->wakeup_time_buffer = state->next_msg_hello_time;
}

int eond_notify_wakeup(eond_state_t* state, void* packet, int max_packet_size)
{
  if (packet == NULL)
    return 0;
  if (HIPSENS_TIME_COMPARE_LARGE_UNDEF
      (state->base->current_time, <, state->next_msg_hello_time))
    return 0;

  /* generate a hello message if time has come */
  eond_check_expiration(state);
  int packet_size = eond_generate_hello_message(state, (byte*)packet, 
						max_packet_size);
  if (packet_size <= 0) {
    STWARN("eond_notify_wakeup: msg hello generation failed\n"); 
    return 0;
  } else return packet_size;
}

/*---------------------------------------------------------------------------*/

#ifdef WITH_NEIGH_OPT
void eond_close(eond_state_t* state)
{
  ASSERT(state->neighbor_table != NULL);
  free(state->neighbor_table);
  state->neighbor_table = NULL;
}
#endif

#ifdef WITH_PRINTF
void eond_pywrite(outstream_t out, eond_state_t* state)
{
  FPRINTF(out, "{ 'type':'eond-state',\n  'address':");
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  address_pywrite(out, my_address);
  FPRINTF(out, ",\n  'time':" FMT_HST, state->base->current_time);
#ifdef WITH_ENERGY
  FPRINTF(out, "\n, 'energy':%d", state->base->energy_class);
#endif

  FPRINTF(out, "\n, 'nextMsgTime':" FMT_HST, state->next_msg_hello_time);

  FPRINTF(out, ",\n  'seq':%d,\n  'neighborTable':{", 
	  state->hello_seq_num);
  int i;
  hipsens_bool isFirst = HIPSENS_TRUE;
  for (i=0; i<EOND_MAX_NEIGHBOR(state); i++)
    if (state->neighbor_table[i].state != EOND_None) {
      if (isFirst) isFirst = HIPSENS_FALSE;
      else FPRINTF(out, ",");

      eond_neighbor_t* neighbor = &(state->neighbor_table[i]);
      FPRINTF(out, "\n    %d: {'address':", i);
      address_pywrite(out, neighbor->address);

      FPRINTF(out, ", 'state':");
      if (neighbor->state == EOND_Sym)
	FPRINTF(out, "'sym'");
      else if (neighbor->state == EOND_Asym)
	FPRINTF(out, "'asym'");
      else FPRINTF(out, "'unknown'");
      
      FPRINTF(out, ", 'asymTime':" FMT_HST ", 'symTime':" FMT_HST, 
	      neighbor->asym_time, neighbor->sym_time);
      FPRINTF(out, ", 'nb_1hop':%d", neighbor->nb_1hop);
#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
      FPRINTF(out, ", 'nb_2hop':%d", neighbor->nb_2hop);
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */
#ifdef WITH_ENERGY
      FPRINTF(out, ", 'energyClass':%d", neighbor->energy_class);
#endif
#ifdef WITH_STAT
      FPRINTF(out, ", 'lastPower':%d", neighbor->last_power);
      FPRINTF(out, ", 'recvCount':%d", neighbor->recv_count);
#endif
      FPRINTF(out, "}");
    }
  FPRINTF(out, "}");
#ifdef WITH_STAT
  FPRINTF(out, ",\n  'stat': { 'rejected-low': %d, 'rejected-high': %d }\n",
	  state->rejected_pwr_low_count, state->rejected_pwr_high_count);
#endif
  FPRINTF(out,"}\n");
}

#endif /* WITH_PRINTF */

/*---------------------------------------------------------------------------*/
