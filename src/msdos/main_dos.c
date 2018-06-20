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
** main.dos.c
**
** Entry point of program
** $Id: main_dos.c,v 1.8 2000/07/04 04:42:50 matt Exp $
*/

#include <stdio.h>
#include <conio.h>
#include <dpmi.h>
#include "types.h"
#include "version.h"

#include "log.h"
#include "osd.h"

#include "dostimer.h"
#include "dos_sb.h"

#include "nsf.h"

/* TODO: configurable */
static int sound_bps = 16;
static int sound_samplerate = 44100;
static boolean stereo = FALSE;

#ifdef DJGPP_USE_NEARPTR
#include <sys/nearptr.h>
#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_NEARPTR | _CRT0_FLAG_NONMOVE_SBRK;
#endif /* DJGPP_USE_NEARPTR */

/* Reduce size of executable - thanks, DJ! */
char **__crt0_glob_function(char *_argument) { return 0; }


#ifdef OSD_LOG
void osd_loginit(void)
{
}

void osd_logshutdown(void)
{
}

void osd_logprint(const char *string)
{
   printf(string);
}
#endif /* OSD_LOG */

static void osd_shutdown(void)
{
   sb_shutdown();
   timer_restore();
   log_printf("\nShutting down emulation normally\n");
}

volatile boolean nsf_frameupdate = FALSE;
void nsf_timerisr(void)
{
   nsf_frameupdate = TRUE;
}

/* bleh, we have to do some special handling for DJGPP 
** -- sorry about this.. =( 
*/
#ifdef __DJGPP__
END_OF_FUNCTION(nsf_timerisr);

void nsf_locktimerdata(void)
{
   LOCK_FUNCTION(nsf_timerisr);
   LOCK_VARIABLE(nsf_frameupdate);
}
#endif /* __DJGPP__ */

static boolean osd_init(nsf_t *nsf)
{
   int buf_size;
   boolean snd_status;

   log_printf("Starting %s v%s\n", APP_STRING, APP_VERSION);
   log_printf("compiled under DJGPP %d.%02d %s %s\n", DJGPP, DJGPP_MINOR, 
      __DATE__, __TIME__);

   log_printf("Installing timer interrupt hook... ");

   nsf_locktimerdata();

   timer_install(nsf->playback_rate, nsf_timerisr);
   log_printf("OK\n");

   log_printf("Initializing sound blaster... ");

   buf_size = sound_samplerate / nsf->playback_rate;

   if (FALSE == sb_init(&sound_samplerate, &sound_bps, &buf_size, &stereo))
      return FALSE;

   return TRUE;
}

static void display_info(nsf_t *nsf)
{
   printf("\nKeys: (esc to quit)\n");
   printf(" - left/right arrows to change track\n");
   printf(" - return restarts track\n");
   printf(" - 1-6 set toggle channels on/off\n");
   printf(" - A/S/D set filter level [none / low-pass / weighted]\n");
   printf("\n");
   printf("NSF Info:\n");
   printf("\tSong name: \t%s\n", nsf->song_name);
   printf("\tArtist:    \t%s\n", nsf->artist_name);
   printf("\tCopyright: \t%s\n", nsf->copyright);
   printf("\tNum. Songs:\t%d\n\n", nsf->num_songs);
}

static void toggle_channel(nsf_t *nsf, int channel)
{
   static boolean enabled[6] = { TRUE, TRUE, TRUE, TRUE, TRUE, TRUE };
   enabled[channel] ^= TRUE;
   
   nsf_setchan(nsf, channel, enabled[channel]);
}

static void track_start(nsf_t *nsf, int track)
{
   /* stop sound */
   sb_stopoutput();
   nsf_playtrack(nsf, track, sound_samplerate, sound_bps, FALSE);
   /* get the sound going */
   sb_startoutput((sbmix_t) nsf->process);
   printf("Playing NSF track %d of %d\t\r", nsf->current_song, nsf->num_songs);
}

static boolean handle_key(nsf_t *nsf)
{
   char ch;

   ch = getch();
   if (0 == ch)
      ch = getch();

   switch(ch) 
   {
   case 27: /* esc */
      printf("\n");
      return TRUE;

   case 77: /* right cursor arrow */
      if (nsf->current_song < nsf->num_songs)
         track_start(nsf, nsf->current_song + 1);
      break;
   case 75: /* left cursor arrow */
      if (nsf->current_song > 1)
         track_start(nsf, nsf->current_song - 1);
      break;
   case 13: /* return */
      /* don't change track number */
      track_start(nsf, nsf->current_song);
      break;
   case '1': case '2': case '3':
   case '4': case '5': case '6':
      toggle_channel(nsf, ch - '1');
      break;
   case 'A': case 'a':
      nsf_setfilter(nsf, NSF_FILTER_NONE);
      break;
   case 'S': case 's':
      nsf_setfilter(nsf, NSF_FILTER_LOWPASS);
      break;
   case 'D': case 'd':
      nsf_setfilter(nsf, NSF_FILTER_WEIGHTED);
      break;
   default:
      break;
   }

   return FALSE;
}

static void play_nsf_loop(nsf_t *nsf)
{
   display_info(nsf);
   track_start(nsf, nsf->current_song);

   while (1)
   {
      while (FALSE == nsf_frameupdate)
         __dpmi_yield(); /* spin */
      nsf_frameupdate = FALSE;

      nsf_frame(nsf);

      if (kbhit())
      {
         if (TRUE == handle_key(nsf))
            break;
      }
   }
}


int main(int argc, char *argv[])
{
   nsf_t *nsf_info;

   if (argc < 2)
   {
      printf("USAGE: %s filename.nsf\n", argv[0]);
      return 0;
   }

   if (log_init())
      return -1;

   nsf_init();

   /* load up an NSF */
   nsf_info = nsf_load(argv[1], NULL, 0);
   if (NULL == nsf_info)
   {
      log_shutdown();
      return -1;
   }

   if (FALSE == osd_init(nsf_info))
   {
      nsf_free(&nsf_info);
      log_shutdown();
      return -1;
   }

   /* shut up and skate! */
   play_nsf_loop(nsf_info);

   osd_shutdown();
   nsf_free(&nsf_info);
   log_shutdown();

   return 0;
}

/*
** $Log: main_dos.c,v $
** Revision 1.8  2000/07/04 04:42:50  matt
** moved DOS specific stuff into here (duh)
**
** Revision 1.7  2000/06/20 00:06:21  matt
** updated to support new NSF core
**
** Revision 1.6  2000/06/13 03:52:11  matt
** updated to support new API
**
** Revision 1.5  2000/06/12 01:12:17  matt
** added NEARPTR stuff, whoops
**
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
