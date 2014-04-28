/*
  rF2 Delta Best Plugin

  Author: Cosimo Streppone <cosimo@streppone.it>
  Date:   April 2014
  URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"

// plugin information

extern "C" __declspec(dllexport)
    const char * __cdecl GetPluginName()               { return PLUGIN_NAME; }

extern "C" __declspec(dllexport)
PluginObjectType __cdecl GetPluginType()               { return PO_INTERNALS; }

extern "C" __declspec(dllexport)
int __cdecl GetPluginVersion()                         { return 6; }

extern "C" __declspec(dllexport)
PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new DeltaBestPlugin); }

extern "C" __declspec(dllexport)
void __cdecl DestroyPluginObject(PluginObject *obj)    { delete((DeltaBestPlugin *)obj); }

bool in_realtime = false;              /* Are we in cockpit? As opposed to monitor */
bool green_flag = false;               /* Is the race in green flag condition? */
bool key_switch = true;                /* Enabled/disabled state by keyboard action */
bool displayed_welcome = false;        /* Whether we displayed the "plugin enabled" welcome message */
unsigned int prev_pos = 0;             /* Meters around the track of the current lap (previous interval) */
unsigned int last_pos = 0;             /* Meters around the track of the current lap */
unsigned int scoring_ticks = 0;        /* Advances every time UpdateScoring() is called */
double current_delta_best = NULL;      /* Current calculated delta best time */
double prev_lap_dist = 0;              /* Used to accurately calculate dt and */
double prev_current_et = 0;            /*   speed of last interval */

/* Keeps information about last and best laps */
struct LapTime {
    double elapsed[50000];             /* Longest possible track is 50km */
    double final;
    double started;
    double ended;
    double interval_offset;
} best_lap, last_lap;

#ifdef ENABLE_LOG
FILE* out_file;
#endif

// DirectX 9 objects, to render some text on screen
LPD3DXFONT g_Font = NULL;
D3DXFONT_DESC FontDesc = { 48, 0, 400, 0, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_PITCH, DELTA_BEST_FONT };
RECT FontPosition, ShadowPosition;

//
// DeltaBestPlugin class
//

void DeltaBestPlugin::WriteLog(const char * const msg)
{
#ifdef ENABLE_LOG
	if (out_file == NULL)
		out_file = fopen(LOG_FILE, "w");

	if (out_file != NULL)
		fprintf(out_file, "%s\n", msg);
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::Startup(long version)
{
	// default HW control enabled to true
	mEnabled = true;
}

void DeltaBestPlugin::Shutdown()
{
}

void DeltaBestPlugin::StartSession()
{
#ifdef ENABLE_LOG
    out_file = fopen(LOG_FILE, "w");
    WriteLog("--STARTSESSION--");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::EndSession()
{
#ifdef ENABLE_LOG
    WriteLog("--ENDSESSION--");
    if (out_file)
        fclose(out_file);
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::EnterRealtime()
{
	// start up timer every time we enter realtime
	mET = 0.0f;
	in_realtime = true;
#ifdef ENABLE_LOG
    WriteLog("---ENTERREALTIME---");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::ExitRealtime()
{
	in_realtime = false;

    /* Reset last lap, so we'll start from scratch next time */
    last_lap.ended = NULL;
    last_lap.final = NULL;
    last_lap.started = NULL;
    last_lap.interval_offset = 0;
    last_lap.elapsed[0] = 0;

    /* Reset delta best state */
   	last_pos = 0;
    prev_lap_dist = 0;
    //prev_current_et = 0;
    current_delta_best = 0;
 
#ifdef ENABLE_LOG
    WriteLog("---EXITREALTIME---");
#endif /* ENABLE_LOG */
}

bool DeltaBestPlugin::NeedToDisplay()
{
	// If we're in the monitor or replay,
	// no delta best should be displayed
	if (! in_realtime)
		return false;

    // Option might be disabled by the user (TAB)
    if (! key_switch)
        return false;

	// If we are in any race/practice phase that's not
	// green flag, we don't need or want Delta Best displayed
	if (! green_flag)
		return false;

	/* We can't display a delta best until we have a best lap recorded */
	if (! best_lap.final)
		return false;

	return true;
}

void DeltaBestPlugin::UpdateScoring(const ScoringInfoV01 &info)
{

	/* No scoring updates should take place if we're
       in the monitor as opposed to the cockpit mode */
	if (! in_realtime)
		return;

    /* Toggle shortcut key. Turns off/on the display of delta time */
    if (KEY_DOWN(MAGIC_KEY))
        key_switch = ! key_switch;

	/* Update plugin context information, used by NeedToDisplay() */
	green_flag = info.mGamePhase == GREEN_FLAG;

	for (long i = 0; i < info.mNumVehicles; ++i) {
    	VehicleScoringInfoV01 &vinfo = info.mVehicle[i];
		if (! vinfo.mIsPlayer)
			continue;

#ifdef ENABLE_LOG
        fprintf(out_file, "mLapStartET=%.3f mCurrentET=%.3f Elapsed=%.3f mLapDist=%.3f/%.3f prevLapDist=%.3f prevCurrentET=%.3f deltaBest=%+2.2f lastPos=%d prevPos=%d\n",
            vinfo.mLapStartET,
            info.mCurrentET,
            (info.mCurrentET - vinfo.mLapStartET),
            vinfo.mLapDist,
            info.mLapDist,
            prev_lap_dist,
            prev_current_et,
            current_delta_best,
            last_pos,
            prev_pos);
#endif /* ENABLE_LOG */

		/* Check if we started a new lap just now */
		bool new_lap = (vinfo.mLapStartET != last_lap.started);
		double curr_lap_dist = vinfo.mLapDist >= 0 ? vinfo.mLapDist : 0;

        if (new_lap) {
			
			/* mLastLapTime is -1 when lap wasn't timed */
			bool was_timed = vinfo.mLastLapTime > 0.0;

			if (was_timed) {
				last_lap.final = vinfo.mLastLapTime;
				last_lap.ended = info.mCurrentET;

#ifdef ENABLE_LOG
                fprintf(out_file, "New LAP: Last = %.3f, started = %.3f, ended = %.3f, interval_offset = %.3f\n",
                    last_lap.final, last_lap.started, last_lap.ended, last_lap.interval_offset);
#endif /* ENABLE_LOG */

				/* Was it the best lap so far? */
				bool best_so_far = (best_lap.final == NULL || (last_lap.final < best_lap.final));
				if (best_so_far) {
#ifdef ENABLE_LOG
                    fprintf(out_file, "Last lap was the best so far (final time = %.3f, previous best = %.3f)\n",
                        last_lap.final, best_lap.final);
#endif /* ENABLE_LOG */

                    /* Complete the mileage of the last lap.
                       This avoids nasty jumps into empty space (+50.xx) when later comparing with best lap */
                    for (unsigned int i = last_pos + 1 ; i <= (unsigned int) info.mLapDist; i++) {
                        /* FIXME: Inaccurate. Should extrapolate last interval */
                        last_lap.elapsed[i] = last_lap.elapsed[i - 1];
                    }

					best_lap = last_lap;
				}

#ifdef ENABLE_LOG
                fprintf(out_file, "Best LAP yet  = %.3f, started = %.3f, ended = %.3f, interval_offset = %3.f\n",
                    best_lap.final, best_lap.started, best_lap.ended, best_lap.interval_offset);
#endif /* ENABLE_LOG */
			}

			/* Prepare to archive the new lap */
			last_lap.started = vinfo.mLapStartET;
			last_lap.final = NULL;
			last_lap.ended = NULL;
			last_lap.interval_offset = info.mCurrentET - vinfo.mLapStartET;
			last_lap.elapsed[0] = 0;
			last_pos = prev_pos = 0;
			prev_lap_dist = 0;
			/* Leave prev_current_et alone, or you have hyper-jumps */
		}

		/* If there's a lap in progress, save the delta updates */
		if (last_lap.started > 0.0) {
			unsigned int meters = (int) (vinfo.mLapDist >= 0 ? vinfo.mLapDist : 0);

			/* It could be that we have stopped our vehicle.
			   In that case (same array position), we want to
			   overwrite the previous value anyway */
			if (meters >= last_pos) {
				double distance_traveled = (vinfo.mLapDist - prev_lap_dist);
				if (distance_traveled < 0)
					distance_traveled = 0;
				double time_interval = (info.mCurrentET - prev_current_et);

                if (meters == last_pos) {
                    last_lap.elapsed[meters] = info.mCurrentET - vinfo.mLapStartET;
#ifdef ENABLE_LOG
                    fprintf(out_file, "[DELTA]     elapsed[%d] = %.3f [same position]\n", meters, last_lap.elapsed[meters]);
#endif /* ENABLE_LOG */
                }
                else {
                    for (unsigned int i = last_pos; i <= meters; i++) {
                        /* Linear interpolation of elapsed time in relation to physical position */
                        double interval_fraction = meters == last_pos ? 1.0 : (1.0 * i - last_pos) / (1.0 * meters - last_pos);
                        last_lap.elapsed[i] = prev_current_et + (interval_fraction * time_interval) - vinfo.mLapStartET;
#ifdef ENABLE_LOG
                        fprintf(out_file, "[DELTA]     elapsed[%d] = %.3f (interval_fraction=%.3f)\n", i, last_lap.elapsed[i], interval_fraction);
#endif /* ENABLE_LOG */
                    }
                }

#ifdef ENABLE_LOG
				fprintf(out_file, "[DELTA] distance_traveled=%.3f time_interval=%.3f [%d .. %d]\n",
					distance_traveled, time_interval, last_pos, meters);
#endif /* ENABLE_LOG */
			}

            prev_pos = last_pos;
			last_pos = meters;
		}

        if (curr_lap_dist > prev_lap_dist)
            prev_lap_dist = curr_lap_dist;

        prev_current_et = info.mCurrentET;
		current_delta_best = CalculateDeltaBest();

	}

}

void DeltaBestPlugin::InitScreen(const ScreenInfoV01& info)
{
    /* Now we know screen X/Y, we can place the text somewhere
       specific (in height) */
	FontPosition.top = info.mHeight / 6.0;
	FontPosition.left = 0;
	FontPosition.right = info.mWidth;
	FontPosition.bottom = info.mHeight;

    ShadowPosition = FontPosition;
    ShadowPosition.top += 2;
    ShadowPosition.left += 2;

	D3DXCreateFontIndirect((LPDIRECT3DDEVICE9) info.mDevice, &FontDesc, &g_Font);
	assert(g_Font != NULL);

#ifdef ENABLE_LOG
   	WriteLog("---INIT SCREEN---");
#endif /* ENABLE_LOG */

}

void DeltaBestPlugin::UninitScreen(const ScreenInfoV01& info)
{
	if (g_Font) {
		g_Font->Release();
		g_Font = NULL;
	}
#ifdef ENABLE_LOG
    WriteLog("---UNINIT SCREEN---");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::DeactivateScreen(const ScreenInfoV01& info)
{
#ifdef ENABLE_LOG
	WriteLog("---DEACTIVATE SCREEN---");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::ReactivateScreen(const ScreenInfoV01& info)
{
#ifdef ENABLE_LOG
	WriteLog("---REACTIVATE SCREEN---");
#endif /* ENABLE_LOG */
}

double DeltaBestPlugin::CalculateDeltaBest()
{
	/* Shouldn't really happen */
	if (!best_lap.final)
		return 0;

	unsigned int m = last_pos;            /* Current position in meters around the track */

	/* By using meters, and backfilling all the missing information,
	   it shouldn't be possible to not have the exact same position in the best lap */
	double last_time_at_pos = last_lap.elapsed[m];
	double best_time_at_pos = best_lap.elapsed[m];
	double delta_best = last_time_at_pos - best_time_at_pos;

	if (delta_best > 99.0)
		delta_best = 99.0;
	else if (delta_best < -99)
		delta_best = -99.0;

	return delta_best;
}

bool DeltaBestPlugin::WantsToDisplayMessage( MessageInfoV01 &msgInfo )
{
    /* Wait until we're in realtime, otherwise
       the message is lost in space */
    if (! in_realtime)
        return false;

    /* We just want to display this message once in this rF2 session */
    if (displayed_welcome)
        return false;

    /* Tell how to toggle display through keyboard */
    msgInfo.mDestination = 0;
    msgInfo.mTranslate = 0;
    sprintf(msgInfo.mText, "DeltaBest " DELTA_BEST_VERSION " plugin enabled (CTRL + D to toggle)");

    /* Don't do it anymore, just once per session */
    displayed_welcome = true;

    return true;
}

void DeltaBestPlugin::RenderScreenAfterOverlays(const ScreenInfoV01 &info)
{
	char lp_deltaBest[15] = "";
	double deltaBest = current_delta_best;

	/* If we're not in realtime, not in green flag, etc...
	   there's no need to display the Delta Best time */
	if (! NeedToDisplay())
		return;

	/* Can't draw without a font object */
	if (g_Font == NULL)
		return;

    //
    // Try to draw a quad on to the screen, but where exactly?
    // This should draw a background grey rectangle behind the delta time.
    //
#if 0
    #define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
    struct CUSTOMVERTEX {
        FLOAT X, Y, Z;
        D3DCOLOR COLOR;
        //FLOAT X, Y, Z, RHW;
        //DWORD COLOR;
    };
    CUSTOMVERTEX vertices[] = {
        { -3.0f,  3.0f, 0.0f, D3DCOLOR_XRGB(0, 0, 255), },
        {  3.0f,  3.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 0), },
        { -3.0f, -3.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0), },
        {  3.0f, -3.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 255), },
    }; 

    // Create a vertex buffer
    LPDIRECT3DDEVICE9 d3d = (LPDIRECT3DDEVICE9) info.mDevice;
    IDirect3DVertexBuffer9* v_buffer;
    VOID* p;
    d3d->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX),
        0, CUSTOMFVF, D3DPOOL_MANAGED, &v_buffer, NULL);

    // Lock v_buffer and load the vertices into it
    v_buffer->Lock(0, 0, (void**) &p, 0);
    memcpy(p, vertices, sizeof(vertices));
    v_buffer->Unlock();

    d3d->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));
    d3d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
#endif

    D3DCOLOR shadowColor = 0xC0585858;
    D3DCOLOR textColor = TextColor(deltaBest);
    //D3DCOLOR textColor = TextColorDifferential3();

	sprintf(lp_deltaBest, "%+2.2f", deltaBest);
    g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &ShadowPosition, DT_CENTER, shadowColor);
	g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &FontPosition,   DT_CENTER, textColor);
}

/* Simple style: negative delta = green, positive delta = red */
D3DCOLOR DeltaBestPlugin::TextColor(double delta)
{
    D3DCOLOR text_color = 0xD0000000;      /* Alpha (transparency) value */
    bool is_negative = delta < 0;
    double cutoff_val = 0.10;
    double abs_val = abs(delta);

    text_color |= is_negative
        ? (COLOR_INTENSITY << 8)
        : (COLOR_INTENSITY << 16);

    /* Blend red or green with white when closer to zero */
    if (abs_val <= cutoff_val) {
        unsigned int col_val = COLOR_INTENSITY * (1 / cutoff_val) * (cutoff_val - abs_val);
        if (is_negative)
            text_color |= (col_val << 16) + col_val;
        else
            text_color |= (col_val << 8) + col_val;
    }

    return text_color;
}

/* iRacing-style, take 3 */
D3DCOLOR DeltaBestPlugin::TextColorDifferential3()
{
    static const D3DCOLOR ALPHA = 0xD0000000;

    /* Calculate the derivative of the best lap and the last lap.
       If the last lap derivative grows faster, then we're losing ground.
       If the last lap derivative grows slower, we're gaining compared to best lap. */

    unsigned int t2 = last_pos;
    unsigned int t1 = prev_pos;
    double last_lap_dt = last_lap.elapsed[t2] - last_lap.elapsed[t1];
    double best_lap_dt = best_lap.elapsed[t2] - best_lap.elapsed[t1];
    double deriv_diff = best_lap_dt - last_lap_dt;
    bool is_negative = deriv_diff <= 0;

    D3DCOLOR text_color = ALPHA;
    text_color |= is_negative ? (COLOR_INTENSITY << 16) : (COLOR_INTENSITY << 8);

#ifdef ENABLE_LOG
    fprintf(out_file, "t1=%d t2=%d lap[t1]=%.3f lap[t2]=%.3f deriv_diff=%.3f\n",
        t1, t2, last_lap.elapsed[t1], last_lap.elapsed[t2], deriv_diff);
#endif

    double abs_val = abs(deriv_diff);
    if (abs_val < 0.05)
        abs_val = 0;
    double cutoff_val = 0.15;

    if (abs_val <= cutoff_val) {
        unsigned int col_val = COLOR_INTENSITY * (1 / cutoff_val) * (cutoff_val - abs_val);
        if (is_negative)
            text_color |= (col_val << 8) + col_val;
        else
            text_color |= (col_val << 16) + col_val;
#ifdef ENABLE_LOG
        fprintf(out_file, "abs_val=%.2f col_val=%d is_neg=%d col=%X\n",
            abs_val, col_val, is_negative ? 1 : 0, text_color);
#endif
    }

    return text_color;
}

void DeltaBestPlugin::PreReset(const ScreenInfoV01 &info)
{
	if (g_Font)
		g_Font->OnLostDevice();
}

void DeltaBestPlugin::PostReset(const ScreenInfoV01 &info)
{
	if (g_Font)
		g_Font->OnResetDevice();
}