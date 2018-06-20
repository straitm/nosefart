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
** in_nsf.c
**
** DLL-specific routines
** $Id: in_nsf.c,v 1.14 2000/07/04 04:44:30 matt Exp $
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "resource.h"
#include "winamp.h"
#include <stdlib.h>

/* NSF specific stuff */
#include "types.h"
#include "log.h"
#include "version.h"
#include "nsf.h"

static In_Module mod;
static char lastfn[MAX_PATH];
static int last_song;
static int decode_pos_ms;
static int kill_thread = FALSE;
static boolean paused = FALSE;
static HANDLE thread_handle = INVALID_HANDLE_VALUE;

/* Bleh.  This whole subclassing thing is based on mamiya's
** NEZAmp code, and it unnecessarily complex and obfuscated.
** If I had any inclination to learn windows, i might fix it.
*/
static struct
{
	BOOL enable;
	HWND hwndWA;
	WNDPROC wpOrigWA;
   HWND hwndEQ;
	WNDPROC wpOrigEQ;
	HWND hwndPE;
	WNDPROC wpOrigPE;
} subclasswinamp = { FALSE };

typedef enum {
	SUBCLASS_WA,
	SUBCLASS_EQ,
	SUBCLASS_PE
} SUBCLASS_TYPE;

static void SubclassWinamp(void);
static void UnsubclassWinamp(void);

/* post this to the main window at EOF (after playback has stopped) */
#define  hwndWinamp        mod.hMainWindow

/* option state type */
typedef struct
{
   int sample_rate;
   int sample_bits;
   int num_chan;
   int priority;
   int filter;
   boolean enabled[6];
} opstate_t;

#define DEFAULT_SAMPRATE   44100
#define DEFAULT_BPS        16
#define DEFAULT_NUMCHAN    1

static opstate_t *nsf_options;

/* osd logging shit */
#ifdef OSD_LOG
void osd_loginit(void) {}
void osd_logshutdown(void) {}
void osd_logprint(const char *string)
{
   MessageBox(NULL, string, "nosefart/debug - listen up!", MB_ICONINFORMATION);
}
#endif /* OSD_LOG */

static nsf_t *nsf_info = NULL;

/* decode thread procedure */
static DWORD WINAPI __stdcall innsf_playthread(void *b);

/* avoid CRT. Evil. Big. Bloated.*/
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
   return TRUE;
}

static BOOL SubclassWinampCommon(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, SUBCLASS_TYPE type)
{
   int list_pos, list_num;

   switch (uMsg)
   {
   case WM_WA_IPC:
      switch (lParam)
      {
      case IPC_PLAYFILE:
      case IPC_STARTPLAY:
      case IPC_SETPLAYLISTPOS:   /* ???? */
         UnsubclassWinamp();
         break;
      }
      break;
   case WM_DROPFILES:
      if (type != SUBCLASS_PE)
         UnsubclassWinamp();
      break;
   case WM_LBUTTONDBLCLK:
      if (type == SUBCLASS_PE)
         UnsubclassWinamp();
      break;
   case WM_COPYDATA:
      if (((COPYDATASTRUCT *)lParam)->dwData == IPC_PLAYFILE)
         UnsubclassWinamp();
      break;
   case WM_CLOSE:
      if (type == SUBCLASS_WA)
      {
         UnsubclassWinamp();
         PostMessage(hwnd, uMsg, wParam, lParam);  /* re-post */
         return FALSE;
      }
      break;
   case WM_WA_MPEG_EOF:
      if (type == SUBCLASS_WA)
      {
         nsf_info->current_song++;
         if (nsf_info->current_song > nsf_info->num_songs)
         {
            nsf_info->current_song = nsf_info->num_songs;

            list_pos = SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_GETLISTPOS);
            list_num = SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_GETLISTLENGTH);

            /* position is 0-based, but length is 1 based, thanks nullsoft */
            if (list_pos >= (list_num - 1))
               break;

            UnsubclassWinamp();

            /* go to next NSF */
            PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON5, 0);
         }
         else
            /* just play the damn thing */
            PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON2, 0);

         return FALSE;
      }
      break;
   case WM_COMMAND:
   case WM_SYSCOMMAND:
      if (0x87d0 <= LOWORD(wParam) && LOWORD(wParam) < 40000)
      {
         /* BOOKMARK */
         UnsubclassWinamp();
         break;
      }

      /* handle a boatload of cases */
      switch (LOWORD(wParam))
      {

      /* handle the back buttons */
      case WINAMP_JUMP10BACK:
      case WINAMP_BUTTON1:    /* back button */
         
         /* should we go to the previous NSF? */
         if (nsf_info->current_song <= 1)
         {
            nsf_info->current_song = 1;
            
            list_pos = SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_GETLISTPOS);

            /* back it up */
            if (list_pos > 0)
            {
               UnsubclassWinamp();
               PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON1, 0);
            }
            
            return FALSE;
         }

         /* nope, inside an NSF */
         if (LOWORD(wParam) == WINAMP_JUMP10BACK)
            nsf_info->current_song -= 10;
         else
            nsf_info->current_song--;

         if (nsf_info->current_song < 1)
            nsf_info->current_song = 1;

         if (SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_ISPLAYING))
         {
            /* is paused? */
            if (3 == SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_ISPLAYING))
            {
               UnsubclassWinamp();
               PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON3, 0);
            }
            PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON2, 0);
         }

         return FALSE;

      case WINAMP_BUTTON2:       /* play current file */
         break;

      case WINAMP_BUTTON3:       /* pause */
         break;

      case WINAMP_BUTTON4:       /* stop */
         UnsubclassWinamp();
         PostMessage(hwnd, uMsg, wParam, lParam);  /* re-post */
         break;

      case WINAMP_BUTTON5: /* fwd button */
      case WINAMP_JUMP10FWD:
         if (nsf_info->current_song >= nsf_info->num_songs)
         {
            nsf_info->current_song = nsf_info->num_songs;

            list_pos = SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_GETLISTPOS);
            list_num = SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
         
            if (list_pos < list_num - 1)
            {
               UnsubclassWinamp();
               PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON5, 0);
            }
            return FALSE;
         }

         if (LOWORD(wParam) == WINAMP_JUMP10FWD)
            nsf_info->current_song += 10;
         else
            nsf_info->current_song++;

         if (nsf_info->current_song > nsf_info->num_songs) 
            nsf_info->current_song = nsf_info->num_songs;

         if (SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_ISPLAYING))
         {
            /* is paused? */
            if (3 == SendMessage(subclasswinamp.hwndWA, WM_WA_IPC, 0, IPC_ISPLAYING))
            {
               UnsubclassWinamp();
               PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON3, 0);
            }
            PostMessage(hwnd, WM_COMMAND, WINAMP_BUTTON2, 0);
         }
         
         return FALSE;

      case WINAMP_FILE_QUIT:
         UnsubclassWinamp();
         PostMessage(hwnd, uMsg, wParam, lParam);  /* re-post */
         return FALSE;

      case WINAMP_FILE_PLAY:     /* open file */
      case IDC_PLAYLIST_PLAY:
      case WINAMP_JUMP:
      case WINAMP_JUMPFILE:
      case WINAMP_FILE_LOC:      /* open URL */
      case WINAMP_FILE_DIR:      /* open folder */

      case WINAMP_BUTTON2_CTRL:
      case WINAMP_BUTTON2_SHIFT:

      case WINAMP_BUTTON4_CTRL:  /* stop list */
      case WINAMP_BUTTON4_SHIFT: /* stop with fadeout */
      case WINAMP_BUTTON1_CTRL:  /* play top of list */
      case WINAMP_BUTTON5_CTRL:  /* play end of list */
         UnsubclassWinamp();
         break;
      }
      break;
   }
   return TRUE;
}


static LRESULT CALLBACK SubclassWAProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   WNDPROC wpOrig = subclasswinamp.wpOrigWA;
   if (!subclasswinamp.enable || SubclassWinampCommon(hwnd, uMsg, wParam, lParam, SUBCLASS_WA))
      return CallWindowProc(wpOrig, hwnd, uMsg, wParam, lParam); 
   else
      return 0;
}

static LRESULT CALLBACK SubclassEQProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   WNDPROC wpOrig = subclasswinamp.wpOrigEQ;
   if (!subclasswinamp.enable || SubclassWinampCommon(hwnd, uMsg, wParam, lParam, SUBCLASS_EQ))
      return CallWindowProc(wpOrig, hwnd, uMsg, wParam, lParam); 
   else
      return 0;
}

static LRESULT CALLBACK SubclassPEProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   WNDPROC wpOrig = subclasswinamp.wpOrigPE;
   if (!subclasswinamp.enable || SubclassWinampCommon(hwnd, uMsg, wParam, lParam, SUBCLASS_PE))
      return CallWindowProc(wpOrig, hwnd, uMsg, wParam, lParam); 
   else
      return 0;
}

static void UnsubclassWinamp(void)
{
   if (subclasswinamp.enable)
   {
      subclasswinamp.enable = FALSE;
      if (subclasswinamp.hwndWA != NULL)
      {
         SetWindowLong(subclasswinamp.hwndWA, GWL_WNDPROC, (LONG)subclasswinamp.wpOrigWA);
         subclasswinamp.hwndWA = NULL;
      }
      if (subclasswinamp.hwndEQ != NULL)
      {
         SetWindowLong(subclasswinamp.hwndEQ, GWL_WNDPROC, (LONG)subclasswinamp.wpOrigEQ);
         subclasswinamp.hwndEQ = NULL;
      }
      if (subclasswinamp.hwndPE != NULL)
      {
         SetWindowLong(subclasswinamp.hwndPE, GWL_WNDPROC, (LONG)subclasswinamp.wpOrigPE);
         subclasswinamp.hwndPE = NULL;
      }   }
}

typedef struct
{
   DWORD dwThreadId;
   LPCSTR lpszClassName;
   HWND hwndRet;
} FINDTHREADWINDOW_WORK;

static BOOL CALLBACK FindThreadWindowsProc(HWND hwnd, LPARAM lParam)
{
   FINDTHREADWINDOW_WORK *pWork = (FINDTHREADWINDOW_WORK *)lParam;
   if (GetWindowThreadProcessId(hwnd, NULL) == pWork->dwThreadId)
   {
      CHAR szClassName[MAX_PATH];
      if (GetClassName(hwnd, szClassName, MAX_PATH))
      {
         if (lstrcmp(szClassName, pWork->lpszClassName) == 0)
         {
            pWork->hwndRet = hwnd;
            return FALSE;
         }
      }
   }
   return TRUE;
}

static HWND FindThreadWindow(LPCSTR lpszClassName, DWORD dwThreadId)
{
   FINDTHREADWINDOW_WORK fwww;
   fwww.dwThreadId = dwThreadId;
   fwww.lpszClassName = lpszClassName;
   fwww.hwndRet = NULL;
   EnumWindows(FindThreadWindowsProc, (LONG)&fwww);
   return fwww.hwndRet;
}

static void SubclassWinamp(void)
{
   if (!subclasswinamp.enable && hwndWinamp != NULL)
   {
      DWORD dwThreadId;
      subclasswinamp.hwndWA = hwndWinamp;
      dwThreadId = GetWindowThreadProcessId(subclasswinamp.hwndWA, NULL);
      subclasswinamp.wpOrigWA = (WNDPROC)SetWindowLong(subclasswinamp.hwndWA, GWL_WNDPROC, (LONG)SubclassWAProc);
      subclasswinamp.hwndEQ = FindThreadWindow("Winamp EQ", dwThreadId);
      if (subclasswinamp.hwndEQ != NULL)
         subclasswinamp.wpOrigEQ = (WNDPROC)SetWindowLong(subclasswinamp.hwndEQ, GWL_WNDPROC, (LONG)SubclassEQProc);
      subclasswinamp.hwndPE = FindThreadWindow("Winamp PE", dwThreadId);
      if (subclasswinamp.hwndPE != NULL)
         subclasswinamp.wpOrigPE = (WNDPROC)SetWindowLong(subclasswinamp.hwndPE, GWL_WNDPROC, (LONG)SubclassPEProc);
      subclasswinamp.enable = TRUE;
   }
}

static void load_options(opstate_t *op, char *filename)
{
   char ini_filename[MAX_PATH + 1];
   char string[80];

   GetModuleFileName(mod.hDllInstance, ini_filename, MAX_PATH);
   strcpy(strrchr(ini_filename, '\\') + 1, filename);
   
   GetPrivateProfileString("Nosefart", "Frequency", "44100", string, 80, ini_filename);
   op->sample_rate = atoi(string);

   GetPrivateProfileString("Nosefart", "ThreadPriority", "Higher", string, 80, ini_filename);
   if (0 == strcmp(string, "Highest"))
      op->priority = THREAD_PRIORITY_HIGHEST;
   else if (0 == strcmp(string, "Normal"))
      op->priority = THREAD_PRIORITY_NORMAL;
   else if (0 == strcmp(string, "Lower"))
      op->priority = THREAD_PRIORITY_BELOW_NORMAL;
   else if (0 == strcmp(string, "Lowest"))
      op->priority = THREAD_PRIORITY_LOWEST;
   else /* default */
      op->priority = THREAD_PRIORITY_ABOVE_NORMAL;

   GetPrivateProfileString("Nosefart", "BitsPerSample", "8", string, 80, ini_filename);
   op->sample_bits = atoi(string);
   
   GetPrivateProfileString("Nosefart", "NumChannels", "1", string, 80, ini_filename);
   op->num_chan = atoi(string);

   GetPrivateProfileString("Nosefart", "Filter", "0", string, 80, ini_filename);
   op->filter = atoi(string);

   GetPrivateProfileString("Nosefart", "Chan1Enabled", "1", string, 80, ini_filename);
   op->enabled[0] = (boolean) atoi(string);
   GetPrivateProfileString("Nosefart", "Chan2Enabled", "1", string, 80, ini_filename);
   op->enabled[1] = (boolean) atoi(string);
   GetPrivateProfileString("Nosefart", "Chan3Enabled", "1", string, 80, ini_filename);
   op->enabled[2] = (boolean) atoi(string);
   GetPrivateProfileString("Nosefart", "Chan4Enabled", "1", string, 80, ini_filename);
   op->enabled[3] = (boolean) atoi(string);
   GetPrivateProfileString("Nosefart", "Chan5Enabled", "1", string, 80, ini_filename);
   op->enabled[4] = (boolean) atoi(string);
   GetPrivateProfileString("Nosefart", "Chan6Enabled", "1", string, 80, ini_filename);
   op->enabled[5] = (boolean) atoi(string);
}

static void save_options(opstate_t *op, char *filename)
{
   char ini_filename[MAX_PATH + 1];
   char string[80];

   GetModuleFileName(mod.hDllInstance, ini_filename, MAX_PATH);
   strcpy(strrchr(ini_filename, '\\') + 1, filename);

   _itoa(op->sample_rate, string, 10);
   WritePrivateProfileString("Nosefart", "Frequency", string, ini_filename);
      
   switch (op->priority)
   {
   case THREAD_PRIORITY_HIGHEST:
      strcpy(string, "Highest");
      break;
   case THREAD_PRIORITY_NORMAL:
      strcpy(string, "Normal");
      break;
   case THREAD_PRIORITY_BELOW_NORMAL:
      strcpy(string, "Lower");
      break;
   case THREAD_PRIORITY_LOWEST:
      strcpy(string, "Lowest");
      break;
   default:
      strcpy(string, "Higher");
      break;
   }
   WritePrivateProfileString("Nosefart", "ThreadPriority", string, ini_filename);

   _itoa(op->sample_bits, string, 10);
   WritePrivateProfileString("Nosefart", "BitsPerSample", string, ini_filename);
   _itoa(op->num_chan, string, 10);
   WritePrivateProfileString("Nosefart", "NumChannels", string, ini_filename);
   _itoa(op->filter, string, 10);
   WritePrivateProfileString("Nosefart", "Filter", string, ini_filename);

   WritePrivateProfileString("Nosefart", "Chan1Enabled", op->enabled[0] ? "1" : "0", ini_filename);
   WritePrivateProfileString("Nosefart", "Chan2Enabled", op->enabled[1] ? "1" : "0", ini_filename);
   WritePrivateProfileString("Nosefart", "Chan3Enabled", op->enabled[2] ? "1" : "0", ini_filename);
   WritePrivateProfileString("Nosefart", "Chan4Enabled", op->enabled[3] ? "1" : "0", ini_filename);
   WritePrivateProfileString("Nosefart", "Chan5Enabled", op->enabled[4] ? "1" : "0", ini_filename);
   WritePrivateProfileString("Nosefart", "Chan6Enabled", op->enabled[5] ? "1" : "0", ini_filename);
}

BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   switch (msg)
   {
   case WM_INITDIALOG:
      return (TRUE);
      break;

   case WM_COMMAND:
      if (HIWORD(wparam) == BN_CLICKED)
      {
         switch (LOWORD(wparam))
         {
         case IDCLOSE:
            EndDialog(hwnd, 0);
            break;
         case IDCANCEL:
            EndDialog(hwnd, 0);
            break;
         case IDC_WWWNOFRENDO:
            ShellExecute(hwnd, NULL, "http://nosefart.sourceforge.net", NULL, NULL, 0);
            break;
         }
      }
      break;      
   }
   return (FALSE);
}

static void DefaultDialogInfo(HWND hwnd)
{
   /* set 44100 Hz and Above normal thread priority */
   SendDlgItemMessage(hwnd, IDC_FREQ, CB_SETCURSEL, 2, 0);
   SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 1, 0);
   SendDlgItemMessage(hwnd, IDC_DSP, CB_SETCURSEL, 0, 0);

   CheckDlgButton(hwnd, IDC_CHECK1, TRUE);
   CheckDlgButton(hwnd, IDC_CHECK2, TRUE);
   CheckDlgButton(hwnd, IDC_CHECK3, TRUE);
   CheckDlgButton(hwnd, IDC_CHECK4, TRUE);
   CheckDlgButton(hwnd, IDC_CHECK5, TRUE);
   CheckDlgButton(hwnd, IDC_CHECK6, TRUE);
   CheckDlgButton(hwnd, IDC_16BIT, FALSE);
   CheckDlgButton(hwnd, IDC_8BIT, TRUE);
}

static void FillDialogInfo(HWND hwnd, opstate_t *op)
{
   SendDlgItemMessage(hwnd, IDC_FREQ, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "22050");
   SendDlgItemMessage(hwnd, IDC_FREQ, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "32000");
   SendDlgItemMessage(hwnd, IDC_FREQ, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "44100");
   SendDlgItemMessage(hwnd, IDC_FREQ, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "48000");

   SendDlgItemMessage(hwnd, IDC_THREAD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Highest");
   SendDlgItemMessage(hwnd, IDC_THREAD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Higher");
   SendDlgItemMessage(hwnd, IDC_THREAD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Normal");
   SendDlgItemMessage(hwnd, IDC_THREAD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Lower");
   SendDlgItemMessage(hwnd, IDC_THREAD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Lowest");

   SendDlgItemMessage(hwnd, IDC_DSP, CB_ADDSTRING, 0, (LPARAM)(LPCSTR) "None");
   SendDlgItemMessage(hwnd, IDC_DSP, CB_ADDSTRING, 0, (LPARAM)(LPCSTR) "Low-pass");
   SendDlgItemMessage(hwnd, IDC_DSP, CB_ADDSTRING, 0, (LPARAM)(LPCSTR) "Weighted");

   switch (op->sample_rate)
   {
   case 22050:
      SendDlgItemMessage(hwnd, IDC_FREQ, CB_SETCURSEL, 0, 0);
      break;
   case 32000:
      SendDlgItemMessage(hwnd, IDC_FREQ, CB_SETCURSEL, 1, 0);
      break;
   case 48000:
      SendDlgItemMessage(hwnd, IDC_FREQ, CB_SETCURSEL, 3, 0);
      break;
   default:
      SendDlgItemMessage(hwnd, IDC_FREQ, CB_SETCURSEL, 2, 0);
      break;
   }

   switch (op->priority)
   {
   case THREAD_PRIORITY_HIGHEST:
      SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 0, 0);
      break;
   case THREAD_PRIORITY_NORMAL:
      SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 2, 0);
      break;
   case THREAD_PRIORITY_BELOW_NORMAL:
      SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 3, 0);
      break;
   case THREAD_PRIORITY_LOWEST:
      SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 4, 0);
      break;
   default:
      SendDlgItemMessage(hwnd, IDC_THREAD, CB_SETCURSEL, 1, 0);
      break;
   }

   switch (op->filter)
   {
   case NSF_FILTER_LOWPASS:
      SendDlgItemMessage(hwnd, IDC_DSP, CB_SETCURSEL, 1, 0);
      break;
   case NSF_FILTER_WEIGHTED:
      SendDlgItemMessage(hwnd, IDC_DSP, CB_SETCURSEL, 2, 0);
      break;
   default:
      SendDlgItemMessage(hwnd, IDC_DSP, CB_SETCURSEL, 0, 0);
      break;
   }

   if (op->sample_bits == 8)
      CheckDlgButton(hwnd, IDC_8BIT, TRUE);
   else
      CheckDlgButton(hwnd, IDC_16BIT, TRUE);

   CheckDlgButton(hwnd, IDC_CHECK1, op->enabled[0]);
   CheckDlgButton(hwnd, IDC_CHECK2, op->enabled[1]);
   CheckDlgButton(hwnd, IDC_CHECK3, op->enabled[2]);
   CheckDlgButton(hwnd, IDC_CHECK4, op->enabled[3]);
   CheckDlgButton(hwnd, IDC_CHECK5, op->enabled[4]);
   CheckDlgButton(hwnd, IDC_CHECK6, op->enabled[5]);
}

static void GetDialogInfo(HWND hwnd, opstate_t *op)
{
   switch (SendDlgItemMessage(hwnd, IDC_FREQ, CB_GETCURSEL, 0, 0))
   {
   case 0:
      op->sample_rate = 22050;
      break;
   case 1:
      op->sample_rate = 32000;
      break;
   case 3:
      op->sample_rate = 48000;
      break;
   default:
      op->sample_rate = 44100;
      break;
   }

   switch (SendDlgItemMessage(hwnd, IDC_THREAD, CB_GETCURSEL, 0, 0))
   {
   case 0:
      op->priority = THREAD_PRIORITY_HIGHEST;
      break;
   case 2:
      op->priority = THREAD_PRIORITY_NORMAL;
      break;
   case 3:
      op->priority = THREAD_PRIORITY_BELOW_NORMAL;
      break;
   case 4:
      op->priority = THREAD_PRIORITY_LOWEST;
      break;
   default:
      op->priority = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
   }

   switch (SendDlgItemMessage(hwnd, IDC_DSP, CB_GETCURSEL, 0, 0))
   {
   case 1:
      op->filter = NSF_FILTER_LOWPASS;
      break;
   case 2:
      op->filter = NSF_FILTER_WEIGHTED;
      break;
   default:
      op->filter = NSF_FILTER_NONE;
      break;
   }
      
   if (IsDlgButtonChecked(hwnd, IDC_16BIT))
      op->sample_bits = 16;
   else
      op->sample_bits = 8;

   op->enabled[0] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK1);
   op->enabled[1] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK2);
   op->enabled[2] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK3);
   op->enabled[3] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK4);
   op->enabled[4] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK5);
   op->enabled[5] = (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK6);
}

static void EnforceOptions(opstate_t *op)
{
   int i;

   for (i = 0; i < 6; i++)
      nsf_setchan(nsf_info, i, op->enabled[i]);

   nsf_setfilter(nsf_info, op->filter);

   SetThreadPriority(thread_handle, op->priority);
}

BOOL CALLBACK ConfigDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   static opstate_t *options = NULL;

   switch (msg)
   {
   case WM_INITDIALOG:
      FillDialogInfo(hwnd, nsf_options);

      options = malloc(sizeof(opstate_t));
      memcpy(options, nsf_options, sizeof(opstate_t));
      break;

   case WM_COMMAND:
      //if (HIWORD(wparam) == BN_CLICKED)
      {
         switch (LOWORD(wparam))
         {
         case IDOK:
            //ASSERT(options);
            GetDialogInfo(hwnd, options);
            EndDialog(hwnd, 1);

            memcpy(nsf_options, options, sizeof(opstate_t));
            free(options);

            /* save options to plugin.ini */
            save_options(nsf_options, "plugin.ini");

            EnforceOptions(nsf_options);
            break;

         case IDCANCEL:
            EndDialog(hwnd, 0);
            EnforceOptions(nsf_options);

            /* we don't want new options */
            free(options);
            break;
         
         case IDC_DEF:
            DefaultDialogInfo(hwnd);
            GetDialogInfo(hwnd, options);
            EnforceOptions(options);
            break;

         case IDC_CHECK1:
            nsf_setchan(nsf_info, 0, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK1));
            break;
         case IDC_CHECK2:
            nsf_setchan(nsf_info, 1, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK2));
            break;
         case IDC_CHECK3:
            nsf_setchan(nsf_info, 2, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK3));
            break;
         case IDC_CHECK4:
            nsf_setchan(nsf_info, 3, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK4));
            break;
         case IDC_CHECK5:
            nsf_setchan(nsf_info, 4, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK5));
            break;
         case IDC_CHECK6:
            nsf_setchan(nsf_info, 5, (boolean) IsDlgButtonChecked(hwnd, IDC_CHECK6));
            break;
         case IDC_DSP:
            nsf_setfilter(nsf_info, (uint8) SendDlgItemMessage(hwnd, IDC_DSP, CB_GETCURSEL, 0, 0));
            break;
         }
      }
      break;

   default:
      break;
   }

   return FALSE;
}

/* Configuration dialog */
static void innsf_config(HWND hwndParent)
{
   DialogBox(mod.hDllInstance, MAKEINTRESOURCE(IDD_CONFIG), hwndParent, ConfigDlgProc);
}

/* About dialog */
static void innsf_about(HWND hwndParent)
{
   DialogBox(mod.hDllInstance, MAKEINTRESOURCE(IDD_ABOUT), hwndParent, AboutDlgProc);
}

/* One time initialization (at Winamp startup) */
static void innsf_init(void) 
{
   /* init the osd-logging routines */
   log_init();

   log_printf("Starting %s v%s\ncompiled under MSVC 6.0 %s %s\n", APP_STRING, APP_VERSION,
      __DATE__, __TIME__);

   memset(lastfn, 0, sizeof(lastfn));

   nsf_options = malloc(sizeof(opstate_t));

   /* give them defaults */
   nsf_options->sample_rate = DEFAULT_SAMPRATE;
   nsf_options->sample_bits = DEFAULT_BPS;
   nsf_options->num_chan = DEFAULT_NUMCHAN;
   
   /* load options from plugin.ini */
   load_options(nsf_options, "plugin.ini");

   /* init the NSF system */
   nsf_init();
}

/* One time de-initialization (at Winamp shutdown) */
static void innsf_quit(void) 
{
   free(nsf_options);

   log_shutdown();
}

static int innsf_play(char *fn) 
{ 
   int max_latency, thread_id;
   int i;

   /* yeah, get our own filthy handler up in there! */
   SubclassWinamp();

   nsf_info = nsf_load(fn, NULL, 0);
   if (NULL == nsf_info)
      return -1;

   /* are we already playing file? */
   if (0 == strcmp(fn, lastfn))
      nsf_info->current_song = last_song;

   nsf_playtrack(nsf_info, nsf_info->current_song, nsf_options->sample_rate, 
      nsf_options->sample_bits, FALSE);

   for (i = 0; i < 6; i++)
      nsf_setchan(nsf_info, i, nsf_options->enabled[i]);

   nsf_setfilter(nsf_info, nsf_options->filter);

   strcpy(lastfn, fn);
   paused = FALSE;
   decode_pos_ms = 0;

   max_latency = mod.outMod->Open(nsf_options->sample_rate, nsf_options->num_chan, 
      nsf_options->sample_bits, 0, 0);
   if (max_latency < 0) /* error opening device */
   {
      log_printf("error opening device\n");
      nsf_free(&nsf_info);
      return -1;
   }

   mod.SetInfo(nsf_options->sample_bits, nsf_options->sample_rate / 1000, nsf_options->num_chan, 0);

   /* initialize vis stuff */
   mod.SAVSAInit(max_latency, nsf_options->sample_rate);
   mod.VSASetInfo(nsf_options->num_chan, nsf_options->sample_rate);
   
   /* set the output plug-ins default volume */
   mod.outMod->SetVolume(-666); 

   kill_thread = FALSE;
   thread_handle = (HANDLE) CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) innsf_playthread,
      (void *) &kill_thread, 0, &thread_id);

   SetThreadPriority(thread_handle, nsf_options->priority);

   return 0; 
}

static void innsf_stop(void) 
{ 
   if (thread_handle != INVALID_HANDLE_VALUE)
   {
      kill_thread = TRUE;

      if (WaitForSingleObject(thread_handle, INFINITE) == WAIT_TIMEOUT)
      {
         MessageBox(mod.hMainWindow, "error asking thread to die!\n", "error killing decode thread", 0);
         TerminateThread(thread_handle, 0);
      }

      CloseHandle(thread_handle);
      thread_handle = INVALID_HANDLE_VALUE;
   }

   mod.outMod->Close();
   mod.SAVSADeInit();

   last_song = nsf_info->current_song;
   nsf_free(&nsf_info);
}

static int innsf_getlength(void)
{
   return 1;
}

/* this POS is called every time Winamp opens a file -- BLEH */
static int innsf_isourfile(char *fn) 
{
   /* see if it's an NSF file */
   if (0 == _strnicmp(".nsf", strrchr(fn, '.'), strlen(fn)))
      return TRUE;

   return FALSE;
}

static void innsf_pause(void)
{ 
   paused = TRUE;
   mod.outMod->Pause(1); 
}

static void innsf_unpause(void)
{ 
   paused = FALSE;
   mod.outMod->Pause(0);
}

static int innsf_ispaused(void)
{ 
   return paused;
}

static int innsf_getoutputtime(void) 
{ 
   return (decode_pos_ms + (mod.outMod->GetOutputTime() - mod.outMod->GetWrittenTime()));
}

static void innsf_setoutputtime(int time_in_ms) 
{
   decode_pos_ms = time_in_ms;
   mod.outMod->Flush(decode_pos_ms);
}

static void innsf_setvolume(int volume)
{
   mod.outMod->SetVolume(volume);
}

static void innsf_setpan(int pan)
{
   mod.outMod->SetPan(pan);
}

/* hmm, think of a better way to do this... */
static nsf_t *infonsf = NULL;

BOOL CALLBACK InfoDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   char buffer[8];

   switch (msg)
   {
   case WM_INITDIALOG:
      SetDlgItemText(hwnd, IDC_TEXT1, infonsf->song_name);
      SetDlgItemText(hwnd, IDC_TEXT2, infonsf->artist_name);
      SetDlgItemText(hwnd, IDC_TEXT3, infonsf->copyright);

      sprintf(buffer, "%d", infonsf->num_songs);
      SetDlgItemText(hwnd, IDC_TEXT4, buffer);

      sprintf(buffer, "%d Hz", infonsf->playback_rate);
      SetDlgItemText(hwnd, IDC_TEXT5, buffer);

      sprintf(buffer, "$%04X", infonsf->load_addr);
      SetDlgItemText(hwnd, IDC_TEXT6, buffer);
      sprintf(buffer, "$%04X", infonsf->init_addr);
      SetDlgItemText(hwnd, IDC_TEXT7, buffer);
      sprintf(buffer, "$%04X", infonsf->play_addr);
      SetDlgItemText(hwnd, IDC_TEXT8, buffer);

      return TRUE;
      break;

   case WM_COMMAND:
      if (HIWORD(wparam) == BN_CLICKED)
      {
         switch (LOWORD(wparam))
         {
         case IDOK:
         case IDCANCEL:
            EndDialog(hwnd, 0);
            break;
         }
      }
      break;
   }

   return FALSE;
}

static int innsf_infoDlg(char *fn, HWND hwnd)
{
   infonsf = nsf_load(fn, NULL, 0);
   if (NULL == infonsf) /* DAH! */
      return 1;

   DialogBox(mod.hDllInstance, MAKEINTRESOURCE(IDD_INFO), hwnd, InfoDlgProc);
   
   nsf_free(&infonsf);
   return 0;
}

static void innsf_getfileinfo(char *filename, char *title, int *length_in_ms)
{
   nsf_t *nsf = NULL;
   boolean free;

   if (!filename || !*filename)  /* currently playing file */
   {
      nsf = nsf_info;
      free = FALSE; /* don't free the NSF */
   }
   else
   {
      nsf = nsf_load(filename, NULL, 0);
      if (NULL == nsf) /* DAH! */
         return;
      free = TRUE; /* free the NSF */
   }

   if (nsf && title)
   {
      sprintf(title, "%s %d/%d",
         nsf->song_name,
         nsf->current_song,
         nsf->num_songs);

      if (strcmp(nsf->artist_name, "<?>"))
      {
         title += strlen(title);
         sprintf(title, " [%s]", nsf->artist_name);
      }
   }

   if (free) /* not currently playing file */
      nsf_free(&nsf);
}

/* get the number of sample blocks needed (srate / refresh) 
** to keep the emulation in sync
*/
static int get_samples(void *buf, int sample_length)
{
   /* do a frame's worth of the music play routine */
   nsf_frame(nsf_info);
   
   /* get some samples */
   nsf_info->process(buf, sample_length);
   return sample_length;
}

static DWORD WINAPI innsf_playthread(void *stop)
{
   int buffer_length, output_length;
   int sample_length;//, multiplier;//, freq_ratio;
   int shift;
   float freq_ratio;
   uint8 *sample_buffer;

   /* length in bytes */
   sample_length = nsf_options->sample_rate / nsf_info->playback_rate;
   //if (sample_length < 576)
   //   sample_length = 576;
   //multiplier = num_chan * (sample_bits / 8);
   //buffer_length = sample_length * multiplier;
   shift = (nsf_options->num_chan - 1) + ((nsf_options->sample_bits / 8) - 1);
   buffer_length = sample_length << shift;

   /* allocate space for decode buffer (twice as large for DSP) */
   sample_buffer = malloc(buffer_length * 2);

   if (NULL == sample_buffer)
   {
      PostMessage(mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
      MessageBox(mod.hMainWindow, "error allocating sample buffer\n", "allocation error", 0);
      return 0;
   }

   mod.SetInfo(nsf_options->sample_bits, nsf_options->sample_rate / 1000, nsf_options->num_chan, 1);
   
   /* we don't want to be doing any divides */
   freq_ratio = (1000 / (float) nsf_info->playback_rate);

   while (0 == *((int *) stop)) 
   {
      /* we need twice as much sample buffer space if dsp is active */
      if (mod.outMod->CanWrite() >= (int) (buffer_length << (mod.dsp_isactive() ? 1 : 0)))
      {  
         output_length = get_samples(sample_buffer, sample_length);

         mod.SAAddPCMData((char *) sample_buffer, nsf_options->num_chan, nsf_options->sample_bits, decode_pos_ms);
         mod.VSAAddPCMData((char *) sample_buffer, nsf_options->num_chan, nsf_options->sample_bits, decode_pos_ms);

         decode_pos_ms += (int) freq_ratio;
        
         if (mod.dsp_isactive())
            output_length = mod.dsp_dosamples((short *) sample_buffer, sample_length, 
               nsf_options->sample_bits, nsf_options->num_chan, nsf_options->sample_rate);

//         while (mod.outMod->CanWrite() < (output_length * multiplier))
//            ;
         /* make sure to account for 16-bit mixing / stereo */
         //mod.outMod->Write(sample_buffer, output_length * multiplier); 
 //        if (mod.outMod->CanWrite() >= (output_length * multiplier))
            //mod.outMod->Write(sample_buffer, output_length * multiplier);
            mod.outMod->Write(sample_buffer, output_length << shift);
      }
      else Sleep(50);
   }

   free(sample_buffer);
   return 0;
}


static In_Module mod = 
{
   IN_VER,
   "Nosefart NSF player v" APP_VERSION " "
#ifdef __alpha
   "(AXP)"
#else
   "(x86)"
#endif
   ,
   NULL, /* hMainWindow (filled in by Winamp) */
   NULL, /* hDllInstance (filled in by Winamp) */
   "nsf\0NES sound file (*.nsf)\0",
   FALSE,/* is_seekable */
   TRUE, /* uses output */
   innsf_config,
   innsf_about,
   innsf_init,
   innsf_quit,
   innsf_getfileinfo,
   innsf_infoDlg,
   innsf_isourfile,
   innsf_play,
   innsf_pause,
   innsf_unpause,
   innsf_ispaused,
   innsf_stop,

   innsf_getlength,
   innsf_getoutputtime,
   innsf_setoutputtime, /* set output time */
   innsf_setvolume,
   innsf_setpan,

   0,0,0,0,0,0,0,0,0, /* vis stuff */
   
   0,0,  /* dsp */
   0,    /* EQ stuff */
   0,    /* setinfo */
   0     /* out_mod */
};

__declspec(dllexport) In_Module *winampGetInModule2(void)
{
   return &mod;
}

/*
** $Log: in_nsf.c,v $
** Revision 1.14  2000/07/04 04:44:30  matt
** minor changes
**
** Revision 1.13  2000/06/28 22:04:19  matt
** not much
**
** Revision 1.12  2000/06/23 11:01:53  matt
** fixed a subclassing problem
**
** Revision 1.11  2000/06/20 04:07:16  matt
** minor modifications
**
** Revision 1.10  2000/06/14 03:30:58  matt
** fixed some subclassing problems
**
** Revision 1.9  2000/06/13 03:52:21  matt
** updated to support new API
**
** Revision 1.8  2000/06/12 01:15:22  matt
** removed nasty trackbar code
**
** Revision 1.7  2000/06/09 16:49:46  matt
** added blade picture to aboot box
**
** Revision 1.6  2000/06/09 15:12:28  matt
** initial revision
**
*/
