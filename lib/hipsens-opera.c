/*---------------------------------------------------------------------------
 *                        OPERA - Main OPERA module
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


/*---------------------------------------------------------------------------
 * Internal API below
 *---------------------------------------------------------------------------*/


/**
 * The external caller should call this function after reaching the
 * date given by a previous opera_get_next_event_time(state)
 *
 * Notice that calling prematurely this function will generally
 * only result into the function doing nothing.
 */
//void internal_opera_handle_event_wakeup
//(opera_state_t* state, hipsens_time_t current_time);

/**
 * The external caller should call this function after reaching the
 * date given by a previous opera_get_next_event_time(state)
 *
 * Notice that calling prematurely this function will generally
 * only result into the function doing nothing.
 * Notice that currently, when this function is called the node
 * will attempt to generate a packet
 * When used in OCARI, `opaque_buffer' is pointer to PACKET_BUFFER
 */
//void internal_opera_handle_event_wakeup_with_buffer
//(opera_state_t* state, hipsens_time_t current_time, void* opaque_buffer);

/**
 * The external caller should call this function every time a packet is
 * received.
 * Notice that during the processing of the packet, OPERA may, in turn, 
 * generate one or two packets (currently) by calling send_packet
 * - `rssi' corresponds to the link quality estimated for the received packet
 * (RSSI <http://en.wwikipedia.org/wiki/Received_signal_strength_indication>)
 * - `packet_data' is the payload of the received packets (Frame Payload)
 * after return of this function, the packet_data is not used by opera
 * (it may be freed)
 * - for OCARI, the `rssi' implementation needs to be defined
 */
//void internal_opera_handle_event_packet
//(opera_state_t* state, hipsens_time_t current_time,
// byte* packet_data, int packet_size, hipsens_u8 rssi);


//void internal_opera_init(opera_state_t* state,
//			 opera_config_t* config, 
//			 hipsens_time_t current_time,
//			 void* opaque_extra_info);

//void internal_opera_start(opera_state_t* state, hipsens_time_t current_time);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void opera_config_init_default(opera_config_t* config)
{
  eond_config_init_default(&config->eond_config);
  eostc_config_init_default(&config->eostc_config);
  serena_config_init_default(&config->serena_config);

  config->eond_start_delay = 0;
  config->eostc_start_delay = 0;
}

void opera_update_wakeup_condition(opera_state_t* state);

static void opera_internal_init(opera_state_t* state)
{
  state->flag_use_colored_slots = HIPSENS_FALSE;
  state->flag_run_serena = HIPSENS_FALSE;
  state->flag_new_color_trigger = HIPSENS_FALSE;
  state->is_colored_tree_root = HIPSENS_FALSE;
  state->should_start_serena = HIPSENS_FALSE;
  state->cycle_transmit_count = 0;
  state->is_blocked = HIPSENS_FALSE;
  state->should_be_reset = HIPSENS_FALSE;
  state->should_inc_colored_tree_seq = HIPSENS_FALSE;

#ifdef WITH_OPERA_ADDRESS_FILTER
  state->filter_nb_address = 0;
  state->filter_mode = FilterNone;
#endif
}

void opera_direct_init(opera_state_t* state, opera_config_t* config,
		       hipsens_time_t current_time, void* opaque_extra_info)
{
  state->base = &(state->base_state);
  state->config = config;
  state->base->opaque_opera = (void*)state;
  base_state_init(&(state->base_state), opaque_extra_info);
  eond_state_init(&state->eond_state,
		  &state->base_state, &config->eond_config);
  eostc_state_init(&state->eostc_state, &state->eond_state,
		   &config->eostc_config);
  new__serena_state_init(&(state->serena_state), &(state->base_state),
			 &config->serena_config);
  state->base_state.current_time = current_time;
  wakeup_condition_init(&state->wakeup_condition);

  opera_internal_init(state);
}

void opera_direct_start(opera_state_t* state, hipsens_time_t current_time)
{
  state->base_state.current_time = current_time;
  state->base_state.opaque_opera = state;

  /* reset EOND / EOSTC / SERENA */
  eond_state_reset(&state->eond_state);
  eostc_state_reset(&state->eostc_state);
  new__serena_state_init(&state->serena_state, &state->base_state,
			 &state->config->serena_config);
  opera_internal_init(state);

  /* start EOND */
  eond_start(&state->eond_state);

  opera_update_wakeup_condition(state);
}

void opera_start_eostc(opera_state_t* state, hipsens_bool is_for_serena)
{ 
  state->is_colored_tree_root = state->is_colored_tree_root || is_for_serena;
  eostc_start(&state->eostc_state, is_for_serena); 
}

/**
 * Returns the next time the OPERA node should be woken up by
 * the external caller (with the function opera_handle_event_wake_up)
 * on one of the conditions (with buffer or without buffer for sending
 * one packet).
 * 'minimum_time' indicates what is the minimum time of a wake up, and
 * then all returned times will be greated or equal to this value
 * (OTHO if it is `undefined_time', it is ignored)
 */
static void internal_opera_get_next_wakeup_condition
(opera_state_t* state, hipsens_wakeup_condition_t* condition)
{
  hipsens_wakeup_condition_t additional_condition;
  wakeup_condition_init(condition);

  int eond_delay = state->config->eond_start_delay;
  if (eond_delay == 0 || state->base_state.current_time >= eond_delay) {
    eond_get_next_wakeup_condition(&state->eond_state, &additional_condition);
    wakeup_condition_update(condition, &additional_condition);
  }

  int eostc_delay = state->config->eostc_start_delay;
  if (eostc_delay == 0 || state->base_state.current_time >= eostc_delay) {
    eostc_get_next_wakeup_condition(&state->eostc_state, &additional_condition);
    wakeup_condition_update(condition, &additional_condition);
  }

  if (state->serena_state.is_started /* XXX!! finished */) {
    serena_get_next_wakeup_condition(&state->serena_state, 
				     &additional_condition);
    wakeup_condition_update(condition, &additional_condition);
  }
}


static hipsens_bool is_address_accepted(opera_state_t* state,
					address_t address)
{
#ifdef WITH_OPERA_ADDRESS_FILTER
  if (state->filter_mode == FilterNone)
    return HIPSENS_TRUE;

  if (state->filter_mode == FilterReject) {
    int i;
    for (i=0; i<state->filter_nb_address; i++) {
      if (hipsens_address_equal(address, state->address_filter[i])) 
	return HIPSENS_FALSE;
    }
    return HIPSENS_TRUE;
  } else if (state->filter_mode == FilterOnlyAccept) {
    int i;
    for (i=0; i<state->filter_nb_address; i++) {
      if (hipsens_address_equal(address, state->address_filter[i])) 
	return HIPSENS_TRUE;
    }
    return HIPSENS_FALSE;
  }

#endif
  return HIPSENS_TRUE;
}


static void opera_process_packet(opera_state_t* state, 
				 byte* initial_packet_data,
				 int initial_packet_size, hipsens_u8 power)
{
  byte* packet_data = initial_packet_data;
  int packet_size = initial_packet_size;

  if (state->config->eond_start_delay >= 2000) { /* XXX!!! */
    HIPSENS_GET_MY_ADDRESS(state->base, my_address);
    state->config->eond_start_delay = 
      state->base->current_time 
        + (my_address[0]+my_address[1]) % 4; /* XXX!! hack for autostart */
  }
  
  STLOG(DBGsimmsg,",'event':'process-packet','time':"
	FMT_HST, state->base->current_time);

#define MSG_SHORT_HEADER_SIZE (2)
  while (packet_size >= MSG_SHORT_HEADER_SIZE) {
    byte message_type = packet_data[0];
    byte message_size = packet_data[1];
    byte header_and_message_size = message_size + MSG_SHORT_HEADER_SIZE;

#ifdef WITH_OPERA_ADDRESS_FILTER
    if (packet_size >= 4) {
      buffer_t buffer;
      buffer_init(&buffer, packet_data+2, packet_size-2);
      address_t sender_address; 
      buffer_get_ADDRESS(&buffer, sender_address);
      if (!is_address_accepted(state, sender_address))
	return;
    }
#endif

    if (header_and_message_size > packet_size) {
      STWARN("bad message size, packet remain.=%d, msg size=%d\n, [%d]",
	     packet_size, header_and_message_size, initial_packet_size);
		  return;
    }

    if (message_type == HIPSENS_MSG_HELLO) {
      /* process a message hello */
      eond_process_hello_message(&(state->eond_state), packet_data, 
				 header_and_message_size, power);
    } else if (message_type == HIPSENS_MSG_COLOR) {
      /* process a color message */
      serena_state_t* serena = &(state->serena_state);
      serena_process_message(serena, packet_data, header_and_message_size);
    } else if (message_type == HIPSENS_MSG_STC) {
      /* process a stc message */
      eostc_process_stc_message(&(state->eostc_state), packet_data,
				header_and_message_size);
    } else if (message_type == HIPSENS_MSG_TREE_STATUS) {
      /* process a tree status message */
      eostc_process_tree_status_message(&(state->eostc_state), packet_data,
					header_and_message_size);
    } else {
      STWARN("unknown message type=%d\n", message_type);
    }

    packet_data += header_and_message_size;
    packet_size -= header_and_message_size;
    break; // XXX!! remove
  }
  if (packet_size > 0) {
    STWARN("unused bytes in packet nb=%d", packet_size);
  }
}

#define FILL_TRANSMIT_BUFFER(buffer, size, next_hop) \
  BEGIN_MACRO \
    (buffer)->payload_size = (size); \
    hipsens_address_copy((buffer)->next_hop_address, broadcast_address); \
    (buffer)->traffic_type = MAC_unconstrained_uncolored_traffic; \
  END_MACRO

/*
 * Called whenever there is a scheduling event
 */
static hipsens_bool opera_handle_event(opera_state_t* state, 
				       hipsens_time_t current_time,
				       transmit_buffer_t* transmit_buffer)
{
  state->base_state.current_time = current_time;
  STLOGA(DBGnode, "handle-event opaque-buffer=%d\n", (transmit_buffer!=NULL));

  hipsens_u8* packet = NULL;
  int max_packet_size = 0;

  if (transmit_buffer != NULL) {
    transmit_buffer->payload_size = 0; // [CA] moved here: fix from MHB
    packet = transmit_buffer->payload;
    max_packet_size = transmit_buffer->max_payload_size;
  } 

  int packet_size = 0;
  int eond_delay = state->config->eond_start_delay;
  if (eond_delay == 0 || state->base_state.current_time >= eond_delay) {
    packet_size = eond_notify_wakeup(&(state->eond_state), 
				     packet, max_packet_size);
  }
  if (packet_size > 0) {
    STLOG(DBGsimmsg,",'event':'generate-packet', 'type':'eond', 'time':"
	  FMT_HST, current_time);
    FILL_TRANSMIT_BUFFER(transmit_buffer, packet_size, broadcast_address);
    packet = NULL; /* reset for the next */
    max_packet_size = 0;
  }

  int eostc_delay = state->config->eostc_start_delay;
  if (eostc_delay == 0 || state->base_state.current_time >= eostc_delay) {
    packet_size = eostc_notify_wakeup(&(state->eostc_state),
				      packet, max_packet_size);
  }
  if (packet_size > 0) {
    STLOG(DBGsimmsg,",'event':'generate-packet', 'type':'eostc', 'time':"
	  FMT_HST, current_time);
    FILL_TRANSMIT_BUFFER(transmit_buffer, packet_size, broadcast_address);
    packet = NULL; /* reset for the next */
    max_packet_size = 0;
  }

  packet_size = serena_notify_wakeup(&(state->serena_state),
				     packet, max_packet_size);
  if (packet_size > 0) {
    STLOG(DBGsimmsg,",'event':'generate-packet', 'type':'serena', 'time':"
	  FMT_HST, current_time);
    FILL_TRANSMIT_BUFFER(transmit_buffer, packet_size, broadcast_address);
    packet = NULL; /* reset just in case */
    max_packet_size = 0;
  }
  opera_update_wakeup_condition(state);

  if (transmit_buffer != NULL && transmit_buffer->payload_size>0) {
    state->cycle_transmit_count ++;
    STLOG(DBGsimmsg>1, ", 'content':");
    STWRITE((DBGsimmsg>1), data_pywrite, transmit_buffer->payload, 
	    transmit_buffer->payload_size);
  }

  return (state->wakeup_condition.wakeup_time_buffer
	  == state->base->current_time);
}

void opera_direct_event_wakeup
(opera_state_t* state, hipsens_time_t current_time)
{ opera_handle_event(state, current_time, NULL); }

void opera_direct_event_wakeup_with_buffer
(opera_state_t* state, hipsens_time_t current_time, 
 transmit_buffer_t* transmit_buffer)
{ opera_handle_event(state, current_time, transmit_buffer); }

void opera_direct_event_packet
(opera_state_t* state, hipsens_time_t current_time,
 byte* packet_data, int packet_size, hipsens_u8 rssi)
{ 
  state->base_state.current_time = current_time;
  STLOGA(DBGnode, "handle-event packet-size=%d\n", packet_size);
  opera_process_packet(state, packet_data, packet_size, rssi);
}

/**
 * Find the next hop for a given destination address,
 * result is stored in result_next_hop_address.
 *
 * Returns HIPSENS_TRUE (1) if an address as been found or HIPSENS_FALSE (0)
 * otherwise
 */
hipsens_bool opera_get_next_hop(opera_state_t* state,
				address_t destination_address,
				address_t result_next_hop_address)
{
  /* if it is my address, return itself */
  HIPSENS_GET_MY_ADDRESS(state->base, my_address);
  if (hipsens_address_equal(my_address, destination_address)) {
    hipsens_address_copy(result_next_hop_address, my_address);
    return HIPSENS_TRUE;
  }

  /* search for an immediate neighbor */
  eond_neighbor_t* neighbor = eond_find_neighbor_by_address
    (&state->eond_state, destination_address);
  if (neighbor != NULL) {
    hipsens_address_copy(result_next_hop_address, destination_address);
    return HIPSENS_TRUE;
  }

  /* search for a tree root (with parent) */
  eostc_tree_t* tree = eond_find_tree_by_address
    (&state->eostc_state, destination_address, HIPSENS_TRUE);
  if (tree == NULL) /* XXX: change api not to do it twice */
    tree = eond_find_tree_by_address
      (&state->eostc_state, destination_address, HIPSENS_FALSE);
  if (tree != NULL) {
    hipsens_address_copy(result_next_hop_address, tree->parent_address);
    return HIPSENS_TRUE;
  }
  
  /* not found */
   hipsens_address_copy(result_next_hop_address, undefined_address);
  return HIPSENS_FALSE;
}


/*---------------------------------------------------------------------------*/

void opera_init(opera_state_t* state, opera_config_t* config,
		void* opaque_extra_info)
{ 
  opera_direct_init(state, config, 0, opaque_extra_info);
  state->base->current_time = 0;
}

static void opera_check_serena_start(opera_state_t* state)
{
  if (state->should_start_serena) {
    state->should_start_serena = HIPSENS_FALSE;
    
    if (state->is_colored_tree_root) {
      STWARN("serena_start flag set on a tree root,\n");
      return;
    }
    
    new__serena_start(&state->serena_state); /* XXX: maybe more checks */
  }
}

void opera_start(opera_state_t* state)
{
  opera_direct_start(state, 0);
}

void opera_inc_colored_tree_seq(opera_state_t* state);
void opera_inc_colored_tree_seq(opera_state_t* state)
{
  // note that the check for my_tree not NULL and colored is also performed
  // by the serial command triggering a call to this function
  if (state->eostc_state.my_tree == NULL
	|| !IS_FOR_SERENA(*state->eostc_state.my_tree)) {
    STWARN("incrementing colored tree seq num, but not a colored tree root");
    return;
  }
  
  state->should_inc_colored_tree_seq = HIPSENS_FALSE;
  state->serena_state.is_finished = HIPSENS_TRUE;
  clear_serena_tree(state->eostc_state.my_tree->serena_info);
  state->eostc_state.my_tree->serena_info->stability_time = HIPSENS_TIME_ADD
      (state->base_state.current_time, state->eostc_state.config->stability_time);
  state->eostc_state.my_tree->serena_info->tree_seq_num ++;
  
#warning "[CA] XXX: check if it is ok to call MaCARIColoringModeOnRequest(FALSE), even if it was already FALSE"  
  // XXX: MaCARIMaxColorRequest(NO_COLOR) should also be called
  hipsens_api_should_run_serena(state->base_state.opaque_extra_info, 
				HIPSENS_FALSE);
}

hipsens_bool opera_event_new_cycle(opera_state_t* state, byte unused)
{
  if (state->is_blocked)
    return HIPSENS_FALSE;
  if (state->should_be_reset) {
    opera_init(state, state->config, state->base_state.opaque_extra_info);
    ASSERT( !state->should_be_reset );
    return HIPSENS_FALSE;
  }
  if (state->should_inc_colored_tree_seq)
    opera_inc_colored_tree_seq(state);

  state->base->current_time++; /* update clock */
  STLOG(DBGsimmsg, ",'time':" FMT_HST, state->base->current_time);

  state->cycle_transmit_count = 0;
  opera_check_serena_start(state);

  /* wake up anyway */
  if (state->wakeup_condition.wakeup_time == undefined_time
      || state->wakeup_condition.wakeup_time == state->base->current_time) {
    opera_handle_event(state, state->base->current_time, NULL);
    opera_update_wakeup_condition(state);
  }

  int rate_limit = state->config->transmit_rate_limit;
  if (rate_limit !=0 && state->cycle_transmit_count >= rate_limit)
    return HIPSENS_FALSE;

  return (state->wakeup_condition.wakeup_time_buffer 
	  <= state->base->current_time);
}

hipsens_bool opera_event_wakeup_with_buffer
(opera_state_t* state, transmit_buffer_t* transmit_buffer)
{
  if (state->is_blocked || state->should_be_reset)
    return HIPSENS_FALSE;

  opera_check_serena_start(state);
  opera_handle_event(state, state->base->current_time, transmit_buffer);
  opera_update_wakeup_condition(state);

  int rate_limit = state->config->transmit_rate_limit;
  if (rate_limit !=0 && state->cycle_transmit_count >= rate_limit)
    return HIPSENS_FALSE;

  return (state->wakeup_condition.wakeup_time_buffer
	  == state->base->current_time);
}

hipsens_bool opera_event_packet_received
(opera_state_t* state, byte* packet_data, int packet_size, hipsens_u8 rssi)
{
  opera_check_serena_start(state);
  //opera_handle_event(state, state->base->current_time, NULL);
  opera_process_packet(state, packet_data, packet_size, rssi);
  opera_update_wakeup_condition(state);

  // XXX: we only transmit at the beginning of the cycle for now
  return HIPSENS_FALSE;

#if 0  
  int rate_limit = state->config->transmit_rate_limit;
  if (rate_limit !=0 && state->cycle_transmit_count >= rate_limit)
    return HIPSENS_FALSE;

  return (state->wakeup_condition.wakeup_time_buffer 
	  == state->base->current_time);
#endif
}

void opera_update_wakeup_condition(opera_state_t* state)
{ internal_opera_get_next_wakeup_condition(state, &state->wakeup_condition); }

/*---------------------------------------------------------------------------*/

void opera_close(opera_state_t* state)
{
#ifdef WITH_NEIGH_OPT
  eond_close(&state->eond_state);
  eostc_close(&state->eostc_state);
#endif
}



#ifdef WITH_PRINTF

void eond_config_pywrite(outstream_t out, eond_config_t* config)
{
  FPRINTF(out, "{'type':'eond-config'");
  FPRINTF(out, ", 'helloInterval':" FMT_HST, config->hello_interval);
  FPRINTF(out, ", 'neighHoldTime':" FMT_HST, config->neigh_hold_time);
  FPRINTF(out, ", 'maxJitterTime':" FMT_HST, config->max_jitter_time);
  FPRINTF(out, ", 'rssiLow':%d", config->link_quality_pwr_low);
  FPRINTF(out, ", 'rssiHigh':%d", config->link_quality_pwr_high);

  FPRINTF(out,"}");
}

void eostc_config_pywrite(outstream_t out, eostc_config_t* config)
{
  FPRINTF(out, "{'type':'eostc-config'");
  FPRINTF(out, ", 'stcInterval':" FMT_HST, config->stc_interval);
  FPRINTF(out, ", 'treeHoldTime':" FMT_HST, config->tree_hold_time);
  FPRINTF(out, ", 'stabilityTime':" FMT_HST, config->stability_time);
  FPRINTF(out, ", 'maxJitterTime':" FMT_HST, config->max_jitter_time);
  FPRINTF(out,"}");  
}

static const char* repr_priority_mode(priority_mode_t mode) {
  switch (mode) {
  case Priority_mode_tree: return "tree";
  case Priority_mode_fixed: return "fixed";
  case Priority_mode_2hop: return "2hop";
  case Priority_mode_3hop: return "3hop";
  default: 
    HIPSENS_FATAL("impossible priority_mode: %d\n", mode); 
    return "???";
  }
}

void serena_config_pywrite(outstream_t out, serena_config_t* config)
{
  FPRINTF(out, "{'type':'serena-config'");
  FPRINTF(out, ", 'msgColorInterval':" FMT_HST, config->msg_color_interval);
  FPRINTF(out, ", 'maxJitterTime':" FMT_HST, config->max_jitter_time);
  FPRINTF(out, ", 'priorityMode': '%s'",
	  repr_priority_mode(config->priority_mode));
  if (config->priority_mode == Priority_mode_fixed)
    FPRINTF(out, ", 'fixedPriority':" FMT_PRIORITY, 
	    GET_PRIORITY(config->fixed_priority));
  FPRINTF(out,"}");
}

void opera_config_pywrite(outstream_t out, opera_config_t* config)
{
  FPRINTF(out, "{'type':'opera-config'");
  FPRINTF(out,",\n 'eond':");
  eond_config_pywrite(out, &config->eond_config);

  FPRINTF(out,",\n 'eostc':");
  eostc_config_pywrite(out, &config->eostc_config);

  FPRINTF(out,",\n 'serena':");
  serena_config_pywrite(out, &config->serena_config);
  FPRINTF(out,"}");
}

void opera_pywrite(outstream_t out, opera_state_t* state)
{
  FPRINTF(out, "{'type':'opera'");

  FPRINTF(out,",\n 'eond':");
  eond_pywrite(out, &(state->eond_state));

  FPRINTF(out,",\n 'eostc':");
  eostc_pywrite(out, &(state->eostc_state));

  FPRINTF(out,",\n 'serena':");
  serena_pywrite(out, &(state->serena_state));

  FPRINTF(out,",\n 'isBlocked':%d", state->is_blocked);
  FPRINTF(out,",\n 'shouldBeReset':%d", state->should_be_reset);

  if (state->is_colored_tree_root) {
    FPRINTF(out, ",\n 'rootInfo': {");
    FPRINTF(out, "'flagUseColoredSlots': %d", state->flag_use_colored_slots);
    FPRINTF(out, ",'flagRunSerena': %d", state->flag_run_serena);
    FPRINTF(out, ",'flagNewColorTrigger': %d}", state->flag_new_color_trigger);
  } else {
    FPRINTF(out, ",\n 'rootInfo': None");
  }

  FPRINTF(out,"}");
}

#if defined(HIPSENS_MEM_MODEL_16BITS) && !defined(HIPSENS_PSEUDO_16BITS)
#define FMT_SZ "%u"
#else
#define FMT_SZ "%lud"
#endif

#define FPRINTF_SIZEOF(out, x) \
  FPRINTF(out, "sizeof(" #x "): " FMT_SZ "\n", sizeof(x))

void opera_write_size(outstream_t out)
{
  FPRINTF(out, "----- sizeof base types:\n");

  FPRINTF_SIZEOF(out, char);
  FPRINTF_SIZEOF(out, short);
  FPRINTF_SIZEOF(out, int);
  FPRINTF_SIZEOF(out, long);
  //PRINTF_SIZEOF(long long); 

  FPRINTF_SIZEOF(out, hipsens_time_t);
  FPRINTF_SIZEOF(out, hipsens_bool);
  FPRINTF_SIZEOF(out, hipsens_u8);
  FPRINTF_SIZEOF(out, hipsens_u16);
  FPRINTF_SIZEOF(out, hipsens_u32);
  FPRINTF(out, "\n");

  FPRINTF(out, "----- sizeof data structures for EOLSR / SERENA:\n");
  FPRINTF(out, "With ADDRESS_SIZE=%d\n", ADDRESS_SIZE);
  FPRINTF(out, "With MAX_NEIGHBOR=%d\n", MAX_NEIGHBOR);
  FPRINTF(out, "With MAX_PACKET_SIZE=%d\n", MAX_PACKET_SIZE);
  FPRINTF(out, "With MAX_STC_TREE=%d (number of routing trees)\n", MAX_STC_TREE);
  FPRINTF(out, "\n");

  FPRINTF(out, "- 'base' state (time, address,...):\n    ");
  FPRINTF_SIZEOF(out, base_state_t);

  FPRINTF(out, "- EOLSR neighbor discovery:\n    ");
  FPRINTF_SIZEOF(out, eond_state_t);
  FPRINTF(out, "    including:\n");
  FPRINTF(out, "        neighbor-table: " FMT_SZ " = %d x " FMT_SZ "\n", 
	 sizeof( ((eond_state_t*)(0))->neighbor_table), MAX_NEIGHBOR,
	 sizeof( ((eond_state_t*)(0))->neighbor_table[0]));

  FPRINTF(out, "- SERENA:\n    ");
  FPRINTF_SIZEOF(out, serena_state_t);

  FPRINTF(out, "- EOLSR Strategic Tree Construction:\n    ");
  FPRINTF_SIZEOF(out, eostc_state_t);
  FPRINTF(out, "    including:\n");
  FPRINTF(out, "        tree-table: " FMT_SZ " = %d x " FMT_SZ "\n", 
	 sizeof( ((eostc_state_t*)(0))->tree), MAX_STC_TREE,
	 sizeof( ((eostc_state_t*)(0))->tree[0]));
  
  FPRINTF(out, "        colored-tree-table: " FMT_SZ " = %d x " FMT_SZ "\n", 
	 sizeof( ((eostc_state_t*)(0))->serena_info), MAX_STC_SERENA_TREE,
	 sizeof( ((eostc_state_t*)(0))->serena_info[0]));

  //  FPRINTF(out, "        of which children info costs: %d = %d x %d\n",
//	 sizeof( ((eostc_state_t*)(0))->tree[0].child) * MAX_STC_TREE,
//	 MAX_STC_TREE, sizeof( ((eostc_state_t*)(0))->tree[0].child));

  FPRINTF(out, "\n");
  FPRINTF(out, "->Full node with Neighbor Discovery + Strategic Tree + SERENA + 1 packet buff.:\n");
  FPRINTF(out, "    "); FPRINTF_SIZEOF(out, opera_state_t); printf("\n");
}


#endif /* WITH_PRINTF */


/*---------------------------------------------------------------------------*/

#define OPERA_SERIAL_VERSION 0x00 // v0.0

typedef enum {
  OPERA_CMD_NONE = 0x00,
  OPERA_CMD_GET_VERSION = 0x01,
  OPERA_CMD_SET_BLOCKED = 0x02,
  OPERA_CMD_GET_BLOCKED = 0x03,
  OPERA_CMD_RESET = 0x04,
  OPERA_CMD_SET_ENERGY_CLASS = 0x05,


  OPERA_SET_EOND_HELLO_INTERVAL = 0x10,
  OPERA_GET_EOND_HELLO_INTERVAL = 0x11,

  OPERA_SET_EOND_NEIGH_HOLD_TIME = 0x12,
  OPERA_GET_EOND_NEIGH_HOLD_TIME = 0x13,

  OPERA_SET_EOND_START_DELAY = 0x14,
  OPERA_GET_EOND_START_DELAY = 0x15,

  OPERA_SET_ENERGY_CLASS = 0x16,
  OPERA_GET_ENERGY_CLASS = 0x17,

  OPERA_SET_EOSTC_STC_INTERVAL = 0x20,
  OPERA_GET_EOSTC_STC_INTERVAL = 0x21,

  OPERA_SET_EOSTC_TREE_HOLD_TIME = 0x22,
  OPERA_GET_EOSTC_TREE_HOLD_TIME = 0x23,
  
  OPERA_SET_EOSTC_STABILITY_TIME = 0x24,
  OPERA_GET_EOSTC_STABILITY_TIME = 0x25,

  OPERA_CMD_START_EOSTC_ROOT = 0x26,
  OPERA_CMD_STOP_EOSTC_ROOT = 0x27,
  OPERA_CMD_GET_EOSTC_ROOT = 0x28,

  OPERA_SET_EOSTC_START_DELAY = 0x29,
  OPERA_GET_EOSTC_START_DELAY = 0x2a,

  OPERA_SET_MY_TREE = 0x2b,
  OPERA_GET_MY_TREE = 0x2c,
  OPERA_GET_TREE_SEQNUM = 0x2d,
  OPERA_INCREASE_TREE_SEQNUM = 0x2e,

  OPERA_SET_SERENA_COLOR_INTERVAL = 0x30,
  OPERA_GET_SERENA_COLOR_INTERVAL = 0x31,

  OPERA_SET_TRANSMIT_RATE_LIMIT = 0x40,
  OPERA_GET_TRANSMIT_RATE_LIMIT = 0x41,

  OPERA_ADDRESS_FILTER_SET = 0x50,
  OPERA_ADDRESS_FILTER_GET = 0x51,

  OPERA_AYT = 0x60
} opera_cmd_t;

#define GET_U16(uptr) ( (((unsigned short)(uptr[0])) << 8)	\
			+ ((unsigned short)(uptr[1])))

#define PUT_U16(uptr, value) BEGIN_MACRO		\
  uptr[0] = (((unsigned short)(value)) >> 8) & 0xffu; \
  uptr[1] = ((unsigned short)(value)) & 0xffu; \
  END_MACRO

#define COMMAND_SET_GET_U16(cmd_code_set, cmd_code_get, name)		\
  COMMAND_SET_U16(cmd_code_set, name)					\
  COMMAND_GET_U16(cmd_code_get, name)

#define COMMAND_SET_U16(cmd_code_set, name)				\
  case (cmd_code_set): {						\
    if (payload_length < 2) {						\
      *result_code = 0xff;						\
      return 0;								\
    }									\
    (name) = GET_U16((payload+1));					\
    *result_code = 0;							\
    return 0;								\
  }									

#define COMMAND_GET_U16(cmd_code_get, name)				\
 case (cmd_code_get): {							\
   PUT_U16(result_array, (name));					\
   *result_code = 0;							\
   return 2;								\
 }


#ifndef WITH_SIMUL
extern opera_state_t opera;
extern opera_config_t opera_config;
#endif

/* This function receives a command from the serial port.
   It must set the 'result_code' and then it may put some 
   result in the array result_array; the return result must be 
   the number of bytes set in the array */
/* Don't change the prototype here, without changing it also in 
   ZOneMGTInterface.c */
/* Note: this function is less structured than could, in an 
   attempt to comsumme less stack */
byte opera_serial_command(
#ifdef WITH_SIMUL
			  opera_state_t* state,
#endif
			  byte* payload, byte payload_length,
                          byte* result_code,
                          byte* result_array, byte max_result_size)
{
#ifndef WITH_SIMUL
  opera_state_t* state = &opera;
  opera_config_t* opera_cfg = &opera_config;
#else
  opera_config_t* opera_cfg = state->config;
#endif
  if (payload_length == 0 || max_result_size < 20 /* 20 in caller */) {
    *result_code = 0xf1u; /* XXX: maybe cannot make difference */
    return 0;
  }

  switch (payload[0]) {
  case OPERA_CMD_GET_VERSION: {
    *result_code = OPERA_SERIAL_VERSION;
    return 0;
  }

  case OPERA_AYT: {
    *result_code = payload_length;
    return 0;
  }

  case OPERA_CMD_SET_BLOCKED: {
    if (payload_length < 1) {
      *result_code = 0xffu;
      return 0;
    }
    *result_code = (state->is_blocked != 0);
    return 0;
  }
  case OPERA_CMD_GET_BLOCKED: {
    *result_code = (state->is_blocked != 0);
    return 0;
  }
  case OPERA_CMD_RESET: {
    state->should_be_reset = HIPSENS_TRUE;
    *result_code = 0xCA;
    return 0;
  }
  
  COMMAND_SET_GET_U16(OPERA_SET_EOND_HELLO_INTERVAL,
		      OPERA_GET_EOND_HELLO_INTERVAL,
		      opera_cfg->eond_config.hello_interval);

  COMMAND_SET_GET_U16(OPERA_SET_EOND_NEIGH_HOLD_TIME,
		      OPERA_GET_EOND_NEIGH_HOLD_TIME,
		      opera_cfg->eond_config.neigh_hold_time);

  COMMAND_SET_GET_U16(OPERA_SET_EOND_START_DELAY,
		      OPERA_GET_EOND_START_DELAY,
		      opera_cfg->eond_start_delay);


  COMMAND_SET_GET_U16(OPERA_SET_EOSTC_STC_INTERVAL,
		      OPERA_GET_EOSTC_STC_INTERVAL,
		      opera_cfg->eostc_config.stc_interval);

  COMMAND_SET_GET_U16(OPERA_SET_EOSTC_TREE_HOLD_TIME,
		      OPERA_GET_EOSTC_TREE_HOLD_TIME,
		      opera_cfg->eostc_config.tree_hold_time);

  COMMAND_SET_GET_U16(OPERA_SET_EOSTC_STABILITY_TIME,
		      OPERA_GET_EOSTC_STABILITY_TIME,
		      opera_cfg->eostc_config.stability_time);


  COMMAND_SET_GET_U16(OPERA_SET_EOSTC_START_DELAY,
		      OPERA_GET_EOSTC_START_DELAY,
		      opera_cfg->eostc_start_delay);


  COMMAND_SET_GET_U16(OPERA_SET_SERENA_COLOR_INTERVAL,
		      OPERA_GET_SERENA_COLOR_INTERVAL,
		      opera_cfg->serena_config.msg_color_interval);


  COMMAND_SET_GET_U16(OPERA_SET_TRANSMIT_RATE_LIMIT,
		      OPERA_GET_TRANSMIT_RATE_LIMIT,
		      opera_cfg->transmit_rate_limit);


  COMMAND_SET_GET_U16(OPERA_SET_ENERGY_CLASS,
		      OPERA_GET_ENERGY_CLASS,
		      opera_cfg->transmit_rate_limit);


  case OPERA_SET_MY_TREE: {
    if (payload_length < 2) {
      *result_code = 0xff;
      return 0;
    }
    if (state->eostc_state.my_tree != NULL) {
      *result_code = 0xfdu;
      return 0;
    }

    /* XXX: race condition */
    *result_code = eostc_start(&state->eostc_state, 
			       GET_U16((payload+1)) != 0);
    return 0;
  }

  case OPERA_GET_MY_TREE: {
    if (state->eostc_state.my_tree != NULL) {
      if (IS_FOR_SERENA(*state->eostc_state.my_tree))
	*result_code = 0x02;
      else *result_code = 0x01;
    } else *result_code = 0x00;
    PUT_U16(result_array, *result_code);
    return 2;
  }

  case OPERA_GET_TREE_SEQNUM: {
    if (state->eostc_state.my_tree == NULL) {
      *result_code = 0x0;
      PUT_U16(result_array, 0);
      return 2;
    }
    if (IS_FOR_SERENA(*state->eostc_state.my_tree)) {
      *result_code = 0x02;
      PUT_U16(result_array, 
	      state->eostc_state.my_tree->serena_info->tree_seq_num);
      return 2;
    } 
    *result_code = 0x01;
    PUT_U16(result_array, 0);
    return 2;
  }

  case OPERA_INCREASE_TREE_SEQNUM: {
    if (state->eostc_state.my_tree == NULL
	|| !IS_FOR_SERENA(*state->eostc_state.my_tree)) {
      *result_code = 0xfdu;
      return 0;
    }
    *result_code = 0x02;    
    state->should_inc_colored_tree_seq = HIPSENS_TRUE;
    return 0;
  }

#ifdef WITH_OPERA_ADDRESS_FILTER
#warning "[CA] compiled with address filters"
  case OPERA_ADDRESS_FILTER_SET: {
    if (payload_length < 2) {
      *result_code = 0xffu;
      return 0;
    }
    if (payload[1] != 0 && payload[1] != 1 && payload[1] != 2) {
      *result_code = 0xfeu;
      return 0;
    }
    state->filter_mode = payload[1];
    state->filter_nb_address = 0;
    payload +=2 ;
    payload_length -= 2;
    while (state->filter_nb_address < MAX_FILTER_ADDRESS
	   && payload_length >= 2) {
      state->address_filter[state->filter_nb_address][0] = payload[0];
      state->address_filter[state->filter_nb_address][1] = payload[1];
      state->filter_nb_address++;
      payload += 2;
      payload_length -= 2;
    }
    *result_code = state->filter_nb_address;
    return 0;
  }
#if 0
  case OPERA_ADDRESS_FILTER_GET: {
    if (state->filter_nb_address * 2 >= max_result_size) {
      *result_code = 0xffu;
      return 0;
    }
  }
#endif
#endif

  default:
    *result_code = 0xf0u;
    return 0;
  }
#ifdef WITH_SIMUL
  return 0;
#endif
}

/*---------------------------------------------------------------------------*/
#warning "[CA] file included here to avoid modifying IAR project file"
#include "hipsens-ocari.c"
/*---------------------------------------------------------------------------*/
