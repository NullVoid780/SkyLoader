struct UiFonts {
  HFONT body;
  HFONT bodySm;
  HFONT small;
  HFONT tiny;
  HFONT semibold;
  HFONT semiboldSm;
  HFONT h1;
  HFONT h2;
  HFONT h3;
  HFONT button;
  HFONT stat;
  HFONT logo;
  HFONT italic;
  HFONT mono;
  HFONT icon;
  HFONT iconSm;
  HFONT iconMd;
  HFONT iconLg;
  HFONT iconXl;
};

UiFonts gFonts{};
HDC gMeasureDC = nullptr;

constexpr wchar_t GLYPH_HOME = 0xE80F;
constexpr wchar_t GLYPH_SETTINGS = 0xE713;
constexpr wchar_t GLYPH_REFRESH = 0xE72C;
constexpr wchar_t GLYPH_ADD = 0xE710;
constexpr wchar_t GLYPH_FOLDER = 0xE8B7;
constexpr wchar_t GLYPH_PLAY = 0xE768;
constexpr wchar_t GLYPH_SUN = 0xE706;
constexpr wchar_t GLYPH_MOON = 0xE708;
constexpr wchar_t GLYPH_CHEVRON_RIGHT = 0xE76C;
constexpr wchar_t GLYPH_CHEVRON_DOWN = 0xE70D;
constexpr wchar_t GLYPH_MINIMIZE = 0xE921;
constexpr wchar_t GLYPH_MAXIMIZE = 0xE922;
constexpr wchar_t GLYPH_RESTORE = 0xE923;
constexpr wchar_t GLYPH_CLOSE = 0xE8BB;
constexpr wchar_t GLYPH_CHECK = 0xE73E;
constexpr wchar_t GLYPH_WARNING = 0xE7BA;
constexpr wchar_t GLYPH_INFO = 0xE946;
constexpr wchar_t GLYPH_MORE = 0xE712;
constexpr wchar_t GLYPH_SYNC = 0xE895;
constexpr wchar_t GLYPH_BOLT = 0xE945;
constexpr wchar_t GLYPH_BULB = 0xEA80;
constexpr wchar_t GLYPH_HEART = 0xEB51;
constexpr wchar_t GLYPH_GAME = 0xE7FC;
constexpr wchar_t GLYPH_STAR = 0xE734;
constexpr wchar_t GLYPH_GLOBE = 0xE774;
constexpr wchar_t GLYPH_DELETE = 0xE72D;
constexpr wchar_t GLYPH_DOWNLOAD = 0xE896;
constexpr wchar_t GLYPH_PUZZLE = 0xEA86;
constexpr wchar_t GLYPH_HISTORY = 0xE81C;
constexpr wchar_t GLYPH_SEARCH = 0xE721;
constexpr wchar_t GLYPH_LINK = 0xE71B;
constexpr wchar_t GLYPH_SHOP = 0xE719;
constexpr wchar_t GLYPH_SAVE = 0xE74E;
constexpr wchar_t GLYPH_PALETTE = 0xE790;
constexpr wchar_t GLYPH_CODE = 0xE943;

struct TextCmd {
  std::wstring value;
  RECT box;
  HFONT font;
  COLORREF color;
  UINT format;
  bool clipped;
  RECT clip;
};

struct IconCmd {
  RECT box;
};

std::vector<TextCmd> gTexts;
std::vector<IconCmd> gIconDraws;
HICON gAppIcon = nullptr;
RECT gTextClip{};
bool gTextClipActive = false;

Gdiplus::Color gpColor(COLORREF color, BYTE alpha = 255) {
  return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

Gdiplus::RectF gpRect(const RECT& rect) {
  return Gdiplus::RectF(static_cast<Gdiplus::REAL>(rect.left),
                        static_cast<Gdiplus::REAL>(rect.top),
                        static_cast<Gdiplus::REAL>(rect.right - rect.left),
                        static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
}

void makeRoundPath(Gdiplus::GraphicsPath& path, const RECT& rect, int radius) {
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  int diameter = radius * 2;
  if (diameter > width) diameter = width;
  if (diameter > height) diameter = height;
  if (diameter <= 1) {
    path.AddRectangle(gpRect(rect));
    return;
  }
  path.AddArc(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top),
              static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 180.0f, 90.0f);
  path.AddArc(static_cast<Gdiplus::REAL>(rect.right - diameter), static_cast<Gdiplus::REAL>(rect.top),
              static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 270.0f, 90.0f);
  path.AddArc(static_cast<Gdiplus::REAL>(rect.right - diameter), static_cast<Gdiplus::REAL>(rect.bottom - diameter),
              static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 0.0f, 90.0f);
  path.AddArc(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.bottom - diameter),
              static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 90.0f, 90.0f);
  path.CloseFigure();
}

void fillRound(Gdiplus::Graphics& graphics, const RECT& rect, int radius, COLORREF color, BYTE alpha = 255) {
  Gdiplus::GraphicsPath path;
  makeRoundPath(path, rect, radius);
  Gdiplus::SolidBrush brush(gpColor(color, alpha));
  graphics.FillPath(&brush, &path);
}

void strokeRound(Gdiplus::Graphics& graphics, const RECT& rect, int radius, COLORREF color,
                 BYTE alpha = 255, float width = 1.0f) {
  Gdiplus::GraphicsPath path;
  makeRoundPath(path, rect, radius);
  Gdiplus::Pen pen(gpColor(color, alpha), width);
  graphics.DrawPath(&pen, &path);
}

void fillRoundGradient(Gdiplus::Graphics& graphics, const RECT& rect, int radius,
                       COLORREF top, COLORREF bottom) {
  Gdiplus::GraphicsPath path;
  makeRoundPath(path, rect, radius);
  Gdiplus::GraphicsState state = graphics.Save();
  graphics.SetClip(&path, Gdiplus::CombineModeIntersect);
  Gdiplus::LinearGradientBrush brush(gpRect(rect), gpColor(top), gpColor(bottom),
                                     Gdiplus::LinearGradientModeVertical);
  graphics.FillRectangle(&brush, gpRect(rect));
  graphics.Restore(state);
}

void fillRect(Gdiplus::Graphics& graphics, const RECT& rect, COLORREF color, BYTE alpha = 255) {
  Gdiplus::SolidBrush brush(gpColor(color, alpha));
  graphics.FillRectangle(&brush, gpRect(rect));
}

void fillCircle(Gdiplus::Graphics& graphics, int centerX, int centerY, int radius, COLORREF color,
                BYTE alpha = 255) {
  Gdiplus::SolidBrush brush(gpColor(color, alpha));
  graphics.FillEllipse(&brush, centerX - radius, centerY - radius, radius * 2, radius * 2);
}

void strokeCircle(Gdiplus::Graphics& graphics, int centerX, int centerY, int radius, COLORREF color,
                  BYTE alpha = 255, float width = 1.0f) {
  Gdiplus::Pen pen(gpColor(color, alpha), width);
  graphics.DrawEllipse(&pen, centerX - radius, centerY - radius, radius * 2, radius * 2);
}

void drawLine(Gdiplus::Graphics& graphics, int x1, int y1, int x2, int y2, COLORREF color,
              BYTE alpha = 255, float width = 1.0f) {
  Gdiplus::Pen pen(gpColor(color, alpha), width);
  graphics.DrawLine(&pen, x1, y1, x2, y2);
}

void addText(const std::wstring& value, const RECT& box, HFONT font, COLORREF color,
             UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE, const RECT* clip = nullptr) {
  if (value.empty()) return;
  const RECT* effectiveClip = clip;
  if (!effectiveClip && gTextClipActive) effectiveClip = &gTextClip;
  TextCmd command{value, box, font, color, format | DT_NOPREFIX,
                  effectiveClip != nullptr, {}};
  if (effectiveClip) command.clip = *effectiveClip;
  gTexts.push_back(command);
}

void addGlyph(wchar_t glyph, const RECT& box, HFONT font, COLORREF color,
              UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE, const RECT* clip = nullptr) {
  addText(std::wstring(1, glyph), box, font, color, format, clip);
}

int measureText(HFONT font, const std::wstring& value) {
  HGDIOBJ previous = SelectObject(gMeasureDC, font);
  SIZE size{};
  GetTextExtentPoint32W(gMeasureDC, value.c_str(), static_cast<int>(value.size()), &size);
  SelectObject(gMeasureDC, previous);
  return size.cx;
}

void flushText(HDC dc) {
  SetBkMode(dc, TRANSPARENT);
  for (const TextCmd& item : gTexts) {
    int saved = 0;
    if (item.clipped) {
      saved = SaveDC(dc);
      IntersectClipRect(dc, item.clip.left, item.clip.top, item.clip.right, item.clip.bottom);
    }
    HGDIOBJ previous = SelectObject(dc, item.font);
    SetTextColor(dc, item.color);
    RECT box = item.box;
    DrawTextW(dc, item.value.c_str(), -1, &box, item.format);
    SelectObject(dc, previous);
    if (item.clipped) RestoreDC(dc, saved);
  }
  gTexts.clear();
}

void flushIcons(HDC dc) {
  for (const IconCmd& item : gIconDraws) {
    const int size = (std::min)(item.box.right - item.box.left, item.box.bottom - item.box.top);
    DrawIconEx(dc, item.box.left, item.box.top, gAppIcon, size, size, 0, nullptr, DI_NORMAL);
  }
  gIconDraws.clear();
}

void addAppIcon(const RECT& box) {
  gIconDraws.push_back({box});
}

HFONT createUiFont(int pixels, int weight, bool italic = false, const wchar_t* family = L"Segoe UI") {
  return CreateFontW(-MulDiv(pixels, gDpi, 96), 0, 0, 0, weight, italic ? TRUE : FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, family);
}

void createUiFonts() {
  gFonts.body = createUiFont(14, FW_NORMAL);
  gFonts.bodySm = createUiFont(13, FW_NORMAL);
  gFonts.small = createUiFont(12, FW_NORMAL);
  gFonts.tiny = createUiFont(11, FW_NORMAL);
  gFonts.semibold = createUiFont(14, FW_SEMIBOLD);
  gFonts.semiboldSm = createUiFont(12, FW_SEMIBOLD);
  gFonts.h1 = createUiFont(32, FW_BOLD);
  gFonts.h2 = createUiFont(24, FW_BOLD);
  gFonts.h3 = createUiFont(17, FW_BOLD);
  gFonts.button = createUiFont(13, FW_SEMIBOLD);
  gFonts.stat = createUiFont(26, FW_BOLD);
  gFonts.logo = createUiFont(19, FW_BOLD);
  gFonts.italic = createUiFont(13, FW_NORMAL, true);
  gFonts.mono = createUiFont(12, FW_NORMAL, false, L"Consolas");
  gFonts.icon = createUiFont(15, FW_NORMAL, false, L"Segoe MDL2 Assets");
  gFonts.iconSm = createUiFont(10, FW_NORMAL, false, L"Segoe MDL2 Assets");
  gFonts.iconMd = createUiFont(17, FW_NORMAL, false, L"Segoe MDL2 Assets");
  gFonts.iconLg = createUiFont(21, FW_NORMAL, false, L"Segoe MDL2 Assets");
  gFonts.iconXl = createUiFont(27, FW_NORMAL, false, L"Segoe MDL2 Assets");
}

void destroyUiFonts() {
  HFONT* fonts[] = {&gFonts.body, &gFonts.bodySm, &gFonts.small, &gFonts.tiny, &gFonts.semibold,
                    &gFonts.semiboldSm, &gFonts.h1, &gFonts.h2, &gFonts.h3, &gFonts.button,
                    &gFonts.stat, &gFonts.logo, &gFonts.italic, &gFonts.mono, &gFonts.icon,
                    &gFonts.iconSm, &gFonts.iconMd, &gFonts.iconLg, &gFonts.iconXl};
  for (HFONT* font : fonts) {
    if (*font) {
      DeleteObject(*font);
      *font = nullptr;
    }
  }
}

struct AccentStyle {
  COLORREF foreground;
  COLORREF background;
  wchar_t glyph;
};

AccentStyle accentForIndex(size_t index) {
  const AccentStyle darkStyles[] = {
    {RGB(255, 110, 120), RGB(58, 23, 29), GLYPH_PUZZLE},
    {RGB(130, 166, 248), RGB(22, 37, 63), GLYPH_GAME},
    {RGB(255, 150, 195), RGB(57, 26, 38), GLYPH_HEART},
    {RGB(255, 178, 101), RGB(57, 37, 18), GLYPH_BOLT},
    {RGB(92, 226, 202), RGB(15, 48, 46), GLYPH_GLOBE},
    {RGB(183, 159, 255), RGB(39, 30, 64), GLYPH_STAR},
  };
  const AccentStyle lightStyles[] = {
    {RGB(225, 29, 46), RGB(253, 235, 238), GLYPH_PUZZLE},
    {RGB(37, 99, 235), RGB(235, 244, 255), GLYPH_GAME},
    {RGB(219, 39, 119), RGB(253, 236, 245), GLYPH_HEART},
    {RGB(234, 115, 0), RGB(255, 244, 229), GLYPH_BOLT},
    {RGB(13, 148, 136), RGB(230, 250, 247), GLYPH_GLOBE},
    {RGB(124, 58, 237), RGB(243, 238, 255), GLYPH_STAR},
  };
  const AccentStyle* styles = gTheme->dark ? darkStyles : lightStyles;
  return styles[index % 6];
}
