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
** hercules.h
**
** x86 Hercules (monochrome) debug output prototypes
** $Id: hercules.h,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#ifndef _HERCULES_H_
#define _HERCULES_H_

/* Dimensions of monchrome display */
#define  HERC_ROWS               25
#define  HERC_COLUMNS            80
#define  HERC_BYTES_PER_CHAR     2
#define  HERC_BYTES_PER_LINE     (HERC_COLUMNS * HERC_BYTES_PER_CHAR)

/* Colors and styles for printing */
#define  HERC_BLACK              0x00
#define  HERC_DARK_UNDERLINE     0x01
#define  HERC_NORMAL             0x02
#define  HERC_BRIGHT             0x0F
#define  HERC_BRIGHT_UNDERLINE   0x09

extern void herc_enable(void);
extern void herc_disable(void);
extern void herc_setstyle(char new_style);
extern void herc_clear(void);
extern void herc_print(const char *string);
extern void herc_printf(const char *format, ...);
extern void herc_init(void);

#endif /* _HERCULES_H_ */

/*
** $Log: hercules.h,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
