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
** hercules.c
**
** x86 Hercules (monochrome) debug output routines
** $Id: hercules.c,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if __DJGPP__
#include <go32.h>
#include <sys/farptr.h>
#endif /* __DJGPP__ */

#include "types.h"
#include "hercules.h"


#ifdef OSD_LOG

static int cx, cy;            /* Position of printing cursor on 80x25 matrix     */
static char style;            /* Style of output, dark, bright, underlined, etc. */
static boolean output_enable; /* Used to "gate" output to monitor                */
static uint16 *herc_video;    /* Pointer to the monochrome video buffer          */


/* Set the printing style */
void herc_setstyle(char new_style)
{
   style = new_style;
}

/* Enable output gate to the monitor */
void herc_enable(void)
{
   output_enable = TRUE;
}

/* Disable output gate to the monitor */
void herc_disable(void)
{
   output_enable = FALSE;
}

/* Scroll the display upward the requested number of lines */
static void herc_scroll(int num_lines)
{
#ifdef __DJGPP__
   char blank_line[HERC_BYTES_PER_LINE];
#endif /* __DJGPP__ */

   if (FALSE == output_enable)
      return;

#ifdef __DJGPP__
   memset(blank_line, 0, HERC_BYTES_PER_LINE);
#endif /* __DJGPP__ */

   while(num_lines-- > 0)
   {
#ifdef __DJGPP__
      movedata(_dos_ds, (unsigned) (herc_video + HERC_BYTES_PER_LINE / 2),
               _dos_ds, (unsigned) herc_video, (HERC_ROWS - 1) * HERC_BYTES_PER_LINE);
#else
      memmove(herc_video, herc_video + (HERC_BYTES_PER_LINE / 2),
         (HERC_ROWS - 1) * HERC_BYTES_PER_LINE);
#endif /* __DJGPP__ */

      /* Now blank out the last line */
#ifdef __DJGPP__
      movedata(_my_ds(), (unsigned) blank_line,
               _dos_ds, (unsigned) (herc_video + ((HERC_ROWS - 1) * HERC_BYTES_PER_LINE / 2)),
               HERC_BYTES_PER_LINE);
#else
      memset(herc_video + ((HERC_ROWS - 1) * HERC_BYTES_PER_LINE / 2), 0,
         HERC_BYTES_PER_LINE);
#endif /* __DJGPP__ */
   }
}

/* Print a line of text, wrapping, scrolling and interpreting newlines as needed */
void herc_print(const char *string)
{
   int length, scroll;
   uint16 character;

   if (FALSE == output_enable)
      return;
  
   length = strlen(string);

   /* Enter main loop and print each character */
   while (length--)
   {
      character = *string++;

      /* Test if this is a control character? */
      if ('\n' == character)
      {
         cx = 0;

         if (++cy >= HERC_ROWS)
         {
            scroll = cy - (HERC_ROWS - 1);
            cy = (HERC_ROWS - 1);
            herc_scroll(scroll);
         } 
      }
      else
      {
         /* Merge character and attribute */
         character |= (uint16) (style << 8);

         /* Display character */
#ifdef __DJGPP__
         _farpokew(_dos_ds, (unsigned) (herc_video + ((cy * HERC_BYTES_PER_LINE / 2) + cx)), character);
#else
         herc_video[(cy * HERC_BYTES_PER_LINE / 2) + cx] = character;
#endif /* __DJGPP__ */

         /* Update cursor position */
         if (++cx >= HERC_COLUMNS)
         {
            cx = 0;
            /* Test for vertical scroll */
            if (++cy >= HERC_ROWS)
            {
               scroll = cy - (HERC_ROWS - 1);
               cy = (HERC_ROWS - 1);
               herc_scroll(scroll);
            }
         }
      }
   }
}

void herc_printf(const char *format, ...)
{
   va_list ap;
   char s[1024 + 1];

   va_start(ap, format);
   vsprintf(s, format, ap);
   va_end(ap);
   herc_print(s);
}

/* Clear the monochrome display */
void herc_clear(void)
{
#ifdef __DJGPP__
   char *clear_buf;
#endif /* __DJGPP__ */

   if (FALSE == output_enable)
      return;

   /* Clear the display */
#ifdef __DJGPP__
   clear_buf = malloc(HERC_ROWS * HERC_BYTES_PER_LINE);
   memset(clear_buf, 0, HERC_ROWS * HERC_BYTES_PER_LINE);
   movedata(_my_ds(), (unsigned) clear_buf,
            _dos_ds, (unsigned) herc_video, HERC_ROWS * HERC_BYTES_PER_LINE);

   free(clear_buf);
#else
   memset(herc_video, 0, HERC_ROWS * HERC_BYTES_PER_LINE);
#endif /* __DJGPP__ */

   /* Reset the cursor */
   cx = 0;
   cy = 0;
}

/* Initialize the monochrome printing system */
void herc_init(void)
{
   /* Position cursor at (0,0) */
   cx = 0;
   cy = 0;
   style = HERC_NORMAL;
   output_enable = TRUE;
   /* Pointer to the monochrome video buffer */
   herc_video = (uint16 *) 0xB0000;
   herc_clear();
}

#endif /* OSD_LOG */

/*
** $Log: hercules.c,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
