/*
    Copyright (C) 2004 Matthew Strait <quadong@users.sf.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifdef HAVE_CONFIG_H
   #include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"


GtkWidget *r_forever, *r_seconds, *r_reps, 
	  *c1, *c2, *c3, *c4, *c5, *c6, 
	  *fs, *help, *about, *error, *audio_error, 
	  *nferror, *spinbutton3 = NULL, *statusbar1 = NULL;
static int pid = 0, track = 1;
static int channels[7] = { 0xdeadbeef, 1, 1, 1, 1, 1, 1 };
gchar * filename = NULL, * oldfilename = NULL;

// these are the defaults
static enum { doforever, doseconds, doreps } mode = doreps;
static int seconds = 60, reps = 2;

static int writingpipe[2], readingpipe[2];

static int starttime;
static int numtracks;
static double speed = 1;

char * songname = NULL, * artist = NULL, * copyright = NULL;

static int rfnfid = 0;

void dostatusbar(char * info)
{
	static char myinfo[81] = "\0";

	// if things have changed	
	if(strcmp(myinfo, info) && statusbar1){
		char p[800];
		char * basename = strrchr(filename, '/');
		basename++;

		// Changes to GTK+ messed this up.
		snprintf(p, 799, 
			//	 "Game: %s\r\n"
			//	 "Artist: %s\r\n"
			//	 "Copyright: %s\r\n"
				 "Playing track %d of %d: %s",
			//	 songname, artist, copyright, 
			track, numtracks, info);

		gtk_statusbar_pop(GTK_STATUSBAR(statusbar1), 0);
		gtk_statusbar_push(GTK_STATUSBAR(statusbar1), 0, p);
	}

	strncpy(myinfo, info, 80);
}


static int gotnothing = 0;

int main_readfromnosefart()
{
	static int field = 0;	
	static int pos = 0;
	static int ready = 0;
	char buf[80];
	static char tempinfo[81] = "\0";
	static char info[81] = "\0";
	static int count = 0;
	int i;
	int gotinhere = 0;

	/* If not, complain.  Probably the sound card is blocked. */
	if(gotnothing == 20){ // XXX what is 20?  It doesn't seem like this happens.
		if(!audio_error) audio_error = create_audio_error();
		gtk_widget_show(audio_error);
	}
	else if(gotnothing == 0 && audio_error /* window */){
		gtk_widget_hide(audio_error);
	}

	fcntl(readingpipe[0], F_SETFL, O_NONBLOCK);

	while(0 < (count = read(readingpipe[0], &buf, 80)))
        {
		gotinhere = 1; gotnothing = 0;

		for(i = 0; i < count; i++)
		{
			if(buf[i] == ','){
				field++;
			}else if(buf[i] == '\r'){
				tempinfo[pos] = '\0';
				field = 0;
				pos = 0;
	
				if(strlen(tempinfo) > 4){ /* at least "1 sec" */
					strcpy(info, tempinfo);
					dostatusbar(info);
				}
			}else if(field == 2 && pos < 80){
				tempinfo[pos] = buf[i];
				pos++;
			}
		}
	}

	if(!gotinhere) gotnothing++;

        return 1;
}

void intro_readfromnosefart()
{
	char * foo = NULL;
	char scratch[1024];
	FILE * pipefile = fdopen(readingpipe[0], "r");
	int i, f;

	if(artist)   {free(artist);    artist    = NULL;}
	if(copyright){free(copyright); copyright = NULL;} // <-- profound!
	if(songname) {free(songname);  songname  = NULL;}

	while(1){
		getline(&foo, &f, pipefile);
		if(songname) free(songname);
		songname = malloc(strlen(foo)+1);

		if(sscanf(foo, "Song name: %[^\n]12", songname)){
			free(foo); foo = NULL;
			break;
		}else if(!strcmp("/dev/dsp is busy.\n", foo)){
			free(foo); foo = NULL;
			if(!audio_error) audio_error = create_audio_error();
			gtk_widget_show(audio_error);
			if(filename) free(filename);
			filename = NULL;
			return;
		}else if(!strncmp("Error opening", foo, strlen("Error opening"))){
			free(foo); foo = NULL;
			filename = NULL;
			if(!error) error = create_error_window();
			fprintf(stderr, "PLAYPLAYPLAYPLAYPLAY\n"); // XXX ?
			gtk_widget_show(error);
			return;
		}else{
			free(foo); foo = NULL;
		}
	}

	getline(&foo, &f, pipefile);
	artist = malloc(strlen(foo)+1);
	sscanf(foo, "Artist: %[^\n]12", artist);
	if(foo) {free(foo); foo = NULL;}

	getline(&foo, &f, pipefile);
	copyright = malloc(strlen(foo)+1);
	sscanf(foo, "Copyright: %[^\n]12", copyright);
	free(foo); foo = NULL;

	rfnfid = gtk_timeout_add(200, main_readfromnosefart, NULL);
}


void killnosefarts()
{
	int status;
	if(pid) kill(pid, 9);
	system("killall -9 nosefart > /dev/null"); /* hack, fix */
	
	/* clean up all cildren */
	while(-1 != wait3(&status, WNOHANG, NULL));
}

void cleanup()
{
	killnosefarts();
	system("stty sane; echo");
}

void sfexit()
{
	fprintf(stderr, "Segmentation fault.  Send a bug report to quadong@users.sf.net\n");
	cleanup();
	exit(1);
}

void otherexit()
{
	cleanup();
	exit(1);
}

char * itoa(int n)
{
        char * s = malloc(4);

        if(n > 999)
                abort();
        s[0] = n/100 + '0';
        s[1] = (n/10)%10 + '0';
        s[2] = n%10 + '0';
        s[3] = 0;

        return s;
}

void settrackmax()
{
	if(spinbutton3)
	{
		GtkObject * sba = gtk_adjustment_new( 1, 1, numtracks, 1, 5, 0 );
		gtk_spin_button_configure( GTK_SPIN_BUTTON(spinbutton3), 
			GTK_ADJUSTMENT(sba), 1, 0 );
		gtk_spin_button_set_wrap(  GTK_SPIN_BUTTON(spinbutton3), 1);
	}
}	

void incrementtrack()
{
	track++;
	if(track > numtracks) track = 1;
	if(spinbutton3) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton3), track);
}

/* SLOOOOOOOW.... (and terrible....) */
void getnumtracksandhandleit()
{
	FILE * tempfile;
	char tempfn[255];
	char cmd[255];

	strcpy(tempfn, "/tmp/nosefart.XXXXXX");
	mkstemp(tempfn);

	snprintf(cmd, 254, "nosefart -i '%s' | grep Number | cut -d: -f2 > %s", filename, tempfn);
	system(cmd);

	tempfile = fopen(tempfn, "r");

	fscanf(tempfile, " %d", &numtracks);

	fclose(tempfile);

	unlink(tempfn);

	settrackmax();
}

/* checks for existance and for NSFness */
int fileisgood(char * filename)
{
	gchar cmd[255];
	struct stat scratchstat;

	if(-1 == stat(filename, &scratchstat))
		return 0;
	snprintf(cmd, 254, "nosefart -i '%s' > /dev/null", filename);
	if(system(cmd))
		return 0;
	return 1;
}

void play()
{
        char * t = itoa(track);
	char * s = itoa(seconds);
	char * r = itoa(reps);
	int mypid = 0;

	if(!filename) return;

	gotnothing = 0;
	if(rfnfid) gtk_timeout_remove(rfnfid);

	if(!fileisgood(filename))
	{
		filename = NULL;
		if(!error) error = create_error_window();
		fprintf(stderr, "PLAYPLAYPLAYPLAYPLAY\n"); // XXX ?
		gtk_widget_show(error);
		return;
	}

	if(pipe(writingpipe) == -1) fprintf(stderr, "writing pipe creation failed!\n");
	if(pipe(readingpipe) == -1) fprintf(stderr, "reading pipe creation failed!\n");

        if(pid)	killnosefarts();

        pid = fork();

        if(!pid)
	{
		char * args[] = { "nosefart", "-t", t, filename,  
				NULL, NULL, NULL, NULL, NULL, NULL, /* channels */
				NULL, NULL, /* play length */
				NULL, NULL, /* speed */
				NULL /* ending NULL */ };
		int argp = 4; /* next available arg */

		int i;
		for(i = 1; i <= 6; i++){
			if(!channels[i]){
				char * newarg = malloc(3);
				snprintf(newarg, 3, "-%d", i);
				args[argp] = newarg;
				argp++;
			}
		}

		{
			char num[32];
			snprintf(num, 31, "%f", speed);

			args[argp] = "-s";
			argp++;
			args[argp] = num;
			argp++;
		}

		close(0);
	        if(dup(writingpipe[0]) == -1) fprintf(stderr, "Trouble with dup to stdin\n" );
		close(writingpipe[1]);

		close(1);
	        if(dup(readingpipe[1]) == -1) fprintf(stderr, "Trouble with dup to stdin\n" );
		close(readingpipe[0]);

		if(mode == doforever)
		{}
		else if(mode == doseconds)
		{
			args[argp] = "-l";
			argp++;
			args[argp] = s;
		}
		else if(mode == doreps)
		{
			args[argp] = "-a";
			argp++;
			args[argp] = r;
		}
	
                execvp("nosefart", args);

		/* this should only be possible if nosefart was deleted 
		between when gnosefart was started and now... */
		fprintf(stderr, "NOT REACHED.  Nosefart not found!\n");
		exit(1);
	}
	/* else */

	intro_readfromnosefart(); /* reads from nosefart's stdout */

	free(t); 
	free(s); 
	free(r);
}

int wakeupandplay()
{
	if(filename)
	{
		int status;
		if(waitpid(pid, &status, WNOHANG))
		{
			gtk_timeout_remove(rfnfid);
			incrementtrack();
			if(filename) play(); // play might discover that it can't play
					     // and set filename to NULL
			else return 1;
		}
	}
	return 1;
}


void
close_gnosefart                        (GtkObject       *object,
                                        gpointer         user_data)
{
	cleanup();
        gtk_main_quit();
}


void
track_spinbutton_changed               (GtkEditable     *editable,
                                        gpointer         user_data)
{
        int newtrack = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
	if(newtrack == track) return; /* this function gets called twice
					everytime the spinbutton is changed.  Dunno why... */
	track = newtrack;
	if(filename) 
		play();
}


void
forever_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
	if(mode != doforever) mode = doforever;
}


void
seconds_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
	if(mode != doseconds) mode = doseconds;
}


void
repetitions_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
	if(mode != doreps) mode = doreps;
}


void
seconds_spinbutton_changed             (GtkEditable     *editable,
                                        gpointer         user_data)
{
        seconds = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
}


void
repetitions_spinbutton_changed         (GtkEditable     *editable,
                                        gpointer         user_data)
{
        reps = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
}


void channel1_toggled (GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "1", 1);
	channels[1] = !channels[1];
}


void channel2_toggled (GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "2", 1);
	channels[2] = !channels[2];
}


void channel3_toggled (GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "3", 1);
	channels[3] = !channels[3];
}


void channel4_toggled (GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "4", 1);
	channels[4] = !channels[4];
}


void channel5_toggled (GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "5", 1);
	channels[5] = !channels[5];
}


void channel6_toggled(GtkToggleButton *togglebutton, gpointer         user_data)
{
	write(writingpipe[1], "6", 1);
	channels[6] = !channels[6];
}


void open_pushed(GtkButton *button, gpointer user_data)
{
      fs = create_fileselection();
      gtk_widget_show(fs);
}

// Should probably be done in a better way...
void savedirectory(char * filename)
{
	char cmd[1024];
	snprintf(cmd, 1023, "echo `dirname '%s'`/ > ~/.gnosefart", filename);
	system(cmd);
}

void handle_fileselection()
{
        filename = (gchar *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
	oldfilename = malloc(strlen(filename) + 1);
	strcpy(oldfilename, filename);

	if(!fileisgood(filename)){
		filename = NULL;
		if(!error) error = create_error_window();
		gtk_widget_show(error);
		return;
	}	

	savedirectory(filename);

	gtk_widget_hide(fs);
        play();
	if(filename) // play might have bombed 
		getnumtracksandhandleit();
}

void on_fileselection_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	switch (response_id)
	{
		/* some of these won't ever happen... */
		case GTK_RESPONSE_ACCEPT:
		case GTK_RESPONSE_OK:
		case GTK_RESPONSE_YES:
		case GTK_RESPONSE_APPLY:
			handle_fileselection();
			break;
		default:
		      gtk_widget_hide(fs);
			
	}
}


void help_pushed                            (GtkButton       *button,
                                        gpointer         user_data)
{
      help = create_dialog2();
      gtk_widget_show(help);
}

void ok_released_in_help                    (GtkButton       *button,
                                        gpointer         user_data)
{
      if(!help) help = create_dialog2();
      gtk_widget_hide(help);
}


void close_pushed                           (GtkButton       *button,
                                        gpointer         user_data)
{
	filename = NULL;
        if(pid)
		killnosefarts();

	gtk_timeout_remove(rfnfid);

	gotnothing = 0;

	track = 1;
        if(spinbutton3) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton3), track);

	gtk_statusbar_pop(GTK_STATUSBAR(statusbar1), 0);
	gtk_statusbar_push(GTK_STATUSBAR(statusbar1), 0, "No file open");
}


void closebutton_pushed_in_error_window     (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(error);	
}


void closebutton_pushed_in_nnf_error_window (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(nferror);	
	//gtk_main_quit();
}

void
closebutton_pushed_in_audio_error_window
                                        (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(audio_error);
}

void
spinbutton3_map                        (GtkWidget       *widget,
                                        gpointer         user_data)
{
	spinbutton3 = widget;
}


void
statusbar1_map                         (GtkWidget       *widget,
                                        gpointer         user_data)
{
	statusbar1 = widget;
	gtk_statusbar_set_has_resize_grip( GTK_STATUSBAR(statusbar1), 0 );
        gtk_statusbar_push(GTK_STATUSBAR(statusbar1), 0, "No file open");
}


void
about_button_pushed                    (GtkButton       *button,
                                        gpointer         user_data)
{
      about = create_about_dialog();
      gtk_widget_show(about);
}


void
ok_released_in_about                   (GtkButton       *button,
                                        gpointer         user_data)
{
      //if(!about) about = create_about_dialog();
      gtk_widget_hide(about);
}


void
on_fileselection_map                   (GtkWidget       *widget,
                                        gpointer         user_data)
{
	if(oldfilename) // get directory from last file selected
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(widget), oldfilename);
	else // attempt to get directory from last session
	{
		struct passwd * pw;
		struct stat scratchstat;
		FILE * f;	
		char olddir[1024];
		char conffilename[1024];
		int uid = geteuid();

		while(1)
		{
			pw = getpwent();
			if(!pw) return; // give up entirely if the user isn't found
			if(pw->pw_uid == uid) break; // got the user
		}
		snprintf(conffilename, 1023, "%s/.gnosefart", pw->pw_dir);

		if(-1 == stat(conffilename, &scratchstat))
		{
			fprintf(stderr, ".gnosefart not found\n");
			return;
		}

		f = fopen(conffilename, "r");
		fscanf(f, "%s", &olddir);
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(widget), olddir); 
		fclose(f);
	}
}

void
spinbutton_mult_changed                (GtkEditable     *editable,
                                        gpointer         user_data)
{
        speed = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(editable));
}



void
on_dialog2_close                       (GtkDialog       *dialog,
                                        gpointer         user_data)
{
	//if(!help) help = dialog;
	gtk_widget_hide(help);
}


void
on_dialog2_destroy                     (GtkObject       *object,
                                        gpointer         user_data)
{
	gtk_widget_hide(help);
}

/* this is needed because alt-1, etc. are often used by the window manager */
gboolean mainwindow_key_press                   (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
  switch (tolower(event->keyval))
  {
	case '1':
		gtk_toggle_button_set_active((GtkToggleButton *)c1, !gtk_toggle_button_get_active((GtkToggleButton *)c1));
		break;
	case '2':
		gtk_toggle_button_set_active((GtkToggleButton *)c2, !gtk_toggle_button_get_active((GtkToggleButton *)c2));
		break;
	case '3':
		gtk_toggle_button_set_active((GtkToggleButton *)c3, !gtk_toggle_button_get_active((GtkToggleButton *)c3));
		break;
	case '4':
		gtk_toggle_button_set_active((GtkToggleButton *)c4, !gtk_toggle_button_get_active((GtkToggleButton *)c4));
		break;
	case '5':
		gtk_toggle_button_set_active((GtkToggleButton *)c5, !gtk_toggle_button_get_active((GtkToggleButton *)c5));
		break;
	case '6':
		gtk_toggle_button_set_active((GtkToggleButton *)c6, !gtk_toggle_button_get_active((GtkToggleButton *)c6));
		break;
  }
  return FALSE;
}


void on_togglebutton1_map                   (GtkWidget       *widget,                                        gpointer         user_data)
{
	c1 = widget;
}

void on_togglebutton2_map                   (GtkWidget       *widget,                                         gpointer         user_data)
{
	c2 = widget;

}

void on_togglebutton3_map                   (GtkWidget       *widget,                                         gpointer         user_data)
{
	c3 = widget;
}

void on_togglebutton4_map                   (GtkWidget       *widget,                                        gpointer         user_data)
{
	c4 = widget;
}

void on_togglebutton5_map                   (GtkWidget       *widget,                                        gpointer         user_data)
{
	c5 = widget;
}

void on_togglebutton6_map                   (GtkWidget       *widget,                                        gpointer         user_data)
{
	c6 = widget;
}

void forever_button_map                     (GtkWidget       *widget,                                        gpointer         user_data)
{
	r_forever = widget;
}

void seconds_button_map                     (GtkWidget       *widget,                                        gpointer         user_data)
{
	r_seconds = widget;
}

void repetitions_button_map                 (GtkWidget       *widget,                                        gpointer         user_data)
{
	r_reps = widget;
}

