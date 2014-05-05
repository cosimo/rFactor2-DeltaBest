/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"

// plugin information

extern "C" __declspec(dllexport)
    const char * __cdecl GetPluginName()                   { return PLUGIN_NAME; }

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
double prev_delta_best = NULL;
double prev_lap_dist = 0;              /* Used to accurately calculate dt and */
double prev_current_et = 0;            /*     speed of last interval */
double inbtw_scoring_traveled = 0;     /* Distance traveled (m) between successive UpdateScoring() calls */
double inbtw_scoring_elapsed = 0;
long render_ticks = 0;
long render_ticks_int = 12;

/* Keeps information about last and best laps */
struct LapTime {
    double elapsed[50000];             /* Longest possible track is 50km */
    double final;
    double started;
    double ended;
    double interval_offset;
} best_lap, last_lap;

#define FONT_NAME_MAXLEN 32

struct PluginConfig {
    unsigned int left;
    unsigned int top;
    unsigned int right;
    unsigned int bottom;
    unsigned int font_size;
    char font_name[FONT_NAME_MAXLEN];
} config;

#ifdef ENABLE_LOG
FILE* out_file;
#endif

// DirectX 9 objects, to render some text on screen
LPD3DXFONT g_Font = NULL;
D3DXFONT_DESC FontDesc = {
    DEFAULT_FONT_SIZE, 0, 400, 0, false, DEFAULT_CHARSET,
    OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_PITCH, DEFAULT_FONT_NAME
};
RECT FontPosition, ShadowPosition;
LPD3DXSPRITE bar = NULL;
LPDIRECT3DTEXTURE9 texture = NULL;

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

void DeltaBestPlugin::StartSession()
{
#ifdef ENABLE_LOG
    out_file = fopen(LOG_FILE, "w");
    WriteLog("--STARTSESSION--");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::EndSession()
{
    ResetLap(last_lap);
    ResetLap(best_lap);
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

    /* Reset last and best lap, so we'll start from scratch next time.

    TODO: keep best lap. Player could use changing setups a few times
    and challenging the same best lap in the same session
    */
    ResetLap(last_lap);
    ResetLap(best_lap);

    /* Reset delta best state */
    last_pos = 0;
    prev_lap_dist = 0;
    //prev_current_et = 0;
    current_delta_best = 0;
    prev_delta_best = 0;

#ifdef ENABLE_LOG
    WriteLog("---EXITREALTIME---");
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::ResetLap(struct LapTime lap)
{
    lap.ended = NULL;
    lap.final = NULL;
    lap.started = NULL;
    lap.interval_offset = 0;

    for (unsigned int i = 0; i < sizeof(lap.elapsed); i++)
        lap.elapsed[i] = 0;
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
            unsigned int meters = round(vinfo.mLapDist >= 0 ? vinfo.mLapDist : 0);

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
                    for (unsigned int i = last_pos; i < meters; i++) {
                        /* Elapsed time at this position already filled in by UpdateTelemetry()? */
                        if (last_lap.elapsed[i] > 0.0)
                            continue;
                        /* Linear interpolation of elapsed time in relation to physical position */
                        double interval_fraction = meters == last_pos ? 1.0 : (1.0 * i - last_pos) / (1.0 * meters - last_pos);
                        last_lap.elapsed[i] = prev_current_et + (interval_fraction * time_interval) - vinfo.mLapStartET;
#ifdef ENABLE_LOG
                        fprintf(out_file, "[DELTA]     elapsed[%d] = %.3f (interval_fraction=%.3f)\n", i, last_lap.elapsed[i], interval_fraction);
#endif /* ENABLE_LOG */
                    }
                    last_lap.elapsed[meters] = info.mCurrentET - vinfo.mLapStartET;
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

        //prev_delta_best = current_delta_best;
        //current_delta_best = CalculateDeltaBest2();

        inbtw_scoring_traveled = 0;
        inbtw_scoring_elapsed = 0;
    }

}

/* We use UpdateTelemetry() to gain notable precision in position updates.
We assume that (-1.0 * LocalVelocity.z) is the forward speed of the
vehicle, which seems to be confirmed by observed data.

Having forward speed means that with a delta-t we can directly measure
the distance traveled at 20hz instead of 5hz of UpdateScoring().

We use this data to complete information on vehicle lap progress
between successive UpdateScoring() calls.
*/

void DeltaBestPlugin::UpdateTelemetry(const TelemInfoV01 &info)
{
    if (! in_realtime)
        return;

    double dt = info.mDeltaTime;
    double forward_speed = - info.mLocalVel.z;
    double distance = forward_speed * dt;

    inbtw_scoring_traveled += distance;
    inbtw_scoring_elapsed  += dt;

    unsigned int inbtw_pos = round(last_pos + inbtw_scoring_traveled);
    if (inbtw_pos > last_pos) {
        last_lap.elapsed[inbtw_pos] = last_lap.elapsed[last_pos] + inbtw_scoring_elapsed;
#ifdef ENABLE_LOG
        fprintf(out_file, "\tNEW inbtw pos=%d elapsed=%.3f (last_pos=%d, t=%.3f, acc_t=%.3f)\n",
            inbtw_pos, inbtw_scoring_elapsed, last_pos, last_lap.elapsed[last_pos], last_lap.elapsed[inbtw_pos]);
#endif /* ENABLE_LOG */
    }

#ifdef ENABLE_LOG
    fprintf(out_file, "\tdt=%.3f fwd_speed=%.3f dist=%.3f inbtw_scoring_traveled=%.3f last_pos(m)=%d\n",
        dt, forward_speed, distance, inbtw_scoring_traveled, last_pos);
#endif /* ENABLE_LOG */
}

void DeltaBestPlugin::InitScreen(const ScreenInfoV01& info)
{
    long screen_width = info.mWidth;
    long screen_height = info.mHeight;

    LoadConfig(config, CONFIG_FILE);

    /* Now we know screen X/Y, we can place the text somewhere
    specific (in height). If everything is zero then apply our
    defaults: (0, height/6) -> (width, height) */

    if (config.right == 0)
        config.right = screen_width;
    if (config.bottom == 0)
        config.bottom = screen_height;
    if (config.top == 0)
        config.top = 148; // screen_height / 6.0;

    FontDesc.Height = config.font_size;
    sprintf(FontDesc.FaceName, config.font_name);

    FontPosition.top = config.top;
    FontPosition.left = config.left;
    FontPosition.right = config.right;
    FontPosition.bottom = config.bottom;

    ShadowPosition = FontPosition;
    ShadowPosition.top += 2;
    ShadowPosition.left += 2;

    D3DXCreateFontIndirect((LPDIRECT3DDEVICE9) info.mDevice, &FontDesc, &g_Font);
    assert(g_Font != NULL);

    D3DXCreateTextureFromFile((LPDIRECT3DDEVICE9) info.mDevice, "Plugins\\DeltaBestBackground.png", &texture);
    D3DXCreateSprite((LPDIRECT3DDEVICE9) info.mDevice, &bar);

    assert(texture != NULL);
    assert(bar != NULL);

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
    if (bar) {
        bar->Release();
        bar = NULL;
    }
    if (texture) {
        texture->Release();
        texture = NULL;
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
    if (! best_lap.final)
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

double DeltaBestPlugin::CalculateDeltaBest2()
{
    /* Shouldn't really happen */
    if (! best_lap.final)
        return 0;

    /* Current position in meters around the track */
    int m = round(last_pos + inbtw_scoring_traveled);

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

void DeltaBestPlugin::DrawDeltaBar(const ScreenInfoV01 &info, double delta, double delta_diff)
{
    LPDIRECT3DDEVICE9 d3d = (LPDIRECT3DDEVICE9) info.mDevice;

    // mirror width = 580px (670, 10) -> (1250, 120)
    // bar  = 580 x 20 (670, 130) -> (1250, 150)
    // center.x = 860; // 1920/2
    // time = (center-100, 155) -> (center+100, 190)

    D3DXVECTOR3 bar_pos, time_pos;

    bar_pos.x = 670;
    bar_pos.y = 130;
    bar_pos.z = 0;

    time_pos.x = 900;
    time_pos.y = 155;
    time_pos.z = 0;

    bar->Begin(D3DXSPRITE_ALPHABLEND);

    RECT bar_rect = { 0, 0, 580, 20 };
    RECT time_rect = { 0, 0, 120, 35 };
    D3DCOLOR bar_grey = D3DCOLOR_RGBA(0x60, 0x60, 0x60, 0xFF);

    bar->Draw(texture, &bar_rect,  NULL, &bar_pos,  bar_grey);

    // Draw delta bar
    D3DCOLOR delta_bar_color;
    D3DXVECTOR3 delta_pos;
    RECT delta_size = { 0, 0, 0, 20 };

    delta_bar_color = BarColor(delta, delta_diff);
    delta_pos.z = 0;
    delta_pos.y = 130;

    if (delta > 0) {
        delta_pos.x = info.mWidth / 2.0;
        int size = delta > 2 ? 290 : (290 * (delta / 2.0));
        delta_size.right = size;
    }
    else if (delta < 0) {
        double half_screen = info.mWidth / 2.0;
        delta_pos.x = delta < -2 ? 670 : (half_screen - (half_screen - 670) * (-delta/2.0));
        int size = half_screen - delta_pos.x;
        delta_size.right = size;
    }

    bar->Draw(texture, &delta_size, NULL, &delta_pos, delta_bar_color);

    // Draw the time text ("-0.18")

    D3DCOLOR shadowColor = 0xC0585858;
    D3DCOLOR textColor = TextColor(delta);

    char lp_deltaBest[15] = "";
    long text_rect_center = delta_pos.x + (delta_size.right / 2);
    const long text_rect_width = 120;
    const long bar_max_width = 580;

    text_rect_center = max(text_rect_center, (info.mWidth - bar_max_width) / 2);

    time_pos.x = text_rect_center - text_rect_width / 2;
    time_rect.right = text_rect_width;

    bar->Draw(texture, &time_rect, NULL, &time_pos, bar_grey);
    bar->End();

    FontPosition.left = time_pos.x;
    FontPosition.right = FontPosition.left + text_rect_width;

    ShadowPosition.left = FontPosition.left + 2;
    ShadowPosition.right = FontPosition.right + 2;

    sprintf(lp_deltaBest, "%+2.2f", delta);
    g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &ShadowPosition, DT_CENTER, shadowColor);
    g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &FontPosition,   DT_CENTER, textColor);

}

void DeltaBestPlugin::RenderScreenAfterOverlays(const ScreenInfoV01 &info)
{

    /* If we're not in realtime, not in green flag, etc...
    there's no need to display the Delta Best time */
    if (! NeedToDisplay())
        return;

}

void DeltaBestPlugin::RenderScreenBeforeOverlays(const ScreenInfoV01 &info)
{

    /* If we're not in realtime, not in green flag, etc...
    there's no need to display the Delta Best time */
    if (! NeedToDisplay())
        return;

    /* Can't draw without a font object */
    if (g_Font == NULL)
        return;

    double delta = current_delta_best;
    double diff = current_delta_best - prev_delta_best;

    /* Calculate the new delta best every n ticks
       and display a suitable value to get there in n ticks */
    if (render_ticks % render_ticks_int == 0) {
        prev_delta_best = current_delta_best;
        current_delta_best = CalculateDeltaBest2();
        diff = current_delta_best - delta;
        double abs_diff = abs(diff);

        if (abs_diff > 1.0) {
            delta = current_delta_best;
        }
        else {
            render_ticks_int = 16;
            if (abs_diff > 0.25)
                render_ticks_int = 1;
            else if (abs_diff > 0.1)
                render_ticks_int = 8;
            delta += diff < 0 ? -0.01 : 0.01;
            current_delta_best = delta;
        }
    }

    render_ticks++;

    DrawDeltaBar(info, delta, diff);
}

/* Simple style: negative delta = green, positive delta = red */
D3DCOLOR DeltaBestPlugin::TextColor(double delta)
{
    D3DCOLOR text_color = 0xE0000000;      /* Alpha (transparency) value */
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

D3DCOLOR DeltaBestPlugin::BarColor(double delta, double delta_diff)
{
    static const D3DCOLOR ALPHA = 0xE0000000;
    bool is_gaining = delta_diff > 0;
    D3DCOLOR bar_color = ALPHA;
    bar_color |= is_gaining ? (COLOR_INTENSITY << 16) : (COLOR_INTENSITY << 8);

    double abs_val = abs(delta_diff);
    double cutoff_val = 0.02;

    if (abs_val <= cutoff_val) {
        unsigned int col_val = COLOR_INTENSITY * (1 / cutoff_val) * (cutoff_val - abs_val);
        if (is_gaining)
            bar_color |= (col_val << 8) + col_val;
        else
            bar_color |= (col_val << 16) + col_val;
    }

    return bar_color;
}

void DeltaBestPlugin::PreReset(const ScreenInfoV01 &info)
{
    if (g_Font)
        g_Font->OnLostDevice();
    if (bar)
        bar->OnLostDevice();
}

void DeltaBestPlugin::PostReset(const ScreenInfoV01 &info)
{
    if (g_Font)
        g_Font->OnResetDevice();
    if (bar)
        bar->OnResetDevice();
}

void DeltaBestPlugin::LoadConfig(struct PluginConfig &config, const char *ini_file)
{
    config.left= GetPrivateProfileInt("Display", "Left", 0, ini_file);
    config.top = GetPrivateProfileInt("Display", "Top", 0, ini_file);
    config.right = GetPrivateProfileInt("Display", "Right", 0, ini_file);
    config.bottom = GetPrivateProfileInt("Display", "Bottom", 0, ini_file);
    config.font_size = GetPrivateProfileInt("Display", "FontSize", DEFAULT_FONT_SIZE, ini_file);
    GetPrivateProfileString("Display", "FontName", DEFAULT_FONT_NAME, config.font_name, FONT_NAME_MAXLEN, ini_file);
}