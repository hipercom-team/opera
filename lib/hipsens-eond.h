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

#ifndef _HIPSENS_EOND_H
#define _HIPSENS_EOND_H

/*---------------------------------------------------------------------------*/

#include "hipsens-config.h"
#include "hipsens-base.h"

/*---------------------------------------------------------------------------*/

#define HIPSENS_MSG_HELLO 'H'

/** if defined, energy_class is stored for each neighbor - default for EOLSR */
#define WITH_ENERGY
/** if defined, field `state' of a neighbor is updated only when 
    eond_check_expiration is called - default for EOLSR */
#define WITH_DELAYED_STATE_UPDATE


typedef struct s_eond_config_t {
  hipsens_u8     link_quality_pwr_low;
  hipsens_u8     link_quality_pwr_high;
  hipsens_time_t neigh_hold_time;
  hipsens_time_t max_jitter_time;
  hipsens_time_t hello_interval;
} eond_config_t;

/**
 * Different states of a neighbor
 */
typedef enum {
  EOND_None = 0, /**< unused neighbor entry (in neighbor_table) */
  EOND_Asym = 1, /**< asymmetric neighbor */
  EOND_Sym = 2   /**< symmetric neighbor */
} eond_neighbor_state_t;

#ifdef WITH_OPERA_SYSTEM_INFO
#define EOND_SYSTEM_INFO 'S'
#endif

#ifdef WITH_OPERA_INPACKET_MSG
#define EOND_INPACKET_MSG 'M'
#endif

#ifdef WITH_INPACKET_LINK_STAT
#define EOND_INPACKET_LINK_STAT 'L'
#endif

/**
 * An entry of the neighbor table
 */
typedef struct s_nd_neighbor_t {
  /** the adress of the neighbor (L_neighbor_addr) */
  address_t address;  

  /** the state of the neighbor
      (updated only when eond_check_expiration is called) */
  eond_neighbor_state_t state; 

  /** time after which the link is no longer considered as symmetric */
  hipsens_time_t sym_time; 

  /** time after which the link is no longer considered as asymmetric */
  hipsens_time_t asym_time; 

  /** number of symmetric neighbors of this neighbor*/
  hipsens_u8 nb_1hop;
#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
  hipsens_u8 nb_2hop;
#endif

#ifdef WITH_INPACKET_LINK_STAT
  hipsens_u16 link_stat;
#endif /* WITH_INPACKET_LINK_STAT */

#ifdef WITH_ENERGY
  /** energy class */
  hipsens_u8 energy_class;
#endif
#ifdef WITH_STAT
  hipsens_u8 last_power;
  hipsens_u8 recv_count;
#endif

} eond_neighbor_t;

typedef void (*eond_observer_func_t)(void* data, address_t neighbor_address,
				     eond_neighbor_state_t old_state, 
				     eond_neighbor_state_t new_state,
				     int neighbor_index);

/**
 * The state of the Neighbor Discovery protocol in EOLSR
 */
typedef struct s_nd_state_t {
  base_state_t* base;

#ifdef WITH_NEIGH_OPT
  /* malloc'ed to avoid minor page faults and a factor 10 slowdown */
  eond_neighbor_t* neighbor_table; 
  //eond_neighbor_t neighbor_table[MAX_NEIGHBOR];
  int current_max_neighbor;
  int max_neighbor_limit;
#else
  eond_neighbor_t neighbor_table[MAX_NEIGHBOR];
#endif  

  hipsens_u16 hello_seq_num;

  hipsens_bool has_neighborhood_changed;

  hipsens_time_t next_msg_hello_time; /**< time for next hello message */

  /* callback for neighborhood change */
  eond_observer_func_t observer_func; /* XXX: put in hipsens-external-api.h */
  void* observer_data;

  eond_config_t* config;

#ifdef WITH_STAT
  /* statistics */
  int rejected_pwr_low_count;
  int rejected_pwr_high_count;
#endif
} eond_state_t;


#ifdef WITH_NEIGH_OPT
#define EOND_MAX_NEIGHBOR(state) ((state)->current_max_neighbor)
#else
#define EOND_MAX_NEIGHBOR(state) MAX_NEIGHBOR
#endif


/**
 * Initialize an EOND configuration with default values
 */
void eond_config_init_default(eond_config_t* config);


void eond_state_init(eond_state_t* state, base_state_t* base_state,
		     eond_config_t* config);

int eond_process_hello_message(eond_state_t* state, 
			       byte* packet, int packet_size, hipsens_u8 power);

int  eond_generate_hello_message(eond_state_t* state, 
				 byte* packet, int max_packet_size);


void eond_start(eond_state_t* state);

void eond_state_reset(eond_state_t* state);

/** return an estimate of the cost when neighbors are receiving a
    transmission from this node */
hipsens_u16 eond_estimate_reception_energy_cost(eond_state_t* state);

/** return an estimate of the number of 2-hop neighbors,
    which is actually an upper bound */
int eond_estimate_nb_2hop(eond_state_t* state);

#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
/** return an estimate of the number of 3-hop neighbors,
    which is actually an upper bound */
int eond_estimate_nb_3hop(eond_state_t* state);
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */

/** return NULL if not found */
eond_neighbor_t* eond_find_neighbor_by_address(eond_state_t*state,
					       address_t address);

void eond_check_expiration(eond_state_t* state);

void eond_get_next_wakeup_condition(eond_state_t* state, 
				    hipsens_wakeup_condition_t* condition);
int eond_notify_wakeup(eond_state_t* state, void* packet, int max_packet_size);

#ifdef WITH_PRINTF
void eond_pywrite(outstream_t out, eond_state_t* state);
#endif

/*---------------------------------------------------------------------------*/

#endif
