#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>
#include <windows.h>

// Pkt odniesienia dla pojedynczego koloru
struct GradPoint {
	float fraction;
	COLORREF rgba;
	bool operator<(const GradPoint& p) const { return fraction < p.fraction; }
};

// Modul 1: Narzedzie wzorowane na Color Pickerze
class GimpColorTool {
public:
	bool RunModal(HINSTANCE inst, HWND hParent, COLORREF& io_rgb);

	static void CvtRGB2HSV(int r, int g, int b, float& th, float& ts, float& tv);
	static void CvtHSV2RGB(float th, float ts, float tv, int& r, int& g, int& b);

private:
	HWND wnd_self_ = nullptr;
	HWND edit_ctrls_[6]{};
	COLORREF base_color_ = 0, curr_color_ = 0;
	float hue_ang_ = 0, sat_amt_ = 0, val_amt_ = 0;

	bool drag_wheel_ = false, drag_tri_ = false;
	bool scr_pick_mode_ = false;
	bool in_sync_ = false;

	std::vector<DWORD> pixels_buf_;
	std::vector<DWORD> wheel_cache_;
	bool has_cache_ = false;

	static HHOOK hook_ptr_;
	static GimpColorTool* inst_active_;

	void DrawGraphic(HDC mem);

	// Dispatchery
	static LRESULT CALLBACK DlgProcBridge(HWND, UINT, WPARAM, LPARAM);
	static LRESULT CALLBACK GrabHook(int, WPARAM, LPARAM);
	LRESULT MsgHandler(HWND, UINT, WPARAM, LPARAM);
};

// Modul 2: Podstawa kontrolera aplikacji
class Win32GradientApp {
public:
	Win32GradientApp(HINSTANCE processInst);
	~Win32GradientApp();
	int StartLoop(int showFlags);

private:
	HINSTANCE module_handle_;
	HACCEL accel_keys_;
	HWND wnd_app_, wnd_track_;
	HICON app_icon_ = nullptr;

	std::vector<GradPoint> nodes_;
	std::vector<DWORD> lut_cache_;
	std::vector<DWORD> canvas_mem_;

	POINT vecA_{ 100, 100 }, vecB_{ 400, 300 };
	bool circular_form_ = false;

	int grip_active_ = 0, grip_hover_ = 0;
	int idx_drag_ = -1, idx_hover_ = -1;
	bool was_shifted_ = false;

	static const wchar_t* wnd_class_name;
	static const wchar_t* track_class_name;

	void DefSetup();
	void BuildLookupSequence();
	bool SetupWinClasses();
	HWND SpawnMainFrame();
	HICON GenerateSysIcon();
	void SetupMenu();

	void DumpBitmap();
	void ExportToCSV();
	void ImportFromCSV();

	void PaintSpectrum(HDC ctx, const RECT& area);
	void PaintAnchors(HDC ctx);
	void ActivateColorDlg(int nodeSz);

	// Callbacks
	static LRESULT CALLBACK CbMain(HWND, UINT, WPARAM, LPARAM);
	static LRESULT CALLBACK CbTrack(HWND, UINT, WPARAM, LPARAM);

	// Obiekty rutynowe
	LRESULT ProcessMain(HWND, UINT, WPARAM, LPARAM);
	LRESULT ProcessTrack(HWND, UINT, WPARAM, LPARAM);

	// Sub-handlery dla calkowitej obfuskacji
	void OnTrackPaint(HWND hwnd);
	void OnTrackMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
