/**
 * @file      noefart_xmms.c
 * @author    Benjamin Gerard <ben@sashipa.com>
 * @date      2003/04/26
 * @brief     nosefart input plugin for xmms
 * @version   $Id: xmms68.c,v 1.3 2002/01/22 15:51:28 ben Exp $
 */
 
/*
 *                        nosefart - xmms input plugin
 *         Copyright (C) 2003 Benjamin Gerard <ben@sashipa.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Standard includes */
#include <glib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* xmms includes */
#include "plugin.h"
#include "util.h"

/* generate config include */
#include "xmms_config.h"

/* nosefart includes */
#include "types.h"
#include "nsf.h"

static InputPlugin nosefart_ip;
static volatile gboolean going, audio_open;
static volatile pthread_t play_thread;

static struct {
  nsf_t * nsf;

  int bufmax;
  int buflen;
  int bufcnt;
  u16 * buffer;
  u16 * bufptr;

  unsigned int n_tracks;
  int cur_track;
  struct {
    int map;
    unsigned int ms;
  } tracks[256];

} app;

#ifndef _ 
# define _(A) (A)
#endif

static void pdebug(const char * fmt, ...)
{
#ifdef DEBUG
  va_list list;
  va_start(list,fmt);
  vfprintf(stderr, fmt, list);
  va_end(list);
#endif
}

volatile struct change_track_s {
  int track;
  int time;
} change_track;

static void ClearChangeTrack(void)
{
  change_track.track = -1;
  change_track.time  = -1;
}

static void SetChangeTrack(int trk, int tim)
{
  change_track.time = tim;
  change_track.track = trk;
}

static gchar * make_name(const char * song, const char * artist,
			 int track, int tracks)
{
  char trackstr[64];
  gchar * name = 0;

  if (song[0]) {
    trackstr[0] = 0;
    if (tracks > 1) {
      if (track >= 1) {
	sprintf(trackstr,"[%02u/%02u]", track, tracks);
      } else {
	sprintf(trackstr,"[%02u]",tracks);
      }
    }
    if (artist[0]) {
      name = g_strdup_printf("%s%s - %s", song, trackstr, artist);
    } else {
      name = g_strdup_printf("%s%s", song, trackstr, artist);
    }
  }
  return name;
}

static void SetInfo(void)
{
  nsf_t * nsf = app.nsf;
  unsigned int ms;
  gchar *name = 0;
  unsigned int track = app.cur_track;

  pdebug("nsf : set info #%02u\n", track);

  ms = 0;
  if (!track || track > app.n_tracks) {
    ms = app.tracks[0].ms;
  } else {
    unsigned int tms = app.tracks[track].ms;
    ms = app.tracks[0].ms;
    name =
      g_strdup_printf("%s by %s track:%02d %02u:%02u copyright:%s :-)",
		      nsf->song_name, nsf->artist_name,
		      track, tms/60000u, tms%60000u/1000u,
		      nsf->copyright);
    nosefart_ip.set_info_text(name);
    g_free(name);
  }
  name = make_name(nsf->song_name, nsf->artist_name, track, app.n_tracks);
  nosefart_ip.set_info(name, ms, 44100, 44100, 1);
  g_free(name); /* $$$ ? */
}

static int UnsetNsf(void)
{
  pdebug("nsf : Unset nsf\n");

  app.nsf = 0;
  app.buflen = 0;
  app.bufcnt = 0;
  app.bufptr = app.buffer;
  app.n_tracks = 0;
  app.cur_track = 0;

  return 0;
}

static int SetNsf(nsf_t * nsf)
{
  int i,j;
  unsigned int newsize;
  unsigned int total_ms;

  pdebug("nsf : Set nsf %p\n", nsf);
  if (app.nsf) {
    pdebug("nsf : nsf already setted\n");
    return -1;
  }

  if (!nsf) {
    goto error;
  }

  /* Setup PCM buffer. */
  newsize = 44100 / nsf->playback_rate;
  pdebug("nsf : buffer-size [%u]\n", newsize);

  if (newsize > app.bufmax) {
    pdebug("nsf : buffer need realloc\n");

    app.buffer =
      realloc(app.buffer, newsize * 2 + 32);
    if (!app.buffer) {
      app.bufmax = 0;
    } else {
      app.bufmax = newsize;
    }
  }
  if (newsize > app.bufmax) {
    goto error;
  }
  app.bufcnt = 0;
  app.buflen = newsize;

  /* Setup track-list. */
  pdebug("nsf : set track times\n");

  for (i=1, j=0, total_ms = 0; i <= nsf->num_songs; ++i) {
    unsigned int ms =
      nsf->song_frames[i] * 1000u / (unsigned int)nsf->playback_rate;
    if (ms < 5000u) {
      continue;
    }
    ++j;
    app.tracks[j].map = i;
    app.tracks[j].ms = ms;
    total_ms += ms;
    pdebug("nsf : track #%02d, mapped on #%02d, ms=%u\n",j,i,ms);
  }
  app.tracks[0].map = 0;
  app.tracks[0].ms = total_ms;

  if (!j) {
    goto error;
  }
  app.n_tracks = j;
  app.cur_track = 0;
  app.nsf = nsf;
  return 0;

 error:
  memset(&app, 0, sizeof(app));
  return -1;
}

static int SetupSong(void)
{
  nsf_t * nsf = app.nsf;
  int track; 

  if (!nsf) {
    pdebug("nsf : setup song, no nsf\n");
    return -1;
  }
  track = app.tracks[app.cur_track].map; 
  pdebug("nsf : setup song #%d/%d map:[%d]\n", app.cur_track, app.n_tracks,
	 track);
  if (nsf_playtrack(nsf, track, 44100, 16, 0) != track) {
    pdebug("nsf : play-track failed\n");
    return -1;
  }
  pdebug("nsf : set filter\n");
  nsf_setfilter(nsf, NSF_FILTER_NONE);

  app.bufcnt = 0;
  SetInfo();
  return 0;
}

static int ApplyChangeTrack(void)
{
  int err;

  if (change_track.track == -1) {
    return 0;
  }

  pdebug("nsf : apply change track #%d %d\n", 
	 change_track.track, change_track.time);

  if (change_track.track == 0) {
    app.cur_track = 0;
    SetInfo();
    err = 0;
  } else if (change_track.track <= app.n_tracks) {
    app.cur_track = change_track.track;
    if (nosefart_ip.output && audio_open && change_track.time != -1) {
      nosefart_ip.output->flush (change_track.time);
    }
    err = SetupSong();
  } else {
    pdebug("track %d out of range %d\n", change_track.track, app.n_tracks);
    err = -1;
  }
  ClearChangeTrack();
  return err;
}

/** Display a simple about dialog. */
static void nosefart_about(void)
{
static GtkWidget *box;
static char * text = 
  "nosefart " VERSION " XMMS plugin\n"
  "\n"
#ifdef DEBUG
  " !!! DEBUG VERSION !!! \n"
  "\n"
#endif       
  "NSF (Nintendo Famicom Sound File) sound emulator\n"
  "Based on nosefart engine (http://sourceforge.net/projects/nosefart)\n"
  "\n"
  "by Benjamin Gerard (2003/04/26)\n"
  "\n"
  "email : ben@sashipa.com\n"
  "\n";

  box = xmms_show_message(
			  _("About nosefart XMMS plugin"),
			  _(text),
			  _("Ok"),
			  FALSE, NULL, NULL);
  gtk_signal_connect(GTK_OBJECT(box), "destroy", gtk_widget_destroyed, &box);
}

static nsf_t * load(char * filename)
{
  int i;
  nsf_t * nsf;
  unsigned int total;

  pdebug("nsf : load [%s]\n", filename);

  nsf = nsf_load(filename,0,0);
  if (!nsf) {
    pdebug("nsf : [%s] load error.\n", filename);
    return 0;
  }
  /* create missing frame info */
  if (!nsf->song_frames) {
    pdebug("nsf : no play time info, alloc buffer\n");
    nsf->song_frames = calloc(nsf->num_songs + 1, sizeof (*nsf->song_frames));
  }

  if (!nsf->song_frames) {
    pdebug ("nsf : [%s] malloc song playing tine buffer failed.\n", filename);
    nsf_free(&nsf);
    return 0;
  }

  for (i=1, total=0; i <= nsf->num_songs; ++i) {
    unsigned int t = nsf->song_frames[i];
    if (!t) {
      t = 2 * 60 * nsf->playback_rate; /* default 2 minutes */
      pdebug("nsf : track #%02d, set default time [%u frames]\n", i, t);
    }
    if (t / nsf->playback_rate < 5) {
      pdebug("nsf : skipping track #%02d (short play time)\n", i);
      t = 0; /* Avoid short time songs (sound FX) */
    }
    nsf->song_frames[i] = t;
    total += t;
  }
  nsf->song_frames[0] = total;

  return nsf;
}


/** Test file format XMMS callback. */
static int nosefart_is_our_file(char *filename)
{
  int result = FALSE;
  FILE *f;

  f = fopen(filename, "rb");
  if (f) {
    char buf[5];
    if (fread(buf,1,5,f) == 5) {
      if (!memcmp(buf,NSF_MAGIC,5)) {
	result = TRUE;
      }
    }
    fclose(f);
  }
  pdebug ("nsf : is our file [%s]\n", result ? "YES" : "NO");
  return result;
}


/** XMMS stop callback.
 */
static void nosefart_stop(void)
{
  pdebug ("nsf : stop\n");

  going = FALSE;
  if (play_thread) {
    pdebug ("nsf : waiting for playing thread end.\n");
    pthread_join(play_thread, NULL);
    if (play_thread) {
      pdebug ("nsf : INTERNAL, thread should not exist here !!!.\n");
      play_thread = 0;
    }
    pdebug ("nsf : playing thread ended.\n");
  } else {
    pdebug ("nsf : no previous thread.\n");
  }
  if (nosefart_ip.output && audio_open) {
    pdebug ("nsf : close audio.\n");
    nosefart_ip.output->close_audio();
    audio_open = FALSE;
    pdebug ("nsf : audio closed.\n");
  } else {
    pdebug ("nsf : no audio to close.\n");
  }
}

static void * play_loop(void *arg)
{
  nsf_t * nsf = app.nsf;

  pdebug("nsf : play-thread\n");

  if (!app.nsf) {
    going = FALSE;
  }
  pdebug("nsf : going [%s]\n", going ? "Yes" : "No");

  app.cur_track = 0;
  nsf->cur_frame = nsf->cur_frame_end = 0;
  while (going) {
    if (nsf->cur_frame >= nsf->cur_frame_end) {
      int next_track = app.cur_track+1;
      pdebug("nsf : frame elapsed [%u > %u] -> track #%02d\n",
	     nsf->cur_frame,nsf->cur_frame_end, next_track);
      
      if (next_track > app.n_tracks) {
	pdebug("nsf : reach last song\n");
	going = FALSE;
	break;
      }
      SetChangeTrack(next_track, -1);
    }
    
    if (ApplyChangeTrack() < 0) {
      going = FALSE;
      break;
    }
    
    if (!app.bufcnt) {
      /* Run a frame. */
      nsf_frame(nsf);
      apu_process(app.buffer, app.buflen);
      app.bufptr = app.buffer;
      app.bufcnt = app.buflen;
    }

    if (going) {
      int tosend = app.bufcnt << 1;
      while (nosefart_ip.output->buffer_free() < tosend && going) {
        xmms_usleep(30000);
      }
      nosefart_ip.add_vis_pcm(nosefart_ip.output->written_time(),
			      FMT_S16_LE, 1, tosend, app.bufptr);
      nosefart_ip.output->write_audio(app.bufptr, tosend);
      tosend >>= 1;
      app.bufcnt -= tosend; 
      app.bufptr += tosend; 
    }
  }
  pdebug("nsf : decoder loop end\n");
  SetChangeTrack(0, -1);
  ApplyChangeTrack();

  if (app.nsf) {
    nsf_free(&app.nsf);
    app.nsf = nsf = 0;
  }

  /* Make sure the output plugin stops prebuffering */
  if (nosefart_ip.output) {
    pdebug("nsf : free audio buffer.\n");
    nosefart_ip.output->buffer_free();
  }
  going = FALSE;
  pdebug("nsf : decode finish\n");
  play_thread = 0;
  pthread_exit(NULL);
}


static void nosefart_play(char *filename)
{
  char c[1024];
  snprintf(c, 1023, "nosefart -a 2 %s &", filename);

  system(c);

  return;

#if 0
  nsf_t * nsf = 0;

  pdebug("nsf : play [%s]\n", filename);

  /* Load disk. */
  nsf = load(filename);
  if (!nsf) {
    goto error;
  }

  while (app.nsf) {
    pdebug("nsf : busy, wait for end of previous thread !\n");
    xmms_usleep(30000);
  }

  if (SetNsf(nsf) < 0) {
    goto error;
  }

  /* open XMMS audio stream */
  pdebug("nsf : open audio\n");
  audio_open = FALSE;
  if (nosefart_ip.output->open_audio(FMT_S16_LE, 44100, 1) == 0) {
    pdebug("nsf : open audio error\n");
    goto error;
  }
  audio_open = TRUE;
  pdebug("nsf : open audio success\n");

  going = TRUE;
  pthread_create((pthread_t *)&play_thread, NULL, play_loop, 0);
  return;

error:
  going = FALSE;
  pdebug("nsf : play failed\n");
  if (nsf) {
    nsf_free(&nsf);
  }

#endif
}

/** XMMS pause callback.
 *
 *    This function just call the output plugin pause function.
 */
static void nosefart_pause(short paused)
{
  pdebug("nsf : pause %s\n", paused ? "ON" : "OFF");
  if (nosefart_ip.output) {
    nosefart_ip.output->pause(paused);
  }
}

static int nosefart_get_time(void)
{
  int t2;
  if (!audio_open || !nosefart_ip.output) {
    return -2;
  }

  if (!going && !nosefart_ip.output->buffer_playing()) {
    t2 = -1;
  } else {
    t2 = nosefart_ip.output->output_time();
  }
  return t2;
}

static void nosefart_song_info(char *filename, char **title, int *length)
{
  nsf_t * nsf;

  *length = -1;
  *title = 0;
  nsf = load(filename);
  if (nsf) {
    *title = make_name(nsf->song_name, nsf->artist_name, -1, nsf->num_songs);
    *length = nsf->song_frames[0] * 1000u / (unsigned int)nsf->playback_rate;
    nsf_free(&nsf);
  }
}

/** XMMS plugin description. */
static char nosefart_description[] =
"nosefart: Nintendo Famicom music player " VERSION;

/* Called when the plugin is loaded
*/
static void nosefart_init(void)
{
  int r, frq;

  pdebug("nsf : nosefart plugin\n");
  nsf_init();
  memset(&app, 0 , sizeof(app));
  play_thread = 0;
}

/** Clean exit XMMS callback. */
static void nosefart_cleanup(void)
{
  pdebug("nsf : clean-up\n");
  if (play_thread) {
    pdebug("nsf : have a thread running. Kill it !\n");
    pthread_join(play_thread, NULL);
    play_thread = 0;
    pdebug("nsf : thread is dead\n");
  }
  pdebug("nsf : bye bye\n");
}

/** Seek XMMS callback.
 *
 *    This is used to change track in the current disk.
 */
static void nosefart_seek(int time)
{
  pdebug("nsf : seek to %d sec\n", time);

  if (going && app.nsf) {
    int i, n;
    unsigned int cnt, t = time * 1000u;
    for (cnt = 0, i = 1, n = app.n_tracks; i <= n; ++i) {
      unsigned int cnt2;

      cnt2 = cnt + app.tracks[i].ms;
      if (t >= cnt && t < cnt2) {
	pdebug("nsf : seek to #%02d\n", i);
        if (i != app.cur_track) {
	  SetChangeTrack(i, cnt);
	}
	break;

      }
      cnt = cnt2;
    }
  }
}

/** XMMS input plugin instance. */
static InputPlugin nosefart_ip = {
  NULL,                 /* !! Handle */
  NULL,                 /* !! Filename */
  nosefart_description, /* Description */
  nosefart_init,        /* Init */
  nosefart_about,       /* Show about */
  NULL,                 /* Configure */
  nosefart_is_our_file,
  NULL,                 /* Scan dir */
  nosefart_play,
  nosefart_stop,
  nosefart_pause,
  nosefart_seek,        /* Seek */
  NULL,                 /* Set eq */
  nosefart_get_time,    /* */
  NULL,                 /* get volume l/r */
  NULL,                 /* set volume l/r */
  nosefart_cleanup,     /* Exit call */
  NULL,                 /* Get vis type (OBSOLETE) */
  NULL,                 /* Sends data to vis pluggin */
  NULL,                 /* !!! Set info */
  NULL,                 /* !!! Set song title */
  nosefart_song_info,
  NULL,                 /* file info box */
  NULL                  /* Output plugin */
};

/** Get input plugin info XMMS callback. */
InputPlugin * get_iplugin_info(void)
{
  return &nosefart_ip;
}
