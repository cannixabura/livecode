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
#include "core.h"

#include "execpt.h"
#include "util.h"
#include "font.h"
#include "sellst.h"
#include "stack.h"
#include "stacklst.h"
#include "card.h"
#include "field.h"
#include "player-platform.h"
#include "aclip.h"
#include "mcerror.h"
#include "param.h"
#include "globals.h"
#include "mode.h"
#include "context.h"
#include "osspec.h"
#include "redraw.h"
#include "gradient.h"

#include "graphics_util.h"

//// PLATFORM PLAYER

#include "platform.h"

static MCPlatformPlayerMediaType ppmediatypes[] =
{
	kMCPlatformPlayerMediaTypeVideo,
	kMCPlatformPlayerMediaTypeAudio,
	kMCPlatformPlayerMediaTypeText,
	kMCPlatformPlayerMediaTypeQTVR,
	kMCPlatformPlayerMediaTypeSprite,
	kMCPlatformPlayerMediaTypeFlash,
};

static const char *ppmediastrings[] =
{
	"video",
	"audio",
	"text",
	"qtvr",
	"sprite",
	"flash"
};

#define CONTROLLER_HEIGHT 26
#define SELECTION_RECT_WIDTH CONTROLLER_HEIGHT / 4
#define LIGHTGRAY 1
#define PURPLE 2
#define SOMEGRAY 3
#define DARKGRAY 4

enum
{
    kMCPlayerControllerPartUnknown,
    
    kMCPlayerControllerPartVolume,
    kMCPlayerControllerPartVolumeBar,
    kMCPlayerControllerPartVolumeWell,
    kMCPlayerControllerPartVolumeSelector,
    kMCPlayerControllerPartPlay,
    kMCPlayerControllerPartScrubBack,
    kMCPlayerControllerPartScrubForward,
    kMCPlayerControllerPartThumb,
    kMCPlayerControllerPartWell,
    kMCPlayerControllerPartSelectionStart,
    kMCPlayerControllerPartSelectionFinish,
    kMCPlayerControllerPartSelectedArea,
    kMCPlayerControllerPartVolumeArea,
    kMCPlayerControllerPartPlayedArea,
    
};

static MCColor controllercolors[] = {
    {0, 0x8000, 0x8000, 0x8000, 0, 0},         /* 50% gray */
    
    {0, 0xCCCC, 0xCCCC, 0xCCCC, 0, 0},         /* 20% gray -- 80% white */
    
    {0, 0xa8a8, 0x0101, 0xffff, 0, 0},         /* Purple */
    
    //{0, 0xcccc, 0x9999, 0xffff, 0, 0},         /* Magenda */
    
    {0, 0x2b2b, 0x2b2b, 0x2b2b, 0, 0},         /* gray */
    
    {0, 0x2222, 0x2222, 0x2222, 0, 0},         /* dark gray */
    
    
};
//-----------------------------------------------------------------------------
// Control Implementation
//

#define XANIM_WAIT 10.0
#define XANIM_COMMAND 1024

MCPlayer::MCPlayer()
{
	flags |= F_TRAVERSAL_ON;
	nextplayer = NULL;
	rect.width = rect.height = 128;
	filename = NULL;
	istmpfile = False;
	scale = 1.0;
	rate = 1.0;
	lasttime = 0;
	starttime = endtime = MAXUINT4;
	disposable = istmpfile = False;
	userCallbackStr = NULL;
	formattedwidth = formattedheight = 0;
	loudness = 100;
    
	m_platform_player = nil;
    
    m_grabbed_part = kMCPlayerControllerPartUnknown;
    m_initial_rate = 0.0;
    m_was_paused = True;
    m_inside = False;
    m_show_volume = false;
    m_scrub_back_is_pressed = false;
    m_scrub_forward_is_pressed = false;
}

MCPlayer::MCPlayer(const MCPlayer &sref) : MCControl(sref)
{
	nextplayer = NULL;
	filename = strclone(sref.filename);
	istmpfile = False;
	scale = 1.0;
	rate = sref.rate;
	lasttime = sref.lasttime;
	starttime = sref.starttime;
	endtime = sref.endtime;
	disposable = istmpfile = False;
	userCallbackStr = strclone(sref.userCallbackStr);
	formattedwidth = formattedheight = 0;
	loudness = sref.loudness;
	
	m_platform_player = nil;
    
    m_grabbed_part = kMCPlayerControllerPartUnknown;
    m_initial_rate = 0.0;
    m_was_paused = True;
    m_inside = False;
    m_show_volume = false;
    m_scrub_back_is_pressed = false;
    m_scrub_forward_is_pressed = false;
}

MCPlayer::~MCPlayer()
{
	// OK-2009-04-30: [[Bug 7517]] - Ensure the player is actually closed before deletion, otherwise dangling references may still exist.
	while (opened)
		close();
	
	playstop();
    
	if (m_platform_player != nil)
		MCPlatformPlayerRelease(m_platform_player);
    
	delete filename;
	delete userCallbackStr;
}

Chunk_term MCPlayer::gettype() const
{
	return CT_PLAYER;
}

const char *MCPlayer::gettypestring()
{
	return MCplayerstring;
}

MCRectangle MCPlayer::getactiverect(void)
{
	return MCU_reduce_rect(getrect(), getflag(F_SHOW_BORDER) ? borderwidth : 0);
}

void MCPlayer::open()
{
	MCControl::open();
	prepare(MCnullstring);
}

void MCPlayer::close()
{
	MCControl::close();
	if (opened == 0)
	{
		state |= CS_CLOSING;
		playstop();
		state &= ~CS_CLOSING;
	}
}

Boolean MCPlayer::kdown(const char *string, KeySym key)
{
    if (state & CS_PREPARED)
    {
        switch (key)
        {
            case XK_Return:
                playpause(!ispaused());
                break;
            case XK_space:
                playpause(!ispaused());
                break;
            case XK_Right:
            {
                if(ispaused())
                    playstepforward();
                else
                {
                    playstepforward();
                    playpause(!ispaused());
                }
            }
                break;
            case XK_Left:
            {
                if(ispaused())
                    playstepback();
                else
                {
                    playstepback();
                    playpause(!ispaused());
                }
            }
                break;
            default:
                break;
        }
    }
	if (!(state & CS_NO_MESSAGES))
		if (MCObject::kdown(string, key))
			return True;
    
	return False;
}

Boolean MCPlayer::kup(const char *string, KeySym key)
{
    return False;
}

Boolean MCPlayer::mfocus(int2 x, int2 y)
{
	if (!(flags & F_VISIBLE || MCshowinvisibles)
        || flags & F_DISABLED && getstack()->gettool(this) == T_BROWSE)
		return False;
    
    Boolean t_success;
    t_success = MCControl::mfocus(x, y);
    if (t_success)
        handle_mfocus(x,y);
    return t_success;
}

void MCPlayer::munfocus()
{
    /*
     if ( m_show_volume )
     {
     if (state & CS_PLAYING || state & CS_PAUSED)
     m_show_volume = false;
     }
     */
	getstack()->resetcursor(True);
	MCControl::munfocus();
}

Boolean MCPlayer::mdown(uint2 which)
{
    if (state & CS_MFOCUSED || flags & F_DISABLED)
		return False;
    if (state & CS_MENU_ATTACHED)
		return MCObject::mdown(which);
	state |= CS_MFOCUSED;
	if (flags & F_TRAVERSAL_ON && !(state & CS_KFOCUSED))
		getstack()->kfocusset(this);
    
	switch (which)
	{
        case Button1:
            switch (getstack()->gettool(this))
		{
            case T_BROWSE:
                message_with_args(MCM_mouse_down, "1");
                handle_mdown(which);
                MCscreen -> addtimer(this, MCM_internal, MCblinkrate);
                break;
            case T_POINTER:
            case T_PLAYER:  //when the movie object is in editing mode
                start(True); //starting draggin or resizing
                playpause(True);  //pause the movie
                break;
            case T_HELP:
                break;
            default:
                return False;
		}
            break;
		case Button2:
            if (message_with_args(MCM_mouse_down, "2") == ES_NORMAL)
                return True;
            break;
		case Button3:
            message_with_args(MCM_mouse_down, "3");
            break;
	}
	return True;
}

Boolean MCPlayer::mup(uint2 which) //mouse up
{
	if (!(state & CS_MFOCUSED))
		return False;
	if (state & CS_MENU_ATTACHED)
		return MCObject::mup(which);
	state &= ~CS_MFOCUSED;
	if (state & CS_GRAB)
	{
		ungrab(which);
		return True;
	}
	switch (which)
	{
        case Button1:
            switch (getstack()->gettool(this))
		{
            case T_BROWSE:
                if (MCU_point_in_rect(rect, mx, my))
                    message_with_args(MCM_mouse_up, "1");
                else
                    message_with_args(MCM_mouse_release, "1");
                MCscreen -> cancelmessageobject(this, MCM_internal);
                handle_mup(which);
                break;
            case T_PLAYER:
            case T_POINTER:
                end();       //stop dragging or moving the movie object, will change controller size
                break;
            case T_HELP:
                help();
                break;
            default:
                return False;
		}
            break;
        case Button2:
        case Button3:
            if (MCU_point_in_rect(rect, mx, my))
                message_with_args(MCM_mouse_up, which);
            else
                message_with_args(MCM_mouse_release, which);
            break;
	}
	return True;
}

Boolean MCPlayer::doubledown(uint2 which)
{
	return MCControl::doubledown(which);
}

Boolean MCPlayer::doubleup(uint2 which)
{
	return MCControl::doubleup(which);
}

void MCPlayer::setrect(const MCRectangle &nrect)
{
	rect = nrect;
	
	if (m_platform_player != nil)
	{
		MCRectangle trect = MCU_reduce_rect(rect, getflag(F_SHOW_BORDER) ? borderwidth : 0);
        
        if (getflag(F_SHOW_CONTROLLER))
            trect . height -= CONTROLLER_HEIGHT;
        
        // MW-2014-04-09: [[ Bug 11922 ]] Make sure we use the view not device transform
        //   (backscale factor handled in platform layer).
		trect = MCRectangleGetTransformedBounds(trect, getstack()->getviewtransform());
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyRect, kMCPlatformPropertyTypeRectangle, &trect);
	}
}

void MCPlayer::timer(MCNameRef mptr, MCParameter *params)
{
    if (MCNameIsEqualTo(mptr, MCM_play_started, kMCCompareCaseless))
    {
        state &= ~CS_PAUSED;
    }
    else
        if (MCNameIsEqualTo(mptr, MCM_play_stopped, kMCCompareCaseless))
        {
            state |= CS_PAUSED;
            
            if (disposable)
            {
                playstop();
                return; //obj is already deleted, do not pass msg up.
            }
        }
        else if (MCNameIsEqualTo(mptr, MCM_play_paused, kMCCompareCaseless))
		{
			state |= CS_PAUSED;
            
		}
        else if (MCNameIsEqualTo(mptr, MCM_internal, kMCCompareCaseless))
        {
            handle_mstilldown(Button1);
            MCscreen -> addtimer(this, MCM_internal, MCblinkrate);
        }
    MCControl::timer(mptr, params);
}

Exec_stat MCPlayer::getprop(uint4 parid, Properties which, MCExecPoint &ep, Boolean effective)
{
	uint2 i = 0;
	switch (which)
	{
#ifdef /* MCPlayer::getprop */ LEGACY_EXEC
        case P_FILE_NAME:
            if (filename == NULL)
                ep.clear();
            else
                ep.setsvalue(filename);
            break;
        case P_DONT_REFRESH:
            ep.setboolean(getflag(F_DONT_REFRESH));
            break;
        case P_CURRENT_TIME:
            ep.setint(getmoviecurtime());
            break;
        case P_DURATION:
            ep.setint(getduration());
            break;
        case P_LOOPING:
            ep.setboolean(getflag(F_LOOPING));
            break;
        case P_PAUSED:
            ep.setboolean(ispaused());
            break;
        case P_ALWAYS_BUFFER:
            ep.setboolean(getflag(F_ALWAYS_BUFFER));
            break;
        case P_PLAY_RATE:
            ep.setr8(rate, ep.getnffw(), ep.getnftrailing(), ep.getnfforce());
            return ES_NORMAL;
        case P_START_TIME:
            if (starttime == MAXUINT4)
                ep.clear();
            else
                ep.setnvalue(starttime);//for QT, this is the selection start time
            break;
        case P_END_TIME:
            if (endtime == MAXUINT4)
                ep.clear();
            else
                ep.setnvalue(endtime); //for QT, this is the selection's end time
            break;
        case P_SHOW_BADGE:
            ep.setboolean(getflag(F_SHOW_BADGE));
            break;
        case P_SHOW_CONTROLLER:
            ep.setboolean(getflag(F_SHOW_CONTROLLER));
            break;
        case P_PLAY_SELECTION:
            ep.setboolean(getflag(F_PLAY_SELECTION));
            break;
        case P_SHOW_SELECTION:
            ep.setboolean(getflag(F_SHOW_SELECTION));
            break;
        case P_CALLBACKS:
            ep.setsvalue(userCallbackStr);
            break;
        case P_TIME_SCALE:
            ep.setint(gettimescale());
            break;
        case P_FORMATTED_HEIGHT:
            ep.setint(getpreferredrect().height);
            break;
        case P_FORMATTED_WIDTH:
            ep.setint(getpreferredrect().width);
            break;
        case P_MOVIE_CONTROLLER_ID:
            ep.setint((int)NULL);
            break;
        case P_PLAY_LOUDNESS:
            ep.setint(getloudness());
            break;
        case P_TRACK_COUNT:
            if (m_platform_player != nil)
            {
                uindex_t t_count;
                MCPlatformCountPlayerTracks(m_platform_player, t_count);
                i = t_count;
            }
            ep.setint(i);
            break;
        case P_TRACKS:
            gettracks(ep);
            break;
        case P_ENABLED_TRACKS:
            getenabledtracks(ep);
            break;
        case P_MEDIA_TYPES:
            ep.clear();
            if (m_platform_player != nil)
            {
                MCPlatformPlayerMediaTypes t_types;
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyMediaTypes, kMCPlatformPropertyTypePlayerMediaTypes, &t_types);
                bool first = true;
                for (i = 0 ; i < sizeof(ppmediatypes) / sizeof(ppmediatypes[0]) ; i++)
                    if ((t_types & (1 << ppmediatypes[i])) != 0)
                    {
                        ep.concatcstring(ppmediastrings[i], EC_COMMA, first);
                        first = false;
                    }
            }
            break;
        case P_CURRENT_NODE:
			if (m_platform_player != nil)
				MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRNode, kMCPlatformPropertyTypeUInt16, &i);
            ep.setint(i);
            break;
        case P_PAN:
		{
			real8 pan = 0.0;
			if (m_platform_player != nil)
				MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRPan, kMCPlatformPropertyTypeDouble, &pan);
            
			ep.setr8(pan, ep.getnffw(), ep.getnftrailing(), ep.getnfforce());
		}
            break;
        case P_TILT:
		{
			real8 tilt = 0.0;
			if (m_platform_player != nil)
				MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRTilt, kMCPlatformPropertyTypeDouble, &tilt);
            
			ep.setr8(tilt, ep.getnffw(), ep.getnftrailing(), ep.getnfforce());
		}
            break;
        case P_ZOOM:
		{
			real8 zoom = 0.0;
			if (m_platform_player != nil)
				MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRZoom, kMCPlatformPropertyTypeDouble, &zoom);
            
			ep.setr8(zoom, ep.getnffw(), ep.getnftrailing(), ep.getnfforce());
		}
            break;
        case P_CONSTRAINTS:
			ep.clear();
			if (m_platform_player != nil)
			{
				MCPlatformPlayerQTVRConstraints t_constraints;
				MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRConstraints, kMCPlatformPropertyTypePlayerQTVRConstraints, &t_constraints);
				ep.appendstringf("%lf,%lf\n", t_constraints . x_min, t_constraints . x_max);
				ep.appendstringf("%lf,%lf\n", t_constraints . y_min, t_constraints . y_max);
				ep.appendstringf("%lf,%lf", t_constraints . z_min, t_constraints . z_max);
			}
            break;
        case P_NODES:
            getnodes(ep);
            break;
        case P_HOT_SPOTS:
            gethotspots(ep);
            break;
#endif /* MCPlayer::getprop */
        default:
            return MCControl::getprop(parid, which, ep, effective);
	}
	return ES_NORMAL;
}

Exec_stat MCPlayer::setprop(uint4 parid, Properties p, MCExecPoint &ep, Boolean effective)
{
	Boolean dirty = False;
	Boolean wholecard = False;
	uint4 ctime;
	MCString data = ep.getsvalue();
    
	switch (p)
	{
#ifdef /* MCPlayer::setprop */ LEGACY_EXEC
        case P_FILE_NAME:
            if (filename == NULL || data != filename)
            {
                delete filename;
                filename = NULL;
                playstop();
                starttime = MAXUINT4; //clears the selection
                endtime = MAXUINT4;
                if (data != MCnullmcstring)
                    filename = data.clone();
                prepare(MCnullstring);
                dirty = wholecard = True;
            }
            break;
        case P_DONT_REFRESH:
            if (!MCU_matchflags(data, flags, F_DONT_REFRESH, dirty))
            {
                MCeerror->add(EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            break;
        case P_ALWAYS_BUFFER:
            if (!MCU_matchflags(data, flags, F_ALWAYS_BUFFER, dirty))
            {
                MCeerror->add(EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            
            // The actual buffering state is determined upon redrawing - therefore
            // we trigger a redraw to ensure we don't unbuffer when it is
            // needed.
            
            if (opened)
                dirty = True;
            break;
        case P_CALLBACKS:
            delete userCallbackStr;
            if (data.getlength() == 0)
                userCallbackStr = NULL;
            else
            {
                userCallbackStr = data.clone();
            }
			SynchronizeUserCallbacks();
            break;
        case P_CURRENT_TIME:
            if (!MCU_stoui4(data, ctime))
            {
                MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                return ES_ERROR;
            }
            setcurtime(ctime);
            if (isbuffering())
                dirty = True;
            break;
        case P_LOOPING:
            if (!MCU_matchflags(data, flags, F_LOOPING, dirty))
            {
                MCeerror->add(EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            if (dirty)
                setlooping((flags & F_LOOPING) != 0); //set/unset movie looping
            break;
        case P_PAUSED:
            playpause(data == MCtruemcstring); //pause or unpause the player
            break;
        case P_PLAY_RATE:
            if (!MCU_stor8(data, rate))
            {
                MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                return ES_ERROR;
            }
            setplayrate();
            break;
        case P_START_TIME: //for QT, this is the selection start time
            if (data.getlength() == 0)
                starttime = endtime = MAXUINT4;
            else
            {
                if (!MCU_stoui4(data, starttime))
                {
                    MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                    return ES_ERROR;
                }
            }
            setselection();
            break;
        case P_END_TIME: //for QT, this is the selection end time
            if (data.getlength() == 0)
                starttime = endtime = MAXUINT4;
            else
            {
                if (!MCU_stoui4(data, endtime))
                {
                    MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                    return ES_ERROR;
                }
            }
            setselection();
            break;
        case P_TRAVERSAL_ON:
            if (MCControl::setprop(parid, p, ep, effective) != ES_NORMAL)
                return ES_ERROR;
            break;
        case P_SHOW_BADGE: //if in the buffering mode we do not want to show/hide the badge
            if (!(flags & F_ALWAYS_BUFFER))
            { //if always buffer flag is not set
                if (!MCU_matchflags(data, flags, F_SHOW_BADGE, dirty))
                {
                    MCeerror->add(EE_OBJECT_NAB, 0, 0, data);
                    return ES_ERROR;
                }
                if (dirty && !isbuffering()) //we are not actually buffering, let's show/hide the badge
                    showbadge((flags & F_SHOW_BADGE) != 0); //show/hide movie's badge
            }
            break;
        case P_SHOW_CONTROLLER:
            if (!MCU_matchflags(data, flags, F_SHOW_CONTROLLER, dirty))
            {
                MCeerror->add(EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            if (dirty)
            {
                showcontroller((flags & F_VISIBLE) != 0
                               && (flags & F_SHOW_CONTROLLER) != 0);
                dirty = False;
            }
            break;
        case P_PLAY_SELECTION: //make QT movie plays only the selected part
            if (!MCU_matchflags(data, flags, F_PLAY_SELECTION, dirty))
            {
                MCeerror->add
                (EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            if (dirty)
                playselection((flags & F_PLAY_SELECTION) != 0);
            break;
        case P_SHOW_SELECTION: //means make QT movie editable
            if (!MCU_matchflags(data, flags, F_SHOW_SELECTION, dirty))
            {
                MCeerror->add
                (EE_OBJECT_NAB, 0, 0, data);
                return ES_ERROR;
            }
            if (dirty)
                editmovie((flags & F_SHOW_SELECTION) != 0);
            break;
        case P_SHOW_BORDER:
        case P_BORDER_WIDTH:
            if (MCControl::setprop(parid, p, ep, effective) != ES_NORMAL)
                return ES_ERROR;
            setrect(rect);
            dirty = True;
            break;
        case P_MOVIE_CONTROLLER_ID:
            break;
        case P_PLAY_LOUDNESS:
            if (!MCU_stoui2(data, loudness))
            {
                MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                return ES_ERROR;
            }
            loudness = MCU_max(0, loudness);
            loudness = MCU_min(loudness, 100);
            setloudness();
            break;
        case P_ENABLED_TRACKS:
            if (!setenabledtracks(data))
            {
                MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
                return ES_ERROR;
            }
            dirty = wholecard = True;
            break;
        case P_CURRENT_NODE:
		{
			uint2 nodeid;
			if (!MCU_stoui2(data,nodeid))
			{
				MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
				return ES_ERROR;
			}
			if (m_platform_player != nil)
				MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRNode, kMCPlatformPropertyTypeUInt16, &nodeid);
		}
            break;
        case P_PAN:
		{
			real8 pan;
			if (!MCU_stor8(data, pan))
			{
				MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
				return ES_ERROR;
			}
			if (m_platform_player != nil)
				MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRPan, kMCPlatformPropertyTypeDouble, &pan);
            
			if (isbuffering())
				dirty = True;
		}
            break;
        case P_TILT:
		{
			real8 tilt;
			if (!MCU_stor8(data, tilt))
			{
				MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
				return ES_ERROR;
			}
			if (m_platform_player != nil)
				MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRTilt, kMCPlatformPropertyTypeDouble, &tilt);
            
			if (isbuffering())
				dirty = True;
		}
            break;
        case P_ZOOM:
		{
			real8 zoom;
			if (!MCU_stor8(data, zoom))
			{
				MCeerror->add(EE_OBJECT_NAN, 0, 0, data);
				return ES_ERROR;
			}
			if (m_platform_player != nil)
				MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyQTVRZoom, kMCPlatformPropertyTypeDouble, &zoom);
            
			if (isbuffering())
				dirty = True;
		}
            break;
        case P_VISIBLE:
        case P_INVISIBLE:
		{
			uint4 oldflags = flags;
			Exec_stat stat = MCControl::setprop(parid, p, ep, effective);
			if (flags != oldflags && !(flags & F_VISIBLE))
				playstop();
			if (m_platform_player != nil)
			{
				bool t_visible;
				t_visible = getflag(F_VISIBLE);
				MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyVisible, kMCPlatformPropertyTypeBool, &t_visible);
			}
            
			return stat;
		}
            break;
#endif /* MCPlayer::setprop */
        default:
            return MCControl::setprop(parid, p, ep, effective);
	}
	if (dirty && opened && flags & F_VISIBLE)
	{
		// MW-2011-08-18: [[ Layers ]] Invalidate the whole object.
		layer_redrawall();
	}
	return ES_NORMAL;
}

// MW-2011-09-23: Make sure we sync the buffer state at this point, rather than
//   during drawing.
void MCPlayer::select(void)
{
	MCControl::select();
	syncbuffering(nil);
}

// MW-2011-09-23: Make sure we sync the buffer state at this point, rather than
//   during drawing.
void MCPlayer::deselect(void)
{
	MCControl::deselect();
	syncbuffering(nil);
}

MCControl *MCPlayer::clone(Boolean attach, Object_pos p, bool invisible)
{
	MCPlayer *newplayer = new MCPlayer(*this);
	if (attach)
		newplayer->attach(p, invisible);
	return newplayer;
}

IO_stat MCPlayer::extendedsave(MCObjectOutputStream& p_stream, uint4 p_part)
{
	return defaultextendedsave(p_stream, p_part);
}

IO_stat MCPlayer::extendedload(MCObjectInputStream& p_stream, const char *p_version, uint4 p_remaining)
{
	return defaultextendedload(p_stream, p_version, p_remaining);
}

IO_stat MCPlayer::save(IO_handle stream, uint4 p_part, bool p_force_ext)
{
	IO_stat stat;
	if (!disposable)
	{
		if ((stat = IO_write_uint1(OT_PLAYER, stream)) != IO_NORMAL)
			return stat;
		if ((stat = MCControl::save(stream, p_part, p_force_ext)) != IO_NORMAL)
			return stat;
		if ((stat = IO_write_string(filename, stream)) != IO_NORMAL)
			return stat;
		if ((stat = IO_write_uint4(starttime, stream)) != IO_NORMAL)
			return stat;
		if ((stat = IO_write_uint4(endtime, stream)) != IO_NORMAL)
			return stat;
		if ((stat = IO_write_int4((int4)(rate / 10.0 * MAXINT4),
		                          stream)) != IO_NORMAL)
			return stat;
		if ((stat = IO_write_string(userCallbackStr, stream)) != IO_NORMAL)
			return stat;
	}
	return savepropsets(stream);
}

IO_stat MCPlayer::load(IO_handle stream, const char *version)
{
	IO_stat stat;
    
	if ((stat = MCObject::load(stream, version)) != IO_NORMAL)
		return stat;
	if ((stat = IO_read_string(filename, stream)) != IO_NORMAL)
		return stat;
	if ((stat = IO_read_uint4(&starttime, stream)) != IO_NORMAL)
		return stat;
	if ((stat = IO_read_uint4(&endtime, stream)) != IO_NORMAL)
		return stat;
	int4 trate;
	if ((stat = IO_read_int4(&trate, stream)) != IO_NORMAL)
		return stat;
	rate = (real8)trate * 10.0 / MAXINT4;
	if ((stat = IO_read_string(userCallbackStr, stream)) != IO_NORMAL)
		return stat;
	return loadpropsets(stream);
}

// MW-2011-09-23: Ensures the buffering state is consistent with current flags
//   and state.
void MCPlayer::syncbuffering(MCContext *p_dc)
{
	bool t_should_buffer;
	
	// MW-2011-09-13: [[ Layers ]] If the layer is dynamic then the player must be buffered.
	t_should_buffer = getstate(CS_SELECTED) || getflag(F_ALWAYS_BUFFER) || getstack() -> getstate(CS_EFFECT) || (p_dc != nil && p_dc -> gettype() != CONTEXT_TYPE_SCREEN) || !MCModeMakeLocalWindows() || layer_issprite();
    
    // MW-2014-04-24: [[ Bug 12249 ]] If we are not in browse mode for this object, then it should be buffered.
    t_should_buffer = t_should_buffer || getstack() -> gettool(this) != T_BROWSE;
	
	if (m_platform_player != nil)
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOffscreen, kMCPlatformPropertyTypeBool, &t_should_buffer);
}

// MW-2007-08-14: [[ Bug 1949 ]] On Windows ensure we load and unload QT if not
//   currently in use.
void MCPlayer::getversion(MCExecPoint &ep)
{
    extern void MCQTGetVersion(MCExecPoint& ep);
    MCQTGetVersion(ep);
}

void MCPlayer::freetmp()
{
	if (istmpfile)
	{
		MCS_unlink(filename);
		delete filename;
		filename = NULL;
	}
}

uint4 MCPlayer::getduration() //get movie duration/length
{
	uint4 duration;
	if (m_platform_player != nil)
		MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &duration);
	else
		duration = 0;
	return duration;
}

uint4 MCPlayer::gettimescale() //get moive time scale
{
	uint4 timescale;
	if (m_platform_player != nil)
		MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyTimescale, kMCPlatformPropertyTypeUInt32, &timescale);
	else
		timescale = 0;
	return timescale;
}

uint4 MCPlayer::getmoviecurtime()
{
	uint4 curtime;
	if (m_platform_player != nil)
		MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &curtime);
	else
		curtime = 0;
	return curtime;
}

void MCPlayer::setcurtime(uint4 newtime)
{
	lasttime = newtime;
	if (m_platform_player != nil)
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &newtime);
}
/*
 void MCPlayer::setselection()
 {
 if (m_platform_player != nil)
 {
 uint4 st, et;
 if (starttime == MAXUINT4 || endtime == MAXUINT4)
 st = et = 0;
 else
 {
 st = starttime;
 et = endtime;
 }
 MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &starttime);
 MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &endtime);
 }
 }
 */
void MCPlayer::setselection()
{
    if (m_platform_player != nil)
	{
		if (starttime == MAXUINT4 && endtime == MAXUINT4)
        {
            starttime = 0;
            endtime = getduration();
        }
        MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &starttime);
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &endtime);
	}
}

void MCPlayer::setlooping(Boolean loop)
{
	if (m_platform_player != nil)
	{
		bool t_loop;
		t_loop = loop;
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyLoop, kMCPlatformPropertyTypeBool, &t_loop);
	}
}

void MCPlayer::setplayrate()
{
	if (m_platform_player != nil)
	{
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyPlayRate, kMCPlatformPropertyTypeDouble, &rate);
		if (rate != 0.0f)
			MCPlatformStartPlayer(m_platform_player);
	}
    
	if (rate != 0)
		state = state & ~CS_PAUSED;
	else
		state = state | CS_PAUSED;
}

void MCPlayer::showbadge(Boolean show)
{
#if 0
	if (m_platform_player != nil)
	{
		bool t_show;
		t_show = show;
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyShowBadge, kMCPlatformPropertyTypeBool, &t_show);
	}
#endif
}

void MCPlayer::editmovie(Boolean edit)
{
#if 0
	if (m_platform_player != nil)
	{
		bool t_edit;
		t_edit = edit;
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyShowSelection, kMCPlatformPropertyTypeBool, &t_edit);
	}
#endif
}

void MCPlayer::playselection(Boolean play)
{
	if (m_platform_player != nil)
	{
		bool t_play;
		t_play = play;
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOnlyPlaySelection, kMCPlatformPropertyTypeBool, &t_play);
	}
}

Boolean MCPlayer::ispaused()
{
	if (m_platform_player != nil)
		return !MCPlatformPlayerIsPlaying(m_platform_player);
}

void MCPlayer::showcontroller(Boolean show)
{
#if 0
	if (m_platform_player != nil)
	{
		bool t_show;
		t_show = show;
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyShowController, kMCPlatformPropertyTypeBool, &t_show);
	}
#endif
    
    // The showController property has changed, this means we must do two things - resize
    // the movie rect and then redraw ourselves to make sure we can see the controller.
    
    setrect(rect);
    layer_redrawall();
}

Boolean MCPlayer::prepare(const char *options)
{
	Boolean ok = False;
    
	if (state & CS_PREPARED || filename == NULL)
		return True;
	
	if (!opened)
		return False;
    
    extern bool MCQTInit(void);
    if (!MCQTInit())
        return False;
    
	if (m_platform_player == nil)
		MCPlatformCreatePlayer(m_platform_player);
    
	if (strnequal(filename, "https:", 6) || strnequal(filename, "http:", 5) || strnequal(filename, "ftp:", 4) || strnequal(filename, "file:", 5) || strnequal(filename, "rtsp:", 5))
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyURL, kMCPlatformPropertyTypeNativeCString, &filename);
	else
		MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFilename, kMCPlatformPropertyTypeNativeCString, &filename);
	
	MCRectangle t_movie_rect;
	MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyMovieRect, kMCPlatformPropertyTypeRectangle, &t_movie_rect);
	
	MCRectangle trect = resize(t_movie_rect);
    
    // Adjust so that the controller isn't included in the movie rect.
    if (getflag(F_SHOW_CONTROLLER))
        trect . height -= CONTROLLER_HEIGHT;
	
	// IM-2011-11-12: [[ Bug 11320 ]] Transform player rect to device coords
    // MW-2014-04-09: [[ Bug 11922 ]] Make sure we use the view not device transform
    //   (backscale factor handled in platform layer).
	trect = MCRectangleGetTransformedBounds(trect, getstack()->getviewtransform());
	
	MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyRect, kMCPlatformPropertyTypeRectangle, &trect);
	
	bool t_looping, t_play_selection;
	
	t_looping = getflag(F_LOOPING);
	t_play_selection = getflag(F_PLAY_SELECTION);
	
	MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &lasttime);
    MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyLoop, kMCPlatformPropertyTypeBool, &t_looping);
    
	setselection();
	MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOnlyPlaySelection, kMCPlatformPropertyTypeBool, &t_play_selection);
	SynchronizeUserCallbacks();
	
	bool t_offscreen;
	t_offscreen = getflag(F_ALWAYS_BUFFER);
	MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOffscreen, kMCPlatformPropertyTypeBool, &t_offscreen);
	
	bool t_visible;
	t_visible = getflag(F_VISIBLE);
	MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyVisible, kMCPlatformPropertyTypeBool, &t_visible);
	
	MCPlatformAttachPlayer(m_platform_player, getstack() -> getwindow());
	
	layer_redrawall();
	
	setloudness();
	
	MCresult -> clear(False);
	
	ok = True;
	
	if (ok)
	{
		state |= CS_PREPARED | CS_PAUSED;
		{
			nextplayer = MCplayers;
			MCplayers = this;
		}
	}
    
	return ok;
}

Boolean MCPlayer::playstart(const char *options)
{
	if (!prepare(options))
		return False;
	playpause(False);
	return True;
}

Boolean MCPlayer::playpause(Boolean on)
{
	if (!(state & CS_PREPARED))
		return False;
	
	Boolean ok;
	ok = False;
	
	if (m_platform_player != nil)
	{
		if (!on)
			MCPlatformStartPlayer(m_platform_player);
		else
			MCPlatformStopPlayer(m_platform_player);
		ok = True;
	}
	
	if (ok)
		setstate(on, CS_PAUSED);
    
	return ok;
}

void MCPlayer::playstepforward()
{
	if (!getstate(CS_PREPARED))
		return;
    
	if (m_platform_player != nil)
		MCPlatformStepPlayer(m_platform_player, 1);
}

void MCPlayer::playfast(Boolean forward)
{
	if (!getstate(CS_PREPARED))
		return;
    
	if (m_platform_player != nil)
		MCPlatformFastPlayer(m_platform_player, forward);
}

/*
 void MCPlayer::playfastforward()
 {
 if (!getstate(CS_PREPARED))
 return;
 
 if (m_platform_player != nil)
 MCPlatformFastForwardPlayer(m_platform_player);
 }
 
 void MCPlayer::playfastback()
 {
 if (!getstate(CS_PREPARED))
 return;
 
 if (m_platform_player != nil)
 MCPlatformFastBackPlayer(m_platform_player);
 }
 */

void MCPlayer::playstepback()
{
	if (!getstate(CS_PREPARED))
		return;
	
	if (m_platform_player != nil)
		MCPlatformStepPlayer(m_platform_player, -1);
}

Boolean MCPlayer::playstop()
{
	formattedwidth = formattedheight = 0;
	if (!getstate(CS_PREPARED))
		return False;
    
	Boolean needmessage = True;
	
	state &= ~(CS_PREPARED | CS_PAUSED);
	lasttime = 0;
	
	if (m_platform_player != nil)
	{
		MCPlatformStopPlayer(m_platform_player);
		
		needmessage = getduration() > getmoviecurtime();
		
		MCPlatformDetachPlayer(m_platform_player);
	}
    
	freetmp();
    
	if (MCplayers != NULL)
	{
		if (MCplayers == this)
			MCplayers = nextplayer;
		else
		{
			MCPlayer *tptr = MCplayers;
			while (tptr->nextplayer != NULL && tptr->nextplayer != this)
				tptr = tptr->nextplayer;
			if (tptr->nextplayer == this)
                tptr->nextplayer = nextplayer;
		}
	}
	nextplayer = NULL;
    
	if (disposable)
	{
		if (needmessage)
			getcard()->message_with_args(MCM_play_stopped, getname());
		delete this;
	}
	else
		if (needmessage)
			message_with_args(MCM_play_stopped, getname());
    
	return True;
}


void MCPlayer::setfilename(const char *vcname,
                           char *fname, Boolean istmp)
{
	setname_cstring(vcname);
	filename = fname;
	istmpfile = istmp;
	disposable = True;
}

void MCPlayer::setvolume(uint2 tloudness)
{
}

MCRectangle MCPlayer::getpreferredrect()
{
	if (!getstate(CS_PREPARED))
	{
		MCRectangle t_bounds;
		MCU_set_rect(t_bounds, 0, 0, formattedwidth, formattedheight);
		return t_bounds;
	}
    
    MCRectangle t_bounds;
	MCU_set_rect(t_bounds, 0, 0, 0, 0);
	if (m_platform_player != nil)
    {
		MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyMovieRect, kMCPlatformPropertyTypeRectangle, &t_bounds);
        // PM-2014-04-28: [[Bug 12299]] Make sure the correct MCRectangle is returned
        return t_bounds;
    }
}

uint2 MCPlayer::getloudness()
{
	if (getstate(CS_PREPARED))
		if (m_platform_player != nil)
			MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyVolume, kMCPlatformPropertyTypeUInt16, &loudness);
	return loudness;
}

void MCPlayer::setloudness()
{
	if (state & CS_PREPARED)
		if (m_platform_player != nil)
			MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyVolume, kMCPlatformPropertyTypeUInt16, &loudness);
}

void MCPlayer::gettracks(MCExecPoint &ep)
{
	ep . clear();
    
	if (getstate(CS_PREPARED))
		if (m_platform_player != nil)
		{
			uindex_t t_track_count;
			MCPlatformCountPlayerTracks(m_platform_player, t_track_count);
			for(uindex_t i = 0; i < t_track_count; i++)
			{
				uint32_t t_id;
				MCAutoPointer<char> t_name;
				uint32_t t_offset, t_duration;
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyId, kMCPlatformPropertyTypeUInt32, &t_id);
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyMediaTypeName, kMCPlatformPropertyTypeNativeCString, &(&t_name));
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyOffset, kMCPlatformPropertyTypeUInt32, &t_offset);
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_offset);
				ep . concatuint(t_id, EC_RETURN, i == 1);
				ep . concatcstring(*t_name, EC_COMMA, false);
				ep . concatuint(t_offset, EC_COMMA, false);
				ep . concatuint(t_duration, EC_COMMA, false);
			}
		}
}

void MCPlayer::getenabledtracks(MCExecPoint &ep)
{
	ep.clear();
    
	if (getstate(CS_PREPARED))
		if (m_platform_player != nil)
		{
			uindex_t t_track_count;
			MCPlatformCountPlayerTracks(m_platform_player, t_track_count);
			for(uindex_t i = 0; i < t_track_count; i++)
			{
				uint32_t t_id;
				uint32_t t_enabled;
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyId, kMCPlatformPropertyTypeUInt32, &t_id);
				MCPlatformGetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyEnabled, kMCPlatformPropertyTypeBool, &t_enabled);
				if (t_enabled)
					ep . concatuint(t_id, EC_RETURN, i == 1);
			}
		}
}

Boolean MCPlayer::setenabledtracks(const MCString &s)
{
	if (getstate(CS_PREPARED))
		if (m_platform_player != nil)
		{
			uindex_t t_track_count;
			MCPlatformCountPlayerTracks(m_platform_player, t_track_count);
			for(uindex_t i = 0; i < t_track_count; i++)
			{
				bool t_enabled;
				t_enabled = false;
				MCPlatformSetPlayerTrackProperty(m_platform_player, i, kMCPlatformPlayerTrackPropertyEnabled, kMCPlatformPropertyTypeBool, &t_enabled);
			}
			char *data = s.clone();
			char *sptr = data;
			while (*sptr)
			{
				char *tptr;
				if ((tptr = strchr(sptr, '\n')) != NULL)
					*tptr++ = '\0';
				else
					tptr = &sptr[strlen(sptr)];
				if (strlen(sptr) != 0)
				{
					uindex_t t_index;
					if (!MCPlatformFindPlayerTrackWithId(m_platform_player, strtol(sptr, NULL, 10), t_index))
					{
						delete data;
						return False;
					}
					
					bool t_enabled;
					t_enabled = true;
					MCPlatformSetPlayerTrackProperty(m_platform_player, t_index, kMCPlatformPlayerTrackPropertyEnabled, kMCPlatformPropertyTypeBool, &t_enabled);
				}
				sptr = tptr;
			}
			delete data;
			MCRectangle t_movie_rect;
			MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyMovieRect, kMCPlatformPropertyTypeRectangle, &t_movie_rect);
			MCRectangle trect = resize(t_movie_rect);
			if (flags & F_SHOW_BORDER)
				trect = MCU_reduce_rect(trect, -borderwidth);
			setrect(trect);
		}
    
	return True;
}

void MCPlayer::getnodes(MCExecPoint &ep)
{
	ep.clear();
	// COCOA-TODO: MCPlayer::getnodes();
}

void MCPlayer::gethotspots(MCExecPoint &ep)
{
	ep.clear();
	// COCOA-TODO: MCPlayer::gethotspots();
}


MCRectangle MCPlayer::resize(MCRectangle movieRect)
{
	int2 x, y;
	MCRectangle trect = rect;
	
	// MW-2011-10-24: [[ Bug 9800 ]] Store the current rect for layer notification.
	MCRectangle t_old_rect;
	t_old_rect = rect;
	
	// MW-2011-10-01: [[ Bug 9762 ]] These got inverted sometime.
	formattedheight = movieRect.height;
	formattedwidth = movieRect.width;
	
	if (!(flags & F_LOCK_LOCATION))
	{
		if (formattedheight == 0)
		{ // audio clip
			trect.height = CONTROLLER_HEIGHT;
			rect = trect;
		}
		else
		{
			x = trect.x + (trect.width >> 1);
			y = trect.y + (trect.height >> 1);
			trect.width = (uint2)(formattedwidth * scale);
			trect.height = (uint2)(formattedheight * scale);
			if (flags & F_SHOW_CONTROLLER)
				trect.height += CONTROLLER_HEIGHT;
			trect.x = x - (trect.width >> 1);
			trect.y = y - (trect.height >> 1);
			if (flags & F_SHOW_BORDER)
				rect = MCU_reduce_rect(trect, -borderwidth);
			else
				rect = trect;
		}
	}
	else
		if (flags & F_SHOW_BORDER)
			trect = MCU_reduce_rect(trect, borderwidth);
	
	// MW-2011-10-24: [[ Bug 9800 ]] If the rect has changed, notify the layer.
	if (!MCU_equal_rect(rect, t_old_rect))
		layer_rectchanged(t_old_rect, true);
	
	return trect;
}

void MCPlayer::markerchanged(uint32_t p_time)
{
    // Search for the first marker with the given time, and dispatch the message.
    for(uindex_t i = 0; i < m_callback_count; i++)
        if (p_time == m_callbacks[i] . time)
            message_with_args(m_callbacks[i] . message, m_callbacks[i] . parameter);
}

void MCPlayer::selectionchanged(void)
{
    MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &starttime);
    MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &endtime);
    
    redrawcontroller();
    timer(MCM_selection_changed, nil);
}

void MCPlayer::currenttimechanged(void)
{
    redrawcontroller();
    
    timer(MCM_current_time_changed, nil);
}

void MCPlayer::SynchronizeUserCallbacks(void)
{
    if (userCallbackStr == nil)
        return;
    
    if (m_platform_player == nil)
        return;
    
    // Free the existing callback table.
    for(uindex_t i = 0; i < m_callback_count; i++)
    {
        MCNameDelete(m_callbacks[i] . message);
        MCNameDelete(m_callbacks[i] . parameter);
    }
    MCMemoryDeleteArray(m_callbacks);
    m_callbacks = nil;
    m_callback_count = 0;
    
    // Now reparse the callback string and build the table.
    char *cblist = strclone(userCallbackStr);
	char *str;
	str = cblist;
	while (*str)
	{
		char *ptr, *data1, *data2;
		if ((data1 = strchr(str, ',')) == NULL)
		{
            //search ',' as separator
			delete cblist;
			return;
		}
		*data1 = '\0';
		data1 ++;
		if ((data2 = strchr(data1, '\n')) != NULL)// more than one callback
			*data2++ = '\0';
		else
			data2 = data1 + strlen(data1);
        
        /* UNCHECKED */ MCMemoryResizeArray(m_callback_count + 1, m_callbacks, m_callback_count);
        m_callbacks[m_callback_count - 1] . time = strtol(str, NULL, 10);
        
        while (isspace(*data1))//strip off preceding and trailing blanks
            data1++;
        ptr = data1;
        while (*ptr)
        {
            if (isspace(*ptr))
            {
                *ptr++ = '\0';
                /* UNCHECKED */ MCNameCreateWithCString(ptr, m_callbacks[m_callback_count - 1] . parameter);
                break;
            }
            ptr++;
        }
        
        /* UNCHECKED */ MCNameCreateWithCString(data1, m_callbacks[m_callback_count - 1] . message);
        
        // If no parameter is specified, use the time.
        if (m_callbacks[m_callback_count - 1] . parameter == nil)
        /* UNCHECKED */ MCNameCreateWithCString(str, m_callbacks[m_callback_count - 1] . parameter);
		
        str = data2;
	}
	delete cblist;
    
    // Now set the markers in the player so that we get notified.
    array_t<uint32_t> t_markers;
    /* UNCHECKED */ MCMemoryNewArray(m_callback_count, t_markers . ptr);
    for(uindex_t i = 0; i < m_callback_count; i++)
        t_markers . ptr[i] = m_callbacks[i] . time;
    t_markers . count = m_callback_count;
    MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyMarkers, kMCPlatformPropertyTypeUInt32Array, &t_markers);
    MCMemoryDeleteArray(t_markers . ptr);
    
	return True;
}

Boolean MCPlayer::isbuffering(void)
{
	if (m_platform_player == nil)
		return false;
	
	bool t_buffering;
	MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOffscreen, kMCPlatformPropertyTypeBool, &t_buffering);
	
	return t_buffering;
}


//-----------------------------------------------------------------------------
//  Redraw Management

// MW-2011-09-06: [[ Redraw ]] Added 'sprite' option - if true, ink and opacity are not set.
void MCPlayer::draw(MCDC *dc, const MCRectangle& p_dirty, bool p_isolated, bool p_sprite)
{
	MCRectangle dirty;
	dirty = p_dirty;
    
	if (!p_isolated)
	{
		// MW-2011-09-06: [[ Redraw ]] If rendering as a sprite, don't change opacity or ink.
		if (!p_sprite)
		{
			dc -> setopacity(blendlevel * 255 / 100);
			dc -> setfunction(ink);
		}
        
		// MW-2009-06-11: [[ Bitmap Effects ]]
		if (m_bitmap_effects == NULL)
			dc -> begin(false);
		else
		{
			if (!dc -> begin_with_effects(m_bitmap_effects, rect))
				return;
			dirty = dc -> getclip();
		}
	}
    
	if (MClook == LF_MOTIF && state & CS_KFOCUSED && !(extraflags & EF_NO_FOCUS_BORDER))
		drawfocus(dc, p_dirty);
	
	if (m_platform_player != nil)
	{
		syncbuffering(dc);
		
		bool t_offscreen;
		MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyOffscreen, kMCPlatformPropertyTypeBool, &t_offscreen);
		
		if (t_offscreen)
		{
			MCRectangle trect = MCU_reduce_rect(rect, flags & F_SHOW_BORDER ? borderwidth : 0);
			
			MCImageDescriptor t_image;
			MCMemoryClear(&t_image, sizeof(t_image));
			t_image.filter = kMCGImageFilterNone;
			MCPlatformLockPlayerBitmap(m_platform_player, t_image . bitmap);
			if (t_image . bitmap != nil)
				dc -> drawimage(t_image, 0, 0, trect.width, trect.height, trect.x, trect.y);
			MCPlatformUnlockPlayerBitmap(m_platform_player, t_image . bitmap);
		}
	}
    
    // Draw our controller
    if (getflag(F_SHOW_CONTROLLER))
    {
        // drawcontroller(dc);
        drawnicecontroller(dc);
    }
    
	if (getflag(F_SHOW_BORDER))
		if (getflag(F_3D))
			draw3d(dc, rect, ETCH_SUNKEN, borderwidth);
		else
			drawborder(dc, rect, borderwidth);
	
	if (!p_isolated)
	{
		if (getstate(CS_SELECTED))
			drawselected(dc);
	}
    
	if (!p_isolated)
		dc -> end();
}

inline MCGColor MCGColorMakeRGBA(MCGFloat p_red, MCGFloat p_green, MCGFloat p_blue, MCGFloat p_alpha)
{
	return ((uint8_t)(p_red * 255) << 16) | ((uint8_t)(p_green * 255) << 8) | ((uint8_t)(p_blue * 255) << 0) | ((uint8_t)(p_alpha * 255) << 24);
}

inline void setRamp(MCGColor *&r_colors, MCGFloat *&r_stops)
{
    if (r_colors == nil)
    /* UNCHECKED */ MCMemoryNewArray(3, r_colors);
    r_colors[0] = MCGColorMakeRGBA(183 / 255.0, 183 / 255.0, 183 / 255.0, 1.0f);
    r_colors[1] = MCGColorMakeRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    r_colors[2] = MCGColorMakeRGBA(183 / 255.0, 183 / 255.0, 183 / 255.0, 1.0f);
    
    if (r_stops == nil)
    /* UNCHECKED */ MCMemoryNewArray(3, r_stops);
    r_stops[0] = (MCGFloat)0.00000;
    r_stops[1] = (MCGFloat)0.50000;
    r_stops[2] = (MCGFloat)1.00000;
}

inline void setTransform(MCGAffineTransform &r_transform, MCGFloat origin_x, MCGFloat origin_y, MCGFloat primary_x, MCGFloat primary_y, MCGFloat secondary_x, MCGFloat secondary_y)
{
    MCGAffineTransform t_transform;
    t_transform . a = primary_x - origin_x;
    t_transform . b = primary_y - origin_y;
    t_transform . c = secondary_x - origin_x;
    t_transform . d = secondary_y - origin_y;
    
    t_transform . tx = origin_x;
    t_transform . ty = origin_y;
    r_transform = t_transform;
}

inline MCGPoint MCRectangleScalePoints(MCRectangle p_rect, MCGFloat p_x, MCGFloat p_y)
{
    return MCGPointMake(p_rect . x + p_x * p_rect . width, p_rect . y + p_y * p_rect . height);
}

inline void MCGraphicsContextAngleAndDistanceToXYOffset(int p_angle, int p_distance, MCGFloat &r_x_offset, MCGFloat &r_y_offset)
{
	r_x_offset = floor(0.5f + p_distance * cos(p_angle * M_PI / 180.0));
	r_y_offset = floor(0.5f + p_distance * sin(p_angle * M_PI / 180.0));
}

void MCPlayer::drawcontroller(MCDC *dc)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolume));
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartPlay));
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartWell));
    
    
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartThumb));
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubBack));
    drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubForward));
    
    if (getflag(F_SHOW_SELECTION))
    {
        drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionStart));
        drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionFinish));
    }
    
    if (m_show_volume)
    {
        drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeBar));
        drawcontrollerbutton(dc, getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeSelector));
    }
    
}

void MCPlayer::drawnicecontroller(MCDC *dc)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    
    dc -> setforeground(controllercolors[DARKGRAY]);
    dc -> fillrect(t_rect, true);
    
    MCGContextRef t_gcontext = nil;
    dc -> lockgcontext(t_gcontext);
    
    drawControllerVolumeButton(t_gcontext);
    if (m_show_volume)
    {
        drawControllerVolumeBarButton(t_gcontext);
        drawControllerVolumeWellButton(t_gcontext);
        drawControllerVolumeAreaButton(t_gcontext);
        drawControllerVolumeSelectorButton(t_gcontext);
    }
    drawControllerPlayPauseButton(t_gcontext);
    drawControllerWellButton(t_gcontext);
    
    
    if (getflag(F_SHOW_SELECTION))
    {
        drawControllerSelectedAreaButton(t_gcontext);
    }
    
    drawControllerScrubForwardButton(t_gcontext);
    drawControllerScrubBackButton(t_gcontext);
    
    drawControllerPlayedAreaButton(t_gcontext);
    drawControllerThumbButton(t_gcontext);
    
    if (getflag(F_SHOW_SELECTION))
    {
        drawControllerSelectionStartButton(t_gcontext);
        drawControllerSelectionFinishButton(t_gcontext);
    }
    dc -> unlockgcontext(t_gcontext);
}

void MCPlayer::drawControllerVolumeButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_volume_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolume);
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    float origin_x = t_volume_rect.x + t_volume_rect.width / 2.0;
	float origin_y = t_volume_rect.y + t_volume_rect.height * 2 / 3;
	float primary_x = t_volume_rect.x + t_volume_rect.width / 2.0;
	float primary_y = t_volume_rect.y + t_volume_rect.height / 3;
	float secondary_x = t_volume_rect.x;
	float secondary_y = t_volume_rect.y + t_volume_rect.height * 2 / 3;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
    
    
    if (m_show_volume)
    {
        MCGContextAddRectangle(p_gcontext, MCRectangleToMCGRectangle(t_volume_rect));
        MCGContextSetFillRGBAColor(p_gcontext, 168 / 255.0, 1 / 255.0, 255 / 255.0, 1.0f); // PURPLE
        MCGContextFill(p_gcontext);
    }
    
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    MCGContextBeginPath(p_gcontext);
    MCGContextMoveTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.2 , 0.4));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.2 , 0.6));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.4 , 0.6));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.5 , 0.7));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.5 , 0.3));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.4 , 0.4));
    MCGContextCloseSubpath(p_gcontext);
    MCGContextFill(p_gcontext);
    
    MCGContextSetStrokeGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    if (getloudness() > 30)
    {
        MCGContextAddArc(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.6 , 0.5), MCGSizeMake(0.05 * t_volume_rect . width, 0.2 * t_volume_rect . height), 0.0, -60, 60);
    }
    
    if (getloudness() > 60)
    {
        MCGContextAddArc(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.66 , 0.5), MCGSizeMake(0.1 * t_volume_rect . width, 0.4 * t_volume_rect . height), 0.0, -60, 60);
    }
    
    if (getloudness() > 95)
    {
        MCGContextAddArc(p_gcontext, MCRectangleScalePoints(t_volume_rect, 0.72 , 0.5), MCGSizeMake(0.2 * t_volume_rect . width, 0.8 * t_volume_rect . height), 0.0, -60, 60);
    }
    
    MCGContextSetStrokeWidth(p_gcontext, t_volume_rect . width / 20.0 );
    MCGContextStroke(p_gcontext);
}

void MCPlayer::drawControllerVolumeBarButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_volume_bar_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeBar);
    
    MCGContextAddRectangle(p_gcontext, MCRectangleToMCGRectangle(t_volume_bar_rect));
    MCGContextSetFillRGBAColor(p_gcontext, 34 / 255.0, 34 / 255.0, 34 / 255.0, 1.0f); // DARKGRAY
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerVolumeSelectorButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_volume_selector_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeSelector);
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    float origin_x = t_volume_selector_rect.x + t_volume_selector_rect.width / 2.0;
	float origin_y = t_volume_selector_rect.y + t_volume_selector_rect.height;
	float primary_x = t_volume_selector_rect.x + t_volume_selector_rect.width / 2.0;
	float primary_y = t_volume_selector_rect.y;
	float secondary_x = t_volume_selector_rect.x - t_volume_selector_rect.width / 2.0;
	float secondary_y = t_volume_selector_rect.y + t_volume_selector_rect.height;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
    
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextSetShouldAntialias(p_gcontext, true);
    MCGContextAddArc(p_gcontext, MCRectangleScalePoints(t_volume_selector_rect, 0.5, 0.5), MCGSizeMake(0.8 * t_volume_selector_rect . width, 0.8 * t_volume_selector_rect . height), 0, 0, 360);
    
    MCGContextFill(p_gcontext);
    
    MCMemoryDeleteArray(t_stops);
    MCMemoryDeleteArray(t_colors);
}

void MCPlayer::drawControllerPlayPauseButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_playpause_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartPlay);
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    float origin_x = t_playpause_rect.x + t_playpause_rect.width / 2.0;
	float origin_y = t_playpause_rect.y + t_playpause_rect.height * 2 / 3;
	float primary_x = t_playpause_rect.x + t_playpause_rect.width / 2.0;
	float primary_y = t_playpause_rect.y + t_playpause_rect.height / 3;
	float secondary_x = t_playpause_rect.x;
	float secondary_y = t_playpause_rect.y + t_playpause_rect.height * 2 / 3;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
    
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    if (ispaused())
    {
        MCGContextMoveTo(p_gcontext, MCRectangleScalePoints(t_playpause_rect, 0.3, 0.3));
        MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_playpause_rect, 0.3, 0.7));
        MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_playpause_rect, 0.7, 0.5));
        MCGContextCloseSubpath(p_gcontext);
    }
    
    else
    {
        MCGRectangle t_grect1, t_grect2;
        
        t_grect1 = MCGRectangleMake(t_playpause_rect . x + 0.2 * t_playpause_rect . width, t_playpause_rect . y + 0.3 * t_playpause_rect . height, 0.2 * t_playpause_rect . width, 0.4 * t_playpause_rect . height);
        MCGContextAddRectangle(p_gcontext, t_grect1);
        
        t_grect2 = MCGRectangleMake(t_playpause_rect . x + 0.5 * t_playpause_rect . width, t_playpause_rect . y + 0.3 * t_playpause_rect . height, 0.2 * t_playpause_rect . width, 0.4 * t_playpause_rect . height);
        MCGContextAddRectangle(p_gcontext, t_grect2);
        
    }
    
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerWellButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_well_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartWell);
    
    MCGBitmapEffects t_effects;
	t_effects . has_drop_shadow = false;
	t_effects . has_outer_glow = false;
	t_effects . has_inner_glow = false;
	t_effects . has_inner_shadow = true;
    t_effects . has_color_overlay = false;
    
    MCGShadowEffect t_inner_shadow;
    t_inner_shadow . color = MCGColorMakeRGBA(56.0 / 255.0, 56.0 / 255.0, 56.0 / 255.0, 56.0 / 255.0);
    t_inner_shadow . blend_mode = kMCGBlendModeClear;
    t_inner_shadow . size = 0;
    t_inner_shadow . spread = 0;
    
    MCGFloat t_x_offset, t_y_offset;
    int t_distance = t_well_rect . height / 5;
    // Make sure we always have an inner shadow
    if (t_distance == 0)
        t_distance = 1;
    
    MCGraphicsContextAngleAndDistanceToXYOffset(270, t_distance, t_x_offset, t_y_offset);
    t_inner_shadow . x_offset = t_x_offset;
    t_inner_shadow . y_offset = t_y_offset;
    t_inner_shadow . knockout = false;
    
    t_effects . inner_shadow = t_inner_shadow;
  
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    MCGContextSetFillRGBAColor(p_gcontext, 0.0f, 0.0f, 0.0f, 1.0f); // BLACK
    MCGRectangle t_rounded_rect = MCRectangleToMCGRectangle(t_well_rect);
    
    MCGContextAddRoundedRectangle(p_gcontext, t_rounded_rect, MCGSizeMake(30, 30));
    
    MCGContextBeginWithEffects(p_gcontext, t_rounded_rect, t_effects);
    
    MCGContextFill(p_gcontext);
    MCGContextEnd(p_gcontext);
}

void MCPlayer::drawControllerThumbButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_thumb_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartThumb);
    
    /*
     t_colors[0] = 4290230199;
     t_colors[1] = 4294967295;
     t_colors[2] = 4290230199;
     */
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    
    float origin_x = t_thumb_rect.x + t_thumb_rect.width / 2.0;
	float origin_y = t_thumb_rect.y + t_thumb_rect.height;
	float primary_x = t_thumb_rect.x + t_thumb_rect.width / 2.0;
	float primary_y = t_thumb_rect.y;
	float secondary_x = t_thumb_rect.x - t_thumb_rect.width / 2.0;
	float secondary_y = t_thumb_rect.y + t_thumb_rect.height;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
    
    ////////////////////////////////////////////////////////
    
    ///////////////////////////////////////////////////////
    
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextSetShouldAntialias(p_gcontext, true);
    MCGRectangle t_grect= MCRectangleToMCGRectangle(t_thumb_rect);
    MCGContextAddRoundedRectangle(p_gcontext, t_grect, MCGSizeMake(10, 10));
        
    MCGContextFill(p_gcontext);
    
    MCMemoryDeleteArray(t_stops);
    MCMemoryDeleteArray(t_colors);
}

void MCPlayer::drawControllerSelectionStartButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_selection_start_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionStart);
    
    MCGBitmapEffects t_effects;
	t_effects . has_drop_shadow = false;
	t_effects . has_outer_glow = true;
	t_effects . has_inner_glow = false;
	t_effects . has_inner_shadow = false;
    t_effects . has_color_overlay = false;
    
    MCGGlowEffect t_outer_glow;
    t_outer_glow . color = MCGColorMakeRGBA(0.0f, 0.0f, 0.0f, 1.0f);
    
    t_outer_glow . blend_mode = kMCGBlendModeClear;
    t_outer_glow . size = t_selection_start_rect . width / 2;
    t_outer_glow . spread = 0;
    //t_outer_glow . inverted = false;
    
    t_effects . outer_glow = t_outer_glow;
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    
    float origin_x = t_selection_start_rect.x + t_selection_start_rect . width / 2.0;
	float origin_y = t_selection_start_rect.y;
	float primary_x = t_selection_start_rect.x + t_selection_start_rect.width / 2.0;
	float primary_y = t_selection_start_rect.y + t_selection_start_rect . height;
	float secondary_x = t_selection_start_rect.x - 2 * t_selection_start_rect.width;
	float secondary_y = t_selection_start_rect.y;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
        
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextSetShouldAntialias(p_gcontext, true);
    MCGRectangle t_grect= MCRectangleToMCGRectangle(t_selection_start_rect);
    MCGContextAddRoundedRectangle(p_gcontext, t_grect, MCGSizeMake(10, 10));
    MCGContextBeginWithEffects(p_gcontext, t_grect, t_effects);
    
    MCGContextFill(p_gcontext);
    MCGContextEnd(p_gcontext);
    MCMemoryDeleteArray(t_stops);
    MCMemoryDeleteArray(t_colors);
}

void MCPlayer::drawControllerSelectionFinishButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_selection_finish_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionFinish);
    
    
    MCGBitmapEffects t_effects;
	t_effects . has_drop_shadow = false;
	t_effects . has_outer_glow = true;
	t_effects . has_inner_glow = false;
	t_effects . has_inner_shadow = false;
    t_effects . has_color_overlay = false;
    
    MCGGlowEffect t_outer_glow;
    t_outer_glow . color = MCGColorMakeRGBA(0.0f, 0.0f, 0.0f, 1.0f);
    
    t_outer_glow . blend_mode = kMCGBlendModeClear;
    t_outer_glow . size = t_selection_finish_rect . width / 2;
    t_outer_glow . spread = 0;
    //t_outer_glow . inverted = false;
    
    t_effects . outer_glow = t_outer_glow;
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    
    float origin_x = t_selection_finish_rect.x + t_selection_finish_rect . width / 2.0;
	float origin_y = t_selection_finish_rect.y;
	float primary_x = t_selection_finish_rect.x + t_selection_finish_rect.width / 2.0;
	float primary_y = t_selection_finish_rect.y + t_selection_finish_rect . height;
	float secondary_x = t_selection_finish_rect.x - 2 * t_selection_finish_rect.width;
	float secondary_y = t_selection_finish_rect.y;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
        
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    MCGRectangle t_grect= MCRectangleToMCGRectangle(t_selection_finish_rect);
    MCGContextAddRoundedRectangle(p_gcontext, t_grect, MCGSizeMake(10, 10));
    MCGContextBeginWithEffects(p_gcontext, t_grect, t_effects);
    MCGContextFill(p_gcontext);
    MCGContextEnd(p_gcontext);
}

void MCPlayer::drawControllerScrubForwardButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_scrub_forward_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubForward);
    
    if (m_scrub_forward_is_pressed)
    {
        MCGContextAddRectangle(p_gcontext, MCRectangleToMCGRectangle(t_scrub_forward_rect));
        MCGContextSetFillRGBAColor(p_gcontext, 168 / 255.0, 1 / 255.0, 255 / 255.0, 1.0f); //PURPLE
        MCGContextFill(p_gcontext);
    }
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    
    float origin_x = t_scrub_forward_rect.x + t_scrub_forward_rect.width / 2.0;
	float origin_y = t_scrub_forward_rect.y + t_scrub_forward_rect.height * 2 / 3;
	float primary_x = t_scrub_forward_rect.x + t_scrub_forward_rect.width / 2.0;
	float primary_y = t_scrub_forward_rect.y + t_scrub_forward_rect . height / 3;
	float secondary_x = t_scrub_forward_rect.x;
	float secondary_y = t_scrub_forward_rect.y + t_scrub_forward_rect.height * 2 / 3;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
        
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    MCGRectangle t_grect;
    t_grect = MCGRectangleMake(t_scrub_forward_rect . x + 0.2 * t_scrub_forward_rect . width, t_scrub_forward_rect . y + 0.3 * t_scrub_forward_rect . height, 0.2 * t_scrub_forward_rect . width, 0.4 * t_scrub_forward_rect . height);
    MCGContextBeginPath(p_gcontext);
    MCGContextAddRectangle(p_gcontext, t_grect);
    
    MCGContextMoveTo(p_gcontext, MCRectangleScalePoints(t_scrub_forward_rect, 0.5, 0.3));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_scrub_forward_rect, 0.5, 0.7));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_scrub_forward_rect, 0.7, 0.5));
    MCGContextCloseSubpath(p_gcontext);
    
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerScrubBackButton(MCGContextRef p_gcontext)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    MCRectangle t_scrub_back_rect = getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubBack);
    
    if (m_scrub_back_is_pressed)
    {
        MCGContextAddRectangle(p_gcontext, MCRectangleToMCGRectangle(t_scrub_back_rect));
        MCGContextSetFillRGBAColor(p_gcontext, 168 / 255.0, 1 / 255.0, 255 / 255.0, 1.0f); //PURPLE
        MCGContextFill(p_gcontext);
    }
    
    MCGColor *t_colors = nil;
    MCGFloat *t_stops = nil;
    setRamp(t_colors, t_stops);
    
    MCGAffineTransform t_transform;
    
    float origin_x = t_scrub_back_rect.x + t_scrub_back_rect.width / 2.0;
	float origin_y = t_scrub_back_rect.y + t_scrub_back_rect.height * 2 / 3;
	float primary_x = t_scrub_back_rect.x + t_scrub_back_rect.width / 2.0;
	float primary_y = t_scrub_back_rect.y + t_scrub_back_rect . height / 3;
	float secondary_x = t_scrub_back_rect.x;
	float secondary_y = t_scrub_back_rect.y + t_scrub_back_rect.height * 2 / 3;
    
    setTransform(t_transform, origin_x, origin_y, primary_x, primary_y, secondary_x, secondary_y);
        
    MCGContextSetFillGradient(p_gcontext, kMCGGradientFunctionLinear, t_stops, t_colors, 3, false, false, 1, t_transform, kMCGImageFilterNone);
    
    MCGContextBeginPath(p_gcontext);
    MCGContextMoveTo(p_gcontext, MCRectangleScalePoints(t_scrub_back_rect, 0.2, 0.5));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_scrub_back_rect, 0.4, 0.3));
    MCGContextLineTo(p_gcontext, MCRectangleScalePoints(t_scrub_back_rect, 0.4, 0.7));
    
    MCGRectangle t_grect;
    t_grect = MCGRectangleMake(t_scrub_back_rect . x + 0.5 * t_scrub_back_rect . width, t_scrub_back_rect . y + 0.3 * t_scrub_back_rect . height, 0.2 * t_scrub_back_rect . width, 0.4 * t_scrub_back_rect . height);
    
    MCGContextAddRectangle(p_gcontext, t_grect);
    MCGContextCloseSubpath(p_gcontext);
       
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerSelectedAreaButton(MCGContextRef p_gcontext)
{
    MCRectangle t_selected_area;
    t_selected_area = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartSelectedArea);
    
    MCGContextAddRectangle(p_gcontext, MCRectangleToMCGRectangle(t_selected_area));
    MCGContextSetFillRGBAColor(p_gcontext, 43 / 255.0, 43 / 255.0, 43 / 255.0, 1.0f); //SOMEGRAY
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerVolumeAreaButton(MCGContextRef p_gcontext)
{
    MCRectangle t_volume_area;
    t_volume_area = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeArea);
    
    MCGContextSetFillRGBAColor(p_gcontext, 168 / 255.0, 1 / 255.0, 255 / 255.0, 1.0f); //PURPLE
    
    MCGRectangle t_grect = MCRectangleToMCGRectangle(t_volume_area);
    MCGContextAddRoundedRectangle(p_gcontext, t_grect, MCGSizeMake(30, 30));
    MCGContextFill(p_gcontext);
    
}

void MCPlayer::drawControllerPlayedAreaButton(MCGContextRef p_gcontext)
{
    MCRectangle t_played_area;
    t_played_area = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartPlayedArea);
    
    MCGContextSetFillRGBAColor(p_gcontext, 168 / 255.0, 1 / 255.0, 255 / 255.0, 1.0f); //PURPLE
    
    MCGRectangle t_rounded_rect = MCRectangleToMCGRectangle(t_played_area);
    
    MCGContextAddRoundedRectangle(p_gcontext, t_rounded_rect, MCGSizeMake(30, 30));
    
    MCGContextFill(p_gcontext);
}

void MCPlayer::drawControllerVolumeWellButton(MCGContextRef p_gcontext)
{
    MCRectangle t_volume_well;
    t_volume_well = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell);
    
    MCGBitmapEffects t_effects;
	t_effects . has_drop_shadow = false;
	t_effects . has_outer_glow = false;
	t_effects . has_inner_glow = false;
	t_effects . has_inner_shadow = true;
    t_effects . has_color_overlay = false;
    
    MCGShadowEffect t_inner_shadow;
    t_inner_shadow . color = MCGColorMakeRGBA(0.0f, 0.0f, 0.0f, 56.0 / 255.0);
    t_inner_shadow . blend_mode = kMCGBlendModeClear;
    t_inner_shadow . size = 0;
    t_inner_shadow . spread = 0;
    
    MCGFloat t_x_offset, t_y_offset;
    int t_distance = t_volume_well . width / 5;
    // Make sure we always have an inner shadow
    if (t_distance == 0)
        t_distance = 1;
    
    MCGraphicsContextAngleAndDistanceToXYOffset(235, t_distance, t_x_offset, t_y_offset);
    t_inner_shadow . x_offset = t_x_offset;
    t_inner_shadow . y_offset = t_y_offset;
    t_inner_shadow . knockout = false;
    
    t_effects . inner_shadow = t_inner_shadow;
    
    MCGContextSetFillRGBAColor(p_gcontext, 0.0f, 0.0f, 0.0f, 1.0f);
    MCGContextSetShouldAntialias(p_gcontext, true);
    
    MCGRectangle t_rounded_rect = MCRectangleToMCGRectangle(t_volume_well);
    
    MCGContextAddRoundedRectangle(p_gcontext, t_rounded_rect, MCGSizeMake(30, 30));
    MCGContextBeginWithEffects(p_gcontext, t_rounded_rect, t_effects);
    MCGContextFill(p_gcontext);
    MCGContextEnd(p_gcontext);
}

int MCPlayer::hittestcontroller(int x, int y)
{
    MCRectangle t_rect;
    t_rect = getcontrollerrect();
    
    if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartPlay), x, y))
        return kMCPlayerControllerPartPlay;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolume), x, y))
        return kMCPlayerControllerPartVolume;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubBack), x, y))
        return kMCPlayerControllerPartScrubBack;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartScrubForward), x, y))
        return kMCPlayerControllerPartScrubForward;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionStart), x, y))
        return kMCPlayerControllerPartSelectionStart;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartSelectionFinish), x, y))
        return kMCPlayerControllerPartSelectionFinish;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartThumb), x, y))
        return kMCPlayerControllerPartThumb;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartWell), x, y))
        return kMCPlayerControllerPartWell;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeSelector), x, y))
        return kMCPlayerControllerPartVolumeSelector;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeWell), x, y))
        return kMCPlayerControllerPartVolumeWell;
    
    else if (MCU_point_in_rect(getcontrollerpartrect(t_rect, kMCPlayerControllerPartVolumeBar), x, y))
        return kMCPlayerControllerPartVolumeBar;
    
    else
        return kMCPlayerControllerPartUnknown;
}

void MCPlayer::drawcontrollerbutton(MCDC *dc, const MCRectangle& p_rect)
{
    dc -> setforeground(dc -> getwhite());
    dc -> fillrect(p_rect, true);
    
    dc -> setforeground(dc -> getblack());
    dc -> setlineatts(1, LineSolid, CapButt, JoinMiter);
    
    dc -> drawrect(p_rect, true);
}

MCRectangle MCPlayer::getcontrollerrect(void)
{
    MCRectangle t_rect;
    t_rect = rect;
    
    if (getflag(F_SHOW_BORDER))
        t_rect = MCU_reduce_rect(t_rect, borderwidth);
    
    t_rect . y = t_rect . y + t_rect . height - CONTROLLER_HEIGHT;
    t_rect . height = CONTROLLER_HEIGHT;
    
    return t_rect;
}

MCRectangle MCPlayer::getcontrollerpartrect(const MCRectangle& p_rect, int p_part)
{
    switch(p_part)
    {
        case kMCPlayerControllerPartVolume:
            return MCRectangleMake(p_rect . x, p_rect . y, CONTROLLER_HEIGHT, CONTROLLER_HEIGHT);
        case kMCPlayerControllerPartPlay:
            return MCRectangleMake(p_rect . x + CONTROLLER_HEIGHT, p_rect . y, CONTROLLER_HEIGHT, CONTROLLER_HEIGHT);
            
        case kMCPlayerControllerPartThumb:
        {
            if (m_platform_player == nil)
                return MCRectangleMake(0, 0, 0, 0);
            
            uint32_t t_current_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &t_current_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            MCRectangle t_well_rect;
            t_well_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartWell);
            
            // The width of the thumb is CONTROLLER_HEIGHT / 2
            int t_active_well_width;
            t_active_well_width = t_well_rect . width - CONTROLLER_HEIGHT / 2;
            
            int t_thumb_left;
            t_thumb_left = t_active_well_width * t_current_time / t_duration;
            
            return MCRectangleMake(t_well_rect . x + t_thumb_left, t_well_rect . y - t_well_rect . height / 2, CONTROLLER_HEIGHT / 2, 2 * t_well_rect . height);
        }
            break;
            
        case kMCPlayerControllerPartWell:
            return MCRectangleMake(p_rect . x + 2 * CONTROLLER_HEIGHT + SELECTION_RECT_WIDTH, p_rect . y + 2 * CONTROLLER_HEIGHT / 5, p_rect . width - 4 * CONTROLLER_HEIGHT - 2 * SELECTION_RECT_WIDTH, CONTROLLER_HEIGHT / 5);
            
        case kMCPlayerControllerPartScrubBack:
            return MCRectangleMake(p_rect . x + p_rect . width - 2 * CONTROLLER_HEIGHT, p_rect . y, CONTROLLER_HEIGHT, CONTROLLER_HEIGHT);
            
        case kMCPlayerControllerPartScrubForward:
            return MCRectangleMake(p_rect . x + p_rect . width - CONTROLLER_HEIGHT, p_rect . y, CONTROLLER_HEIGHT, CONTROLLER_HEIGHT);
            
        case kMCPlayerControllerPartSelectionStart:
        {
            uint32_t t_start_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &t_start_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            MCRectangle t_well_rect, t_thumb_rect;
            t_well_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartWell);
            t_thumb_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartThumb);
            
            int t_active_well_width;
            t_active_well_width = t_well_rect . width - t_thumb_rect . width;
            
            int t_selection_start_left;
            t_selection_start_left = t_active_well_width * t_start_time / t_duration;
            
            return MCRectangleMake(t_well_rect . x + t_selection_start_left, t_well_rect . y - CONTROLLER_HEIGHT / 6, SELECTION_RECT_WIDTH, t_well_rect . height + CONTROLLER_HEIGHT / 3);
        }
            
        case kMCPlayerControllerPartSelectionFinish:
        {
            uint32_t t_finish_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &t_finish_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            MCRectangle t_well_rect, t_thumb_rect;
            t_well_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartWell);
            t_thumb_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartThumb);
            
            int t_active_well_width;
            t_active_well_width = t_well_rect . width - t_thumb_rect . width;;
            
            int t_selection_finish_left;
            t_selection_finish_left = t_active_well_width * t_finish_time / t_duration;
            
            return MCRectangleMake(t_well_rect . x + t_selection_finish_left + CONTROLLER_HEIGHT / 2 - SELECTION_RECT_WIDTH, t_well_rect . y - CONTROLLER_HEIGHT / 6, SELECTION_RECT_WIDTH, t_well_rect . height + CONTROLLER_HEIGHT / 3);
        }
            break;
            
        case kMCPlayerControllerPartVolumeBar:
            return MCRectangleMake(p_rect . x , p_rect . y - 3 * CONTROLLER_HEIGHT, CONTROLLER_HEIGHT, 3 * CONTROLLER_HEIGHT);
            
        case kMCPlayerControllerPartVolumeSelector:
        {
            MCRectangle t_volume_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell);
            MCRectangle t_volume_bar_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeBar);
            
            // The width and height of the volumeselector are CONTROLLER_HEIGHT / 2
            int32_t t_actual_height = t_volume_well_rect . height - CONTROLLER_HEIGHT / 2;
            
            int32_t t_x_offset = t_volume_well_rect . y - t_volume_bar_rect . y;
            
            return MCRectangleMake(p_rect . x + CONTROLLER_HEIGHT / 4 , t_volume_well_rect . y + t_volume_well_rect . height - t_actual_height * loudness / 100 - CONTROLLER_HEIGHT / 2, CONTROLLER_HEIGHT / 2, CONTROLLER_HEIGHT / 2 );
        }
            break;
        case kMCPlayerControllerPartSelectedArea:
        {
            uint32_t t_start_time, t_finish_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &t_start_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &t_finish_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            MCRectangle t_well_rect, t_thumb_rect;
            t_well_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartWell);
            t_thumb_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartThumb);
            
            int t_active_well_width;
            t_active_well_width = t_well_rect . width - t_thumb_rect . width;
            
            int t_selection_start_left, t_selection_finish_left;
            t_selection_start_left = t_active_well_width * t_start_time / t_duration;
            t_selection_finish_left = t_active_well_width * t_finish_time / t_duration;
            
            return MCRectangleMake(t_well_rect . x + t_selection_start_left, t_well_rect . y, t_selection_finish_left - t_selection_start_left + t_thumb_rect . width, t_well_rect . height);
        }
            break;
            
        case kMCPlayerControllerPartVolumeArea:
        {
            MCRectangle t_volume_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell);
            MCRectangle t_volume_selector_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeSelector);
            int32_t t_bar_height = t_volume_well_rect . height;
            int32_t t_bar_width = t_volume_well_rect . width;
            int32_t t_width = t_volume_well_rect . width;
            
            int32_t t_x_offset = (t_bar_width - t_width) / 2;
            // Adjust y by 2 pixels
            return MCRectangleMake(t_volume_well_rect. x , t_volume_selector_rect . y + 2 , t_width, t_volume_well_rect . y + t_volume_well_rect . height - t_volume_selector_rect . y );
        }
            break;
            
        case kMCPlayerControllerPartPlayedArea:
        {
            uint32_t t_start_time, t_current_time, t_finish_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            if (getflag(F_SHOW_SELECTION))
            {
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &t_start_time);
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &t_finish_time);
            }
            else
            {
                t_start_time = 0;
                t_finish_time = t_duration;
            }
            
            
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &t_current_time);
            
            if (t_current_time == 0)
                t_current_time = t_start_time;
            if (t_current_time > t_finish_time)
                t_current_time = t_finish_time;
            if (t_current_time < t_start_time)
                t_current_time = t_start_time;
            
            MCRectangle t_well_rect, t_thumb_rect;
            t_well_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartWell);
            t_thumb_rect = getcontrollerpartrect(p_rect, kMCPlayerControllerPartThumb);
            
            int t_active_well_width;
            t_active_well_width = t_well_rect . width - t_thumb_rect . width;
            
            int t_selection_start_left, t_current_time_left;
            t_selection_start_left = t_active_well_width * t_start_time / t_duration;
            t_current_time_left = t_active_well_width * t_current_time / t_duration;
            
            return MCRectangleMake(t_well_rect . x + t_selection_start_left, t_well_rect . y, t_current_time_left - t_selection_start_left + t_thumb_rect . width / 2, t_well_rect . height);
        }
            break;
        case kMCPlayerControllerPartVolumeWell:
        {
            MCRectangle t_volume_bar_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeBar);
            int32_t t_width = CONTROLLER_HEIGHT / 4;
            
            int32_t t_x_offset = (t_volume_bar_rect . width - t_width) / 2;
            
            return MCRectangleMake(t_volume_bar_rect . x + t_x_offset, t_volume_bar_rect . y + t_x_offset, t_width, t_volume_bar_rect . height - 2 * t_x_offset);
        }
            break;
        default:
            break;
    }
    
    return MCRectangleMake(0, 0, 0, 0);
}

void MCPlayer::redrawcontroller(void)
{
    if (!getflag(F_SHOW_CONTROLLER))
        return;
    
    layer_redrawrect(getcontrollerrect());
}

void MCPlayer::handle_mdown(int p_which)
{
    m_inside = true;
    
    int t_part;
    t_part = hittestcontroller(mx, my);
    
    /*
     if (m_show_volume && t_part != kMCPlayerControllerPartVolumeSelector && t_part != kMCPlayerControllerPartVolumeBar && t_part != kMCPlayerControllerPartVolume && t_part != kMCPlayerControllerPartVolumeWell)
     {
     m_show_volume = False;
     layer_redrawall();
     }
     */
    switch(t_part)
    {
        case kMCPlayerControllerPartPlay:
            if (getstate(CS_PREPARED))
            {
                playpause(!ispaused());
            }
            else
                playstart(nil);
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartPlay));
            
            break;
        case kMCPlayerControllerPartVolume:
        {
            if (!m_show_volume)
                m_show_volume = true;
            else
                m_show_volume = false;
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeBar));
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolume));
            layer_redrawall();
        }
            break;
            
        case kMCPlayerControllerPartVolumeSelector:
        {
            m_grabbed_part = t_part;
        }
            break;
            
        case kMCPlayerControllerPartVolumeWell:
        case kMCPlayerControllerPartVolumeBar:
        {
            if (!m_show_volume)
                return;
            MCRectangle t_part_volume_selector_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeSelector);
            MCRectangle t_volume_well;
            t_volume_well = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell);
            int32_t t_new_volume, t_height;
            
            t_height = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell) . height;
            t_new_volume = (t_volume_well . y + t_volume_well . height - my) * 100 / (t_height);
            
            if (t_new_volume < 0)
                t_new_volume = 0;
            if (t_new_volume > 100)
                t_new_volume = 100;
            
            loudness = t_new_volume;
            
            setloudness();
            
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeBar));
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeSelector));
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolume));
            
        }
            break;
            
            
        case kMCPlayerControllerPartWell:
        {
            MCRectangle t_part_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartWell);
            
            uint32_t t_new_time, t_duration;
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            t_new_time = (mx - t_part_well_rect . x) * t_duration / t_part_well_rect . width;
            setcurtime(t_new_time);
            
            layer_redrawall();
            layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartThumb));
        }
            break;
        case kMCPlayerControllerPartScrubBack:
            
            m_scrub_back_is_pressed = true;
            // This is needed for handle_mup
            m_was_paused = ispaused();
            
            if(ispaused())
                playstepback();
            else
            {
                playstepback();
                playpause(!ispaused());
            }
            m_grabbed_part = t_part;
            break;
            
        case kMCPlayerControllerPartScrubForward:
            
            m_scrub_forward_is_pressed = true;
            // This is needed for handle_mup
            m_was_paused = ispaused();
            
            if(ispaused())
                playstepforward();
            else
            {
                playstepforward();
                playpause(!ispaused());
            }
            m_grabbed_part = t_part;
            break;
            
        case kMCPlayerControllerPartSelectionStart:
        case kMCPlayerControllerPartSelectionFinish:
        case kMCPlayerControllerPartThumb:
            m_grabbed_part = t_part;
            break;
            
        default:
            break;
    }
}

void MCPlayer::handle_mfocus(int x, int y)
{
    if (state & CS_MFOCUSED)
    {
        switch(m_grabbed_part)
        {
            case kMCPlayerControllerPartVolumeSelector:
            {
                MCRectangle t_part_volume_selector_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeSelector);
                MCRectangle t_volume_well, t_volume_bar;
                t_volume_well = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell);
                t_volume_bar = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeBar);
                
                int32_t t_new_volume, t_height, t_offset;
                t_offset = t_volume_well . y - t_volume_bar . y;
                
                t_height = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartVolumeWell) . height;
                
                t_new_volume = (getcontrollerrect() . y - t_offset - y ) * 100 / (t_height);
                
                if (t_new_volume < 0)
                    t_new_volume = 0;
                if (t_new_volume > 100)
                    t_new_volume = 100;
                loudness = t_new_volume;
                
                setloudness();
                
                layer_redrawall();
            }
                break;
                
            case kMCPlayerControllerPartThumb:
            {
                MCRectangle t_part_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartWell);
                
                int32_t t_new_time, t_duration;
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
                
                t_new_time = (x - t_part_well_rect . x) * t_duration / t_part_well_rect . width;
                
                if (t_new_time < 0)
                    t_new_time = 0;
                setcurtime(t_new_time);
                
                layer_redrawall();
                layer_redrawrect(getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartThumb));
            }
                break;
                
                
            case kMCPlayerControllerPartSelectionStart:
            {
                MCRectangle t_part_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartWell);
                uint32_t t_new_start_time, t_duration;
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
                
                t_new_start_time = (x - t_part_well_rect . x) * t_duration / t_part_well_rect . width;
                setstarttime(t_new_start_time);
                MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyStartTime, kMCPlatformPropertyTypeUInt32, &t_new_start_time);
                
                layer_redrawall();
            }
                break;
                
            case kMCPlayerControllerPartSelectionFinish:
            {
                MCRectangle t_part_well_rect = getcontrollerpartrect(getcontrollerrect(), kMCPlayerControllerPartWell);
                
                uint32_t t_new_finish_time, t_duration;
                MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
                
                t_new_finish_time = (x - t_part_well_rect . x) * t_duration / t_part_well_rect . width;
                setendtime(t_new_finish_time);
                MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyFinishTime, kMCPlatformPropertyTypeUInt32, &t_new_finish_time);
                
                layer_redrawall();
                
            }
                break;
                
            case kMCPlayerControllerPartScrubBack:
            case kMCPlayerControllerPartScrubForward:
                if (MCU_point_in_rect(getcontrollerpartrect(getcontrollerrect(), m_grabbed_part), x, y))
                {
                    m_inside = True;
                }
                else
                {
                    m_inside = False;
                }
                break;
            default:
                break;
        }
    }
    
}

void MCPlayer::handle_mstilldown(int p_which)
{
    switch (m_grabbed_part)
    {
        case kMCPlayerControllerPartScrubForward:
        {
            uint32_t t_current_time, t_duration;
            
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &t_current_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            if (t_current_time > t_duration)
                t_current_time = t_duration;
            
            double t_rate;
            if (m_inside)
            {
                t_rate = m_initial_rate;
                m_initial_rate += 0.5;
            }
            else
                t_rate = 0.0;
            
            MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyPlayRate, kMCPlatformPropertyTypeDouble, &t_rate);
        }
            break;
            
        case kMCPlayerControllerPartScrubBack:
        {
            uint32_t t_current_time, t_duration;
            
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyCurrentTime, kMCPlatformPropertyTypeUInt32, &t_current_time);
            MCPlatformGetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyDuration, kMCPlatformPropertyTypeUInt32, &t_duration);
            
            if (t_current_time < 0.0)
                t_current_time = 0.0;
            
            double t_rate;
            if (m_inside)
            {
                t_rate = m_initial_rate;
                m_initial_rate -= 0.5;
            }
            else
                t_rate = 0.0;
            
            MCPlatformSetPlayerProperty(m_platform_player, kMCPlatformPlayerPropertyPlayRate, kMCPlatformPropertyTypeDouble, &t_rate);
        }
            break;
            
        default:
            break;
    }
    
}

void MCPlayer::handle_mup(int p_which)
{
    switch (m_grabbed_part)
    {
        case kMCPlayerControllerPartScrubBack:
            m_scrub_back_is_pressed = false;
            playpause(m_was_paused);
            layer_redrawall();
            break;
            
        case kMCPlayerControllerPartScrubForward:
            m_scrub_forward_is_pressed = false;
            playpause(m_was_paused);
            layer_redrawall();
            break;
        default:
            break;
    }
    
    m_grabbed_part = kMCPlayerControllerPartUnknown;
    m_initial_rate = 0.0;
}