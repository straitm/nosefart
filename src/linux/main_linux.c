/* Modified by Matthew Strait Oct-Nov 2002 */
/* Note that this runs in FreeBSD as well as Linux (and may well run in other
UNIX systems */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/soundcard.h>
#include <sys/stat.h>

#include "types.h"
#include "nsf.h"

#include "config.h"

/* for auto-time calculation.  We ignore most of the stuff in there, but I 
just including this is easier than trying to rip out just the relevant bits
and nothing else. */
#include <nsfinfo.h>

/* for shared memory used in auto-time calc */
#include        <sys/types.h>
#include        <sys/wait.h>
#include        <sys/ipc.h>
#include        <sys/shm.h>


#ifndef bool
  #define bool boolean
#endif
#ifndef true
  #define true TRUE
#endif

/* This saves the trouble of passing our ONE nsf around the stack all day */
static nsf_t *nsf = 0;

/* thank you, nsf_playtrack, for making me pass freq and bits to you */
static int freq = 44100;
static int bits = 8;

/* sound */
static int dataSize;
static int bufferSize;
static unsigned char *buffer = 0, *bufferPos = 0;
static int audiofd;

static int * plimit_frames = NULL;
static int shm_id;

static int frames; /* I like global variables too much */

static struct termios oldterm;

/* whether the channels are enabled or not.  Moved out here by Matthew Strait
so that it could be displayed */
static bool enabled[6] = { true, true, true, true, true, true };
 
static int pid = 1; /* something non-zero by default */

/* takes the number of repetitions desired and returns the number of frames
to play */
static int get_time(int repetitions, char * filename, int track)
{
	/* raw result, with intro, without intro */
	int result, wintro, wointro;

	result = time_info(filename, track);

	wintro = result / 0x1000;
	wointro = result % 0x1000;

	return wintro + (repetitions - 1)*wointro;
}

void cleanup(int sig)
{
     /* Clean up the shared memory. */
     shmctl( shm_id, IPC_RMID, NULL );
     shmdt( plimit_frames );

     /* if the child, which doesn't know about the new or old terminal modes,
     does this, it screws up the terminal. */ 
     if(pid != 0) 
       tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);

     fprintf(stderr, "\n");

     exit(0);
}

void dowait(int sig)
{
	wait(&sig); /* sig is a throwaway */
}

/* If all goes well, this spawns a child process that does the time
calculation while letting the parent start playing the music.  When the
child has the answer, it puts it in shared memory and exits.  If
something goes wrong, it falls back to having the parent do the calculation. */
void handle_auto_calc(char * filename, int track, int reps)
{
        /* When it's time to exit, the parent needs to know to clean up. */
	/* I could exhaustively list signals that result in termination here,
	but these are by far the most common. */
        if( signal(SIGTERM, cleanup) == SIG_ERR ||
	    signal(SIGINT, cleanup) == SIG_ERR ||
	    signal(SIGHUP, cleanup) == SIG_ERR ||
	    signal(SIGUSR1, dowait) == SIG_ERR )
	{
                fprintf( stderr, "Trouble setting signal handlers.\n" );
		exit(1);
	}

        /* Create the shared memory. */
	shm_id = shmget( IPC_PRIVATE, 1, 0664 | IPC_CREAT | IPC_EXCL  );
        if( shm_id == -1 )
        {
                perror( "Trouble with shared memory (1)" );
                fprintf(stderr, "Falling back to the slow approach...\n\n");
        	goto fallback;
	} 

       	/* Attach the shared memory to the current process' address space. */
       	plimit_frames = (int *)shmat( shm_id, (int *)0, 0 );
       	if( plimit_frames == (int *)-1 )
       	{
       	        perror( "Trouble with shared memory (2)" );
       	        shmctl( shm_id, IPC_RMID, NULL );
                fprintf(stderr, "Falling back to the slow approach...\n\n");
        	goto fallback;
       	}

	/* 0 for unlimited until the time calculation returns */
	*plimit_frames = 0;

	pid = fork();
	if(pid == -1)
	{
		perror("fork failed");
		fprintf(stderr, "Falling back to the slow approach...\n\n");
		goto fallback;
	}
	/* the child does the auto-time calculation */
	else if( pid == 0)
	{
		*plimit_frames = get_time(reps, filename, track);

                /* Clean up the shared memory. */
                shmctl( shm_id, IPC_RMID, NULL );
                shmdt( plimit_frames );

		/* Hey!  wait() for me! */
		/* Actually, just let the child be a zombie until the parent
		finishes.  Making the parent wait() often makes the music skip */
		/* I could insert a sleep(100000) here to prevent people from
		saying "What's with this zombie thing?!?", but that wastes
		memory */
		//kill(getppid(), SIGUSR1);

		exit(0);
	}

	/* meanwhile, the parent moves on and starts playing music */
	return;

	fallback:
		*plimit_frames = get_time(reps, filename, track);
}

/* HAS ROOT PERMISSIONS -- BE CAREFUL */
/* Open up the DSP, then drop the root permissions */
static void open_hardware(const char *device)
{
   struct stat status;

   /* Open the file (with root permissions, if we have them) */
   if(-1 == (audiofd = open(device, O_WRONLY, 0)))
   {
      switch(errno)
      {
      case EBUSY:
         printf("%s is busy.\n", device);
         exit(1);
      default:
         printf("Unable to open %s.  Check the permissions.\n", device);
         exit(1);
      }
   }

   /* For safety, we should check that device is, in fact, a device.
      `nosefart -d /etc/passwd MegaMan2.nsf` wouldn't sound so pretty. */
   if(-1 == fstat(audiofd, &status))
   {
      switch(errno)
      {
      case EFAULT:
      case ENOMEM:
         printf("Out of memory.\n");
         exit(1);
      case EBADF:
      case ELOOP:
      case EACCES:
      default:
         printf("Unable to stat %s.\n", device);
         exit(1);
      }
   }
   /* if it's not a char device and it's not /dev/dsp */
   /* The second check is because when run with esddsp, /dev/dsp
   doesn't show up as a char device. The original author (Matthew Conte) seems
   to have thought that esddsp should work without this hack.  Is doing this 
   bad? --Matthew Strait */
   if( !S_ISCHR(status.st_mode) && strcmp("/dev/dsp", device))
   {
      printf("%s is not a character device.\n", device);
      exit(1);
   }

   /* Drop root permissions */
   if(geteuid() != getuid()) setuid(getuid());
}

/* Configure the DSP */
static void init_hardware(void)
{
   int stereo = 0;
   int param, retval, logDataSize;
   int format;
   
   switch(bits)
   {
   case 8:
      format = AFMT_U8;
      break;
   case 16:
      format = AFMT_S16_NE;
      break;
   default:
      printf("Bad sample depth: %i\n", bits);
      exit(1);
   }
   
   /* sound buffer */
   dataSize = freq / nsf->playback_rate * (bits / 8);

   /* Configure the DSP */
   logDataSize = -1;
   while((1 << ++logDataSize) < dataSize);
   param = 0x10000 | logDataSize + 4;
   retval = ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &param);
   if(-1 == retval)
   {
      printf("Unable to set buffer size\n");
   }
   param = stereo;
   retval = ioctl(audiofd, SNDCTL_DSP_STEREO, &param);
   if(retval == -1 || param != stereo)
   {
      printf("Unable to set audio channels.\n");
      exit(1);
   }
   param = format;
   retval = ioctl(audiofd, SNDCTL_DSP_SETFMT, &param);
   if(retval == -1 || param != format)
   {
      printf("Unable to set audio format.\n");
      printf("Wanted %i, got %i\n", format, param);
      exit(1);
   }
   param = freq;
   retval = ioctl(audiofd, SNDCTL_DSP_SPEED, &param);
   if(retval == -1 || (abs (param - freq) > (freq / 10)))
   {
      printf("Unable to set audio frequency.\n");
      printf("Wanted %i, got %i\n", freq, param);
      exit(1);
   }
   retval = ioctl(audiofd, SNDCTL_DSP_GETBLKSIZE, &param);
   if(-1 == retval)
   {
      printf("Unable to get buffer size\n");
      exit(1);
   }
   /* set up our data buffer */
   bufferSize = param;
   buffer = malloc((bufferSize / dataSize + 1) * dataSize);
   bufferPos = buffer;
   memset(buffer, 0, bufferSize);
}

/* close what we've opened */
static void close_hardware(void)
{
   close(audiofd);
   free(buffer);
   buffer = 0;
   bufferSize = 0;
   bufferPos = 0;
}

static void show_help(void)
{
   printf("Usage: %s [OPTIONS] filename\n", NAME);
   printf("Play an NSF (NES Sound Format) file.\n");
   printf("\nOptions:\n");
   printf("\t-h  \tHelp\n");
   printf("\t-v  \tVersion information\n");
   printf("\n\t-t x\tStart playing track x (default: 1)\n");
   printf("\n\t-d x\tUse device x (default: /dev/dsp)\n");
   printf("\t-s x\tPlay at x times the normal speed.\n");
   printf("\t-f x\tUse x sampling rate (default: 44100)\n");
   printf("\t-B x\tUse sample size of x bits (default: 8)\n");
   printf("\t-l x\tLimit total playing time to x seconds (0 = unlimited)\n");
   printf("\t-r x\tLimit total playing time to x frames (0 = unlimited)\n");
   printf("\t-b x\tSkip the first x frames\n");
   printf("\t-a x\tCalculate song length and play x repetitions (0 = intro only)\n");
   printf("\t-i\tJust print file information and exit\n");
   printf("\t-x\tStart with track x disabled (1 <= x <= 6)\n");
   printf("\nPlease send bug reports to quadong@users.sf.net\n");
   exit(0);
}


static void show_warranty(void)
{
  printf("\nThis software comes with absolutely no warranty. ");
  printf("You may redistribute this\nprogram under the terms of the GPL. ");
  printf("(See the file COPYING in the source tree.)\n\n");
}

static void show_info(void)
{
   printf("%s -- NSF player for Linux\n", NAME);
   printf("Version " VERSION " ");
   printf("Compiled with GCC %i.%i on %s %s\n", __GNUC__, __GNUC_MINOR__,
         __DATE__, __TIME__);
   printf("\nNSF support by Matthew Conte. ");
   printf("Inspired by the MSP 0.50 source release by Sevy\nand Marp. ");
   printf("Ported by Neil Stevens.  Some changes by Matthew Strait.\n");

   exit(0);
}

static void printsonginfo(int current_frame, int total_frames, int limited)
{
   /*Why not printf directly?  Our termios hijinks for input kills the output*/
   char *hi = (char *)malloc(255);
   char blank[82]; 
   memset(blank, ' ', 80);
   blank[80] = '\r';
   blank[81] = '\0';

   snprintf(hi, 254, 
   total_frames != 0 ? 
   "Playing track %d/%d, channels %c%c%c%c%c%c, %d/%d sec, %d/%d frames\r":
   (limited ?
   "Playing track %d/%d, channels %c%c%c%c%c%c, %d/? sec, %11$d/? frames (Working...)\r":
   "Playing track %d/%d, channels %c%c%c%c%c%c, %d sec, %11$d frames\r"), 
   nsf->current_song, nsf->num_songs, 
   enabled[0]?'1':'-',      enabled[1]?'2':'-',
   enabled[2]?'3':'-',      enabled[3]?'4':'-',
   enabled[4]?'5':'-',      enabled[5]?'6':'-',
   abs((int)((float)(current_frame + nsf->playback_rate)/(float)nsf->playback_rate) - 1), /* the sound gets buffered a lot... */
   abs((int)((float)(total_frames + nsf->playback_rate)/(float)nsf->playback_rate) -1),   /* this is something of an estimate */
   current_frame,
   total_frames
   );

   if(!(current_frame%10))
      write(STDOUT_FILENO, (void *)blank, strlen(blank));

   write(STDOUT_FILENO, (void *)hi, strlen(hi));
   free(hi);
}

static void sync_channels(void)
{
   int channel;
   /* this is going to get run when a track starts and all channels start out 
      enabled, so just turn off the right ones */
   for( channel = 0; channel < 6; channel++)
      if(!enabled[channel])
         nsf_setchan(nsf, channel, enabled[channel]);
}

/* start track, display which it is, and what channels are enabled */
static void nsf_setupsong(void)
{
   printsonginfo(0, 0, 0);
   nsf_playtrack(nsf, nsf->current_song, freq, bits, 0);
   sync_channels();
   return;
}

/* display info about an NSF file */
static void nsf_displayinfo(void)
{
   printf("Keys:\n");
   printf("q:\tquit\n");
   printf("x:\tplay the next track\n");
   printf("z:\tplay the previous track\n");
   printf("return:\trestart track\n");
   printf("1-6:\ttoggle channels\n\n");

   printf("NSF Information:\n");
   printf("Song name:       %s\n", nsf->song_name);
   printf("Artist:          %s\n", nsf->artist_name);
   printf("Copyright:       %s\n", nsf->copyright);
   printf("Number of Songs: %d\n\n", nsf->num_songs);

   fflush(stdout); /* needed for gnosefart */
}

/* handle keypresses */
static int nsf_handlekey(char ch, int doautocalc, char * filename, int reps)
{
   ch = tolower(ch);
   switch(ch)
   {
   case 'q':
   case 27: /* escape */
      return 1;
   case 'x':
      /* wrapping around added by Matthew Strait */
      if(nsf->current_song == nsf->num_songs)
	nsf->current_song = 1;
      else
        nsf->current_song++;
      nsf_setupsong();
      frames = 0;      
      if(doautocalc)
      {
	int foo;
	/* clean up previous calculation */
        wait(&foo); /* ch is a throwaway */
	handle_auto_calc(filename, nsf->current_song, reps);
      }      
      break;
   case 'z':
      if(1 == nsf->current_song)
	nsf->current_song = nsf->num_songs;
      else
        nsf->current_song--;
      nsf_setupsong();
      frames = 0;      
      if(doautocalc)
      {
	int foo;
	/* clean up previous calculation */
        wait(&foo); /* ch is a throwaway */
	handle_auto_calc(filename, nsf->current_song, reps);
      }      
      break;
   case '\n':
      nsf_playtrack(nsf, nsf->current_song, freq, bits, 0);
      sync_channels();
      break;
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':
   case '6':
      enabled[ch - '1'] = !enabled[ch - '1'];
      nsf_setchan(nsf, ch - '1', enabled[ch - '1']);
//      printsonginfo(0, 0, 0);
      break;
   default:
      break;
   }
   return 0;
}

/* load the nsf, so we know how to set up the audio */
static int load_nsf_file(char *filename)
{
   nsf_init();
   /* load up an NSF file */
   nsf = nsf_load(filename, 0, 0);
   if(!nsf)
   {
      printf("Error opening \"%s\"\n", filename);
      exit(1);
   }
}

static void setup_term()
{
   struct termios term;

   /* UI settings */
   tcgetattr(STDIN_FILENO, &term);
   oldterm = term;
   term.c_lflag &= ~ICANON;
   term.c_lflag &= ~ECHO;
   tcsetattr(STDIN_FILENO, TCSANOW, &term);

   fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
}

/* Make some noise, run the show */
static void play(char * filename, int track, int doautocalc, int reps, int starting_frame, int limited)
{
   int done = 0;
   int starting_time = time(NULL) - 1; /* the 1 helps with rounding error */
   frames = 0;

   /* determine which track to play */
   if(track > nsf->num_songs || track < 1)
   {
      nsf->current_song = nsf->start_song;
      fprintf(stderr, "track %d out of range, playing track %d\n", track,
         nsf->current_song);
   }
   else
      nsf->current_song = track;


   /* display file information */
   nsf_displayinfo();
   nsf_setupsong();

   setup_term();

   while(!done)
   {
      char ch;
      int foo;

      nsf_frame(nsf);
      frames++;

      printsonginfo(frames, *plimit_frames, limited);

      /* don't waste time if skipping frames (this check speeds it up a lot) */
      if(frames >= starting_frame)
         apu_process(bufferPos, dataSize / (bits / 8));
      
      bufferPos += dataSize;
      
      if(bufferPos >= buffer + bufferSize)
      {
         if(frames >= starting_frame)
           write(audiofd, buffer, bufferPos - buffer);
         bufferPos = buffer;
      }

      if(*plimit_frames != 0 && frames >= *plimit_frames)
      {
	done = 1;
	//nsf_handlekey('x', doautocalc, filename, reps); /* an idea */
      }

      if(1 == fread((void *)&ch, 1, 1, stdin))
         done = nsf_handlekey(ch, doautocalc, filename, reps);
   }
   tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
   fprintf(stderr, "\n");
}

/* free what we've allocated */
static void close_nsf_file(void)
{
   nsf_free(&nsf);
   nsf = 0;
}

/* HAS ROOT PERMISSIONS -- BE CAREFUL */
int main(int argc, char **argv)
{
   char *device = "/dev/dsp";
   char *filename;
   int track = 1;
   int done = 0;
   int justdisplayinfo = 0;
   int doautocalc = 0;
   int reps = 0, limit_time = 0, starting_frame = 0;
   int limited = 0;
   float speed_multiplier = 1;

   /* parse options */
   const char *opts = "123456hvid:t:f:B:s:l:r:b:a:";

   plimit_frames = (int *)malloc(sizeof(int));
   plimit_frames[0] = 0;

   while(!done)
   {
      char c;
      switch(c = getopt (argc, argv, opts))
      {
      case EOF:
         done = 1;
         break;
      case '1' ... '6':
         enabled[(int)(c - '0' - 1)] = 0;
	 break;
      case 'v':
         show_info();
         break;
      case 'd':
         device = malloc( strlen(optarg) + 1 );
         strcpy(device, optarg);
         break;
      case 't':
         track = strtol(optarg, 0, 10);
         break;
      case 'f':
         freq = strtol(optarg, 0, 10);
         break;
      case 'B':
         bits = strtol(optarg, 0, 10);
         break;
      case 's':
	 speed_multiplier = atof(optarg);
	 break;
      case 'i':
	justdisplayinfo = 1;
	break;
      case 'l':
	limit_time = atoi(optarg);
	limited = 1;
	break;
      case 'r':
	*plimit_frames = atoi(optarg);
	limited = 1;
	break;
      case 'b':
        starting_frame = atoi(optarg);
        break;
      case 'a':
	doautocalc = 1;
	limited = 1;
	reps = atoi(optarg);
	break;
      case 'h':
      case ':':
      case '?':
      default:
         show_help();
         break;
      }
   }

   show_warranty();

   /* filename comes after all other options */
   if(argc <= optind)
   show_help();
   filename = malloc( strlen(argv[optind]) + 1 );
   strcpy(filename, argv[optind]);

   if(doautocalc)
   {
        printf(
          "Using song length calculation. Note that this isn't perfectly accurate.\n\n");

	handle_auto_calc(filename, track, reps);
   }
   /* open_hardware uses, then discards, root permissions */
   if(!justdisplayinfo) 
	open_hardware(device);

   load_nsf_file(filename);

   nsf->playback_rate *= speed_multiplier;

   if(justdisplayinfo)
	nsf_displayinfo();
   else
   {
	init_hardware();   

	if(limit_time != 0)
		*plimit_frames = limit_time*nsf->playback_rate;

	play(filename, track, doautocalc, reps, starting_frame, limited);
   }

   close_nsf_file();

   if(!justdisplayinfo) 
	close_hardware();

   return 0;
}
