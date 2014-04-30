//
// rF2 Delta Best plugin
//

#ifndef _INTERNALS_EXAMPLE_H
#define _INTERNALS_EXAMPLE_H

#include "InternalsPlugin.hpp"
#include <assert.h>
#include <math.h>               /* for rand() */
#include <stdio.h>              /* for sample output */
#include <d3dx9.h>              /* DirectX9 main header */

#define DELTA_BEST_VERSION      "v7"
#define PLUGIN_NAME             "rF2 Delta Best - 2014.04.30"
#undef  ENABLE_LOG              /* To enable file logging (Plugins/DeltaBest.log) */
#define LOG_FILE                "Plugins\\DeltaBest.log"
#define CONFIG_FILE             "Plugins\\DeltaBest.ini"
#define GREEN_FLAG              5
#define COLOR_INTENSITY         0xF0
#define DEFAULT_FONT_SIZE       48
#define DEFAULT_FONT_NAME       "Arial Black"

/* Toggle plugin with CTRL + a magic key. Reference:
http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx */
#define MAGIC_KEY               (0x44)      /* "D" */
#define KEY_DOWN(k)             ((GetAsyncKeyState(k) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000))

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

    bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);

    // GAME INPUT
    bool HasHardwareInputs() { return false; }
    void UpdateHardware(const float fDT) { mET += fDT; }   // update the hardware with the time between frames
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

    void RenderScreenBeforeOverlays(const ScreenInfoV01 &info){} // before rFactor overlays
    void RenderScreenAfterOverlays(const ScreenInfoV01 &info);   // after rFactor overlays

    void PreReset(const ScreenInfoV01 &info);					 // after detecting device lost but before resetting
    void PostReset(const ScreenInfoV01 &info);					 // after resetting

    void ThreadStarted(long type) {}                             // called just after a primary thread is started (type is 0=multimedia or 1=simulation)
    void ThreadStopping(long type) {}                            // called just before a primary thread is stopped (type is 0=multimedia or 1=simulation)

private:

    double CalculateDeltaBest();
    void LoadConfig(struct PluginConfig &config, const char *ini_file);
    bool NeedToDisplay();
    void WriteLog(const char * const msg);
    D3DCOLOR TextColor(double delta);
    D3DCOLOR TextColorDifferential(unsigned int t1, unsigned int t2);
    D3DCOLOR TextColorDifferential2();
    D3DCOLOR TextColorDifferential3();

    //
    // Current status
    //

    float mET;                             /* needed for the hardware example */
    bool mEnabled;                         /* needed for the hardware example */

};

#endif // _INTERNALS_EXAMPLE_H