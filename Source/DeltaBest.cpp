//
// rF2 Delta Best Plugin
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

extern "C" __declspec(dllexport)
int __cdecl GetPluginVersion()                         { return 5; } // InternalsPluginV05 functionality required

extern "C" __declspec(dllexport)
PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new DeltaBestPlugin); }

extern "C" __declspec(dllexport)
void __cdecl DestroyPluginObject(PluginObject *obj)  { delete((DeltaBestPlugin *)obj); }


//
// Static plugin context data
//

static bool in_realtime = false;
static FILE* out_file;

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

void DeltaBestPlugin::UpdateScoring(const ScoringInfoV01 &info)
{
	bool inRealtime = info.mInRealtime;
	double lapDist = info.mLapDist,
		currET = info.mCurrentET,
		endET = info.mEndET,
		lapStartET = 0;

	for (long i = 0; i < info.mNumVehicles; ++i) {
		VehicleScoringInfoV01 &vinfo = info.mVehicle[i];
		if (vinfo.mIsPlayer) {
			lapStartET = vinfo.mLapStartET;
			break;
		}
	}
}

void DeltaBestPlugin::InitScreen(const ScreenInfoV01& info)
{
	// Now we know screen X/Y, we can place the text somewhere specific
	FontPosition.top = 200;
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
	// TODO
	//overlayTextureManager->release( false );
	//d3dManager->preReset( info.mDevice );
	//d3dManager->postReset(info.mDevice, info.mWidth, info.mHeight, extractColorDepthFromPixelFormat( info.mPixelFormat ), info.mWindowed, (unsigned short)info.mRefreshRate, info.mAppWindow );
	WriteLog("---DEACTIVATE SCREEN---");
}

void DeltaBestPlugin::ReactivateScreen(const ScreenInfoV01& info)
{
	WriteLog("---REACTIVATE SCREEN---");
}

void DeltaBestPlugin::RenderScreenBeforeOverlays(const ScreenInfoV01 &info)
{
}

float DeltaBestPlugin::CalculateDeltaBest()
{
	float deltaBest = -0.12f;
	//float someRand = (float) rand() / RAND_MAX;
	//deltaBest += someRand;
	return deltaBest;
}

void DeltaBestPlugin::RenderScreenAfterOverlays(const ScreenInfoV01 &info)
{
	char lp_deltaBest[10] = "";
	float deltaBest = 0.0;
	D3DCOLOR textColor;

	if (in_realtime) {
		if (g_Font != NULL) {
			deltaBest = CalculateDeltaBest();
			textColor = (deltaBest > 0) ? 0xffff0000 : 0xff00ff00;
			sprintf(lp_deltaBest, "%+3.2f", deltaBest);
			g_Font->DrawText(NULL, (LPCSTR)lp_deltaBest, -1, &FontPosition, DT_CENTER, textColor);
		}
	}
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