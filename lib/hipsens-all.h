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

#ifndef _HIPSENS_ALL_H
#define _HIPSENS_ALL_H

/*---------------------------------------------------------------------------*/

#include "hipsens-config.h"

/*--------------------------------------------------*/

#include <stdlib.h>

#ifdef WITH_FILE_IO
#include <stdio.h>
#endif

#ifndef IS_EMBEDDED
#include <errno.h>
#endif /* IS_EMBEDDED */

/*--------------------------------------------------*/

#include "hipsens-base.h"
#include "hipsens-eond.h"
#include "hipsens-eostc.h"
#include "hipsens-oserena.h"
#include "hipsens-opera.h"
#include "hipsens-opera-coloring.h"

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_ALL_H */

