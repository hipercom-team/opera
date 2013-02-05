/*---------------------------------------------------------------------------
 *                    OPERA - Base Definitions/Functions
 *---------------------------------------------------------------------------
 * Author: Cedric Adjih
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

/* XXX: hack: this file is included directly in another C file:
   in order to avoid modifying the Project because of differing IAR versions
 */

#include "hipsens-all.h"

/*---------------------------------------------------------------------------*/

/* XXX: should be moved to somewhere else */

/* [CONFIG-OPERA] uncomment and change values here */
/* Interval of Hello messages */
#define OCARI_HELLO_INTERVAL  5 /* cycles */

/* How long before a neighbor expires */
#define OCARI_NEIGH_HOLD_TIME ((OCARI_HELLO_INTERVAL*7)/2) /* cycles */

/* How long to delay Hello message generation */
//#define OCARI_DELAY_EOND 2000 /* cycles */
#define OCARI_DELAY_EOND 0 /* cycles */

/* Interval of STC message */
#define OCARI_STC_INTERVAL    5 /* cycles */

/* How long before the tree completly expires */
#define OCARI_TREE_HOLD_TIME  ((OCARI_STC_INTERVAL*7)/2) /* cycles */

/* How long to delay STC message generation */
#define OCARI_DELAY_EOSTC 0 /* cycles */


/* How long before the network is considered stable (locally) */
#define OCARI_STABILITY_TIME 20 /* cycles */
  
/* Interval of Color messages */
#define OCARI_COLOR_INTERVAL 5 /* cycles */

/*---------------------------------------------------------------------------*/

static void opera_update_config(opera_config_t* config)
{
  /* Interval of Hello messages */
  config->eond_config.hello_interval = OCARI_HELLO_INTERVAL; /* cycles */

  /* How long before a neighbor expires */
  config->eond_config.neigh_hold_time = OCARI_NEIGH_HOLD_TIME; /* cycles */
  
  /* How long to delay Hello message generation */
  config->eond_start_delay = OCARI_DELAY_EOND; /* cycles */


  /* Interval of STC message */
  config->eostc_config.stc_interval = OCARI_STC_INTERVAL; /* cycles */
  
  /* How long before the tree completly expires */
  config->eostc_config.tree_hold_time = OCARI_TREE_HOLD_TIME; /* cycles */

  /* How long to delay Hello message generation */
  config->eostc_start_delay = OCARI_DELAY_EOSTC; /* cycles */


  /* How long before the network is considered stable (locally) */
  config->eostc_config.stability_time = OCARI_STABILITY_TIME; /* cycles */
  
  /* Interval of Color messages */
  config->serena_config.msg_color_interval = OCARI_COLOR_INTERVAL; /* cycles */

  /* Maximum number of messages sent per cycle */
#ifdef WITH_SIMUL
  config->transmit_rate_limit = 0; /* message per cycle */
#else
  config->transmit_rate_limit = 1; /* message per cycle (for OCARI) */
#endif /*WITH_SIMUL*/
}

/*---------------------------------------------------------------------------*/

#ifndef WITH_SIMUL

extern opera_config_t opera_config;
extern opera_state_t opera;

void ocari_init_opera_config(void);
void ocari_init_opera_config(void)
{
  opera_update_config(&opera_config);
//#warning "[CA] next line should be re-enabled"
  opera_init(&opera, &opera_config, NULL);
}

#endif /* WITH_SIMUL */

/*---------------------------------------------------------------------------*/
