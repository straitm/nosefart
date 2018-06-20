/*
	File:    ConfigView.cpp
	Author:  Eli Dayan
	Created: 03/28/01 15:57:18
*/

#include	"ConfigView.h"


ConfigView::ConfigView (BRect frame)
	: BView (frame, "config_view", B_FOLLOW_ALL, B_NAVIGABLE)
{
}


ConfigView::~ConfigView()
{
}


void
ConfigView::AttachedToWindow (void)
{
	SetViewColor (ui_color (B_PANEL_BACKGROUND_COLOR));
	
	fFreqMenu = new BPopUpMenu ("freq_menu");
	fFreqMenu->AddItem (new BMenuItem ("11025 Hz", new BMessage (kMsgSetFreq11)));
	fFreqMenu->AddItem (new BMenuItem ("22050 Hz", new BMessage (kMsgSetFreq22)));
	fFreqMenu->AddItem (new BMenuItem ("44100 Hz", new BMessage (kMsgSetFreq44)));
	fFreqMenu->AddItem (new BMenuItem ("48000 Hz", new BMessage (kMsgSetFreq48)));
	
	fFreqField = new BMenuField (BRect (10, 10, 160, 30), "freq_field",
								 "Playback Rate:", fFreqMenu, true);
	fFreqMenu->SetTargetForItems (this);
	
	fQualityBox = new BBox (BRect (10, 50, 150, 120));
	fQualityBox->SetLabel ("Sound Quality:");
	fRadio8 =  new BRadioButton (BRect (10, 20, 50, 0), "radio_8", "8-bit", 
								 new BMessage (kMsgSetQual8));
	fRadio16 = new BRadioButton	(BRect (10, 40, 56, 0), "radio_16", "16-bit", 
								 new BMessage (kMsgSetQual16));
								 

	fFilterMenu = new BPopUpMenu ("filter_menu");
	fFilterMenu->AddItem (new BMenuItem ("None", new BMessage (kMsgSetFilterNone)));
	fFilterMenu->AddItem (new BMenuItem ("Low Pass", new BMessage (kMsgSetFilterLP)));
	fFilterMenu->AddItem (new BMenuItem ("Weighted", new BMessage (kMsgSetFilterWeight)));
	fFilterField = new BMenuField (BRect (170, 10, 300, 30), "filter_field",
								   "Filter Type:", fFilterMenu, true);
	fFilterField->SetDivider (58);
	fFilterMenu->SetTargetForItems (this);
	

	fChannelsBox = new BBox (BRect (170, 50, 300, 200));
	fChannelsBox->SetLabel ("Enabled Channels:");	
	
	fSquare1 = new BCheckBox (BRect (10, 20, 75, 0), "square1_checkbox", "Square 1",									  new BMessage (kMsgSetSq1));
	fSquare2 = new BCheckBox (BRect (10, 40, 75, 0), "square2_checkbox", "Square 2",
							  new BMessage (kMsgSetSq2));
	fTriangle = new BCheckBox (BRect (10, 60, 75, 0), "triangle_checkbox", "Triangle",
							   new BMessage (kMsgSetTri));
	fNoise = new BCheckBox (BRect (10, 80, 75, 0), "noise_checkbox", "Noise",
							new BMessage (kMsgSetNoise));
	fPCM = new BCheckBox (BRect (10, 100, 75, 0), "pcm_checkbox", "PCM",
						  new BMessage (kMsgSetPCM));
	fExternal = new BCheckBox (BRect (10, 120, 75, 0), "ext_checkbox", "External",
							   new BMessage (kMsgSetExt));
	
	
	fCancel = new BButton (BRect (10, 140, 79, 0), "cancel_button", "Cancel",
						   new BMessage (kMsgCancel));
	fDefaults = new BButton (BRect (80, 140, 150, 0), "defaults_button", "Defaults",
							 new BMessage (kMsgDefaults));
	fOk = new BButton (BRect (10, 170, 150, 0), "ok_button", "Okay", 
	 				   new BMessage (kMsgOK));													   					   						   						  				
	AddChild (fFreqField);	
	AddChild (fQualityBox);
	fQualityBox->AddChild (fRadio8);
	fQualityBox->AddChild (fRadio16);
	AddChild (fFilterField);
	AddChild (fChannelsBox);
	fChannelsBox->AddChild (fSquare1);	
	fChannelsBox->AddChild (fSquare2);	
	fChannelsBox->AddChild (fTriangle);
	fChannelsBox->AddChild (fNoise);
	fChannelsBox->AddChild (fPCM);
	fChannelsBox->AddChild (fExternal);	
	AddChild (fCancel);
	AddChild (fDefaults);
	AddChild (fOk);										   							   
}


void
ConfigView::SetCurrentConfig (opstate_t *opstate)
{
	BMenuItem *item;

	switch (opstate->sampleRate) {
		case 11025:
			item = fFreqMenu->FindItem ("11025 Hz");
			item->SetMarked (true);
			break;
			
		case 22050:
			item = fFreqMenu->FindItem ("22050 Hz");
			item->SetMarked (true);
			break;
			
		case 44100:
			item = fFreqMenu->FindItem ("44100 Hz");
			item->SetMarked (true);
			break;
			
		case 48000:
			item = fFreqMenu->FindItem ("48000 Hz");
			item->SetMarked (true);
			break;
	}
	
	if (opstate->sampleBits == 8) {
		fRadio8->SetValue (B_CONTROL_ON);
		fRadio16->SetValue (B_CONTROL_OFF);
	} else if (opstate->sampleBits == 16) {
		fRadio16->SetValue (B_CONTROL_ON);
		fRadio8->SetValue (B_CONTROL_OFF);
	}
	
	switch (opstate->filter) {
		case NSF_FILTER_NONE:
			item = fFilterMenu->FindItem ("None");
			item->SetMarked (true);
			break;
			
		case NSF_FILTER_WEIGHTED:
			item = fFilterMenu->FindItem ("Weighted");
			item->SetMarked (true);
			break;
		
		case NSF_FILTER_LOWPASS:
			item = fFilterMenu->FindItem ("Low Pass");
			item->SetMarked (true);
			break;
	}
	
	fSquare1->SetValue (opstate->chanEnabled[0]);
	fSquare2->SetValue (opstate->chanEnabled[1]);
	fTriangle->SetValue (opstate->chanEnabled[2]);
	fNoise->SetValue (opstate->chanEnabled[3]);
	fPCM->SetValue (opstate->chanEnabled[4]);
	fExternal->SetValue (opstate->chanEnabled[5]);
	
}	


void
ConfigView::SetDefaultConfig (void)
{
	BMenuItem *defaultItem;
	
	defaultItem = fFreqMenu->FindItem ("44100 Hz");
	defaultItem->SetMarked (true);
	
	defaultItem = fFilterMenu->FindItem ("None");
	defaultItem->SetMarked (true);
	
	fRadio16->SetValue (B_CONTROL_ON);
	fRadio8->SetValue (B_CONTROL_OFF);
	
	fSquare1->SetValue (B_CONTROL_ON);
	fSquare2->SetValue (B_CONTROL_ON);
	fTriangle->SetValue (B_CONTROL_ON);
	fNoise->SetValue (B_CONTROL_ON);
	fPCM->SetValue (B_CONTROL_ON);
	fExternal->SetValue (B_CONTROL_ON);
}
