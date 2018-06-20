/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** dostimer.c
**
** DOS timer routines
** $Id: dostimer.c,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#include <go32.h>
#include <pc.h>
#include <dpmi.h>
#include "types.h"
#include "dos_ints.h"
#include "dostimer.h"

static _go32_dpmi_seginfo old_handler, new_handler;
static timerhandler_t timer_handler;

/* Reprogram the PIC timer to fire at a specified value */
void timer_adjust(int hertz)
{
   int time;

   if (0 == hertz)
      time = 0;
   else
      time = BPS_TO_TIMER(hertz);

   outportb(0x43, 0x34);
   outportb(0x40, time & 0xFF);
   outportb(0x40, time >> 8);
}

/* Lock code, data, and chain an interrupt handler */
void timer_install(int hertz, timerhandler_t func_ptr)
{
   timer_handler = func_ptr;

   /* Save the old vector, stuff the new one in there */
   _go32_dpmi_get_protected_mode_interrupt_vector(TIMER_INT, &old_handler);
   new_handler.pm_offset = (int) timer_handler;
   new_handler.pm_selector = _go32_my_cs();
   _go32_dpmi_chain_protected_mode_interrupt_vector(TIMER_INT, &new_handler);

   /* Set PIC to fire at desired refresh rate */
   timer_adjust(hertz);
}

/* Remove the timer handler */
void timer_restore(void)
{
   /* Restore previous timer setting */
   timer_adjust(0);

   /* Remove the interrupt handler */
   _go32_dpmi_set_protected_mode_interrupt_vector(TIMER_INT, &old_handler);
}

/*
** $Log: dostimer.c,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
