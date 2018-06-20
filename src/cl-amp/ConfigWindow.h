/*
	File:    ConfigWindow.h
	Author:  Eli Dayan
	Created: 03/07/01 14:35:15
*/


#ifndef		_CONFIG_WINDOW_H_
#define		_CONFIG_WINDOW_H_

#include <be/app/Message.h>
#include <be/interface/Window.h>

#include "ConfigView.h"
#include "Misc.h"

class NosefartPlugin;

class ConfigWindow : public BWindow
{	
	public:
	ConfigWindow (NosefartPlugin *parent, opstate_t *opstate);
	virtual ~ConfigWindow();
	
	public:
	virtual bool QuitRequested (void);
	virtual void MessageReceived (BMessage *message);

	private:
	ConfigView	*fConfigView;
	
	private:
	NosefartPlugin *fParent;
	opstate_t *fOpstate;
	bool fConfigChanged;
	
	private:
	int32 fFrequency;
	int32 fQuality;
	int32 fFilter;
	
	bool fSquare1Enabled;
	bool fSquare2Enabled;
	bool fTriangleEnabled;
	bool fNoiseEnabled;
	bool fPCMEnabled;
	bool fExtEnabled;
	
};

#endif	//	_CONFIG_WINDOW_H_
