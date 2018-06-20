/*
	File:    NsfPlugin.cpp
	Author:  Eli Dayan
	Created: 12/09/00 16:45:55
*/

#include	"NosefartPlugin.h"



NosefartPlugin::NosefartPlugin (PluginConstructorStruct *info)
	: InputPlugin ("Nosefart 2.3", "Nosefart NSF Player by Matthew Conte "
				   "(plugin by Eli Dayan)")
{
	fNsf = NULL;
	fNsfOptions = NULL;
	fSampleBuffer = NULL;
	fBufferLength = 0;
	fSampleLength = 0;
	fShift = 0;
	
	fConfigWindow = NULL;
	fNsfOptions = new opstate_t;
}


NosefartPlugin::~NosefartPlugin()
{	
}


void
NosefartPlugin::Init (void)
{	
	//fNsfOptions = new opstate_t;
	
	SetDefaultOptions();
	
	LoadOptions();

	nsf_init();
}


void
NosefartPlugin::Cleanup (void)
{
	SaveOptions();
	
	nsf_free (&fNsf);
	delete fNsfOptions;
	delete fSampleBuffer;
}


bool
NosefartPlugin::About (bool question)
{
	if (question) {
		return true;
	}

	(new AboutWindow())->Show();
	
	return true;
}


bool
NosefartPlugin::Prefs (bool question)
{
	if (question) {
		return true;
	}
	
	(new ConfigWindow (this, this->fNsfOptions))->Show();

	return true;
}


bool
NosefartPlugin::Edit (const char *fileName, bool question)
{
	if (question) {
		return true;
	}
	
	BAlert *infoAlert;
	BTextView *textView;
	BFont font;
	BString	alertText;
	BString	songName;
	BString	artistName;
	BString	copyright;
	BString	startSong;
	BString	totalSongs;
	BString	loadAddr;
	BString	initAddr;
	BString	playAddr;
	
	nsf_t *tempNSF;
	char  addr[5];
	int32 offset = 0;
	int32 length = 0;
	
	
	tempNSF = nsf_load (const_cast<char *> (fileName), NULL, 0);
	if (tempNSF == NULL) {
		return false;	// bail
	}
	
	
	songName << reinterpret_cast<char *> (tempNSF->song_name) 
			 << "\n\n\n";
			 
	artistName << "Artist(s):\n"
			   << reinterpret_cast<char *> (tempNSF->artist_name)
			   << "\n\n";
			   
	copyright << "Copyright:\n"
			  << reinterpret_cast<char *> (tempNSF->copyright)
			  << "\n\n";
			  
	startSong << "Start Song:\t"
			  << static_cast<uint32> (tempNSF->start_song)
			  << "\n";
			  
	totalSongs << "Total Songs:\t"
			   << static_cast<uint32> (tempNSF->num_songs)
			   << "\n\n";
			   
			   
	sprintf (addr, "$%04x", tempNSF->load_addr);
	loadAddr << "Load Address:\t\t"
			 << addr
			 << "\n";
			 
	sprintf (addr, "$%04x", tempNSF->init_addr);
	initAddr << "Init Address:\t\t"
			 <<	addr
			 << "\n";
			 
	sprintf (addr, "$%04x", tempNSF->play_addr);		 
	playAddr << "Play Address:\t\t"
			 << addr
			 << "\n\n\n\n";

	alertText << songName
			  << artistName
			  << copyright
			  << startSong
			  << totalSongs
			  << loadAddr
			  << initAddr
			  << playAddr;

	
	infoAlert = new BAlert (NULL, alertText.String(), "Okay");
	textView = infoAlert->TextView();
	textView->SetStylable (true);
	
	
	// song name
	length = songName.Length();
	offset += length;
	
	textView->Select (0, length - 3);
	textView->SetFontAndColor (be_bold_font);
	textView->GetFontAndColor (0, &font, NULL);
	font.SetSize (18);
	textView->SetFontAndColor (&font);
	
	
	// artist(s)
	length = artistName.Length();
	textView->Select (offset, offset + 11);
	textView->SetFontAndColor (be_bold_font);
	offset += 11;
	
	textView->Select (offset, offset + (length - 11));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 11);
	
	
	// copyright
	length = copyright.Length();
	textView->Select (offset, offset + 10);
	textView->SetFontAndColor (be_bold_font);
	offset += 10;
	
	textView->Select (offset, offset + (length - 10));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 10);
	
	
	// start song
	length = startSong.Length();
	textView->Select (offset, offset + 11);
	textView->SetFontAndColor (be_bold_font);
	offset += 11;
	
	textView->Select (offset, offset + (length - 11));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 11);
	
	
	// total songs
	length = totalSongs.Length();
	textView->Select (offset, offset + 12);
	textView->SetFontAndColor (be_bold_font);
	offset += 12;
	
	textView->Select (offset, offset + (length - 12));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 12);
	
	
	// load addr
	length = loadAddr.Length();
	textView->Select (offset, offset + 13);
	textView->SetFontAndColor (be_bold_font);
	offset += 13;
	
	textView->Select (offset, offset + (length - 13));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 13);
	
	
	// init addr
	length = initAddr.Length();
	textView->Select (offset, offset + 13);
	textView->SetFontAndColor (be_bold_font);
	offset += 13;
	
	textView->Select (offset, offset + (length - 13));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 13);
	
	
	// play addr
	length = playAddr.Length();
	textView->Select (offset, offset + 13);
	textView->SetFontAndColor (be_bold_font);
	offset += 13;
	
	textView->Select (offset, offset + (length - 13));
	textView->SetFontAndColor (be_fixed_font);
	offset += (length - 13);
	
	infoAlert->Go (NULL);
	
	nsf_free (&tempNSF);
	
	return true;		  			 			 
}


bool
NosefartPlugin::GetMimeType (BMimeType *mimeType, int nr)
{	
	if (nr == 0) {
		mimeType->SetTo ("audio/x-nsf");
		mimeType->SetLongDescription ("Nintendo Sound File");
		return true;
	} else {
		return false;
	}
}


bool
NosefartPlugin::IsOur (const char *fileName)
{
	BString tempFileName (fileName);
	int32	pos;
	
	if (tempFileName.ICompare (kNsfURL, kUrlLength) == 0) {
		pos = tempFileName.FindLast ('?');
		
		if (pos == B_ERROR) {
			return false;
		}
		
		pos -= kUrlLength;
		tempFileName.Remove (0, kUrlLength);
		tempFileName.Remove (pos, (tempFileName.Length() - pos));
				
		return IsNsf (tempFileName.String());		
	} else {
		return IsNsf (fileName);	
	}
	
	return false;
}


bool
NosefartPlugin::GetSongInfo (const char *fileName, PlayerInfoStruct *info)
{	
	BString	tempFileName (fileName);
	BString tempSong;
	int32	pos;
	nsf_t	*tempNSF;
	int32	song = 1;
	char	playListEntry[B_FILE_NAME_LENGTH + kUrlLength];
	
	if (tempFileName.ICompare (kNsfURL, kUrlLength) == 0) {
		pos = tempFileName.FindLast ('?');
		
		if (pos == B_ERROR) {
			return false;
		}
		
		pos -= kUrlLength;
		tempSong = tempFileName.Remove (0, kUrlLength);
		tempFileName.Remove (pos, (tempFileName.Length() - pos));		
		
		tempNSF = nsf_load (const_cast <char *> (tempFileName.String()), NULL, 0);
		if (tempNSF == NULL) {
			return false;
		}
		
		if (tempNSF->num_songs > 1) {
			tempSong.Remove (0, pos + 1);
			song = atoi (tempSong.String());
			if (song > tempNSF->num_songs) {
				nsf_free (&tempNSF);
				return false;
			}
			
			sprintf (info->Title, "%s [%ld/%d]", 
					 reinterpret_cast <char *> (tempNSF->song_name), song, 
					 tempNSF->num_songs);
		} else {
			sprintf (info->Title, 
					 reinterpret_cast <char *> (tempNSF->song_name));
		}
			
	} else {
		tempNSF = nsf_load (const_cast <char *> (fileName), NULL, 0); 
		if (tempNSF == NULL) {
			return false;
		}
							
		if (tempNSF->num_songs > 1) {
			if (song > tempNSF->num_songs) {
				nsf_free (&tempNSF);
				return false;
			}
			
			sprintf (info->Title, "%s [%d/%d]",
					 reinterpret_cast <char *> (tempNSF->song_name), 
					 tempNSF->start_song, tempNSF->num_songs);
		} else {
			sprintf (info->Title,
					 reinterpret_cast <char *> (tempNSF->song_name));
		}
			
		// take care of the sub-songs
		for (int32 i = 1; i <= tempNSF->num_songs; i++) {
			if (i != tempNSF->start_song) {
				sprintf (playListEntry, "%s%s?%ld", kNsfURL, fileName, i);
				if (i < tempNSF->start_song) {
					SendToCLAmp_AddFile (playListEntry, info->SongId);
				} else {
					SendToCLAmp_AddFile (playListEntry);
				}
			}
		}
	}
	
	nsf_free (&tempNSF);		
	
	return true;
	
}


bool
NosefartPlugin::InitPlaying (const char *fileName, PlayerInfoStruct *info)
{
	BString	tempFileName (fileName);
	BString tempSong;
	int32	pos;
	int32	song = 0;
	bool	songGiven = false;
	
	LoadOptions();
	
	if (tempFileName.ICompare (kNsfURL, kUrlLength) == 0) {
		pos = tempFileName.FindLast ('?');
		
		if (pos == B_ERROR) {
			return false;
		}
		
		pos -= kUrlLength;
		tempSong = tempFileName.Remove (0, kUrlLength);
		tempFileName.Remove (pos, (tempFileName.Length() - pos));
		
		tempSong.Remove (0, pos + 1);
		song = atoi (tempSong.String());
		songGiven = true;
		
		
		fNsf = nsf_load (const_cast<char *> (tempFileName.String()), NULL, 0);
		if (fNsf == NULL) {
			return false;
		}
	} else {
		fNsf = nsf_load (const_cast<char *> (fileName), NULL, 0);
		if (fNsf == NULL) {
			return false;
		}	
	}
	
	if (songGiven == true) {
		nsf_playtrack (fNsf, song, fNsfOptions->sampleRate,
					   fNsfOptions->sampleBits, 0);
	} else {
		nsf_playtrack (fNsf, fNsf->start_song, fNsfOptions->sampleRate,
					   fNsfOptions->sampleBits, 0);
	}
							
	for (int32 i = 0; i < 6; i++) {
		nsf_setchan (fNsf, i, fNsfOptions->chanEnabled[i]);
	}
	
	nsf_setfilter (fNsf, fNsfOptions->filter);
	
	fSampleLength = (fNsfOptions->sampleRate / (fNsf->playback_rate));
	fShift = (fNsfOptions->numChannels - 1) + 
			 ((fNsfOptions->sampleBits / 8) - 1);
	fBufferLength = fSampleLength << fShift;	
	fSampleBuffer = new uint8[2 * fBufferLength];
	
	info->Flags = INPLUG_NO_TOTTIME | INPLUG_NO_CURRTIME;
	info->BitRate =  fNsfOptions->sampleBits;
	info->Frequency = fNsfOptions->sampleRate;
	info->Stereo = (fNsfOptions->numChannels == 2) ? true : false;
	info->SampleIsOnly8Bits = (fNsfOptions->sampleBits == 8) ? true : false;
	
	return true;
}


int
NosefartPlugin::GetAudio (char **buffer, int size)
{	
	nsf_frame (fNsf);
	fNsf->process (fSampleBuffer, fBufferLength >> fShift);
	*buffer = reinterpret_cast<char *> (fSampleBuffer);
	
	return fSampleLength << fShift;
}


bool
NosefartPlugin::IsNsf (const char *fileName)
{
	BString	tempFileName (fileName);
	int32	pos;
	
	pos = tempFileName.FindLast ('.');
	if (pos == B_ERROR) {
		return false;
	}
	
	tempFileName.Remove (0, pos + 1);
	if (tempFileName.ICompare ("nsf") == 0) {
		return true;
	} 
	
	return false;
}


void
NosefartPlugin::SetDefaultOptions (void)
{
	fNsfOptions->sampleRate = kDefaultSampleRate;
	fNsfOptions->sampleBits = kDefaultBps;
	fNsfOptions->numChannels = kDefaultNumChannels;
	fNsfOptions->filter = kDefaultFilter;
	
	for (int32 i = 0; i < 6; i++) {
		fNsfOptions->chanEnabled[i] = true;
	}
}


void
NosefartPlugin::LoadOptions (void)
{
	BFile optionsFile;
	status_t err;
	uint32 headerBuffer;
	
	
	err = optionsFile.SetTo ("/boot/home/config/settings/nosefart.options", B_READ_ONLY);
	optionsFile.Read (&headerBuffer, 4);	
	
	if (err == B_NO_ERROR && headerBuffer == kNoseHeader) {
		uint8 byte1;
		uint8 byte2;
		
		optionsFile.Read (&byte1, 1);
		optionsFile.Read (&byte2, 1);		
		
		switch ((byte1 & 0xc0) >> 0x06) {
			case 0x00:
				fNsfOptions->sampleRate = 11025;
				break;
			
			case 0x01:
				fNsfOptions->sampleRate = 22050;
				break;
				
			case 0x02:
				fNsfOptions->sampleRate = 44100;
				break;
				
			case 0x04:
				fNsfOptions->sampleRate = 48000;
				break;
				
			default:
				fNsfOptions->sampleRate = 44100;
		}
		
		
		if (byte1 & 0x20) {
			fNsfOptions->sampleBits = 16;
		} else {
			fNsfOptions->sampleBits = 8;
		}
		
		switch ((byte1 & 0x18) >> 0x03) {
			case 0x00:
				fNsfOptions->filter = NSF_FILTER_NONE;
				break;
				
			case 0x01:
				fNsfOptions->filter = NSF_FILTER_WEIGHTED;
				break;
				
			case 0x02:
				fNsfOptions->filter = NSF_FILTER_LOWPASS;
				break;
				
			default:
				fNsfOptions->filter = NSF_FILTER_NONE;
		}
			
		fNsfOptions->chanEnabled[0] = byte2 & 0x80;
		fNsfOptions->chanEnabled[1] = byte2 & 0x40;
		fNsfOptions->chanEnabled[2] = byte2 & 0x20;
		fNsfOptions->chanEnabled[3] = byte2 & 0x10;
		fNsfOptions->chanEnabled[4] = byte2 & 0x08;
		fNsfOptions->chanEnabled[5] = byte2 & 0x04;
						
	} else {
		uint8 byte1 = 0xa0;
		uint8 byte2 = 0xfc;
		optionsFile.SetTo ("/boot/home/config/settings/nosefart.options", 
						   B_WRITE_ONLY | B_CREATE_FILE);


		optionsFile.Write (&kNoseHeader, 4);
		optionsFile.Write (&byte1, 1);
		optionsFile.Write (&byte2, 1);
		
		fNsfOptions->sampleRate = 44100;
		fNsfOptions->sampleBits = 16;
		fNsfOptions->numChannels = 1;
		fNsfOptions->filter = NSF_FILTER_NONE;
		
		fNsfOptions->chanEnabled[0] = true;
		fNsfOptions->chanEnabled[1] = true;
		fNsfOptions->chanEnabled[2] = true;
		fNsfOptions->chanEnabled[3] = true;
		fNsfOptions->chanEnabled[4] = true;
		fNsfOptions->chanEnabled[5] = true;												   
	}
}



void
NosefartPlugin::SaveOptions (void)
{
	BFile optionsFile ("/boot/home/config/settings/nosefart.options",
						B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
	
	uint8 byte1 = 0x00;
	uint8 byte2 = 0x00;					
	
	optionsFile.Write (&kNoseHeader, 4);
	
	
	switch (fNsfOptions->sampleRate) {
		case 11025:
			byte1 |= 0x00;
			break;
			
		case 22050:
			byte1 |= 0x40;
			break;
			
		case 44100:
			byte1 |= 0x80;
			break;
			
		case 48000:
			byte1 |= 0xc0;
			break;
			
		default:
			byte1 |= 0x80;
	}
	
	byte1 |= fNsfOptions->sampleBits == 16 ? 0x20 : 0x00;
	
	switch (fNsfOptions->filter) {
		case NSF_FILTER_NONE:
			byte1 |= 0x00;
			break;
			
		case NSF_FILTER_WEIGHTED:
			byte1 |= 0x08;
			break;
			
		case NSF_FILTER_LOWPASS:
			byte1 |= 0x10;
			break;
			
		default:
			byte1 |= 0x00;
	}
	
	byte2 |= fNsfOptions->chanEnabled[0] == true ? 0x80 : 0;
	byte2 |= fNsfOptions->chanEnabled[1] == true ? 0x40 : 0;
	byte2 |= fNsfOptions->chanEnabled[2] == true ? 0x20 : 0;
	byte2 |= fNsfOptions->chanEnabled[3] == true ? 0x10 : 0;
	byte2 |= fNsfOptions->chanEnabled[4] == true ? 0x08 : 0;
	byte2 |= fNsfOptions->chanEnabled[5] == true ? 0x04 : 0;
		
	optionsFile.Write (&byte1, 1);
	optionsFile.Write (&byte2, 1);					
}


void 
NosefartPlugin::AbortPlaying (void)
{
}


void 
NosefartPlugin::Pause (bool on)
{
}


void
NosefartPlugin::NewSpeed (int32 value)
{
}


void
NosefartPlugin::NewVolume (int32 value)
{
}


void
NosefartPlugin::JumpTo (int32 newTime)
{
}


void
NosefartPlugin::CleanupPlaying (void)
{
}

