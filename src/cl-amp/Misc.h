/*
	File:    Misc.h
	Author:  Eli Dayan
	Created: 03/06/01 15:42:09
*/

#ifndef		_MISC_H_
#define		_MISC_H_

#include	<be/storage/Resources.h>
#include	<be/app/Message.h>
#include	<be/storage/File.h>
#include	<be/interface/Bitmap.h>
#include	<be/translation/TranslationUtils.h>
#include	<be/interface/Window.h>
#include	<be/interface/Screen.h>

typedef struct
{
	int32 sampleRate;
	int32 sampleBits;
	int32 numChannels;
	int32 filter;
	bool chanEnabled[6];
} opstate_t;


namespace Misc
{
	void CenterWindow (BWindow *window);
	void BlitToBBitmap (BBitmap *&destBitmap, uint8 *bits);	

};
	
#endif	//	_MISC_H_


