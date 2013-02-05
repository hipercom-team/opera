/*---------------------------------------------------------------------------
 *                        OPERA / General Configuration
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

#ifndef _HIPSENS_CONFIG_H
#define _HIPSENS_CONFIG_H

/*---------------------------------------------------------------------------*/

#ifdef CC2530
#include "hipsens-cc2530.h"
#endif

/*---------------------------------------------------------------------------*/

#define WITH_ENERGY /* always defined in EOLSR */

/**
 * \def WITH_FILE_LOG
 * when defined, logging functions write in a (FILE*), otherwise
 * PRINTF is used
 */

/**
 * \def WITH_SMALL_MEMORY
 * when defined, the machine is known to have small memory
 */

/** 
 * \def WITH_PRINTF
 * when defined, functions calling 'printf' are compiled in
 */

/*#define WITH_PRINTF*/

// XXX:
//#define DBG_SERENA

// XXX:
//#define IS_EMBEDDED


//-- WITH_DBG
// when defined, debug statements are executed: XXX - clean all of them


//-- WITH_STAT
// when defined incorporate statistics

//-- WITH_SIMUL_ADDR
// when defined the address is the address of a simulation

/*---------------------------------------------------------------------------*/

// XXX: not used now

//-- WITH_EMPTY_HELLO: 
// when defined, the HELLO not include the list of neighbors (nor their state)
// hence only act as beacon.
// this only allows detection of asymetric links 
// USED ONLY IN hipsens-treend NOT in hipsens-eond
//#define WITH_EMPTY_HELLO

//-- WITH_LINK_STAT
// when defined, a bitmap is included in the neighbor discovery which
// represents the time at which HELLOs have been received
// (see constant LINK_STAT_LOG_SIZE below)
// USED ONLY IN hipsens-treend NOT in hipsens-eond
//#define WITH_LINK_STAT

//-- WITH_SERENA:
// when defined, extra statistics useful for SERENA are compiled in
#define WITH_SERENA 

/*--------------------------------------------------*/

//-- ADDRESS_SIZE
#ifndef ADDRESS_SIZE
#define ADDRESS_SIZE 8
#endif 

//-- MAX_PACKET_SIZE
// the maximum size of control message payload
#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE 128
#endif 

//-- MAX_NEIGHBOR
// the maximum number of neighbors in the neighbor table
#ifndef MAX_NEIGHBOR
#define MAX_NEIGHBOR 20
#endif

//-- NB_COLOR_MAX
// the maximum number of colors useful to color the full graph
#ifndef NB_COLOR_MAX
#define NB_COLOR_MAX 128
#endif

#ifdef WITH_LINK_STAT
#ifndef LINK_STAT_LOG_SIZE
#define LINK_STAT_LOG_SIZE 80 
#endif /* LINK_STAT_BITMAP_SIZE */
#endif /* WITH_LINK_STAT */

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_CONFIG_H */
