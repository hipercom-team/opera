/*---------------------------------------------------------------------------
 *               OPERA - Inter-module Exchange for Coloring
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

#ifndef _HIPSENS_OPERA_COLORING_H
#define _HIPSENS_OPERA_COLORING_H

/*---------------------------------------------------------------------------*/

void hipsens_eostc_event_my_tree_flags_change(eostc_state_t* state, 
					      eostc_tree_t* tree,
					      hipsens_u8 new_flags);

void hipsens_eostc_event_tree_stability(eostc_state_t* state, 
					eostc_tree_t* tree);

int opera_set_serena_topology_info(opera_state_t* state,
				   eostc_tree_t* tree);

void opera_on_coloring_finished(struct s_serena_state_t* state);

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_OPERA_COLORING_H */
