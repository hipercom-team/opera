/*---------------------------------------------------------------------------
 *               OPERA - Configuration of for OCARI (CC2530)
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

#ifndef _HIPSENS_CC2530_H
#define _HIPSENS_CC2530_H

/*---------------------------------------------------------------------------*/

#define HIPSENS_MEM_MODEL_16BITS
#define WITH_SMALL_MEMORY
#define IS_EMBEDDED
#define WITH_SERENA
#define WITH_FRAME_TIME
#define WITH_SERENA_EOND

#define WITH_OPERA_SYSTEM_INFO
#define WITH_OPERA_INPACKET_MSG

#define ADDRESS_SIZE 2
#define MAX_PACKET_SIZE 128
#define MAX_NEIGHBOR 10
#define MAX_STC_TREE 4
#define MAX_STC_SERENA_TREE 2

#define DBGsrn 0
#define DBGnd 0
#define DBGstc 0
#define DBGnode 0
#define DBGsim 0

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_CC2530_H */
