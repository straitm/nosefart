/*
	File:    CL-AmpExport.cpp
	Author:  Eli Dayan
	Created: 12/09/00 17:06:14
*/


#include 	"NosefartPlugin.h"


extern "C" _EXPORT InputPlugin*
NewPlugin (PluginConstructorStruct *info)
{
	info->Version = CURRENT_INPUT_PLUGIN_VERSION;
	NosefartPlugin *plugin = new NosefartPlugin (info);
	
	return plugin;
}

