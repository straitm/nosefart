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
** dostimer.h
**
** DOS timer routine defines / prototypes
** $Id: dostimer.h,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#ifndef _DOSTIMER_H_
#define _DOSTIMER_H_

#define  TIMER_INT         0x08

typedef void (*timerhandler_t)(void);

extern void timer_install(int hertz, timerhandler_t func_ptr);
extern void timer_restore(void);
extern void timer_adjust(int hertz);

#endif /* _DOSTIMER_H_ */

/*
** $Log: dostimer.h,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
