/*---------------------------------------------------------------------------
 *                    OPERA - Interfaces with external modules
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

#ifndef _HIPSENS_EXTERNAL_API
#define _HIPSENS_EXTERNAL_API

/* these are the definitions of callback functions -- that OPERA should call 
   XXX: maybe merge with hipsens-opera.h 
*/

/*---------------------------------------------------------------------------*/


/*--------------------------------------------------*/

/*
[T1,T2] : trafic contraint
[T2,T3] : trafic non contraint colore'
[T2,T3] : trafic non contraint non colore'
 */

typedef enum {
  MAC_unspecified_traffic = 0, /* should not be used */
  MAC_constrained_traffic = 1,
  MAC_unconstrained_colored_traffic = 2,
  MAC_unconstrained_uncolored_traffic = 3
} MAC_traffic_type_t;

/*---------------------------------------------------------------------------*/

/* Interface External -> OPERA */

struct s_opera_state_t; /* defined in hipsens-opera.h */

/* call to OPERA, to indicate that it should be running SERENA */
void opera_external_notify_should_run_serena(struct s_opera_state_t* opera,
					     hipsens_bool should_run);

/* call to OPERA, to indicate that whether the colors are being used.
   If `should_use_color' is HIPSENS_TRUE (1), then OPERA will 
   call hipsens_api_set_color_info(...) */
void opera_external_notify_color_use(struct s_opera_state_t* opera,
				     hipsens_bool should_use_color);

/*--------------------------------------------------*/

/* Interface OPERA -> External */
/* opaque_extra_info is not used in OCARI */

/* Called by OPERA at tree root, to announce that SERENA should be
   started on all nodes */
void hipsens_api_should_run_serena
(void* opaque_extra_info, hipsens_bool should_run);

/* Called by OPERA at tree root, to announce the number of colors
    on all nodes */
void hipsens_api_set_nb_color
(void* opaque_extra_info, int nb_color);

/* Called by OPERA to give the coloring information:
   unlike the rest of OPERA, the colors passed are numbered starting from 1 */
void hipsens_api_set_color_info
(void* opaque_extra_info, byte node_color, 
 byte nb_neighbor_color, byte* neighbor_color_list);

/**
 * Returns the address of the node, by filling the (byte array) result_address
 * in OCARI, the address_t is the current short address.
 *
 * XXX:note - can the address change during one cycle or only at beginning ?
 */
void hipsens_api_get_my_address(void* opaque_extra_info,
				address_t result_address);

/*---------------------------------------------------------------------------*/

/* acknowledges an opera_external_notify_should_run_serena(...) */
void hipsens_api_response_should_run_serena(void* opaque_extra_info);

#if 0

/* The following function should no longer be used */

/* XXX: color min = 1 */ 
/**
 * This function is called by the OPERA module at the root of the EOSTC tree (1)
 * to notify changes in the colors or in the coloring:
 * - `should_use_colored_slots': indicates whether T2-T3 should be using 
 *    colored slots or not. 
 * - `should_run_serena': indicates if the nodes should be running SERENA or not
 *   (i.e. this is used to start or stop sending Color messages)
 * - if and only if `should_use_colored_slots' is HIPSENS_TRUE, the value 
 *   `new_color_trigger' should be considered.
 *    the boolean value passed in `new_color_trigger' is changed every time
 *    that new colors are available in OPERA, and must be used by MACARI ;
 *    hence upon change of this value, they must be retrieved through a call 
 *    to `opera_get_color_info'
 *    (Note: at the very first call, `new_color_trigger' is set to 1)
 *
 * The values passed are reflected in the beacons of MACARI as follows:
 * - the beacon has a field `nb_color' (2) indicating the number of colors
 *   used in T2-T3 ;
 *   . if `should_use_colored_slots' is false, `nb_color' is set to 0 
 *   . if `should_use_colored_slots' is true, `nb_color' should be set to
 *     the value returned from `opera_get_color_info'
 * - `should_run_serena' corresponds to an equivalent bit in the beacon
 * - `new_color_trigger' corresponds to an equivalent bit in the beacon
 * 
 * This function is called in several cases, including:
 * - the coloring (SERENA) should start
 * - the coloring is finished and new colors are available
 *   (through `opera_get_color_info')
 * - OPERA (at the CPAN) has detected the coloring has failed and decides
 *   to stop SERENA (typically it will be restarted later)
 * - OPERA (at the CPAN) decides to stop using colored slots
 *
 * (1) the root of the tree should be the CPAN; for OPERA, it is the node that
 *     called `void opera_start_eostc(..., with_serena = HIPSENS_TRUE);'
 * (2) nb_color == Max Color + 1
 */

void hipsens_api_set_beacon_status(void* opaque_extra_info/*not used in OCARI*/,
				   hipsens_bool should_use_colored_slots,
				   hipsens_bool should_run_serena,
				   hipsens_bool new_color_trigger);

/*
  XXX:Note: in the specification there is MaCARI.ColoringMode.request
  and then MaCARI.ColoringMode.confirmation: here we don't have confirmation
  and thus we don't handle the case where MaCARI confirmation is "FALSE"
 */
#endif

/*---------------------------------------------------------------------------*/

#endif /*_HIPSENS_EXTERNAL_API*/
