/*
  Copyright 2008,2009,2010 Alun Williams
  This file is part of MAF.
  MAF is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  MAF is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with MAF.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
$Log: signal.c $
Revision 1.2  2009/09/14 10:32:05Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
use of windows.h wrapped by awwin.h so we can clean up warnings and packing


*/


#include "awwin.h"
#include <signal.h>
#include <stdlib.h>

typedef void (*signal_handler)(int signal_nr);

static signal_handler ctrlc_action       = SIG_DFL;    /* SIGINT   */
static signal_handler ctrlbreak_action   = SIG_DFL;    /* SIGBREAK */
static signal_handler abort_action       = SIG_DFL;    /* SIGABRT  */
static signal_handler term_action        = SIG_DFL;    /* SIGTERM  */
static int handler_installed = 0;

static BOOL WINAPI ctrlevent_capture (DWORD CtrlType)
{
  signal_handler ctrl_action;
  signal_handler *pctrl_action;
  int sigcode;

  /*Identify the type of event and fetch the corresponding action description.*/

  if (CtrlType == CTRL_C_EVENT)
  {
    ctrl_action = *(pctrl_action = &ctrlc_action);
    sigcode = SIGINT;
  }
  else
  {
    ctrl_action = *(pctrl_action = &ctrlbreak_action);
    sigcode = SIGBREAK;
  }

  if (ctrl_action != SIG_IGN)
    *pctrl_action = SIG_DFL;

  if (ctrl_action == SIG_DFL)
    return FALSE;

  if (ctrl_action != SIG_IGN)
    (*ctrl_action)(sigcode);

  return TRUE;
}

/**/

signal_handler signal(int signum,signal_handler sigact)
{
  signal_handler oldsigact;

 /* Check for values of sigact supported on other platforms but not on this one */
 if (sigact == SIG_ACK || sigact == SIG_SGE)
   return SIG_ERR;

  /* if SIGINT or SIGBREAK, make sure the handler is installed to capture ^C
     and ^Break events. */
  if ((signum == SIGINT || signum == SIGBREAK) && !handler_installed)
    if (SetConsoleCtrlHandler(ctrlevent_capture, TRUE) == TRUE )
      handler_installed = TRUE;
    else
      return SIG_ERR;

  switch (signum)
  {
    case SIGINT:
      oldsigact = ctrlc_action;
      ctrlc_action = sigact;
      return oldsigact;

    case SIGBREAK:
      oldsigact = ctrlbreak_action;
      ctrlbreak_action = sigact;
      return oldsigact;

    case SIGABRT:
      oldsigact = abort_action;
      abort_action = sigact;
      return oldsigact;

    case SIGTERM:
      oldsigact = term_action;
      term_action = sigact;
      return oldsigact;
  }
  return SIG_ERR;
}
