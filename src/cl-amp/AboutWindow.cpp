/*
	File:    AboutWindow.cpp
	Author:  Eli Dayan
	Created: 05/21/01 21:07:33
*/

#include	"AboutWindow.h"


AboutWindow::AboutWindow()
	: BWindow (BRect (0, 0, 0, 0), "About nosefart", B_FLOATING_WINDOW_LOOK,
			   B_MODAL_APP_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE, 
			   B_ASYNCHRONOUS_CONTROLS)
{	
	Lock();
	
	ResizeTo (220, 380);
	Misc::CenterWindow (this);
	
	BView *backView = new BView (Bounds(), "about_view", B_FOLLOW_ALL, 0);
	AddChild (backView);
	backView->SetViewColor (ui_color (B_PANEL_BACKGROUND_COLOR));
	
	BView *noseLogoView = new BView (BRect (60, 10, 156, 42), "logo", B_FOLLOW_ALL, 0);
	backView->AddChild (noseLogoView);
	
	
	BBitmap *noseLogoBitmap = new BBitmap (BRect (0, 0, 96, 32), B_CMAP8);
	Misc::BlitToBBitmap (noseLogoBitmap, 
						 const_cast<uint8 *> (noseBitmaps::kNoseLogoBits));
	
	noseLogoView->SetViewBitmap (noseLogoBitmap);
	
	
	BStringView *infoView = new BStringView (BRect (0, 50, 220, 60), NULL, 
								 			 "nosefart NSF Player for CL-Amp");
	infoView->SetAlignment (B_ALIGN_CENTER);							 
	backView->AddChild (infoView);
	
	infoView = new BStringView (BRect (0, 65, 220, 75), NULL, "Copyleft 2001 Matt Conte "
															   "and Eli Dayan");
	infoView->SetAlignment (B_ALIGN_CENTER);								   
	backView->AddChild (infoView);
	
	BView *bladeLogoView = new BView (BRect (10, 85, 210, 238), "blade", B_FOLLOW_ALL, 0);
	backView->AddChild (bladeLogoView);
	
	BBitmap *bladeLogoBitmap = new BBitmap (BRect (0, 0, 200, 153), B_CMAP8);
	Misc::BlitToBBitmap (bladeLogoBitmap,
						 const_cast<uint8 *> (noseBitmaps::kBladeLogoBits));
	bladeLogoView->SetViewBitmap (bladeLogoBitmap);						 
	
	BBox *urlBox = new BBox (BRect (10, 250, 210, 330));
	urlBox->SetLabel ("Related URLs:");
	backView->AddChild (urlBox);
	
	BButton *noseButton = new BButton (BRect (12, 16, 188, 40), "nose_web", 
									   "nosefart home page", new BMessage (kNoseMsg));
	BButton *sandwichButton = new BButton (BRect (12, 46, 188, 70), "sandwich_web",
										   "Eli's homepage", new BMessage (kSandwichMsg));
	
	
	BButton *doneButton = new BButton (BRect (60, 340, 156, 360), "done",
									   "Done", new BMessage (kDoneMsg));					   										   												
	urlBox->AddChild (noseButton);
	urlBox->AddChild (sandwichButton);
	
	backView->AddChild (doneButton);
	doneButton->MakeDefault (true);										   
	
	Unlock();
}


AboutWindow::~AboutWindow()
{	
}


void
AboutWindow::MessageReceived (BMessage *message)
{
	
	switch (message->what) {
		case kNoseMsg:
		{	
			char *link = "http://www.baisoku.org/";
			be_roster->Launch ("text/html", 1, &link);
		} break;
			
		case kSandwichMsg:
		{
			char *link = "http://www.vectorstar.net/~scanty";
			be_roster->Launch ("text/html", 1, &link);
		} break;
		
		case kDoneMsg:
			Quit();
			break;
			
		default:
			BWindow::MessageReceived (message);
	}
}			   
