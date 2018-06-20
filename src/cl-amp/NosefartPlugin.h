/*
	File:    NsfPlugin.h
	Author:  Eli Dayan
	Created: 12/09/00 16:37:15
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <be/interface/Alert.h>
#include <be/interface/TextView.h>
#include <be/support/String.h>

#include "includes/InputPlugin.h"
#include "../types.h"
#include "../machine/nsf.h"

#include "AboutWindow.h"
#include "ConfigWindow.h"


const int32 kDefaultSampleRate = 44100;
const int32 kDefaultBps = 16;
const int32 kDefaultNumChannels = 1;
const int32 kDefaultFilter = NSF_FILTER_NONE; 
const int32 kUrlLength = 6;

const char kNsfURL[] = "nsf://";
const uint32 kNoseHeader = 0x45534f4e;


class NosefartPlugin : public InputPlugin
{	
	public:
	NosefartPlugin (PluginConstructorStruct *info);
	virtual ~NosefartPlugin();
	
	public:
	virtual void Init (void);
	virtual void Cleanup (void);
	
	// thread A
	public:
	virtual bool About (bool question);
	virtual bool Prefs (bool question);
	virtual bool Edit (const char *fileName, bool question);
	virtual bool GetMimeType (BMimeType *mimeType, int nr);
	virtual	bool IsOur (const char *fileName);
	virtual bool GetSongInfo (const char *fileName, PlayerInfoStruct *info);
	virtual	void AbortPlaying (void);
	virtual void Pause (bool on);
	virtual void NewSpeed (int32 value);
	virtual void NewVolume (int32 value);
		
	public:
	// thread B
	virtual bool InitPlaying (const char *fileName, PlayerInfoStruct *info);
	virtual int	GetAudio (char **buffer, int size);
	virtual void JumpTo (int32 newTime);
	virtual void CleanupPlaying (void);	
	
	public:
	nsf_t *Nsf (void) { return fNsf; };
	opstate_t *Opstate (void) { return fNsfOptions; };
	//void AcquireNewOpstate (opstate_t *newOpstate);
	
	private:
	bool IsNsf (const char *name);
	void SetDefaultOptions (void);
	
	public:
	void LoadOptions (void);
	void SaveOptions (void);
	
	private:
	nsf_t	  *fNsf;
	opstate_t *fNsfOptions;
	
	uint8 *fSampleBuffer;
	int32 fBufferLength;
	int32 fSampleLength;
	int32 fShift;
	
	private:
	ConfigWindow *fConfigWindow;
	AboutWindow	 *fAboutWindow;
};
