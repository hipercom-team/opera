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

/**
 * This part implements the scheduling logic of a node including
 * - EOLSR Neighbor Discovery
 * - EOLSR Strategic Tree Construction
 * - SERENA
 * 
 * OPERA is the orchestrating module
 * The name can also stand for the whole EOLSR ND + EOLSR STC + SERENA as
 *   Optimized Protocols for Energy-efficient Routing and node Activation
 */

/*
   How to use OPERA: (updated - 24 june 2011)

   -- global variables --
   opera_config_t opera_config;
   opera_state_t opera;

   -- init --
   opera_config_init_default(&opera_config);
   opera_init(&opera, &opera_config, NULL);
   opera_start(&opera); -- reset state and start generating packets

   -- main event loop usage --
   BOOLEAN shouldSendPacket = opera_event_new_cycle(&opera, ...);
   while (shouldSendPacket && we_can_still_send_some_packet) {
     hipsens_transmit_buffer_t buffer;
     try_to_get_some_packet_buffer();
     if (buffer not available)
       break;
     buffer.payload = ...;
     buffer.max_payload_size = ...;
     shouldSendPacket = opera_event_wakeup_with_buffer(&opera, &buffer);
     if (buffer.payload_size > 0) 
       ... actually send packet ...;
   }  

   -- receiving packets --
   BOOLEAN shouldSendPacket = opera_event_packet(&opera, 
                 packet_data, packet_size, rssi);
   ... do the same while loop as above ...
 */

/*

XXX: TODO

- enlever la partie "reinitialisation des tables" de opera_start (qui de toute
  façon est faite en doublon dans opera_init) ; l'initialisation
  se fera forcément par opera_init (ou bien aussi par une function `opera_reset'
  qui pourrait faire la même chose)
- ajouter une function `opera_stop' (ce qui correspond à ne plus générer de
  messages, mais éventuellement de pouvoir traiter des messages?)

- implémenter la logique de telle façon que l'utilisation normale soit:
  . opera_config_init_default (fait une fois lors du boot)
  . opera_init
    [... opera peut recevoir des paquets mais ne génère pas ...]
  . opera_start
    [... le protocole tourne ...]

  Et après le `opera_start', les 3 cas suites sont acceptées:
    cas 1) Arret temporaire et redémarrage:
     . opera_stop
       [... apres quelques secondes ...]
     . opera_start
       [... on continue avec les tables existantes - expirations ...]

    cas 2) Arret, reset et redémarrage:
      . opera_stop
        [... apres quelque temps ...]
      . opera_init (ou opera_reset)
      . opera_start
        [... et donc on continue avec des tables vidées ...]

   cas 3) Reset direct
      . opera_init (ou opera_reset) (qui arrête donc OPERA en même temps 
        qu'il réinitialise)
      . opera_start
        [... on reprend ...]

*/

#ifndef _HIPSENS_OPERA_H
#define _HIPSENS_OPERA_H

/*---------------------------------------------------------------------------*/

#include "hipsens-config.h"
#include "hipsens-base.h"
#include "hipsens-eond.h"
#include "hipsens-eostc.h"
#include "hipsens-oserena.h"
#include "hipsens-external-api.h"

/*---------------------------------------------------------------------------*/

//#warning "[CA] undef OPERA_ADDRESS_FILTER"
#define WITH_OPERA_ADDRESS_FILTER

/** XXX: in construction */
typedef struct s_opera_config_t {
  eond_config_t   eond_config;
  eostc_config_t  eostc_config;
  serena_config_t serena_config;

  hipsens_time_t eond_start_delay;
  hipsens_time_t eostc_start_delay;
  int transmit_rate_limit;
} opera_config_t;

/**
 * OPERA, the main interface for EOLSR strategic + SERENA, it includes
 * the state of sub-modules EOND, EOSTC, SERENA
 *
 * All the content is initialized by opera_init(...)
 */
typedef struct s_opera_state_t {
  base_state_t* base; /* just for ST...() macros to work XXX:remove */
  base_state_t base_state;
  eond_state_t eond_state;
  eostc_state_t eostc_state;
  serena_state_t serena_state;
  opera_config_t* config;
  hipsens_wakeup_condition_t wakeup_condition;

  /* the following values are useful mainly for the root node */
  hipsens_bool is_colored_tree_root   :1; /**< is it the CPAN ?,
   it is set to HIPSENS_TRUE (1) iff opera_start_eostc is called 
   with arg. `is_for_serena' == HIPSENS_TRUE (1) */

  /* number of packet sent during the frame */
  int cycle_transmit_count; /* XXX:should be only when WITH_FRAME defined */

  //hipsens_bool flag_use_colored_slots :1; /**< note: set in MACARI beacon */
  //hipsens_bool flag_run_serena        :1; /**< note: set in MACARI beacon */
  //hipsens_bool flag_new_color_trigger :1; /**< note: set in MACARI beacon */

  hipsens_bool has_set_color :1; /**< was coloring finished ? */

  //hipsens_bool should_

  /* those may be set externally: */
  hipsens_bool should_start_serena :1; /**<  */
  hipsens_bool is_blocked          :1; /**<  */
  hipsens_bool should_be_reset     :1; /**< is opera frozen (no processing) */
  hipsens_bool should_inc_colored_tree_seq :1; /**< triggers recoloring */
  hipsens_bool should_stop_stc_generation :1; /**< stop STC */

    
#ifdef WITH_OPERA_ADDRESS_FILTER
#define MAX_FILTER_ADDRESS 3
  byte filter_nb_address;
  enum { FilterNone = 0, FilterReject = 1, FilterOnlyAccept = 2 } filter_mode;
  address_t address_filter[MAX_FILTER_ADDRESS];
#endif

} opera_state_t;

/**
 * Initialize an OPERA configuration with default values
 */
void opera_config_init_default(opera_config_t* config);

/**
 * Initialize an OPERA node.
 * - state is typically a variable defined in the caller and called every time
 * - config is a pointer to a configuration variable
 *   the configuration variable must stay valid for the whole lifetime of the
 *   opera_state_t (i.e. it is not copied ; a pointer to `config' is kept).
 * - opaque_extra_info is some information that is going to be passed back
 *
 * An opera_state_t structure must be initialized before any other function
 * using it is called with the opera_state_t variable.
 */
void opera_init(opera_state_t* state,
		opera_config_t* config,
		void* opaque_extra_info); /* extra_info is used for sim. */

#if 0
/*
  - 1) use EOLSR information to find a route
  //- 2) use information from MACARI tree if no EOLSR route is available ???
  //     - loop problem
 */
#endif
/**
 * Find the next hop for a given destination address,
 * result is stored in result_next_hop_address.
 *
 * Returns HIPSENS_TRUE (1) if an address as been found or HIPSENS_FALSE (0)
 * otherwise.
 */
hipsens_bool opera_get_next_hop(opera_state_t* state,
				address_t destination_address,
				address_t result_next_hop_address);

/**
 * Starts OPERA as follows:
 * - resets the state of the internal EOND, EOSTC, SERENA
 * - starts EOND (Neighbor Discovery)
 *
 * Notes:
 * - `opera_init' must have been called with `state' prior to this function
 * - after this call, `opera_get_next_event_time' will return a useful time
 * - calling the function with an already started `state', is allowed
 *   and results in "restarting" (`opera_init' does not need to be called again)
 */
void opera_start(opera_state_t* state);

/* XXX: to document */
void opera_close(opera_state_t* state);

/**
 * Starts the EOSTC part of OPERA as a root. 
 *
 * The node will be the root of a tree.
 * - with_serena indicates whether the tree is intended to be for SERENA
 *  (in which case a special processing with confirmation will be done,
 *  and then SERENA will automatic start for coloring).
 *
 * Notes: 
 * - `opera_start' must have been called prior to this function
 * - other nodes, that are not root, should not start EOSTC. Normal EOSTC 
 *   (without the root STC message generation special to the tree root) is 
 *   running on other nodes, right after opera_start.
 * - only one node in the network should start EOSTC with `with_serena' true
 *   (and normally it should be the CPAN). Results are undefined otherwise
 *   since there would be two or more coloring at the same time.
 */
void opera_start_eostc(opera_state_t* state, hipsens_bool with_serena);

//void opera_handle_event_association(opera_state_t* state, address_t parent);

/*---------------------------------------------------------------------------*/

/**
 * This function is the main "scheduling" for OPERA.
 * It is called periodically exactly once per OCARI cycle
 * - it returns whether OPERA wishes to have a packet buffer or not
 * This function should be called only after `'opera_init' was called.
 *
 * XXX: specify if `opera_start' must be called before or not
 * XXX: specify what happens if a beacon is lost, is it called ... etc ?
 * 
 */
hipsens_bool opera_event_new_cycle(opera_state_t* state,
		   byte unused_What_Is_Current_Tn /*XXX: decide if useful*/);


typedef struct s_opera_packet_t {
  /* filled by caller */
  byte* payload;
  int max_payload_size;
  
  /* filled by OPERA */
  int payload_size; /* XXX: merge with max_payload ? */
  address_t next_hop_address; /* address of the next_hop or XXX for broadcast 
			       (see also global var. `undefined_address') */
  MAC_traffic_type_t traffic_type; /* the period/queue in which the packet */
  /* XXX: maybe specify some kind of jitter (expressed in backoffs?) ??? */
  /* more generic than that - managed by MACARI */
} transmit_buffer_t;

/**
 * This function is called every time OPERA has indicated that it wishes
 * to be called with a buffer (from return values of `opera_event_new_cycle', 
 * or `opera_event_packet').
 * The caller should fill the values:
 * . transmit_buffer.payload: pointer to the payload (must not be NULL)
 * . max_payload_size: maximum payload size (must be > 0)
 *
 * The return value indicates whether opera_event_wakeup_with_buffer
 * wishes to be called again with another transmit buffer ; in addition:
 * . if payload_size > 0, this indicates that the buffer should be sent
 *   and other fields are properly set (next_hop_address, traffic_type)
 *
 * (if transmit_buffer == NULL: not possible to send a packet)
 */
hipsens_bool opera_event_wakeup_with_buffer
(opera_state_t* state, transmit_buffer_t* transmit_buffer);

/**
 * The external caller should call this function every time a packet is
 * received.
 * - `rssi' corresponds to the link quality estimated for the received packet
 * (RSSI <http://en.wwikipedia.org/wiki/Received_signal_strength_indication>)
 * - `packet_data' is the payload of the received packets (Frame Payload)
 * after return of this function, the packet_data is not used by opera
 * (it may be freed)
 * - for OCARI, the `rssi' implementation needs to be defined
 * - the return value indicates if OPERA wishes to send a packet immediatly
 *   (typically as a result of the packet just received).
 */
hipsens_bool opera_event_packet_received
(opera_state_t* state, byte* packet_data, int packet_size, hipsens_u8 rssi);

/*---------------------------------------------------------------------------
 * Coloring API
 *---------------------------------------------------------------------------*/

typedef struct s_color_info {
  hipsens_u8 nb_color; /**< nb_color == number of colors */
  hipsens_u8 node_color; /**< node color, counting from 1 (unlike in oserena) */
  bitmap_t neighbor_color_bitmap; /**< bitmap_t defined in hipsens-oserena.h */
} color_info_t;

#if 0
/**
 * Function designed to be used with a `color_info_t.neighbor_color_bitmap'
 * as first parameter.
 * Returns HIPSENS_TRUE (1) if `color' is in `color_bitmap' or HIPSENS_FALSE (0)
 * otherwise.
 *
 * Note: 
 * - argument `color' is counting from 1 (i.e. >=1); 
 *   as is `color_info_t.node_color'. These are the only two places where
 *   colors are exchanged between OPERA and the external world.
 *   But notice that internally the SERENA colors are counting from 0.
 *
 */
hipsens_bool opera_check_color_in_bitmap(opera_state_t* state,
					 bitmap_t* color_bitmap, 
					 hipsens_u8 color);


/**
 * Returns the current coloring information in result_color_info
 * This function should only be called by the tree root node (CPAN of OCARI),
 * after OPERA/SERENA had called hipsens_api_set_beacon_status with proper
 * parameters.
 * The function returns HIPSENS_FALSE [== 0] if a coloring is not used
 * (for instance if coloring has just been detected to be invalid), or
 * HIPSENS_TRUE [== 1] otherwise.
 *
 * If the MACARI beacons have a nb_color>0 and there is no color,
 * the node is expected to send only in the grey period of T2-T3.
 */
hipsens_bool opera_get_color_info(opera_state_t* state, 
				  color_info_t* result_color_info);
#endif

/**
 * This function is called by MACARI whether there is a change in the 
 * bit in the received beacon indicating that SERENA should run
 *
 * XXX: can this function be called on the CPAN? (currently func. returns in
 *      this case)
 */
/* void opera_event_serena_run_change
   (opera_state_t* state, hipsens_bool should_run_serena); */

/*---------------------------------------------------------------------------*/

#ifdef WITH_PRINTF
/**
 * Dump the state of OPERA as a Python data structure (almost JSON).
 */
void opera_pywrite(outstream_t out, opera_state_t* state);

void opera_config_pywrite(outstream_t out, opera_config_t* state);
#endif /* WITH_PRINTF */

/*---------------------------------------------------------------------------
 * Internal API - direct scheduling API - not for OCARI
 *---------------------------------------------------------------------------*/

/**
 * The external caller should call this function after reaching the
 * date given by a previous opera_get_next_event_time(state)
 *
 * Notice that calling prematurely this function will generally
 * only result into the function doing nothing.
 */
void opera_direct_event_wakeup
(opera_state_t* state, hipsens_time_t current_time);

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
void opera_direct_event_wakeup_with_buffer
(opera_state_t* state, hipsens_time_t current_time,
 transmit_buffer_t* transmit_buffer);

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
void opera_direct_event_packet
(opera_state_t* state, hipsens_time_t current_time,
 byte* packet_data, int packet_size, hipsens_u8 rssi);
void opera_direct_start(opera_state_t* state, hipsens_time_t current_time);



void opera_direct_init(opera_state_t* state, opera_config_t* config,
		       hipsens_time_t current_time, void* opaque_extra_info); //added-ridha (for file hipsens-opera)


#ifdef WITH_SIMUL
//void opera_update_config(opera_config_t* config);
#endif

#ifdef WITH_SIMUL
byte opera_serial_command(opera_state_t* state,
			  byte* payload, byte frameLength,
                          byte* resultCode,
                          byte* resultArray, byte maxResultSize);
#else
byte opera_serial_command(byte* payload, byte frameLength,
                          byte* resultCode,
                          byte* resultArray, byte maxResultSize);
#endif

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_OPERA_H */
