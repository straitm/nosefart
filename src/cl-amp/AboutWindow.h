/*
	File:    AboutWindow.h
	Author:  Eli Dayan
	Created: 05/21/01 21:04:59
*/

#ifndef		_ABOUT_WINDOW_H_
#define		_ABOUT_WINDOW_H_

#include <be/interface/StringView.h>
#include <be/interface/Window.h>
#include <be/interface/Bitmap.h>
#include <be/interface/Box.h>
#include <be/interface/Button.h>
#include <be/app/Roster.h>

#include "Misc.h"
#include "bitmaps.h"

const int32 kNoseMsg = 'NOSE';
const int32 kSandwichMsg = 'SAND';
const int32 kDoneMsg = 'DONE';


class AboutWindow : public BWindow
{
	public:
	AboutWindow();
	virtual ~AboutWindow();
	
	public:
	virtual void MessageReceived (BMessage *message);
};
	

#endif	//	_ABOUT_WINDOW_H_
