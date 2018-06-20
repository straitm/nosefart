/*
	File:    Misc.cpp
	Author:  Eli Dayan
	Created: 03/06/01 15:46:59
*/

#include	"Misc.h"


void
Misc::CenterWindow (BWindow *window)
{
	BScreen screen (window);
		
	float x;
	float y;
	
	x = (screen.Frame().Width() - window->Frame().Width()) / 2;
	y = (screen.Frame().Height() - window->Frame().Height()) / 2;
	
	window->MoveTo (x, y);
}


void 
Misc::BlitToBBitmap (BBitmap *&destBitmap, uint8 *bits)	
{
	int32 height, width;
	
	height = static_cast<int32> (destBitmap->Bounds().Height());
	width = static_cast<int32> (destBitmap->Bounds().Width());
	
	uint8 *dest = reinterpret_cast<uint8 *> (destBitmap->Bits());
	int32 bytesPerRow = destBitmap->BytesPerRow();
	
	
	for (int32 y = 0; y < height; y++) {
		memcpy (dest + (bytesPerRow * y), bits + (y * width), width);
	}

}
