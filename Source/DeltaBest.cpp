//
// rF2 Delta Best Plugin
//
// Author: Cosimo Streppone <cosimo@streppone.it>
// April 2014
//

#include "DeltaBest.hpp"

#include <assert.h>
#include <math.h>               // for rand()
#include <stdio.h>              // for sample output
#include <d3dx9.h>              // DirectX9 main header

// plugin information

extern "C" __declspec(dllexport)
const char * __cdecl GetPluginName()                   { return("rF2 Delta Best - 2014.04.20"); }

extern "C" __declspec(dllexport)
PluginObjectType __cdecl GetPluginType()               { return(PO_INTERNALS); }

// InternalsPluginV05 only functionality would be required,
// but v6 gives us 10hz scoring updates, so we go for 6 instead of 5
// XXX Ehm, should give us 10hz scoring updates, but doesn't.
extern "C" __declspec(dllexport)
int __cdecl GetPluginVersion()                         { return 6; }

extern "C" __declspec(dllexport)
PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new DeltaBestPlugin); }

extern "C" __declspec(dllexport)
void __cdecl DestroyPluginObject(PluginObject *obj)  { delete((DeltaBestPlugin *)obj); }


//
// Current status
//

bool in_realtime = false;              /* Are we in cockpit? As opposed to monitor */
bool green_flag = false;               /* Is the race in green flag condition? */
unsigned int last_pos = 0;             /* Meters around the track of the current lap */
unsigned int scoring_ticks = 0;        /* Advances every time UpdateScoring() is called */
double current_delta_best = NULL;      /* Current calculated delta best time */
double prev_lap_dist = 0;              /* Used to accurately calculate dt and */
double prev_current_et = 0;            /*   speed of last interval */

/* Keeps information about last and best laps */
struct LapTime {
	//double distance[10000];
	double elapsed[50000];             /* Longest possible track is 50km */
	double final = NULL;
	double started = NULL;
	double ended = NULL;
	double interval_offset = 0.0;
} best_lap, last_lap;

FILE* out_file;

// DirectX 9 objects, to render some text on screen
LPD3DXFONT g_Font = NULL;
D3DXFONT_DESC FontDesc = { 40, 0, 400, 0, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_PITCH, "Consolas Bold" };
RECT FontPosition;


//
// DeltaBestPlugin class
//

void DeltaBestPlugin::WriteLog(const char * const msg)
{
	if (out_file == NULL)
		out_file = fopen("Plugins\\DeltaBest.log", "w");

	if (out_file != NULL)
		fprintf(out_file, "%s\n", msg);
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
	out_file = fopen("Plugins\\DeltaBest.log", "w");
	WriteLog("--STARTSESSION--");
}

void DeltaBestPlugin::EndSession()
{
	WriteLog("--ENDSESSION--");
	if (out_file)
		fclose(out_file);
}

void DeltaBestPlugin::EnterRealtime()
{
	// start up timer every time we enter realtime
	mET = 0.0f;
	in_realtime = true;
	WriteLog("---ENTERREALTIME---");
}

void DeltaBestPlugin::ExitRealtime()
{
	in_realtime = false;
	WriteLog("---EXITREALTIME---");
}

bool DeltaBestPlugin::NeedToDisplay()
{
	// If we're in the monitor or replay,
	// no delta best should be displayed
	if (! in_realtime)
		return false;

	// If we are in any race/practice phase that's not
	// green flag, we don't need or want Delta Best displayed
	if (! green_flag)
		return false;

	// We can't display a delta best until we have a best lap recorded
	if (! best_lap.final)
		return false;

	return true;
}

void DeltaBestPlugin::UpdateScoring(const ScoringInfoV01 &info)
{

	// No scoring updates should take place if we're in the monitor
	// as opposed to the cockpit mode
	if (! in_realtime)
		return;

	// Update plugin context information, used by NeedToDisplay()
	green_flag = info.mGamePhase == GREEN_FLAG;


	for (long i = 0; i < info.mNumVehicles; ++i) {

		VehicleScoringInfoV01 &vinfo = info.mVehicle[i];
		if (! vinfo.mIsPlayer)
			continue;

		fprintf(out_file, "mLapStartET=%.3f mCurrentET=%.3f Elapsed=%.3f mLapDist=%.3f/%.3f prevLapDist=%.3f prevCurrentET=%.3f deltaBest=%+2.2f\n",
			vinfo.mLapStartET,
			info.mCurrentET,
			(info.mCurrentET - vinfo.mLapStartET),
			vinfo.mLapDist,
			info.mLapDist,
			prev_lap_dist,
			prev_current_et,
			current_delta_best
		);

		/* Check if we started a new lap just now */
		bool new_lap = (vinfo.mLapStartET != last_lap.started);
		double curr_lap_dist = vinfo.mLapDist >= 0 ? vinfo.mLapDist : 0;

		if (new_lap) {
			
			/* mLastLapTime is -1 when lap wasn't timed */
			bool was_timed = vinfo.mLastLapTime > 0.0;

			if (was_timed) {
				last_lap.final = vinfo.mLastLapTime;
				last_lap.ended = info.mCurrentET;

				fprintf(out_file, "New LAP: Last = %.3f, started = %.3f, ended = %.3f, interval_offset = %.3f\n",
					last_lap.final, last_lap.started, last_lap.ended, last_lap.interval_offset);

				/* Was it the best lap so far? */
				bool best_so_far = (best_lap.final == NULL || (last_lap.final < best_lap.final));
				if (best_so_far) {
					fprintf(out_file, "Last lap was the best so far (final time = %.3f, previous best = %.3f)\n",
						last_lap.final, best_lap.final);
					best_lap = last_lap;
				}

				fprintf(out_file, "Best LAP yet  = %.3f, started = %.3f, ended = %.3f, interval_offset = %3.f\n",
					best_lap.final, best_lap.started, best_lap.ended, best_lap.interval_offset);
			}

			/* Prepare to archive the new lap */
			last_lap.started = vinfo.mLapStartET;
			last_lap.final = NULL;
			last_lap.ended = NULL;
			last_lap.interval_offset = info.mCurrentET - vinfo.mLapStartET;
			last_lap.elapsed[0] = 0;
			scoring_ticks = 0;
			last_pos = 0;
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
				double avg_speed = (time_interval == 0)	? 0	: distance_traveled / time_interval;

                if (meters == last_pos) {
                    last_lap.elapsed[meters] = info.mCurrentET - vinfo.mLapStartET;
                    fprintf(out_file, "[DELTA]     elapsed[%d] = %.3f [same position]\n", last_lap.elapsed[meters]);
                }
                else {
                    for (unsigned int i = last_pos; i <= meters; i++) {
                        /* Linear interpolation of elapsed time in relation to physical position */
                        double interval_fraction = meters == last_pos ? 1.0 : (1.0 * i - last_pos) / (1.0 * meters - last_pos);
                        last_lap.elapsed[i] = prev_current_et + (interval_fraction * time_interval);
                        last_lap.elapsed[i] -= vinfo.mLapStartET;
                        fprintf(out_file, "[DELTA]     elapsed[%d] = %.3f (interval_fraction=%.3f)\n", i, last_lap.elapsed[i], interval_fraction);
                    }
                }

				fprintf(out_file, "[DELTA] distance_traveled=%.3f time_interval=%.3f avg_speed=%.3f [%d .. %d]\n",
					distance_traveled, time_interval, avg_speed, last_pos, meters);
			}

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
	// Now we know screen X/Y, we can place the text somewhere specific:
	// 4/5th of the screen height
	FontPosition.top = info.mHeight / 6.0;
	FontPosition.left = 0;
	FontPosition.right = info.mWidth;
	FontPosition.bottom = info.mHeight;

	D3DXCreateFontIndirect((LPDIRECT3DDEVICE9) info.mDevice, &FontDesc, &g_Font);
	assert(g_Font != NULL);

	WriteLog("---INIT SCREEN---");
}

void DeltaBestPlugin::UninitScreen(const ScreenInfoV01& info)
{
	if (g_Font) {
		g_Font->Release();
		g_Font = NULL;
	}
	WriteLog("---UNINIT SCREEN---");
}

void DeltaBestPlugin::DeactivateScreen(const ScreenInfoV01& info)
{
	WriteLog("---DEACTIVATE SCREEN---");
}

void DeltaBestPlugin::ReactivateScreen(const ScreenInfoV01& info)
{
	WriteLog("---REACTIVATE SCREEN---");
}

void DeltaBestPlugin::RenderScreenBeforeOverlays(const ScreenInfoV01 &info)
{
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

	///*
	//Extrapolate the best lap recording at the exact time
	//we need to compare it with out current lap
	//*/
	//
	///* First let's calculate the best lap slope
	//(none other than average speed in the tick) */
	//double bestSpeed, lastSpeed;
	//unsigned int t = scoring_ticks;
	//if (t > 0) {
	//	bestSpeed = (best_lap.distance[t] - best_lap.distance[t - 1]) /
	//		(best_lap.elapsed[t] - best_lap.elapsed[t - 1]);
	//	lastSpeed = (last_lap.distance[t] - last_lap.distance[t - 1]) /
	//		(last_lap.elapsed[t] - last_lap.elapsed[t - 1]);
	//}
	//else {
	//	bestSpeed = best_lap.distance[t] / best_lap.elapsed[t];
	//	lastSpeed = last_lap.distance[t] / last_lap.elapsed[t];
	//}

	///* We can now use the speed to extract the distance at the exact
	//   same moment (elapsed) for the current lap */
	//double bestLapDistanceAtTime, deltaBest;

	//if (t > 0) {
	//	bestLapDistanceAtTime = best_lap.distance[t - 1]
	//		+ (bestSpeed * (last_lap.elapsed[t] - best_lap.elapsed[t - 1]));
	//}
	//else {
	//	bestLapDistanceAtTime = bestSpeed * last_lap.elapsed[t];
	//}

	///* AAh, this is a steaming pile of poo */
	////if (lastSpeed < 0.001)
	////	lastSpeed = 0.01;

	//deltaBest = (bestLapDistanceAtTime - last_lap.distance[t]) / bestSpeed;

	///* Avoid ridiculous values */
	//if (deltaBest > 99)
	//	deltaBest = 99.0;
	//else if (deltaBest < -99)
	//	deltaBest = -99.0;

	//fprintf(out_file, "[DELTA] t=%d best.distance[t] = %.3f @ %.3f last.distance[t] = %.3f @ %.3f best.speed = %.3f best.atTimeX = %.3f delta = %.3f\n",
	//	t, best_lap.distance[t], best_lap.elapsed[t], last_lap.distance[t], last_lap.elapsed[t],
	//	bestSpeed, bestLapDistanceAtTime, deltaBest);

	//return deltaBest;
}

void DeltaBestPlugin::RenderScreenAfterOverlays(const ScreenInfoV01 &info)
{
	char lp_deltaBest[15] = "";
	double deltaBest = current_delta_best;
	D3DCOLOR textColor;

	// If we're not in realtime, not in green flag, etc...
	// there's no need to display the Delta Best time.
	if (! NeedToDisplay())
		return;

	/* Can't draw without a font object */
	if (g_Font == NULL)
		return;

	textColor = (deltaBest > 0) ? 0xffff0000 : 0xff00ff00;
	sprintf(lp_deltaBest, "%+2.2f", deltaBest);
	g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &FontPosition, DT_CENTER, textColor);

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

void DeltaBestPlugin::ThreadStarted(long type)
{
}

void DeltaBestPlugin::ThreadStopping (long type)
{
}