//
// rF2 Delta Best plugin
//

#ifndef _INTERNALS_EXAMPLE_H
#define _INTERNALS_EXAMPLE_H

#include "InternalsPlugin.hpp"
#include <assert.h>
#include <math.h>               // for rand()
#include <stdio.h>              // for sample output
#include <d3dx9.h>              // DirectX9 main header


#undef ENABLE_LOG              /* To enable file logging (Plugins/DeltaBest.log) */
#define GREEN_FLAG 5


// This is used for the app to use the plugin for its intended purpose
class DeltaBestPlugin : public InternalsPluginV06
{

public:

	// Constructor/destructor
	DeltaBestPlugin() {}
	~DeltaBestPlugin() {}

	// These are the functions derived from base class InternalsPlugin
	// that can be implemented.
	void Startup(long version);    // game startup
	void Shutdown();               // game shutdown

	void EnterRealtime();          // entering realtime
	void ExitRealtime();           // exiting realtime

	void StartSession();           // session has started
	void EndSession();             // session has ended

	// GAME OUTPUT
	long WantsTelemetryUpdates() { return 0; }
	void UpdateTelemetry(const TelemInfoV01 &info) { }

	bool WantsGraphicsUpdates() { return false; }
	void UpdateGraphics(const GraphicsInfoV02 &info) { }

	// GAME INPUT
	bool HasHardwareInputs() { return false; }
	void UpdateHardware(const float fDT) { mET += fDT; } // update the hardware with the time between frames
	void EnableHardware() { mEnabled = true; }             // message from game to enable hardware
	void DisableHardware() { mEnabled = false; }           // message from game to disable hardware

	// See if the plugin wants to take over a hardware control.  If the plugin takes over the
	// control, this method returns true and sets the value of the float pointed to by the
	// second arg.  Otherwise, it returns false and leaves the float unmodified.
	bool CheckHWControl(const char * const controlName, float &fRetVal) { return false;	}
	bool ForceFeedback(float &forceValue) { return false; }

	// SCORING OUTPUT
	bool WantsScoringUpdates() { return true; }
	void UpdateScoring(const ScoringInfoV01 &info);

	// COMMENTARY INPUT
	bool RequestCommentary(CommentaryRequestInfoV01 &info) { return false; }

	// VIDEO EXPORT (sorry, no example code at this time)
	virtual bool WantsVideoOutput() { return false; }
	virtual bool VideoOpen(const char * const szFilename, float fQuality, unsigned short usFPS, unsigned long fBPS,
		unsigned short usWidth, unsigned short usHeight, char *cpCodec = NULL) {
		return(false);
	} // open video output file
	virtual void VideoClose() {}                                 // close video output file
	virtual void VideoWriteAudio(const short *pAudio, unsigned int uNumFrames) {} // write some audio info
	virtual void VideoWriteImage(const unsigned char *pImage) {} // write video image

	// SCREEN NOTIFICATIONS

	void InitScreen(const ScreenInfoV01 &info);                  // Now happens right after graphics device initialization
	void UninitScreen(const ScreenInfoV01 &info);                // Now happens right before graphics device uninitialization

	void DeactivateScreen(const ScreenInfoV01 &info);            // Window deactivation
	void ReactivateScreen(const ScreenInfoV01 &info);            // Window reactivation

	void RenderScreenBeforeOverlays(const ScreenInfoV01 &info);  // before rFactor overlays
	void RenderScreenAfterOverlays(const ScreenInfoV01 &info);   // after rFactor overlays

	void PreReset(const ScreenInfoV01 &info);					 // after detecting device lost but before resetting
	void PostReset(const ScreenInfoV01 &info);					 // after resetting

	void ThreadStarted(long type);                               // called just after a primary thread is started (type is 0=multimedia or 1=simulation)
	void ThreadStopping(long type);                              // called just before a primary thread is stopped (type is 0=multimedia or 1=simulation)

private:

	double CalculateDeltaBest();
	bool NeedToDisplay();
    void WriteLog(const char * const msg);
    D3DCOLOR TextColor(double deltaBest);

    //
    // Current status
    //

    float mET;                             /* needed for the hardware example */
	bool mEnabled;                         /* needed for the hardware example */

    bool in_realtime = false;              /* Are we in cockpit? As opposed to monitor */
    bool green_flag = false;               /* Is the race in green flag condition? */
    unsigned int last_pos = 0;             /* Meters around the track of the current lap */
    unsigned int scoring_ticks = 0;        /* Advances every time UpdateScoring() is called */
    double current_delta_best = NULL;      /* Current calculated delta best time */
    double prev_lap_dist = 0;              /* Used to accurately calculate dt and */
    double prev_current_et = 0;            /*   speed of last interval */

    /* Keeps information about last and best laps */
    struct LapTime {
        double elapsed[50000];             /* Longest possible track is 50km */
        double final = NULL;
        double started = NULL;
        double ended = NULL;
        double interval_offset = 0.0;
    } best_lap, last_lap;

    FILE* out_file;

    // DirectX 9 objects, to render some text on screen
    LPD3DXFONT g_Font = NULL;
    D3DXFONT_DESC FontDesc;
    RECT FontPosition;

};

#endif // _INTERNALS_EXAMPLE_H