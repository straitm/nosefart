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
** dos_sb.h
**
** DOS Sound Blaster header file
** $Id: dos_sb.h,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#ifndef _DOS_SB_H_
#define _DOS_SB_H_

/* These should be changed depending on the platform */
#ifndef  int8
#define  int8     char
#endif
#ifndef  int16
#define  int16    short
#endif
#ifndef  int32
#define  int32    int
#endif

#ifndef  uint8
#define  uint8    unsigned char
#endif
#ifndef  uint16
#define  uint16   unsigned short
#endif
#ifndef  uint32
#define  uint32   unsigned int
#endif

#ifndef  boolean
#define  boolean  int8
#endif

#ifndef  TRUE
#define  TRUE     1
#endif
#ifndef  FALSE
#define  FALSE    0
#endif

typedef void (*sbmix_t)(void *buffer, int size);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern boolean sb_init(int *sample_rate, int *bps, int *buf_size, boolean *stereo);
extern void sb_shutdown(void);
extern int sb_startoutput(sbmix_t fillbuf);
extern void sb_stopoutput(void);

/* in case we *really* need to change the sample rate during playback */
extern void sb_setrate(int rate);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _DOS_SB_H_ */

/*
** $Log: dos_sb.h,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
