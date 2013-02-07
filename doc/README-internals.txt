---------------------------------------------------------------------------
This file is part of the internal documentation for the OPERA part for the
OCARI stack
-- (original file 2011/03/30 - 2011/04/01 -- Cedric Adjih)
Copyright Inria 2011-2013 - Part of OPERA distributed under LGPLv3 or later.
---------------------------------------------------------------------------

. General comments about the files:

- hipsens-config.h defines the configuration and default parameters:
  for OCARI the content is mostly overidden by hipsens-cc2530.h
  where all necessary defines should be put (or in IAR compiler options),
  provided that CC2530 is #defined

- hipsens-macro.h defines complicated macros (for debug, log, ...)
  however because of the full-state-dump functionality in Python wrappers,
  LOG/DEBUG is mostly unused in practice (see below "about printf")
  what remains is:
  - WARN(base_state, ...)
  - FATAL(base_state, ...)
  - ASSERT(...)
  - PRINTF(...) does nothing is WITH_PRINTF is not defined (default for OCARI)
  - FPRINTF(...) does nothing if WITH_PRINTF is not defined, does
    the same thing as PRINTF if WITH_FILE_IO is not defined, otherwise 
    is equivalent to fprintf
  - the STLOG/STWRITE/STLOGA/... include 'if (should_log)' which is comes from
    a value currently #defined to 0 in hipsens-cc2530.h, hence the compiler
    is expected to remove them

- hipsens-base.h/hipsens-base.c includes general functions/structures which
  were intially not specific to OCARI (and has grown...)
  It includes:
  - base_state_t = the base state for OPERA shared in submodules, which 
     currently includes the clock, the address of the node, the energy class
  - time related macros:
    . hipsens_time_t = the representation of the time in OPERA
    . HIPSENS_TIME_TO_SEC/... macros to convert seconds, milliseconds to
      hipsens_time_t
  - definitions of the integer types 1 byte, 2 bytes, 4 bytes, and of bool
    (has hipsens_u8, hipsens_s8, ... hipsens_bool)
  - address related functions (copy, compare)
  - an unused attempt at a random generator

- hipsens-opera.h/hipsens-opera.c is the main module for access to EOLSR+SERENA
  - although the modules EOND, EOSTC, SERENA could be accessed directly, the
    goal of OPERA is to provide a more simple API which should be easier
    to understand and use (see <http://en.wikipedia.org/wiki/Facade_pattern>)
  - the status of the API is changing

- hipsens-eond.h/hipsens-eond.c includes all the EOLSR neighbor discovery
  (EOND) module.

- hipsens-eostc.h/hipsens-eostc.c includes all the EOLSR Strategic tree
  construction protocol (EOSTC). This module requires the EOND module.
  It is under construction.

- hipsens-oserena.h/hipsens-oserena.c includes all the SERENA protocol.
  This module requires the EOND module. Later it will probably
  requires the EOSTC module, or some interaction between the two.
  The MAX_COLOR handling is not implemented
  The interaction with the tree is TBD and is not implemented.

- eosimul-simple.c is a simple example of the use of OPERA, it should not
  be included in the compilation

---------------------------------------------------------------------------

. General discussion:
  - about printf
    . the code can output its state as python datastructure (almost json)
    . however to use this functionnality WITH_PRINTF should be #defined
    . when WITH_PRINTF, printf is called ; where not available, 
      cg-printf.c/cg-printf.h have been used (i.e. for USB CC2531) but
      they are not added to the git repository right now

---------------------------------------------------------------------------
