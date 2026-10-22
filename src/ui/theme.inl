struct Theme {
  bool dark;
  COLORREF bg;
  COLORREF sidebar;
  COLORREF card;
  COLORREF cardAlt;
  COLORREF cardHover;
  COLORREF border;
  COLORREF borderSoft;
  COLORREF text;
  COLORREF muted;
  COLORREF dim;
  COLORREF accent;
  COLORREF accentHover;
  COLORREF accentSoft;
  COLORREF accentBorder;
  COLORREF green;
  COLORREF greenBg;
  COLORREF greenBorder;
  COLORREF red;
  COLORREF redBg;
  COLORREF redBorder;
  COLORREF input;
  COLORREF inputBorder;
  COLORREF blue;
  COLORREF blueBg;
  COLORREF purple;
  COLORREF purpleBg;
  COLORREF orange;
  COLORREF orangeBg;
  COLORREF pink;
  COLORREF pinkBg;
  COLORREF teal;
  COLORREF tealBg;
};

Theme gDarkTheme{};
Theme gLightTheme{};
Theme* gTheme = &gDarkTheme;

void initThemes() {
  gDarkTheme = {
    true,
    RGB(9, 13, 18),
    RGB(11, 16, 22),
    RGB(16, 22, 30),
    RGB(13, 19, 26),
    RGB(22, 30, 40),
    RGB(30, 40, 52),
    RGB(23, 31, 41),
    RGB(238, 243, 249),
    RGB(147, 160, 177),
    RGB(101, 113, 130),
    RGB(244, 63, 75),
    RGB(255, 88, 99),
    RGB(55, 22, 28),
    RGB(116, 36, 46),
    RGB(61, 220, 151),
    RGB(15, 44, 36),
    RGB(30, 88, 70),
    RGB(255, 107, 118),
    RGB(52, 20, 25),
    RGB(118, 40, 49),
    RGB(11, 16, 22),
    RGB(34, 45, 58),
    RGB(96, 140, 240),
    RGB(21, 34, 59),
    RGB(167, 139, 250),
    RGB(38, 29, 63),
    RGB(255, 169, 77),
    RGB(56, 36, 18),
    RGB(255, 126, 157),
    RGB(57, 26, 37),
    RGB(45, 212, 191),
    RGB(15, 46, 44),
  };

  gLightTheme = {
    false,
    RGB(244, 247, 251),
    RGB(255, 255, 255),
    RGB(255, 255, 255),
    RGB(248, 250, 253),
    RGB(243, 246, 251),
    RGB(224, 230, 239),
    RGB(235, 240, 246),
    RGB(15, 23, 42),
    RGB(71, 85, 105),
    RGB(100, 116, 139),
    RGB(225, 29, 46),
    RGB(199, 18, 34),
    RGB(253, 235, 238),
    RGB(247, 189, 196),
    RGB(5, 150, 105),
    RGB(231, 248, 240),
    RGB(178, 235, 213),
    RGB(220, 38, 38),
    RGB(254, 235, 235),
    RGB(250, 195, 195),
    RGB(248, 250, 253),
    RGB(217, 224, 234),
    RGB(59, 130, 246),
    RGB(235, 244, 255),
    RGB(124, 58, 237),
    RGB(243, 238, 255),
    RGB(234, 115, 0),
    RGB(255, 244, 229),
    RGB(219, 39, 119),
    RGB(253, 236, 245),
    RGB(13, 148, 136),
    RGB(230, 250, 247),
  };
}

HBRUSH gInputBrush = nullptr;

void applyThemePalette(bool dark) {
  gDarkMode = dark;
  gTheme = dark ? &gDarkTheme : &gLightTheme;
  if (gInputBrush) DeleteObject(gInputBrush);
  gInputBrush = CreateSolidBrush(gTheme->input);
  if (gGamePathEdit) InvalidateRect(gGamePathEdit, nullptr, TRUE);
  if (gDownloadPathEdit) InvalidateRect(gDownloadPathEdit, nullptr, TRUE);
  if (gMainWindow && gThemeReady) {
    BOOL useDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(gMainWindow, 20, &useDark, sizeof(useDark));
    int corner = 2;
    DwmSetWindowAttribute(gMainWindow, 33, &corner, sizeof(corner));
    COLORREF border = gTheme->bg;
    DwmSetWindowAttribute(gMainWindow, 34, &border, sizeof(border));
    InvalidateRect(gMainWindow, nullptr, FALSE);
  }
}
