/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2012 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#ifndef BRLTTY_INCLUDED_SCR_DRIVER
#define BRLTTY_INCLUDED_SCR_DRIVER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* this header file is used to create the driver structure
 * for a dynamically loadable screen driver.
 */

#include "scr.h"
#include "scr_real.h"

/* Routines provided by this screen driver. */
static void scr_initialize (MainScreen *main);

#ifdef SCRPARMS
  static const char *const scr_parameters[] = {SCRPARMS, NULL};
#endif /* SCRPARMS */

#ifndef SCRSYMBOL
#  define SCRSYMBOL CONCATENATE(scr_driver_,DRIVER_CODE)
#endif /* SCRSYMBOL */

#ifndef SCRCONST
#  define SCRCONST const
#endif /* SCRCONST */

extern SCRCONST ScreenDriver SCRSYMBOL;
SCRCONST ScreenDriver SCRSYMBOL = {
  DRIVER_DEFINITION_INITIALIZER,

#ifdef SCRPARMS
  scr_parameters,
#else /* SCRPARMS */
  NULL,
#endif /* SCRPARMS */

  scr_initialize
};

DRIVER_VERSION_DECLARATION(scr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BRLTTY_INCLUDED_SCR_DRIVER */
