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
** dos_ints.h
**
** General DOS interrupt defines
** $Id: dos_ints.h,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#ifndef _DOS_INTS_H_
#define _DOS_INTS_H_

/* Thanks, Allegro! */
#define  BPS_TO_TIMER(x)            (1193182L / (long)(x))
#define  END_OF_FUNCTION(x)         void x##_end(void) {}
#define  END_OF_STATIC_FUNCTION(x)  static void x##_end(void) {}
#define  LOCK_VARIABLE(x)           _go32_dpmi_lock_data((void*)&x, sizeof(x))
#define  LOCK_FUNCTION(x)           _go32_dpmi_lock_code(x, (long)x##_end - (long)x)

#define  disable_ints()             __asm__ __volatile__ ("cli")
#define  enable_ints()              __asm__ __volatile__ ("sti")

#endif /* _DOS_INTS_H_ */

/*
** $Log: dos_ints.h,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
