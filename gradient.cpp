#include "gradient.h"
#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <windowsx.h>

#pragma comment(lib, "Comctl32.lib")

#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846
#endif

// IDs dla Menu Glownego
enum CmdMenu : WORD {
  ACT_RESET = 4001,
  ACT_RADIAL,
  ACT_LINEAR,
  ACT_EXP_BMP,
  ACT_LOAD_CSV,
  ACT_SAVE_CSV
};

const wchar_t *Win32GradientApp::wnd_class_name = L"WntGradMainC";
const wchar_t *Win32GradientApp::track_class_name = L"WntGradStripC";

// Punkt wejsciowy logiki
Win32GradientApp::Win32GradientApp(HINSTANCE processInst)
    : module_handle_(processInst), accel_keys_(nullptr), wnd_app_(nullptr),
      wnd_track_(nullptr) {
  DefSetup();
  app_icon_ = GenerateSysIcon();
  SetupWinClasses();
  wnd_app_ = SpawnMainFrame();
  SetupMenu();
}

// Czyszczenie stanowisk
Win32GradientApp::~Win32GradientApp() {
  if (accel_keys_)
    DestroyAcceleratorTable(accel_keys_);
  if (app_icon_)
    DestroyIcon(app_icon_);
}

// Resetuje stan aplikacji do domyślnego.
// Ustawia domyślne punkty gradientu (od czarnego do białego),
// resetuje pozycję wektorów początkowego i końcowego,
// a następnie wymusza ponowne narysowanie okien.
void Win32GradientApp::DefSetup() {
  nodes_ = {{0.0f, RGB(0, 0, 0)}, {1.0f, RGB(255, 255, 255)}};
  vecA_ = {100, 100};
  vecB_ = {400, 300};
  BuildLookupSequence();
  if (wnd_app_) {
    InvalidateRect(wnd_app_, nullptr, FALSE);
    InvalidateRect(wnd_track_, nullptr, FALSE);
  }
}

// Renderuje tablicę (tzw. Lookup Table) 1024 kolorów dla szybkiego dostępu.
// Mapuje i interpoluje kolory z utworzonych punktów kontrolnych (odcinkami).
// Szybko udostępnia odcienie dla pętli, eliminując potrzebę liczenia kolorów co piksel.
void Win32GradientApp::BuildLookupSequence() {
  lut_cache_.resize(1024);
  std::vector<GradPoint> proxy = nodes_;
  std::sort(proxy.begin(), proxy.end());

  int sz = (int)proxy.size();
  for (int idx = 0; idx < 1024; ++idx) {
    float ratio = (float)idx / 1023.0f;
    auto anchor =
        std::lower_bound(proxy.begin(), proxy.end(), GradPoint{ratio, 0});

    if (anchor == proxy.begin()) {
      lut_cache_[idx] = RGB(GetBValue(anchor->rgba), GetGValue(anchor->rgba),
                            GetRValue(anchor->rgba));
    } else if (anchor == proxy.end()) {
      lut_cache_[idx] =
          RGB(GetBValue(proxy[sz - 1].rgba), GetGValue(proxy[sz - 1].rgba),
              GetRValue(proxy[sz - 1].rgba));
    } else {
      auto aR = anchor;
      auto aL = std::prev(anchor);
      float dist = aR->fraction - aL->fraction;
      float lerpVal = (dist > 0.0001f) ? (ratio - aL->fraction) / dist : 0.0f;

      int r0 = GetRValue(aL->rgba), g0 = GetGValue(aL->rgba),
          b0 = GetBValue(aL->rgba);
      int r1 = GetRValue(aR->rgba), g1 = GetGValue(aR->rgba),
          b1 = GetBValue(aR->rgba);

      BYTE nR = (BYTE)(r0 + lerpVal * (r1 - r0));
      BYTE nG = (BYTE)(g0 + lerpVal * (g1 - g0));
      BYTE nB = (BYTE)(b0 + lerpVal * (b1 - b0));

      lut_cache_[idx] = RGB(nB, nG, nR);
    }
  }
}

// Tabela styli okien i wywolanie procedur Win32 APIs
bool Win32GradientApp::SetupWinClasses() {
  WNDCLASSEXW cx{sizeof(WNDCLASSEXW),
                 CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
                 CbMain,
                 0,
                 0,
                 module_handle_,
                 app_icon_,
                 LoadCursorW(nullptr, IDC_ARROW),
                 (HBRUSH)(COLOR_BTNFACE + 1),
                 nullptr,
                 wnd_class_name,
                 app_icon_};
  if (!RegisterClassExW(&cx))
    return false;

  cx.lpfnWndProc = CbTrack;
  cx.lpszClassName = track_class_name;
  cx.hIcon = nullptr;
  cx.hIconSm = nullptr;
  return RegisterClassExW(&cx) != 0;
}

// Procedura wykladania kanwy okien na pulpit Windows'a
HWND Win32GradientApp::SpawnMainFrame() {
  RECT geometry = {0, 0, 600, 400};
  AdjustWindowRect(&geometry, WS_OVERLAPPEDWINDOW, TRUE);
  int width = geometry.right - geometry.left;
  int height = geometry.bottom - geometry.top;

  HWND mw = CreateWindowExW(0, wnd_class_name, L"Gradient Editor",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                            nullptr, nullptr, module_handle_, this);

  wnd_track_ =
      CreateWindowExW(0, track_class_name, nullptr, WS_CHILD | WS_VISIBLE, 0, 0,
                      0, 0, mw, nullptr, module_handle_, this);

  return mw;
}

// Tworzy i konfiguruje główne menu aplikacji, zawierające opcje pliku i edycji.
// Rejestruje także skróty klawiszowe (akceleratory) dla szybkiego dostępu.
void Win32GradientApp::SetupMenu() {
  HMENU bMenu = CreateMenu();
  HMENU cFile = CreatePopupMenu();
  AppendMenuW(cFile, MF_STRING, ACT_EXP_BMP, L"&Export BMP\tCtrl+E");
  AppendMenuW(cFile, MF_STRING, ACT_SAVE_CSV, L"&Save CSV\tCtrl+S");
  AppendMenuW(cFile, MF_STRING, ACT_LOAD_CSV, L"&Load CSV\tCtrl+O");
  AppendMenuW(bMenu, MF_POPUP, (UINT_PTR)cFile, L"&File");

  HMENU cMode = CreatePopupMenu();
  AppendMenuW(cMode, MF_STRING, ACT_LINEAR, L"Mode: &Linear");
  AppendMenuW(cMode, MF_STRING, ACT_RADIAL, L"Mode: &Radial");
  AppendMenuW(cMode, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(cMode, MF_STRING, ACT_RESET, L"&Reset\tCtrl+R");
  AppendMenuW(bMenu, MF_POPUP, (UINT_PTR)cMode, L"&Edit");
  SetMenu(wnd_app_, bMenu);

  ACCEL map[] = {{FCONTROL | FVIRTKEY, 'R', ACT_RESET},
                 {FCONTROL | FVIRTKEY, 'E', ACT_EXP_BMP},
                 {FCONTROL | FVIRTKEY, 'S', ACT_SAVE_CSV},
                 {FCONTROL | FVIRTKEY, 'O', ACT_LOAD_CSV},
                 {FCONTROL | FVIRTKEY, 'L', ACT_LINEAR},
                 {FCONTROL | FSHIFT | FVIRTKEY, 'L', ACT_RADIAL}};
  accel_keys_ = CreateAcceleratorTableW(map, 6);
}

// Zapisuje aktualny widok płótna gradientu do pliku BMP.
// Oblicza wymiary rysunku, generuje nagłówki pliku bitmapy (BITMAPFILEHEADER i BITMAPINFOHEADER),
// a następnie kopiuje surowe piksele 32-bit z pamięci na dysk.
void Win32GradientApp::DumpBitmap() {
  if (canvas_mem_.empty())
    return;
  RECT area;
  GetClientRect(wnd_app_, &area);
  int cw = area.right - 10, ch = area.bottom - 65;

  wchar_t fpath[MAX_PATH] = L"export.bmp";
  OPENFILENAMEW of = {sizeof(OPENFILENAMEW)};
  of.hwndOwner = wnd_app_;
  of.lpstrFilter = L"BMP Image (*.bmp)\0*.bmp\0";
  of.lpstrFile = fpath;
  of.nMaxFile = MAX_PATH;
  of.lpstrDefExt = L"bmp";
  of.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  if (!GetSaveFileNameW(&of))
    return;

  std::ofstream fs(fpath, std::ios::binary);
  if (!fs)
    return;

  BITMAPFILEHEADER fhead = {};
  fhead.bfType = 0x4D42;
  fhead.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fhead.bfSize = fhead.bfOffBits + (DWORD)(cw * ch * 4);

  BITMAPINFOHEADER ihead = {};
  ihead.biSize = sizeof(BITMAPINFOHEADER);
  ihead.biWidth = cw;
  ihead.biHeight = -ch;
  ihead.biPlanes = 1;
  ihead.biBitCount = 32;
  ihead.biCompression = BI_RGB;

  fs.write(reinterpret_cast<char *>(&fhead), sizeof(fhead));
  fs.write(reinterpret_cast<char *>(&ihead), sizeof(ihead));
  fs.write(reinterpret_cast<char *>(canvas_mem_.data()), cw * ch * 4);
}

// Eksportuje zdefiniowane przez użytkownika węzły kolorów do pliku tekstowego CSV.
// Konwertuje proporcje i kolory punktów do formatu (pozycja,#RRGGBB).
void Win32GradientApp::ExportToCSV() {
  wchar_t p_out[MAX_PATH] = L"saved.csv";
  OPENFILENAMEW fw = {sizeof(OPENFILENAMEW)};
  fw.hwndOwner = wnd_app_;
  fw.lpstrFilter = L"CSV Format (*.csv)\0*.csv\0";
  fw.lpstrFile = p_out;
  fw.nMaxFile = MAX_PATH;
  fw.lpstrDefExt = L"csv";
  fw.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

  if (GetSaveFileNameW(&fw)) {
    std::ofstream stream(p_out);
    if (!stream)
      return;
    stream.imbue(std::locale("C"));

    for (const auto &nd : nodes_) {
      stream << nd.fraction << ",#" << std::hex << std::uppercase
             << std::setw(6) << std::setfill('0')
             << ((GetRValue(nd.rgba) << 16) | (GetGValue(nd.rgba) << 8) |
                 GetBValue(nd.rgba))
             << "\n";
    }
  }
}

// Wczytuje format (pozycja,#RRGGBB) z pliku CSV by przywrócić zapisaną listę węzłów.
// Parsuje tekst liniami załatać braki i wymusza ponowne zbudowanie tablicy na płótnie.
void Win32GradientApp::ImportFromCSV() {
  wchar_t p_in[MAX_PATH] = L"";
  OPENFILENAMEW fw = {sizeof(OPENFILENAMEW)};
  fw.hwndOwner = wnd_app_;
  fw.lpstrFilter = L"CSV Format (*.csv)\0*.csv\0";
  fw.lpstrFile = p_in;
  fw.nMaxFile = MAX_PATH;
  fw.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameW(&fw)) {
    std::ifstream stream(p_in);
    if (!stream)
      return;

    std::vector<GradPoint> _cache;
    std::string buffer;
    while (std::getline(stream, buffer)) {
      if (buffer.empty())
        continue;
      std::stringstream lex(buffer);
      lex.imbue(std::locale("C"));

      float stR;
      char c1, c2;
      unsigned int crgb;
      if (lex >> stR >> c1 >> c2 >> std::hex >> crgb) {
        if (c1 == ',' && c2 == '#') {
          _cache.push_back(
              {stR, RGB((crgb >> 16) & 0xFF, (crgb >> 8) & 0xFF, crgb & 0xFF)});
        }
      }
    }

    if (_cache.size() >= 2) {
      nodes_ = _cache;
      std::sort(nodes_.begin(), nodes_.end());
      BuildLookupSequence();
      InvalidateRect(wnd_app_, nullptr, FALSE);
      InvalidateRect(wnd_track_, nullptr, FALSE);
    }
  }
}

// Metoda tworzenia pikselowej teczy na kursor w lewym gornym oknie
HICON Win32GradientApp::GenerateSysIcon() {
  const int pxS = 32;
  HDC ctx = GetDC(NULL);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = pxS;
  bi.bmiHeader.biHeight = -pxS;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *_rt;
  HBITMAP bRGB = CreateDIBSection(ctx, &bi, DIB_RGB_COLORS, &_rt, NULL, 0);
  HBITMAP bMSK = CreateBitmap(pxS, pxS, 1, 1, NULL);
  DWORD *bdata = (DWORD *)_rt;

  for (int k = 0; k < pxS * pxS; ++k) {
    int r = k / pxS, c = k % pxS;
    float h_ = c * 360.0f / pxS;
    float v_ = 1.0f - r * 0.5f / pxS;
    int vr, vg, vb;
    GimpColorTool::CvtHSV2RGB(h_, 1.0f, v_, vr, vg, vb);
    bdata[k] = (vr << 16) | (vg << 8) | vb;
  }

  HDC bmCtx = CreateCompatibleDC(ctx);
  SelectObject(bmCtx, bMSK);
  PatBlt(bmCtx, 0, 0, pxS, pxS, BLACKNESS);
  SelectObject(bmCtx, NULL);
  DeleteDC(bmCtx);
  ICONINFO idata = {TRUE, 0, 0, bMSK, bRGB};
  HICON icn = CreateIconIndirect(&idata);
  DeleteObject(bRGB);
  DeleteObject(bMSK);
  ReleaseDC(NULL, ctx);
  return icn;
}

// Wywolanie logiki petli przez uzytkownika
int Win32GradientApp::StartLoop(int showFlags) {
  ShowWindow(wnd_app_, showFlags);
  MSG iter{};
  while (GetMessageW(&iter, nullptr, 0, 0)) {
    if (!TranslateAcceleratorW(wnd_app_, accel_keys_, &iter)) {
      TranslateMessage(&iter);
      DispatchMessageW(&iter);
    }
  }
  return (int)iter.wParam;
}

LRESULT Win32GradientApp::CbMain(HWND hw, UINT type, WPARAM wP, LPARAM lP) {
  Win32GradientApp *prx =
      (type == WM_NCCREATE)
          ? static_cast<Win32GradientApp *>(
                reinterpret_cast<LPCREATESTRUCTW>(lP)->lpCreateParams)
          : reinterpret_cast<Win32GradientApp *>(
                GetWindowLongPtrW(hw, GWLP_USERDATA));
  if (type == WM_NCCREATE)
    SetWindowLongPtrW(hw, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(prx));
  return prx ? prx->ProcessMain(hw, type, wP, lP)
             : DefWindowProcW(hw, type, wP, lP);
}

// Procedura handlowania inputem glownej kanwy
LRESULT Win32GradientApp::ProcessMain(HWND hw, UINT code, WPARAM wP,
                                      LPARAM lP) {
  if (code == WM_GETMINMAXINFO) {
    reinterpret_cast<MINMAXINFO *>(lP)->ptMinTrackSize = {400, 300};
    return 0;
  } else if (code == WM_SIZE) {
    if (wnd_track_)
      SetWindowPos(wnd_track_, nullptr, 5, HIWORD(lP) - 55, LOWORD(lP) - 10, 50,
                   SWP_NOZORDER);
    InvalidateRect(hw, nullptr, FALSE);
    return 0;
  } else if (code == WM_ERASEBKGND)
    return 1;
  else if (code == WM_PAINT) {
    PAINTSTRUCT pInfo;
    HDC hCore = BeginPaint(hw, &pInfo);
    RECT aData;
    GetClientRect(hw, &aData);
    aData.left += 5;
    aData.top += 5;
    aData.right -= 5;
    aData.bottom -= 60;

    HDC buffCtx = CreateCompatibleDC(hCore);
    HBITMAP buffBmp = CreateCompatibleBitmap(hCore, aData.right - aData.left,
                                             aData.bottom - aData.top);
    HBITMAP oldBmp = (HBITMAP)SelectObject(buffCtx, buffBmp);
    PaintSpectrum(buffCtx,
                  {0, 0, aData.right - aData.left, aData.bottom - aData.top});
    PaintAnchors(buffCtx);
    BitBlt(hCore, aData.left, aData.top, aData.right - aData.left,
           aData.bottom - aData.top, buffCtx, 0, 0, SRCCOPY);
    SelectObject(buffCtx, oldBmp);
    DeleteObject(buffBmp);
    DeleteDC(buffCtx);
    EndPaint(hw, &pInfo);
    return 0;
  } else if (code == WM_LBUTTONDOWN) {
    int ax = GET_X_LPARAM(lP) - 5, ay = GET_Y_LPARAM(lP) - 5;
    auto dist = [](int x1, int y1, int x2, int y2) {
      return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    };
    if (dist(ax, ay, vecA_.x, vecA_.y) <= 10.0)
      grip_active_ = 1;
    else if (dist(ax, ay, vecB_.x, vecB_.y) <= 10.0)
      grip_active_ = 2;
    if (grip_active_)
      SetCapture(hw);
    return 0;
  } else if (code == WM_MOUSEMOVE) {
    int ax = GET_X_LPARAM(lP) - 5, ay = GET_Y_LPARAM(lP) - 5;
    if (grip_active_ > 0 && (wP & MK_LBUTTON)) {
      if (grip_active_ == 1)
        vecA_ = {ax, ay};
      if (grip_active_ == 2)
        vecB_ = {ax, ay};
      InvalidateRect(hw, nullptr, FALSE);
    } else {
      auto dist = [](int x1, int y1, int x2, int y2) {
        return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
      };
      int hov = (dist(ax, ay, vecA_.x, vecA_.y) <= 10.0)   ? 1
                : (dist(ax, ay, vecB_.x, vecB_.y) <= 10.0) ? 2
                                                           : 0;
      if (hov != grip_hover_) {
        grip_hover_ = hov;
        InvalidateRect(hw, nullptr, FALSE);
      }
    }
    return 0;
  } else if (code == WM_LBUTTONUP) {
    grip_active_ = 0;
    ReleaseCapture();
    return 0;
  } else if (code == WM_COMMAND) {
    int uid = LOWORD(wP);
    if (uid == ACT_RESET)
      DefSetup();
    else if (uid == ACT_LINEAR) {
      circular_form_ = false;
      InvalidateRect(hw, nullptr, FALSE);
    } else if (uid == ACT_RADIAL) {
      circular_form_ = true;
      InvalidateRect(hw, nullptr, FALSE);
    } else if (uid == ACT_EXP_BMP)
      DumpBitmap();
    else if (uid == ACT_SAVE_CSV)
      ExportToCSV();
    else if (uid == ACT_LOAD_CSV)
      ImportFromCSV();
    return 0;
  } else if (code == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hw, code, wP, lP);
}

// Główna procedura operująca surowymi pikselami w kanwie, rysująca sam gradient.
// Używa interpolacji liniowej lub wektorowego dystansu (dla trybu radialnego), upewniając się 
// że korzysta w locie z LookUp Table dla każdego elementu siatki (skrzydeł dwubiegunowych).
void Win32GradientApp::PaintSpectrum(HDC ctx, const RECT &area) {
  int dx = area.right - area.left, dy = area.bottom - area.top;
  if (dx <= 0 || dy <= 0)
    return;
  canvas_mem_.resize(dx * dy);

  float ax = (float)(vecB_.x - vecA_.x);
  float ay = (float)(vecB_.y - vecA_.y);
  float squareLen = ax * ax + ay * ay;
  float maxRadius = std::sqrt(squareLen);

  for (int idx = 0; idx < dx * dy; ++idx) {
    int rr = idx / dx, cc = idx % dx;
    float factor = 0;
    float nx = (float)(cc - vecA_.x), ny = (float)(rr - vecA_.y);

    if (circular_form_) {
      float pitagoras = std::sqrt(nx * nx + ny * ny);
      factor = (maxRadius > 0.0001f) ? (pitagoras / maxRadius) : 0.0f;
    } else {
      factor = (squareLen > 0.0001f) ? ((nx * ax + ny * ay) / squareLen) : 0;
    }

    factor = std::max(0.0f, std::min(1.0f, factor));
    canvas_mem_[idx] = lut_cache_[(int)(factor * 1023)];
  }

  BITMAPINFO _meta = {0};
  _meta.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  _meta.bmiHeader.biWidth = dx;
  _meta.bmiHeader.biHeight = -dy;
  _meta.bmiHeader.biPlanes = 1;
  _meta.bmiHeader.biBitCount = 32;
  _meta.bmiHeader.biCompression = BI_RGB;
  SetDIBitsToDevice(ctx, area.left, area.top, dx, dy, 0, 0, 0, dy,
                    canvas_mem_.data(), &_meta, DIB_RGB_COLORS);
}

// Wyrysowuje interaktywne uchwyty punktów kontrolnych Start i End za pomocą GDI.
// Tworzy estetyczne kółka uwzględniając różne warunki hover i drag dla lepszej widoczności kanwy.
void Win32GradientApp::PaintAnchors(HDC ctx) {
  auto pin_point = [&](POINT _v, bool _hov) {
    HPEN op = CreatePen(PS_SOLID, _hov ? 3 : 2, RGB(255, 255, 255));
    HPEN ip = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ rBr = SelectObject(ctx, GetStockObject(NULL_BRUSH));
    HGDIOBJ rOp = SelectObject(ctx, op);
    Ellipse(ctx, _v.x - 6, _v.y - 6, _v.x + 6, _v.y + 6);
    HGDIOBJ rIp = SelectObject(ctx, ip);
    Ellipse(ctx, _v.x - 4, _v.y - 4, _v.x + 4, _v.y + 4);

    SelectObject(ctx, rBr);
    SelectObject(ctx, rOp);
    SelectObject(ctx, rIp);
    DeleteObject(op);
    DeleteObject(ip);
  };

  pin_point(vecA_, grip_hover_ == 1 || grip_active_ == 1);
  pin_point(vecB_, grip_hover_ == 2 || grip_active_ == 2);
}

LRESULT Win32GradientApp::CbTrack(HWND hw, UINT type, WPARAM wP, LPARAM lP) {
  Win32GradientApp *prx =
      (type == WM_NCCREATE)
          ? static_cast<Win32GradientApp *>(
                reinterpret_cast<LPCREATESTRUCTW>(lP)->lpCreateParams)
          : reinterpret_cast<Win32GradientApp *>(
                GetWindowLongPtrW(hw, GWLP_USERDATA));
  if (type == WM_NCCREATE)
    SetWindowLongPtrW(hw, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(prx));
  return prx ? prx->ProcessTrack(hw, type, wP, lP)
             : DefWindowProcW(hw, type, wP, lP);
}

// Aktywuje okno modalne dialogu GIMP Color Picker'a.
// Pobiera referencyjny piksel do zmiany, wstrzymuje wątek a na zmianę wymusza przebudowę (Invalidate) podglądu.
void Win32GradientApp::ActivateColorDlg(int nodeSz) {
  GimpColorTool tPicker;
  if (tPicker.RunModal(module_handle_, wnd_app_, nodes_[nodeSz].rgba)) {
    BuildLookupSequence();
    InvalidateRect(wnd_track_, nullptr, FALSE);
    InvalidateRect(wnd_app_, nullptr, FALSE);
  }
}

// Rysuje cały dolny pasek kontrolny gradientu przy zdarzeniu wywołanym przez WinAPI MSG.
// Używa Double Bufferingu do naniesienia pomniejszonej wersji gradientu oraz interaktywnych znaczników i trójkątów.
void Win32GradientApp::OnTrackPaint(HWND hwnd) {
  PAINTSTRUCT pi;
  HDC drw = BeginPaint(hwnd, &pi);
  RECT bx;
  GetClientRect(hwnd, &bx);
  HDC offHdc = CreateCompatibleDC(drw);
  HBITMAP offBmp = CreateCompatibleBitmap(drw, bx.right, bx.bottom);
  HBITMAP oldBmp = (HBITMAP)SelectObject(offHdc, offBmp);
  FillRect(offHdc, &bx, (HBRUSH)(COLOR_BTNFACE + 1));

  RECT tbx = {15, 10, bx.right - 15, bx.bottom - 10};
  BITMAPINFO idesc = {0};
  idesc.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  idesc.bmiHeader.biWidth = 1024;
  idesc.bmiHeader.biHeight = 1;
  idesc.bmiHeader.biPlanes = 1;
  idesc.bmiHeader.biBitCount = 32;
  idesc.bmiHeader.biCompression = BI_RGB;
  SetStretchBltMode(offHdc, COLORONCOLOR);
  StretchDIBits(offHdc, tbx.left, tbx.top, tbx.right - tbx.left,
                tbx.bottom - tbx.top, 0, 0, 1024, 1, lut_cache_.data(), &idesc,
                DIB_RGB_COLORS, SRCCOPY);

  int scope = tbx.right - tbx.left;
  for (size_t u = 0; u < nodes_.size(); ++u) {
    int xPos = tbx.left + (int)(nodes_[u].fraction * scope);
    bool trigger = (idx_drag_ == (int)u || idx_hover_ == (int)u);

    POINT apex[3] = {{xPos, tbx.top - 2},
                     {xPos - 6, tbx.bottom + 6},
                     {xPos + 6, tbx.bottom + 6}};

    HBRUSH uBr = CreateSolidBrush(nodes_[u].rgba);
    HPEN uPn = CreatePen(PS_SOLID, trigger ? 2 : 1,
                         trigger ? RGB(255, 200, 0) : RGB(0, 0, 0));
    HGDIOBJ rBr = SelectObject(offHdc, uBr);
    HGDIOBJ rPn = SelectObject(offHdc, uPn);

    Polygon(offHdc, apex, 3);

    SelectObject(offHdc, rBr);
    SelectObject(offHdc, rPn);
    DeleteObject(uBr);
    DeleteObject(uPn);
  }

  BitBlt(drw, 0, 0, bx.right, bx.bottom, offHdc, 0, 0, SRCCOPY);
  SelectObject(offHdc, oldBmp);
  DeleteObject(offBmp);
  DeleteDC(offHdc);
  EndPaint(hwnd, &pi);
}

// Centralna jednostka obsługująca mysz na dolnym pasku kontrolnym.
// Dekoduje kliknięcia (nowe punkty), śledzenie ruchu, czy zwalnianie CaptureHwnd (usunięcie punktów).
void Win32GradientApp::OnTrackMouse(HWND hwnd, UINT msType, WPARAM wP,
                                    LPARAM lP) {
  int posX = GET_X_LPARAM(lP);
  RECT g_;
  GetClientRect(hwnd, &g_);
  int wide = g_.right - 30;

  if (msType == WM_LBUTTONDOWN) {
    idx_drag_ = -1;
    for (size_t z = 0; z < nodes_.size(); ++z) {
      if (std::abs(posX - (15 + (int)(nodes_[z].fraction * wide))) <= 6) {
        idx_drag_ = (int)z;
        was_shifted_ = false;
        break;
      }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    SetCapture(hwnd);
  } else if (msType == WM_MOUSEMOVE) {
    TRACKMOUSEEVENT th = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&th);
    if (idx_drag_ != -1 && (wP & MK_LBUTTON)) {
      was_shifted_ = true;
      nodes_[idx_drag_].fraction =
          std::max(0.0f, std::min(1.0f, (float)(posX - 15) / wide));
      BuildLookupSequence();
      InvalidateRect(hwnd, nullptr, FALSE);
      InvalidateRect(wnd_app_, nullptr, FALSE);
    } else {
      int thv = -1;
      for (size_t z = 0; z < nodes_.size(); ++z) {
        if (std::abs(posX - (15 + (int)(nodes_[z].fraction * wide))) <= 6) {
          thv = (int)z;
          break;
        }
      }
      if (thv != idx_hover_) {
        idx_hover_ = thv;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
  } else if (msType == WM_RBUTTONDOWN && nodes_.size() > 2) {
    for (size_t z = 0; z < nodes_.size(); ++z) {
      if (std::abs(posX - (15 + (int)(nodes_[z].fraction * wide))) <= 6) {
        nodes_.erase(nodes_.begin() + z);
        BuildLookupSequence();
        InvalidateRect(hwnd, nullptr, FALSE);
        InvalidateRect(wnd_app_, nullptr, FALSE);
        break;
      }
    }
  } else if (msType == WM_LBUTTONDBLCLK) {
    float stp = std::max(0.0f, std::min(1.0f, (float)(posX - 15) / wide));
    DWORD cl = lut_cache_[(int)(stp * 1023)];
    nodes_.push_back({stp, RGB(GetBValue(cl), GetGValue(cl), GetRValue(cl))});
    std::sort(nodes_.begin(), nodes_.end());
    BuildLookupSequence();
    InvalidateRect(hwnd, nullptr, FALSE);
    InvalidateRect(wnd_app_, nullptr, FALSE);
  }
}

// Handler paska na dole gradientu
LRESULT Win32GradientApp::ProcessTrack(HWND hwnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lParam) {
  if (uMsg == WM_ERASEBKGND)
    return 1;
  else if (uMsg == WM_PAINT) {
    OnTrackPaint(hwnd);
    return 0;
  } else if (uMsg == WM_MOUSELEAVE) {
    idx_hover_ = -1;
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  } else if (uMsg == WM_LBUTTONUP) {
    if (idx_drag_ != -1) {
      if (!was_shifted_)
        ActivateColorDlg(idx_drag_);
      std::sort(nodes_.begin(), nodes_.end());
      idx_drag_ = -1;
      ReleaseCapture();
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  } else if (uMsg == WM_LBUTTONDOWN || uMsg == WM_MOUSEMOVE ||
             uMsg == WM_RBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) {
    OnTrackMouse(hwnd, uMsg, wParam, lParam);
    return 0;
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============== GIMP COLOR TOOL ============== //

HHOOK GimpColorTool::hook_ptr_ = NULL;
GimpColorTool *GimpColorTool::inst_active_ = nullptr;

enum {
  UI_H = 3000,
  UI_S,
  UI_V,
  UI_R,
  UI_G,
  UI_B,
  BTN_RST,
  BTN_PIP,
  BTN_ACPT,
  BTN_DECL
};

// Algorytm transformacji przestrzeni barw z klasycznego RGB na składowe formatu HSV (Hue, Saturation, Value).
// Dokonuje podziału by poprawie ulokować kolor na kole dialogowym Color Pickera.
void GimpColorTool::CvtRGB2HSV(int r, int g, int b, float &th, float &ts,
                               float &tv) {
  float rd = r / 255.0f, gd = g / 255.0f, bd = b / 255.0f;
  float peak = std::max({rd, gd, bd}), pit = std::min({rd, gd, bd});
  float span = peak - pit;
  tv = peak;
  ts = (peak > 0) ? (span / peak) : 0;
  if (span == 0)
    th = 0;
  else if (peak == rd)
    th = fmod(((gd - bd) / span), 6.0f);
  else if (peak == gd)
    th = ((bd - rd) / span) + 2.0f;
  else if (peak == bd)
    th = ((rd - gd) / span) + 4.0f;
  th *= 60.0f;
  if (th < 0)
    th += 360.0f;
}

// Wykonuje konwersję odwrotną - mapuje ułamkową przestrzeń HSV bezpośrednio na kanały RGB Win32.
// Interpoluje kolory w sześciu skali wektora obrębu kątów odcienia (skali 360).
void GimpColorTool::CvtHSV2RGB(float th, float ts, float tv, int &r, int &g,
                               int &b) {
  float chroma = tv * ts;
  float mx = chroma * (1.0f - std::abs(fmod(th / 60.0f, 2.0f) - 1.0f));
  float add = tv - chroma;
  float rr = 0, gg = 0, bb = 0;

  if (th < 60) {
    rr = chroma;
    gg = mx;
  } else if (th < 120) {
    rr = mx;
    gg = chroma;
  } else if (th < 180) {
    gg = chroma;
    bb = mx;
  } else if (th < 240) {
    gg = mx;
    bb = chroma;
  } else if (th < 300) {
    rr = mx;
    bb = chroma;
  } else {
    rr = chroma;
    bb = mx;
  }

  r = (int)((rr + add) * 255);
  g = (int)((gg + add) * 255);
  b = (int)((bb + add) * 255);
}

// Funkcja globalna skanujaca pulpity
LRESULT CALLBACK GimpColorTool::GrabHook(int nC, WPARAM wP, LPARAM lP) {
  if (nC >= 0 && wP == WM_LBUTTONDOWN && inst_active_ &&
      inst_active_->scr_pick_mode_) {
    MSLLHOOKSTRUCT *meta = (MSLLHOOKSTRUCT *)lP;
    HDC dsp = GetDC(NULL);
    inst_active_->curr_color_ = GetPixel(dsp, meta->pt.x, meta->pt.y);
    ReleaseDC(NULL, dsp);

    inst_active_->scr_pick_mode_ = false;
    SetWindowTextW(inst_active_->wnd_self_, L"GIMP Color Picker");

    int R = GetRValue(inst_active_->curr_color_);
    int G = GetGValue(inst_active_->curr_color_);
    int B = GetBValue(inst_active_->curr_color_);
    inst_active_->CvtRGB2HSV(R, G, B, inst_active_->hue_ang_,
                             inst_active_->sat_amt_, inst_active_->val_amt_);

    SendMessage(inst_active_->wnd_self_, WM_APP + 7, 0, 0);
    InvalidateRect(inst_active_->wnd_self_, nullptr, FALSE);
    return 1;
  }
  return CallNextHookEx(hook_ptr_, nC, wP, lP);
}

// Obfuskacja okna modalnego Win32
bool GimpColorTool::RunModal(HINSTANCE inst, HWND hParent, COLORREF &io_rgb) {
  INITCOMMONCONTROLSEX mx = {sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
  InitCommonControlsEx(&mx);

  base_color_ = io_rgb;
  curr_color_ = io_rgb;
  CvtRGB2HSV(GetRValue(curr_color_), GetGValue(curr_color_),
             GetBValue(curr_color_), hue_ang_, sat_amt_, val_amt_);
  inst_active_ = this;

  WNDCLASSEXW wca{};
  if (GetClassInfoExW(inst, L"DlgCbx32", &wca) == 0) {
    wca = {sizeof(WNDCLASSEXW),
           0,
           DlgProcBridge,
           0,
           0,
           inst,
           nullptr,
           LoadCursorW(nullptr, IDC_ARROW),
           (HBRUSH)(COLOR_BTNFACE + 1),
           nullptr,
           L"DlgCbx32",
           nullptr};
    RegisterClassExW(&wca);
  }

  EnableWindow(hParent, FALSE);

  wnd_self_ = CreateWindowExW(
      WS_EX_DLGMODALFRAME, L"DlgCbx32", L"GIMP Color Picker",
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
      CW_USEDEFAULT, 690, 420, hParent, nullptr, inst, this);

  hook_ptr_ =
      SetWindowsHookExW(WH_MOUSE_LL, GrabHook, GetModuleHandle(NULL), 0);

  SetActiveWindow(wnd_self_);
  SetForegroundWindow(wnd_self_);
  SetFocus(wnd_self_);

  MSG spin;
  bool pass = false;
  while (IsWindow(wnd_self_) && GetMessageW(&spin, nullptr, 0, 0)) {
    if (spin.message == WM_KEYDOWN && spin.wParam == VK_RETURN) {
      pass = true;
      break;
    }
    if (spin.message == WM_KEYDOWN && spin.wParam == VK_ESCAPE) {
      break;
    }
    if (!IsDialogMessage(wnd_self_, &spin)) {
      TranslateMessage(&spin);
      DispatchMessageW(&spin);
    }
  }

  if (hook_ptr_)
    UnhookWindowsHookEx(hook_ptr_);
  hook_ptr_ = nullptr;
  inst_active_ = nullptr;
  EnableWindow(hParent, TRUE);
  SetActiveWindow(hParent);
  SetForegroundWindow(hParent);
  DestroyWindow(wnd_self_);
  if (pass)
    io_rgb = curr_color_;
  return pass;
}

// Tworzy rzut szaty graficznej "kółka i trójkąta" w okienku Color Pickera używając matematycznego renderowania trójkąta.
// Wykorzystuje współrzędne barycentryczne trójkąta dla precyzyjnego mapowania koordynat S/V w obrębie danego Hue.
void GimpColorTool::DrawGraphic(HDC mem) {
  int bw = 320, bh = 350;
  DWORD null_col = GetSysColor(COLOR_BTNFACE);
  float center = 160.0f, ROut = 140.0f, RIn = 110.0f;

  if (!has_cache_) {
    wheel_cache_.assign(bw * bh, null_col);
    for (int pIdx = 0; pIdx < bw * bh; ++pIdx) {
      int cx = pIdx % bw, cy = pIdx / bw;
      float mx = cx - center, my = center - cy;
      float len = std::sqrt(mx * mx + my * my);

      if (len > RIn && len < ROut) {
        float theta = std::atan2(my, mx) * 180.0f / (float)MATH_PI;
        if (theta < 0)
          theta += 360.0f;
        int R, G, B;
        CvtHSV2RGB(theta, 1.0f, 1.0f, R, G, B);
        wheel_cache_[pIdx] = RGB(B, G, R);
      }
    }
    has_cache_ = true;
  }

  pixels_buf_ = wheel_cache_;

  // Barycentric implementation
  float hRad = hue_ang_ * (float)MATH_PI / 180.0f;
  float qx = center + RIn * std::cos(hRad), qy = center - RIn * std::sin(hRad);
  float wx = center + RIn * std::cos(hRad + 2.0f * (float)MATH_PI / 3.0f),
        wy = center - RIn * std::sin(hRad + 2.0f * (float)MATH_PI / 3.0f);
  float ex = center + RIn * std::cos(hRad - 2.0f * (float)MATH_PI / 3.0f),
        ey = center - RIn * std::sin(hRad - 2.0f * (float)MATH_PI / 3.0f);

  float div = (wy - ey) * (qx - ex) + (ex - wx) * (qy - ey);
  int mnx = std::max(0, (int)std::min({qx, wx, ex}) - 1);
  int mxx = std::min(bw, (int)std::max({qx, wx, ex}) + 2);
  int mny = std::max(0, (int)std::min({qy, wy, ey}) - 1);
  int mxy = std::min(bh, (int)std::max({qy, wy, ey}) + 2);

  for (int yR = mny; yR < mxy; ++yR) {
    for (int xC = mnx; xC < mxx; ++xC) {
      float pfX = (float)xC, pfY = (float)yR;
      float fA = ((wy - ey) * (pfX - ex) + (ex - wx) * (pfY - ey)) / div;
      float fB = ((ey - qy) * (pfX - ex) + (qx - ex) * (pfY - ey)) / div;
      float fC = 1.0f - fA - fB;

      if (fA >= -0.01f && fB >= -0.01f && fC >= -0.01f) {
        fA = std::clamp(fA, 0.0f, 1.0f);
        fB = std::clamp(fB, 0.0f, 1.0f);
        float val_comp = fA + fB;
        float sat_comp = (val_comp > 1e-4f) ? (fA / val_comp) : 0.0f;

        int cR, cG, cB;
        CvtHSV2RGB(hue_ang_, sat_comp, val_comp, cR, cG, cB);
        pixels_buf_[yR * bw + xC] = RGB(cB, cG, cR);
      }
    }
  }

  BITMAPINFO bmiSpec = {0};
  bmiSpec.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmiSpec.bmiHeader.biWidth = bw;
  bmiSpec.bmiHeader.biHeight = -bh;
  bmiSpec.bmiHeader.biPlanes = 1;
  bmiSpec.bmiHeader.biBitCount = 32;
  bmiSpec.bmiHeader.biCompression = BI_RGB;
  StretchDIBits(mem, 0, 0, bw, bh, 0, 0, bw, bh, pixels_buf_.data(), &bmiSpec,
                DIB_RGB_COLORS, SRCCOPY);

  // Helpers dla kolecek
  auto mark_circ = [&](float zx, float zy, int rad) {
    HPEN sp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ ps = SelectObject(mem, sp);
    HGDIOBJ bs = SelectObject(mem, GetStockObject(NULL_BRUSH));
    Ellipse(mem, (int)(zx - rad), (int)(zy - rad), (int)(zx + rad),
            (int)(zy + rad));
    SelectObject(mem, ps);
    SelectObject(mem, bs);
    DeleteObject(sp);
  };

  mark_circ(center + (RIn + 15.0f) * std::cos(hRad),
            center - (RIn + 15.0f) * std::sin(hRad), 6);
  float tA = sat_amt_ * val_amt_, tB = val_amt_ - tA, tC = 1.0f - val_amt_;
  mark_circ(tA * qx + tB * wx + tC * ex, tA * qy + tB * wy + tC * ey, 5);
}

LRESULT GimpColorTool::DlgProcBridge(HWND hw, UINT uM, WPARAM wP, LPARAM lP) {
  GimpColorTool *cpl =
      (uM == WM_NCCREATE)
          ? static_cast<GimpColorTool *>(
                reinterpret_cast<LPCREATESTRUCTW>(lP)->lpCreateParams)
          : reinterpret_cast<GimpColorTool *>(
                GetWindowLongPtrW(hw, GWLP_USERDATA));
  if (uM == WM_NCCREATE)
    SetWindowLongPtrW(hw, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cpl));
  return cpl ? cpl->MsgHandler(hw, uM, wP, lP) : DefWindowProcW(hw, uM, wP, lP);
}

// Procedura handlera w dialogu
LRESULT GimpColorTool::MsgHandler(HWND hw, UINT uM, WPARAM wP, LPARAM lP) {
  auto SyncScales = [&]() {
    in_sync_ = true;
    for (int i = 0; i < 6; ++i) {
      int val = 0;
      if (i == 0) val = (int)hue_ang_;
      else if (i == 1) val = (int)(sat_amt_ * 100);
      else if (i == 2) val = (int)(val_amt_ * 100);
      else if (i == 3) val = GetRValue(curr_color_);
      else if (i == 4) val = GetGValue(curr_color_);
      else if (i == 5) val = GetBValue(curr_color_);

      SendMessage(GetDlgItem(hw, UI_H + i), TBM_SETPOS, TRUE, (LPARAM)val);
      wchar_t buf[16];
      swprintf(buf, 16, L"%d", val);
      SetWindowTextW(edit_ctrls_[i], buf);
    }
    in_sync_ = false;
  };

  if (uM == WM_CREATE) {
    int oX = 330, oY = 20, step = 40;
    const wchar_t *tags[] = {L"H", L"S", L"V", L"R", L"G", L"B"};
    int ceilData[] = {360, 100, 100, 255, 255, 255};

    for (int i = 0; i < 6; ++i) {
      CreateWindowExW(0, L"STATIC", tags[i], WS_CHILD | WS_VISIBLE, oX,
                       oY + i * step, 20, 20, hw, nullptr, nullptr, nullptr);
      HWND trg = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr,
                                 WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | WS_TABSTOP, oX + 20,
                                 oY + i * step - 5, 200, 30, hw,
                                 (HMENU)(INT_PTR)(UI_H + i), nullptr, nullptr);
      SendMessage(trg, TBM_SETRANGE, TRUE, MAKELPARAM(0, ceilData[i]));
      edit_ctrls_[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                                       WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                                       oX + 225, oY + i * step - 3, 40, 24, hw,
                                       (HMENU)(INT_PTR)(UI_H + 10 + i), nullptr, nullptr);
    }

    CreateWindowExW(0, L"BUTTON", L"Pipette (Screen)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    oX, 270, 120, 30, hw, (HMENU)BTN_PIP, nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Reset to Old", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    oX + 130, 270, 100, 30, hw, (HMENU)BTN_RST, nullptr,
                    nullptr);
    CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, oX, 330, 100,
                    35, hw, (HMENU)BTN_ACPT, nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, oX + 110,
                    330, 100, 35, hw, (HMENU)BTN_DECL, nullptr, nullptr);
    SyncScales();
    return 0;
  } else if (uM == WM_APP + 7) {
    SyncScales();
    return 0;
  } else if (uM == WM_PAINT) {
    PAINTSTRUCT psi;
    HDC drawDev = BeginPaint(hw, &psi);
    RECT rcd;
    GetClientRect(hw, &rcd);
    HDC pOff = CreateCompatibleDC(drawDev);
    HBITMAP mapBmp = CreateCompatibleBitmap(drawDev, rcd.right, rcd.bottom);
    SelectObject(pOff, mapBmp);
    FillRect(pOff, &rcd, (HBRUSH)(COLOR_BTNFACE + 1));

    DrawGraphic(pOff);

    SetBkMode(pOff, TRANSPARENT);
    RECT rxn = {610, 20, 660, 130}, rxo = {610, 130, 660, 240};
    HBRUSH brN = CreateSolidBrush(curr_color_);
    FillRect(pOff, &rxn, brN);
    DeleteObject(brN);
    HBRUSH brO = CreateSolidBrush(base_color_);
    FillRect(pOff, &rxo, brO);
    DeleteObject(brO);

    TextOutW(pOff, 615, 25, L"New", 3);
    TextOutW(pOff, 615, 135, L"Old", 3);
    BitBlt(drawDev, 0, 0, rcd.right, rcd.bottom, pOff, 0, 0, SRCCOPY);
    DeleteObject(mapBmp);
    DeleteDC(pOff);
    EndPaint(hw, &psi);
    return 0;
  } else if (uM == WM_HSCROLL) {
    HWND trg = (HWND)lP;
    int idt = GetDlgCtrlID(trg);
    int pxl = (int)SendMessage(trg, TBM_GETPOS, 0, 0);
    if (idt >= UI_H && idt <= UI_V) {
      if (idt == UI_H)
        hue_ang_ = (float)pxl;
      if (idt == UI_S)
        sat_amt_ = pxl / 100.0f;
      if (idt == UI_V)
        val_amt_ = pxl / 100.0f;
      int r, g, b;
      CvtHSV2RGB(hue_ang_, sat_amt_, val_amt_, r, g, b);
      curr_color_ = RGB(r, g, b);
    } else if (idt >= UI_R && idt <= UI_B) {
      int r = GetRValue(curr_color_), g = GetGValue(curr_color_),
          b = GetBValue(curr_color_);
      if (idt == UI_R)
        r = pxl;
      if (idt == UI_G)
        g = pxl;
      if (idt == UI_B)
        b = pxl;
      curr_color_ = RGB(r, g, b);
      CvtRGB2HSV(r, g, b, hue_ang_, sat_amt_, val_amt_);
    }
    SyncScales();
    InvalidateRect(hw, nullptr, FALSE);
    return 0;
  } else if (uM == WM_COMMAND) {
    int wid = LOWORD(wP);
    int code = HIWORD(wP);
    if (code == EN_CHANGE && !in_sync_) {
      for (int i = 0; i < 6; ++i) {
        if (wid == UI_H + 10 + i) {
          wchar_t buf[16];
          GetWindowTextW(edit_ctrls_[i], buf, 16);
          int val = _wtoi(buf);
          if (i == 0) hue_ang_ = (float)std::clamp(val, 0, 360);
          else if (i == 1) sat_amt_ = std::clamp(val, 0, 100) / 100.0f;
          else if (i == 2) val_amt_ = std::clamp(val, 0, 100) / 100.0f;
          
          if (i < 3) {
            int r, g, b;
            CvtHSV2RGB(hue_ang_, sat_amt_, val_amt_, r, g, b);
            curr_color_ = RGB(r, g, b);
          } else {
            int r = GetRValue(curr_color_), g = GetGValue(curr_color_), b = GetBValue(curr_color_);
            if (i == 3) r = std::clamp(val, 0, 255);
            else if (i == 4) g = std::clamp(val, 0, 255);
            else if (i == 5) b = std::clamp(val, 0, 255);
            curr_color_ = RGB(r, g, b);
            CvtRGB2HSV(r, g, b, hue_ang_, sat_amt_, val_amt_);
          }
          SyncScales();
          InvalidateRect(hw, nullptr, FALSE);
          break;
        }
      }
    }
    if (wid == BTN_RST) {
      curr_color_ = base_color_;
      CvtRGB2HSV(GetRValue(curr_color_), GetGValue(curr_color_),
                 GetBValue(curr_color_), hue_ang_, sat_amt_, val_amt_);
      SyncScales();
      InvalidateRect(hw, nullptr, FALSE);
    } else if (wid == BTN_PIP) {
      scr_pick_mode_ = true;
      SetWindowTextW(hw, L"Click screen surface!");
    } else if (wid == BTN_ACPT)
      PostMessage(hw, WM_KEYDOWN, VK_RETURN, 0);
    else if (wid == BTN_DECL)
      PostMessage(hw, WM_KEYDOWN, VK_ESCAPE, 0);
    return 0;
  } else if (uM == WM_LBUTTONDOWN) {
    int xPos = GET_X_LPARAM(lP), yPos = GET_Y_LPARAM(lP);
    if (xPos > 320)
      return 0;
    float h_diff = xPos - 160.0f, v_diff = 160.0f - yPos;
    float lng = std::sqrt(h_diff * h_diff + v_diff * v_diff);

    if (lng > 110.0f && lng < 140.0f) {
      drag_wheel_ = true;
      SetCapture(hw);
    } else if (lng < 110.0f) {
      drag_tri_ = true;
      SetCapture(hw);
    }
    return 0;
  } else if (uM == WM_MOUSEMOVE) {
    if (drag_wheel_ || drag_tri_) {
      int cx = GET_X_LPARAM(lP), cy = GET_Y_LPARAM(lP);
      if (drag_wheel_) {
        hue_ang_ =
            std::atan2(160.0f - cy, cx - 160.0f) * 180.0f / (float)MATH_PI;
        if (hue_ang_ < 0)
          hue_ang_ += 360.0f;
        // Add saturation bounce-back for black
        if (sat_amt_ < 0.01f && val_amt_ < 0.01f) {
          sat_amt_ = 1.0f;
          val_amt_ = 1.0f;
        }
      } else {
        float hRad = hue_ang_ * (float)MATH_PI / 180.0f;
        float qx = 160.0f + 110.0f * std::cos(hRad),
              qy = 160.0f - 110.0f * std::sin(hRad);
        float wx = 160.0f +
                   110.0f * std::cos(hRad + 2.0f * (float)MATH_PI / 3.0f),
              wy = 160.0f -
                   110.0f * std::sin(hRad + 2.0f * (float)MATH_PI / 3.0f);
        float ex = 160.0f +
                   110.0f * std::cos(hRad - 2.0f * (float)MATH_PI / 3.0f),
              ey = 160.0f -
                   110.0f * std::sin(hRad - 2.0f * (float)MATH_PI / 3.0f);

        float dpX = (float)cx, dpY = (float)cy;
        float bottom = (wy - ey) * (qx - ex) + (ex - wx) * (qy - ey);
        float mX = std::max(
            0.0f, ((wy - ey) * (dpX - ex) + (ex - wx) * (dpY - ey)) / bottom);
        float mY = std::max(
            0.0f, ((ey - qy) * (dpX - ex) + (qx - ex) * (dpY - ey)) / bottom);
        float mZ = std::max(0.0f, 1.0f - mX - mY);

        float total = mX + mY + mZ;
        mX /= total;
        mY /= total;
        val_amt_ = mX + mY;
        sat_amt_ = (val_amt_ > 1e-4f) ? (mX / val_amt_) : 0.0f;
      }
      int r, g, b;
      CvtHSV2RGB(hue_ang_, sat_amt_, val_amt_, r, g, b);
      curr_color_ = RGB(r, g, b);
      SyncScales();
      InvalidateRect(hw, nullptr, FALSE);
    }
    return 0;
  } else if (uM == WM_LBUTTONUP) {
    drag_wheel_ = drag_tri_ = false;
    ReleaseCapture();
    return 0;
  } else if (uM == WM_CLOSE) {
    DestroyWindow(hw);
    return 0;
  }

  return DefWindowProcW(hw, uM, wP, lP);
}
