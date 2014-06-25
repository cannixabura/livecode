/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"
#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"
#include "sysdefs.h"
#include "globals.h"
#include "object.h"
#include "stack.h"
#include "cdata.h"
#include "objptr.h"
#include "field.h"
#include "object.h"
#include "button.h"
#include "card.h"
#include "exec.h"
#include "player.h"
#include "exec-interface.h"

// MERG-2014-06-25 [[ PlatformPlayer ]]
#ifdef FEATURE_PLATFORM_PLAYER
#include "platform.h"
#endif

////////////////////////////////////////////////////////////////////////////////

static void MCMultimediaQTVRConstraintsParse(MCExecContext& ctxt, MCStringRef p_input, MCMultimediaTrackList& r_output)
{
}

static void MCMultimediaQTVRConstraintsFormat(MCExecContext& ctxt, const MCMultimediaQTVRConstraints& p_input, MCStringRef& r_output)
{
	if (MCStringFormat(r_output, "%f,%f\n%f,%f\n%f,%f", p_input . minpan, p_input . maxpan,
						p_input . mintilt, p_input . maxtilt, p_input . minzoom, p_input . maxzoom))
		return;

	ctxt . Throw();
}

static void MCMultimediaQTVRConstraintsFree(MCExecContext& ctxt, MCMultimediaTrackList& p_input)
{
}

static MCExecCustomTypeInfo _kMCMultimediaQTVRConstraintsTypeInfo =
{
	"Multimedia.QTVRConstraints",
	sizeof(MCMultimediaQTVRConstraints),
	(void *)MCMultimediaQTVRConstraintsParse,
	(void *)MCMultimediaQTVRConstraintsFormat,
	(void *)MCMultimediaQTVRConstraintsFree
};

//////////

static void MCMultimediaTrackFormat(MCExecContext& ctxt, const MCMultimediaTrack& p_input, MCStringRef& r_output)
{
	if (MCStringFormat(r_output, "%d,%@,%d,%d", p_input . id, p_input . name,
									  p_input . offset, p_input . duration))
		return;

	ctxt . Throw();
}

static void MCMultimediaTrackParse(MCExecContext& ctxt, MCStringRef p_input, MCMultimediaTrack& r_output)
{
}

static void MCMultimediaTrackFree(MCExecContext& ctxt, MCMultimediaTrack& p_input)
{
	MCValueRelease(p_input . name);
}

static MCExecCustomTypeInfo _kMCMultimediaTrackTypeInfo =
{
	"Multimedia.Track",
	sizeof(MCMultimediaTrack),
	(void *)MCMultimediaTrackParse,
	(void *)MCMultimediaTrackFormat,
	(void *)MCMultimediaTrackFree
};

//////////

struct MCMultimediaQTVRNode
{
	uint2 id;
	MCMultimediaQTVRNodeType type;
};

static void MCMultimediaQTVRNodeFormat(MCExecContext& ctxt, const MCMultimediaQTVRNode& p_input, MCStringRef& r_output)
{
	if (MCStringFormat(r_output, "%d,%s", p_input . id, p_input . type == kMCQTVRNodePanoramaType ? "panorama" : "object"))
		return;	

	ctxt . Throw();
}

static void MCMultimediaQTVRNodeParse(MCExecContext& ctxt, MCStringRef p_input, MCMultimediaQTVRNode& r_output)
{
}

static void MCMultimediaQTVRNodeFree(MCExecContext& ctxt, MCMultimediaQTVRNode& p_input)
{
}

static MCExecCustomTypeInfo _kMCMultimediaQTVRNodeTypeInfo =
{
	"Multimedia.QTVRNode",
	sizeof(MCMultimediaQTVRNode),
	(void *)MCMultimediaQTVRNodeParse,
	(void *)MCMultimediaQTVRNodeFormat,
	(void *)MCMultimediaQTVRNodeFree
};

//////////

struct MCMultimediaQTVRHotSpot
{
	uint2 id;
	MCMultimediaQTVRHotSpotType type;
};

static void MCMultimediaQTVRHotSpotFormat(MCExecContext& ctxt, const MCMultimediaQTVRHotSpot& p_input, MCStringRef& r_output)
{
	switch (p_input . type)
	{
	case kMCQTVRHotSpotLinkType:
		if (MCStringFormat(r_output, "%d,%s", p_input . id, "link"))
			return;
		break;
	case kMCQTVRHotSpotURLType:
		if (MCStringFormat(r_output, "%d,%s", p_input . id, "url"))
			return;
		break;
	case kMCQTVRHotSpotUndefinedType:
		if (MCStringFormat(r_output, "%d,%s", p_input . id, "undefined"))
			return;
		break;
	}

	ctxt . Throw();
}

static void MCMultimediaQTVRHotSpotParse(MCExecContext& ctxt, MCStringRef p_input, MCMultimediaQTVRHotSpot& r_output)
{
}

static void MCMultimediaQTVRHotSpotFree(MCExecContext& ctxt, MCMultimediaQTVRHotSpot& p_input)
{
}

static MCExecCustomTypeInfo _kMCMultimediaQTVRHotSpotTypeInfo =
{
	"Multimedia.QTVRHotSpot",
	sizeof(MCMultimediaQTVRHotSpot),
	(void *)MCMultimediaQTVRHotSpotParse,
	(void *)MCMultimediaQTVRHotSpotFormat,
	(void *)MCMultimediaQTVRHotSpotFree
};

//////////

static MCExecSetTypeElementInfo _kMCInterfaceMediaTypesElementInfo[] =
{	
	{ "video", PLAYER_MEDIA_TYPE_VIDEO_BIT },
	{ "audio", PLAYER_MEDIA_TYPE_AUDIO_BIT },
	{ "text", PLAYER_MEDIA_TYPE_TEXT_BIT },
	{ "qtvr", PLAYER_MEDIA_TYPE_QTVR_BIT },
	{ "sprite", PLAYER_MEDIA_TYPE_SPRITE_BIT },
	{ "flash", PLAYER_MEDIA_TYPE_FLASH_BIT },
};

static MCExecSetTypeInfo _kMCInterfaceMediaTypesTypeInfo =
{
	"Interface.MediaTypes",
	sizeof(_kMCInterfaceMediaTypesElementInfo) / sizeof(MCExecSetTypeElementInfo),
	_kMCInterfaceMediaTypesElementInfo
};

//////////

MCExecSetTypeInfo *kMCInterfaceMediaTypesTypeInfo = &_kMCInterfaceMediaTypesTypeInfo;
MCExecCustomTypeInfo *kMCMultimediaTrackTypeInfo = &_kMCMultimediaTrackTypeInfo;
MCExecCustomTypeInfo *kMCMultimediaQTVRConstraintsTypeInfo = &_kMCMultimediaQTVRConstraintsTypeInfo;
MCExecCustomTypeInfo *kMCMultimediaQTVRNodeTypeInfo = &_kMCMultimediaQTVRNodeTypeInfo;
MCExecCustomTypeInfo *kMCMultimediaQTVRHotSpotTypeInfo = &_kMCMultimediaQTVRHotSpotTypeInfo;

static void copy_custom_list_as_string(MCExecContext& ctxt, MCExecCustomTypeInfo *p_type, void *p_elements, uindex_t p_count, char_t p_delimiter, MCStringRef& r_string)
{
	MCAutoListRef t_list;
	if (!MCListCreateMutable(p_delimiter, &t_list))
		goto throw_error;

	for(uindex_t i = 0; i < p_count; i++)
	{
		MCAutoStringRef t_element_as_string;
		((void(*)(MCExecContext&, void *, MCStringRef&))p_type -> format)(ctxt, (byte_t *)p_elements + p_type -> size * i, &t_element_as_string);
		if (ctxt . HasError())
			return;

		if (!MCListAppend(*t_list, *t_element_as_string))
			goto throw_error;
	}

	if (!MCListCopyAsString(*t_list, r_string))
		goto throw_error;
	
	return;

throw_error:
	ctxt . Throw();
	return;
}

////////////////////////////////////////////////////////////////////////////////

void MCPlayer::Redraw(void)
{
	if (!opened || !(flags & F_VISIBLE))
		return;

	// MW-2011-08-18: [[ Layers ]] Invalidate the whole object.
	layer_redrawall();
}

////////////////////////////////////////////////////////////////////////////////

void MCPlayer::GetFileName(MCExecContext& ctxt, MCStringRef& r_name)
{
	if (filename == nil)
		return;
	
	r_name = MCValueRetain(filename);
}

void MCPlayer::SetFileName(MCExecContext& ctxt, MCStringRef p_name)
{
	if (filename == nil || p_name == nil ||
		!MCStringIsEqualTo(p_name, filename, kMCCompareExact))
	{
		MCValueRelease(filename);
		filename = NULL;
		playstop();
		starttime = MAXUINT4; //clears the selection
		endtime = MAXUINT4;
		if (p_name != nil)
			filename = MCValueRetain(p_name);
		prepare(kMCEmptyString);
		Redraw();
	}
}

void MCPlayer::GetDontRefresh(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DONT_REFRESH);
}

void MCPlayer::SetDontRefresh(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_DONT_REFRESH))
		Redraw();
}

void MCPlayer::GetCurrentTime(MCExecContext& ctxt, uinteger_t& r_time)
{
	r_time = getmoviecurtime();
}

void MCPlayer::SetCurrentTime(MCExecContext& ctxt, uinteger_t p_time)
{
	setcurtime(p_time);
	if (isbuffering())
		Redraw();
}

void MCPlayer::GetDuration(MCExecContext& ctxt, uinteger_t& r_duration)
{
	r_duration = getduration();
}

void MCPlayer::GetLooping(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_LOOPING);
}

void MCPlayer::SetLooping(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_LOOPING))
	{
		setlooping((flags & F_LOOPING) != 0); //set/unset movie looping
		Redraw();
	}
}

void MCPlayer::GetPaused(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = ispaused() == True;
}

void MCPlayer::SetPaused(MCExecContext& ctxt, bool setting)
{
	playpause(setting); //pause or unpause the player
}

void MCPlayer::GetAlwaysBuffer(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_ALWAYS_BUFFER);
}

void MCPlayer::SetAlwaysBuffer(MCExecContext& ctxt, bool setting)
{
	// The actual buffering state is determined upon redrawing - therefore
	// we trigger a redraw to ensure we don't unbuffer when it is 
	// needed.

	if (changeflag(setting, F_ALWAYS_BUFFER) || opened)
		Redraw();
}

void MCPlayer::GetPlayRate(MCExecContext& ctxt, double& r_rate)
{
	r_rate = rate;
}

void MCPlayer::SetPlayRate(MCExecContext& ctxt, double p_rate)
{
	rate = p_rate;
	setplayrate();
}

void MCPlayer::GetStartTime(MCExecContext& ctxt, uinteger_t*& r_time)
{
	if (starttime == MAXUINT4)
		r_time = nil;
	else
		*r_time = starttime; //for QT, this is the selection start time
}

void MCPlayer::SetStartTime(MCExecContext& ctxt, uinteger_t* p_time)
{
	//for QT, this is the selection start time
	if (p_time == nil)
		starttime = endtime = MAXUINT4;
	else
	{
		starttime = *p_time;
#ifndef _MOBILE
		if (endtime == MAXUINT4) //if endtime is not set, set it to the length of movie
			endtime = getduration();
		else
			if (starttime > endtime)
				endtime = starttime;
#endif
	}
	setselection();
}

void MCPlayer::GetEndTime(MCExecContext& ctxt, uinteger_t*& r_time)
{
	if (endtime == MAXUINT4)
		r_time = nil;
	else
		*r_time = endtime; //for QT, this is the selection's end time
}

void MCPlayer::SetEndTime(MCExecContext& ctxt, uinteger_t* p_time)
{
	//for QT, this is the selection end time
	if (p_time == nil)
		starttime = endtime = MAXUINT4;
	else
	{
		endtime = *p_time;
#ifndef _MOBILE
		if (starttime == MAXUINT4)
			starttime = 0;
		else
			if (starttime > endtime)
				starttime = endtime;
#endif
	}
	setselection();
}

void MCPlayer::GetShowBadge(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_SHOW_BADGE);
}

void MCPlayer::SetShowBadge(MCExecContext& ctxt, bool setting)
{
	//if in the buffering mode we do not want to show/hide the badge
	if (!(flags & F_ALWAYS_BUFFER))
	{ //if always buffer flag is not set
		if (changeflag(setting, F_SHOW_BADGE))
		{
			if (!isbuffering()) //we are not actually buffering, let's show/hide the badge
				showbadge((flags & F_SHOW_BADGE) != 0); //show/hide movie's badge
			Redraw();
		}
	}
}

void MCPlayer::GetShowController(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_SHOW_CONTROLLER);
}

void MCPlayer::SetShowController(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_SHOW_CONTROLLER))
		showcontroller((flags & F_VISIBLE) != 0 && (flags & F_SHOW_CONTROLLER) != 0);
}

void MCPlayer::GetPlaySelection(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_PLAY_SELECTION);
}

void MCPlayer::SetPlaySelection(MCExecContext& ctxt, bool setting)
{
	//make QT movie plays only the selected part
	if (changeflag(setting, F_PLAY_SELECTION))
		playselection((flags & F_PLAY_SELECTION) != 0);
}

void MCPlayer::GetShowSelection(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_SHOW_SELECTION);
}

void MCPlayer::SetShowSelection(MCExecContext& ctxt, bool setting)
{
	//means make QT movie editable
	if (changeflag(setting, F_SHOW_SELECTION))
		editmovie((flags & F_SHOW_SELECTION) != 0);
}

void MCPlayer::GetCallbacks(MCExecContext& ctxt, MCStringRef& r_callbacks)
{
	r_callbacks = MCValueRetain(userCallbackStr);
}

void MCPlayer::SetCallbacks(MCExecContext& ctxt, MCStringRef p_callbacks)
{
    // MERG-2014-06-25: [[ PlatformPlayer ]]
#ifndef FEATURE_PLATFORM_PLAYER
#ifdef FEATURE_QUICKTIME
    deleteUserCallbacks(); //delete all callbacks for this player
#endif
#endif
    MCValueAssign(userCallbackStr, p_callbacks);
    
    if (!MCStringIsEmpty(userCallbackStr))
    {
#ifndef FEATURE_PLATFORM_PLAYER
#ifdef FEATURE_QUICKTIME
        installUserCallbacks(); //install all callbacks for this player
#endif
#endif
    }
#ifdef FEATURE_PLATFORM_PLAYER
    SynchronizeUserCallbacks();
#endif	
}

void MCPlayer::GetTimeScale(MCExecContext& ctxt, uinteger_t& r_scale)
{
	r_scale = gettimescale();
}

void MCPlayer::GetFormattedHeight(MCExecContext& ctxt, integer_t& r_height)
{
	r_height = getpreferredrect() . height;
}

void MCPlayer::GetFormattedWidth(MCExecContext& ctxt, integer_t& r_width)
{
	r_width = getpreferredrect() . width;
}

void MCPlayer::GetMovieControllerId(MCExecContext& ctxt, integer_t& r_id)
{
#ifndef FEATURE_QUICKTIME
	r_id = (int)NULL;
#else
#ifndef FEATURE_PLATFORM_PLAYER
    // COCOA-TODO
	r_id = (int4)theMC;
#endif
#endif
}

void MCPlayer::SetMovieControllerId(MCExecContext& ctxt, integer_t p_id)
{
	setmoviecontrollerid(p_id);
}

void MCPlayer::GetPlayLoudness(MCExecContext& ctxt, uinteger_t& r_loudness)
{
	r_loudness = getloudness();
}

void MCPlayer::SetPlayLoudness(MCExecContext& ctxt, uinteger_t p_loudness)
{
	loudness = MCU_min(p_loudness, (uint4)100);
	setloudness();
}

void MCPlayer::GetTrackCount(MCExecContext& ctxt, uinteger_t& r_count)
{
	r_count = gettrackcount();
}

void MCPlayer::GetMediaTypes(MCExecContext& ctxt, intset_t& r_types)
{
	MCPlayerMediaTypeSet types;
	types = getmediatypes();
	r_types = (intenum_t)types;
}

void MCPlayer::GetCurrentNode(MCExecContext& ctxt, uinteger_t& r_node)
{
	r_node = getcurrentnode();
}

void MCPlayer::SetCurrentNode(MCExecContext& ctxt, uinteger_t p_node)
{
	if (changecurrentnode(p_node))
		Redraw();
}

void MCPlayer::GetPan(MCExecContext& ctxt, double& r_pan)
{
	r_pan = getpan();
}

void MCPlayer::SetPan(MCExecContext& ctxt, double p_pan)
{
	if (changepan(p_pan))
		Redraw();
}

void MCPlayer::GetTilt(MCExecContext& ctxt, double& r_tilt)
{
	r_tilt = gettilt();
}

void MCPlayer::SetTilt(MCExecContext& ctxt, double p_tilt)
{
	if (changetilt(p_tilt))
		Redraw();
}

void MCPlayer::GetZoom(MCExecContext& ctxt, double& r_zoom)
{
	r_zoom = getzoom();
}

void MCPlayer::SetZoom(MCExecContext& ctxt, double p_zoom)
{
	if (changezoom(p_zoom))
		Redraw();
}

void MCPlayer::GetQTTracks(MCExecContext& ctxt, MCStringRef& r_tracks)
{
#ifdef FEATURE_QUICKTIME
	uint2 trackcount = gettrackcount();	
	MCAutoArray<MCMultimediaTrack> t_track_array;

	for (uindex_t i = 1 ; i <= trackcount ; i++)
	{
		MCMultimediaTrack t_track;
		if (!gettrack(i, t_track . id, t_track . name, t_track . offset, t_track . duration))
			break;
		t_track_array . Push(t_track);
	}

	copy_custom_list_as_string(ctxt, kMCMultimediaTrackTypeInfo, t_track_array . Ptr(), t_track_array . Size(), '\n', r_tracks);

	for (uindex_t i = 0; i < t_track_array . Size(); i++)
		MCMultimediaTrackFree(ctxt, t_track_array[i]);
#endif
}

void MCPlayer::GetAVITracks(MCExecContext& ctxt,  MCStringRef& r_tracks)
{
}

void MCPlayer::GetX11Tracks(MCExecContext& ctxt,  MCStringRef& r_tracks)
{
}

void MCPlayer::GetTracks(MCExecContext& ctxt, MCStringRef& r_tracks)
{
	if (getstate(CS_PREPARED))
    // MERG-2014-06-25: [[ PlatformPlayer ]]
#ifdef FEATURE_PLATFORM_PLAYER
    if (m_platform_player != nil)
	{
		uindex_t t_track_count;
		MCPlatformCountPlayerTracks(m_platform_player, t_track_count);
        MCAutoListRef t_tracks_list;
        /* UNCHECKED */ MCListCreateMutable('\n', &t_tracks_list);
            
			for(uindex_t i = 0; i < t_track_count; i++)
			{
				MCAutoStringRef t_track;
                MCAutoStringRef t_name;
                
				uint32_t t_id;
				uint32_t t_offset, t_duration;
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyId, kMCPlatformPropertyTypeUInt32, &t_id);
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyMediaTypeName, kMCPlatformPropertyTypeNativeCString, &(&t_name));
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyOffset, kMCPlatformPropertyTypeUInt32, &t_offset);
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_offset);
                /* UNCHECKED */ MCStringFormat(&t_track, "%u,%@,%u,%u", t_id, *t_name, t_offset, t_duration);
                /* UNCHECKED */ MCListAppend(*t_tracks_list, *t_track);
			}
            /* UNCHECKED */ MCListCopyAsString(*t_tracks_list, r_tracks);
		}
#else
#ifdef FEATURE_QUICKTIME
		if (qtstate == QT_INITTED)
			GetQTTracks(ctxt, r_tracks);
#ifdef TARGET_PLATFORM_WINDOWS
		else
			GetAVITracks(ctxt, r_tracks);
#endif
#elif defined(X11)
		GetX11Tracks(ctxt, r_tracks);
#else
	0 == 0;
#endif
#endif
}

void MCPlayer::GetConstraints(MCExecContext& ctxt, MCMultimediaQTVRConstraints& r_constraints)
{
    // MERG-2014-06-25: [[ PlatformPlayer ]]
#ifdef FEATURE_PLATFORM_PLAYER
    if (m_platform_player != nil)
        MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRConstraints, kMCPlatformPropertyTypePlayerQTVRConstraints, (MCPlatformPlayerQTVRConstraints*)&(r_constraints));
#else
#ifdef FEATURE_QUICKTIME
		getqtvrconstraints(1, r_constraints . minpan, r_constraints . maxpan);
		getqtvrconstraints(2, r_constraints . mintilt, r_constraints . maxtilt);
		getqtvrconstraints(3, r_constraints . minzoom, r_constraints . maxzoom);
#endif
#endif
}

void MCPlayer::GetNodes(MCExecContext& ctxt, MCStringRef& r_nodes)
{
    MCStringRef t_nodes;
    t_nodes = MCValueRetain(kMCEmptyString);
#ifdef FEATURE_PLATFORM_PLAYER
	// COCOA-TODO: MCPlayer::getnodes();
#else
#ifdef FEATURE_QUICKTIME
	uint2 nodecount = getnodecount();
	MCAutoArray<MCMultimediaQTVRNode> t_node_array;

	for (uindex_t i = 1 ; i <= nodecount ; i++)
	{
		MCMultimediaQTVRNode t_node;
		if (!getnode(i, t_node . id, t_node . type))
			break;
		t_node_array . Push(t_node);
	}

    MCValueRelease(t_nodes);
	copy_custom_list_as_string(ctxt, kMCMultimediaQTVRNodeTypeInfo, t_node_array . Ptr(), t_node_array . Size(), '\n', t_nodes);

	for (uindex_t i = 0; i < t_node_array . Size(); i++)
		MCMultimediaQTVRNodeFree(ctxt, t_node_array[i]);
#endif
#endif
    r_nodes = t_nodes;
}
	
void MCPlayer::GetHotSpots(MCExecContext& ctxt, MCStringRef& r_spots)
{
    MCStringRef t_spots;
    t_spots = MCValueRetain(kMCEmptyString);
#ifdef FEATURE_PLATFORM_PLAYER
	// COCOA-TODO: MCPlayer::gethotspots();
#else
#ifdef FEATURE_QUICKTIME
	uint2 hotspotcount = gethotspotcount();
	MCAutoArray<MCMultimediaQTVRHotSpot> t_spot_array;

	for (uindex_t i = 1 ; i <= hotspotcount ; i++)
	{
		MCMultimediaQTVRHotSpot t_spot;
		if (!gethotspot(i, t_spot . id, t_spot . type))
			break;
		t_spot_array . Push(t_spot);
	}

    MCValueRelease(t_spots);
	copy_custom_list_as_string(ctxt, kMCMultimediaQTVRHotSpotTypeInfo, t_spot_array . Ptr(), t_spot_array . Size(), '\n', t_spots);

	for (uindex_t i = 0; i < t_spot_array . Size(); i++)
		MCMultimediaQTVRHotSpotFree(ctxt, t_spot_array[i]);
#endif
#endif
    r_spots = t_spots;
}

void MCPlayer::SetShowBorder(MCExecContext& ctxt, bool setting)
{
    MCControl::SetShowBorder(ctxt, setting);
    setrect(rect);
    Redraw();
}

void MCPlayer::SetBorderWidth(MCExecContext& ctxt, uinteger_t width)
{
    MCObject::SetBorderWidth(ctxt, width);
    setrect(rect);
    Redraw();
}

void MCPlayer::SetVisibility(MCExecContext& ctxt, uinteger_t part, bool setting, bool visible)
{
    uint4 oldflags = flags;
    MCObject::SetVisibility(ctxt, part, setting, visible);
    if (flags != oldflags && !(flags & F_VISIBLE))
        playstop();
    // MERG-2014-06-25: [[ PlatformPlayer ]]
#ifdef FEATURE_PLATFORM_PLAYER
    if (m_platform_player != nil)
    {
        bool t_visible;
        t_visible = getflag(F_VISIBLE);
        MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyVisible, kMCPlatformPropertyTypeBool, &t_visible);
    }
#else
#ifdef FEATURE_QUICKTIME
    if (theMC != NULL)
        qt_setcontrollervisible();
#endif
#endif
}

void MCPlayer::SetVisible(MCExecContext& ctxt, uinteger_t part, bool setting)
{
    SetVisibility(ctxt, part, setting, true);
}

void MCPlayer::SetInvisible(MCExecContext& ctxt, uinteger_t part, bool setting)
{
    SetVisibility(ctxt, part, setting, false);
}

void MCPlayer::SetTraversalOn(MCExecContext& ctxt, bool setting)
{
    MCObject::SetTraversalOn(ctxt, setting);
    
    if (!ctxt . HasError())
    {
#ifndef FEATURE_PLATFORM_PLAYER
#ifdef FEATURE_QUICKTIME
    if (qtstate == QT_INITTED && getstate(CS_PREPARED))
        qt_enablekeys((flags & F_TRAVERSAL_ON) != 0);
#endif
#endif
    }
}

void MCPlayer::GetEnabledTracks(MCExecContext& ctxt, uindex_t& r_count, uinteger_t*& r_tracks)
{
    // MERG-2014-06-25 [[ PlatformPlayer ]]
#ifdef FEATURE_PLATFORM_PLAYER
    if (m_platform_player != nil)
    {
        uindex_t t_track_count;
        MCPlatformCountPlayerTracks(m_platform_player, t_track_count);
        uinteger_t *t_track_ids;
        uindex_t t_count;
        t_count = 0;
        
        for(uindex_t i = 0; i < t_track_count; i++)
        {
            uint32_t t_id;
            uint32_t t_enabled;
            MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyId, kMCPlatformPropertyTypeUInt32, &t_id);
            MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyEnabled, kMCPlatformPropertyTypeBool, &t_enabled);
            if (t_enabled)
            {
                MCMemoryReallocate(t_track_ids, ++t_count * sizeof(uinteger_t), t_track_ids);
                t_track_ids[t_count - 1] = t_id;
            }
        }
        
        r_count = t_count;
        r_tracks = t_track_ids;
    }
#else
#ifdef FEATURE_QUICKTIME
	if (getstate(CS_PREPARED))
        if (qtstate == QT_INITTED)
			qt_getenabledtracks(r_count, r_tracks);
#ifdef TARGET_PLATFORM_WINDOWS
		else
			avi_getenabledtracks(r_count, r_tracks);
#endif
#elif defined(X11)
    x11_getenabledtracks(r_count, r_tracks);
#else
    r_count = 0;
#endif
#endif
}