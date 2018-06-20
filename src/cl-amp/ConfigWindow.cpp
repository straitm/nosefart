/*
	File:    ConfigWindow.cpp
	Author:  Eli Dayan
	Created: 03/07/01 14:35:23
*/


#include	"NosefartPlugin.h"
#include	"ConfigWindow.h"


ConfigWindow::ConfigWindow (NosefartPlugin *parent, opstate_t *opstate)
	: BWindow (BRect (0, 0, 0, 0),  "nosefart Configuration", B_FLOATING_WINDOW_LOOK, 					   B_MODAL_APP_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE,				   			   B_ASYNCHRONOUS_CONTROLS),
			   fParent (parent),
			   fOpstate (opstate)
{
	Lock();
	
	ResizeTo (310, 210);
	Misc::CenterWindow (this);
	
	fConfigView = new ConfigView (Bounds());
	AddChild (fConfigView);
	fConfigView->SetCurrentConfig (fOpstate);
	
	fConfigChanged = false;

	Unlock();
}


ConfigWindow::~ConfigWindow()
{
}

bool
ConfigWindow::QuitRequested (void)
{
	if (fConfigChanged == true) {
		BAlert *confirmAlert = new BAlert ("nosefart", "You have changed the current"
												   		" configuration.  Would you like"
												   		" to save the changes?",
														"Cancel", "Discard", "Save",
											B_WIDTH_AS_USUAL, B_OFFSET_SPACING, 														B_WARNING_ALERT);
	
		int32 button_index = confirmAlert->Go();										
	
		switch (button_index) {
			case 0:
				return false;
				break;
			
			case 1:
				fParent->LoadOptions();
				return true;
				break;
			
			case 2:
				fParent->SaveOptions();
				return true;
				break;
		}
	}
	
	return true;
}

void
ConfigWindow::MessageReceived (BMessage *message)
{
	switch (message->what) {
		case kMsgSetFreq11:
			fOpstate->sampleRate = 11025;
			fConfigChanged = true;
			break;
			
		case kMsgSetFreq22:
			fOpstate->sampleRate = 22050;
			fConfigChanged = true;
			break;
			
		case kMsgSetFreq44:
			fOpstate->sampleRate = 44100;
			fConfigChanged = true;
			break;
			
		case kMsgSetFreq48:
			fOpstate->sampleRate = 48000;
			fConfigChanged = true;
			break;
			
			
		case kMsgSetQual8:
			fOpstate->sampleBits = 8;
			fConfigChanged = true;
			break;
			
		case kMsgSetQual16:
			fOpstate->sampleBits = 16;
			fConfigChanged = true;
			break;
			
			
		case kMsgSetFilterNone:
			fOpstate->filter = NSF_FILTER_NONE;
			fConfigChanged = true;
			break;
			
		case kMsgSetFilterLP:
			fOpstate->filter = NSF_FILTER_LOWPASS;
			fConfigChanged = true;
			break;
			
		case kMsgSetFilterWeight:
			fOpstate->filter = NSF_FILTER_WEIGHTED;
			fConfigChanged = true;
			break; 
			
			
		case kMsgSetSq1:
			fOpstate->chanEnabled[0] = fConfigView->Sq1Enabled();
			fConfigChanged = true;
			break;
			
		case kMsgSetSq2:
			fOpstate->chanEnabled[1] = fConfigView->Sq2Enabled();
			fConfigChanged = true;
			break;
			
		case kMsgSetTri:
			fOpstate->chanEnabled[2] = fConfigView->TriEnabled();
			fConfigChanged = true;
			break;
			
		case kMsgSetNoise:
			fOpstate->chanEnabled[3] = fConfigView->NoiseEnabled();
			fConfigChanged = true;
			break;
			
		case kMsgSetPCM:
			fOpstate->chanEnabled[4] = fConfigView->PCMEnabled();
			fConfigChanged = true;
			break;
			
		case kMsgSetExt:
			fOpstate->chanEnabled[5] = fConfigView->ExtEnabled();
			fConfigChanged = true;
			break;
					
		
		case kMsgOK:
			fConfigView->SetCurrentConfig (fOpstate);
			fParent->SaveOptions();
			Close();
			break;
			
		case kMsgDefaults:
			fConfigView->SetDefaultConfig();
			break;
			
		case kMsgCancel:
			fParent->LoadOptions();
			Close();
			break;
			
		default:	
			BWindow::MessageReceived (message);
	}
}
