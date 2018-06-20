/*
	File:    ConfigView.h
	Author:  Eli Dayan
	Created: 03/28/01 15:52:43
*/

#ifndef		_CONFIG_VIEW_H_
#define		_CONFIG_VIEW_H_

#include <be/interface/View.h>
#include <be/interface/PopUpMenu.h>
#include <be/interface/MenuItem.h>
#include <be/interface/MenuField.h>
#include <be/interface/Box.h>
#include <be/interface/RadioButton.h>
#include <be/interface/CheckBox.h>
#include <be/interface/Button.h>

#include "../types.h"
#include "../machine/nsf.h"

#include "Misc.h"


const int32 kMsgSetFreq11 = 'SF11';
const int32 kMsgSetFreq22 = 'SF22';
const int32 kMsgSetFreq44 = 'SF44';
const int32 kMsgSetFreq48 = 'SF48';

const int32 kMsgSetQual8 = 'SQ08';
const int32 kMsgSetQual16 = 'SQ16';

const int32 kMsgSetFilterNone = 'SFNO';
const int32 kMsgSetFilterLP = 'SFLP';
const int32 kMsgSetFilterWeight = 'SFW0';

const int32 kMsgSetSq1 = 'SSQ1';
const int32 kMsgSetSq2 = 'SSQ2';
const int32 kMsgSetTri = 'STRI';
const int32 kMsgSetNoise = 'SNOI';
const int32 kMsgSetPCM = 'SPCM';
const int32 kMsgSetExt = 'SEXT';

const int32	kMsgCancel = 'CNCL';
const int32 kMsgDefaults = 'DFLT';
const int32 kMsgOK = 'OKAY';


class ConfigView : public BView
{
	public:
	ConfigView (BRect frame);
	virtual ~ConfigView();
	
	public:
	virtual void AttachedToWindow (void);
	
	public:
	void SetCurrentConfig (opstate_t *opstate);
	void SetDefaultConfig (void);
	
	public:
	bool Sq1Enabled (void) { return fSquare1->Value(); };
	bool Sq2Enabled (void) { return fSquare2->Value(); };
	bool TriEnabled (void) { return fTriangle->Value(); };
	bool NoiseEnabled (void) { return fNoise->Value(); };
	bool PCMEnabled (void) { return fPCM->Value(); };
	bool ExtEnabled (void) { return fExternal->Value(); };
	
	private:
	BMenu			*fFreqMenu;
	BMenuField		*fFreqField;
	BBox			*fQualityBox;
	BRadioButton	*fRadio8;
	BRadioButton	*fRadio16;
	BMenu			*fFilterMenu;
	BMenuField		*fFilterField;
	BBox			*fChannelsBox;
	BCheckBox		*fSquare1;
	BCheckBox		*fSquare2;
	BCheckBox		*fTriangle;
	BCheckBox		*fNoise;
	BCheckBox		*fPCM;
	BCheckBox		*fExternal;
	BButton			*fCancel;
	BButton			*fDefaults;
	BButton			*fOk;
};

#endif	//	_CONFIG_VIEW_H_

