struct Hotspot {
  RECT rect;
  int id;
};

enum UiHotspot {
  UI_NONE = 0,
  UI_NAV_HOME,
  UI_NAV_PLUGINS,
  UI_NAV_MARKETPLACE,
  UI_NAV_SETTINGS,
  UI_THEME_TOGGLE,
  UI_MINIMIZE,
  UI_MAXIMIZE,
  UI_CLOSE,
  UI_LAUNCH,
  UI_HERO_SETTINGS,
  UI_STAT_PLUGINS,
  UI_STAT_ACTIVE,
  UI_STAT_UPDATES,
  UI_STAT_GAME,
  UI_QUICK_ADD,
  UI_QUICK_FOLDER,
  UI_QUICK_UPDATES,
  UI_VIEW_PLUGINS,
  UI_ADD_PLUGIN,
  UI_OPEN_FOLDER,
  UI_CHECK_UPDATES,
  UI_BROWSE_GAME,
  UI_AUTODETECT_GAME,
  UI_THEME_DARK,
  UI_THEME_LIGHT,
  UI_EMPTY_ADD,
  UI_SETTINGS_GENERAL,
  UI_SETTINGS_UPDATES,
  UI_SETTINGS_GAME,
  UI_SETTINGS_PLUGINS,
  UI_SETTINGS_DOWNLOADS,
  UI_SETTINGS_APPEARANCE,
  UI_SETTINGS_ADVANCED,
  UI_SETTINGS_SAVE,
  UI_SETTINGS_RESET,
  UI_SETTINGS_BROWSE_DOWNLOAD,
  UI_SETTINGS_OPEN_DOWNLOAD,
  UI_SETTINGS_OPEN_DATA,
  UI_SETTINGS_LANGUAGE,
  UI_TOGGLE_STARTUP,
  UI_TOGGLE_MINIMIZE,
  UI_TOGGLE_CHECK,
  UI_TOGGLE_REMEMBER,
  UI_TOGGLE_AUTOLOAD,
  UI_TOGGLE_CONFIRM,
  UI_TOGGLE_STEAM,
  UI_ROW_HOVER = 9000,
  UI_ROW_TOGGLE = 10000,
  UI_ROW_MENU = 11000,
  UI_MENU_ITEM = 15000,
};

std::vector<Hotspot> gHotspots;
int gHoverId = 0;
int gPressedId = 0;
POINT gMousePoint{};
int gMenuIndex = -1;
POINT gMenuAnchor{};
int gPluginScroll = 0;
int gPluginScrollMax = 0;
int gSettingsScroll = 0;
int gSettingsScrollMax = 0;
int gSettingsCategory = 0;
RECT gHotspotClip{};
bool gHotspotClipActive = false;
bool gMouseTracked = false;

int scale(int value) {
  return MulDiv(value, gDpi, 96);
}

int clampInt(int value, int low, int high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

bool containsPoint(const RECT& rect, int x, int y) {
  return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void addHotspot(int id, const RECT& rect) {
  RECT effective = rect;
  if (gHotspotClipActive) {
    RECT clipped{};
    if (!IntersectRect(&clipped, &rect, &gHotspotClip)) return;
    effective = clipped;
  }
  if (effective.right <= effective.left || effective.bottom <= effective.top) return;
  gHotspots.push_back({effective, id});
}

int hotspotAt(int x, int y) {
  for (auto it = gHotspots.rbegin(); it != gHotspots.rend(); ++it) {
    if (containsPoint(it->rect, x, y)) return it->id;
  }
  return UI_NONE;
}

bool isHot(int id) {
  return gHoverId == id;
}

bool isDown(int id) {
  return gPressedId == id;
}

COLORREF mixColor(COLORREF first, COLORREF second, int amount) {
  const int inverse = 255 - amount;
  return RGB((GetRValue(first) * inverse + GetRValue(second) * amount) / 255,
             (GetGValue(first) * inverse + GetGValue(second) * amount) / 255,
             (GetBValue(first) * inverse + GetBValue(second) * amount) / 255);
}

COLORREF liftColor(COLORREF color, int amount) {
  return RGB(clampInt(GetRValue(color) + amount, 0, 255),
             clampInt(GetGValue(color) + amount, 0, 255),
             clampInt(GetBValue(color) + amount, 0, 255));
}

void drawCardBg(Gdiplus::Graphics& graphics, const RECT& rect, int radius, bool hovered) {
  if (gTheme->dark) {
    const COLORREF base = hovered ? gTheme->cardHover : gTheme->card;
    fillRoundGradient(graphics, rect, radius, liftColor(base, 4), base);
  } else {
    fillRound(graphics, rect, radius, hovered ? gTheme->cardHover : gTheme->card);
  }
  strokeRound(graphics, rect, radius, hovered ? gTheme->border : gTheme->borderSoft);
}

void drawDivider(Gdiplus::Graphics& graphics, int left, int right, int y) {
  drawLine(graphics, left, y, right, y, gTheme->borderSoft);
}

void drawIconTile(Gdiplus::Graphics& graphics, const RECT& rect, const AccentStyle& style, HFONT font) {
  fillRound(graphics, rect, scale(10), style.background);
  strokeRound(graphics, rect, scale(10), style.foreground, 32);
  addGlyph(style.glyph, rect, font, style.foreground);
}

std::wstring versionLabel(const std::wstring& version) {
  if (version.empty()) return L"";
  if (version[0] >= L'0' && version[0] <= L'9') return L"v" + version;
  return version;
}

std::wstring pluginMetaLine(const PluginView& plugin) {
  std::wstring line = versionLabel(plugin.version);
  if (!plugin.author.empty()) {
    if (!line.empty()) line += L" \u2022 ";
    line += plugin.author;
  }
  if (line.empty()) line = L"Local plugin";
  return line;
}

void drawPrimaryButton(Gdiplus::Graphics& graphics, const RECT& rect, const std::wstring& label,
                       wchar_t glyph, int id) {
  const bool hover = isHot(id);
  const bool down = isDown(id);
  const COLORREF fill = down ? mixColor(gTheme->accent, RGB(0, 0, 0), 45)
                             : (hover ? gTheme->accentHover : gTheme->accent);
  fillRound(graphics, rect, scale(9), fill);
  if (hover) strokeRound(graphics, rect, scale(9), gTheme->accentHover, 120);
  const int iconWidth = glyph ? scale(18) : 0;
  const int gap = glyph ? scale(7) : 0;
  const int contentWidth = iconWidth + gap + measureText(gFonts.button, label);
  int contentLeft = rect.left + ((rect.right - rect.left) - contentWidth) / 2;
  if (glyph) {
    RECT iconBox{contentLeft, rect.top, contentLeft + iconWidth, rect.bottom};
    addGlyph(glyph, iconBox, gFonts.icon, RGB(255, 255, 255));
    contentLeft += iconWidth + gap;
  }
  RECT textBox{contentLeft, rect.top, rect.right - scale(6), rect.bottom};
  addText(label, textBox, gFonts.button, RGB(255, 255, 255),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  addHotspot(id, rect);
}

void drawSecondaryButton(Gdiplus::Graphics& graphics, const RECT& rect, const std::wstring& label,
                         wchar_t glyph, int id) {
  const bool hover = isHot(id);
  fillRound(graphics, rect, scale(9), hover ? gTheme->cardHover : gTheme->cardAlt);
  strokeRound(graphics, rect, scale(9), hover ? gTheme->border : gTheme->borderSoft);
  const int iconWidth = glyph ? scale(18) : 0;
  const int gap = glyph ? scale(7) : 0;
  const int contentWidth = iconWidth + gap + measureText(gFonts.button, label);
  int contentLeft = rect.left + ((rect.right - rect.left) - contentWidth) / 2;
  if (glyph) {
    RECT iconBox{contentLeft, rect.top, contentLeft + iconWidth, rect.bottom};
    addGlyph(glyph, iconBox, gFonts.icon, gTheme->muted);
    contentLeft += iconWidth + gap;
  }
  RECT textBox{contentLeft, rect.top, rect.right - scale(6), rect.bottom};
  addText(label, textBox, gFonts.button, hover ? gTheme->text : gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  addHotspot(id, rect);
}

void drawStatusPill(Gdiplus::Graphics& graphics, const RECT& rect, bool enabled, int id,
                    const RECT* clip) {
  const bool hover = id && isHot(id);
  if (enabled) {
    fillRound(graphics, rect, rect.bottom - rect.top, gTheme->greenBg);
    strokeRound(graphics, rect, rect.bottom - rect.top, hover ? gTheme->green : gTheme->greenBorder);
  } else {
    fillRound(graphics, rect, rect.bottom - rect.top, hover ? gTheme->cardHover : gTheme->cardAlt);
    strokeRound(graphics, rect, rect.bottom - rect.top, gTheme->borderSoft);
  }
  RECT iconBox{rect.left + scale(8), rect.top, rect.left + scale(22), rect.bottom};
  addGlyph(enabled ? GLYPH_CHECK : GLYPH_CLOSE, iconBox, gFonts.iconSm,
           enabled ? gTheme->green : gTheme->dim, DT_CENTER | DT_VCENTER | DT_SINGLELINE, clip);
  RECT textBox = rect;
  textBox.left += scale(21);
  addText(enabled ? L"Enabled" : L"Disabled", textBox, gFonts.small,
          enabled ? gTheme->green : gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE, clip);
  if (id) addHotspot(id, rect);
}

void drawSwitch(Gdiplus::Graphics& graphics, const RECT& rect, bool on, int id) {
  const bool hover = isHot(id);
  const int radius = (rect.bottom - rect.top) / 2;
  fillRound(graphics, rect, radius, on ? gTheme->accent : (hover ? gTheme->cardHover : gTheme->cardAlt));
  strokeRound(graphics, rect, radius, on ? gTheme->accent : gTheme->border);
  const int knob = rect.bottom - rect.top - scale(6);
  const int knobX = on ? rect.right - knob - scale(3) : rect.left + scale(3);
  fillCircle(graphics, knobX + knob / 2, (rect.top + rect.bottom) / 2, knob / 2,
             on ? RGB(255, 255, 255) : gTheme->muted);
  addHotspot(id, rect);
}

void drawKebab(Gdiplus::Graphics& graphics, const RECT& rect, int id) {
  if (isHot(id)) fillRound(graphics, rect, scale(7), gTheme->cardHover);
  addGlyph(GLYPH_MORE, rect, gFonts.icon, isHot(id) ? gTheme->text : gTheme->dim);
  addHotspot(id, rect);
}

void drawViewAll(const RECT& rect, const std::wstring& label, int id) {
  RECT textBox = rect;
  textBox.right -= scale(15);
  addText(label, textBox, gFonts.small, isHot(id) ? gTheme->text : gTheme->muted,
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
  RECT chevronBox{rect.right - scale(14), rect.top, rect.right, rect.bottom};
  addGlyph(GLYPH_CHEVRON_RIGHT, chevronBox, gFonts.iconSm,
           isHot(id) ? gTheme->text : gTheme->muted);
  addHotspot(id, rect);
}

void drawScrollbar(Gdiplus::Graphics& graphics, const RECT& area, int scroll, int maximum) {
  if (maximum <= 0) return;
  const int trackWidth = scale(4);
  RECT track{area.right - scale(9), area.top + scale(4), area.right - scale(5), area.bottom - scale(4)};
  fillRound(graphics, track, trackWidth / 2, gTheme->borderSoft);
  const int trackHeight = track.bottom - track.top;
  const int contentHeight = trackHeight + maximum;
  int thumbHeight = trackHeight * trackHeight / (contentHeight > 0 ? contentHeight : 1);
  if (thumbHeight < scale(30)) thumbHeight = scale(30);
  const int travel = trackHeight - thumbHeight;
  const int thumbTop = track.top + (maximum > 0 ? travel * scroll / maximum : 0);
  RECT thumb{track.left, thumbTop, track.right, thumbTop + thumbHeight};
  fillRound(graphics, thumb, trackWidth / 2, gTheme->dim, 180);
}

void drawEmptyState(Gdiplus::Graphics& graphics, const RECT& area, const wchar_t* title,
                    const std::wstring& subtitle, const std::wstring& buttonLabel, int buttonId) {
  const int centerX = (area.left + area.right) / 2;
  RECT tile{centerX - scale(31), area.top + scale(56), centerX + scale(31), area.top + scale(118)};
  fillRound(graphics, tile, scale(16), gTheme->accentSoft);
  strokeRound(graphics, tile, scale(16), gTheme->accentBorder);
  addGlyph(GLYPH_PUZZLE, tile, gFonts.iconXl, gTheme->accent);
  RECT titleBox{area.left, tile.bottom + scale(18), area.right, tile.bottom + scale(44)};
  addText(title, titleBox, gFonts.h3, gTheme->text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  RECT subtitleBox{centerX - scale(210), titleBox.bottom + scale(4), centerX + scale(210),
                   titleBox.bottom + scale(46)};
  addText(subtitle, subtitleBox, gFonts.small, gTheme->muted,
          DT_CENTER | DT_TOP | DT_WORDBREAK);
  RECT button{centerX - scale(76), subtitleBox.bottom + scale(16), centerX + scale(76),
              subtitleBox.bottom + scale(54)};
  drawPrimaryButton(graphics, button, buttonLabel, GLYPH_ADD, buttonId);
}

void drawTitlebar(Gdiplus::Graphics& graphics, int width, bool maximized) {
  const int height = scale(62);
  fillRect(graphics, {0, 0, width, height}, gTheme->bg);
  drawLine(graphics, 0, height - 1, width, height - 1, gTheme->borderSoft);

  RECT iconBox{scale(20), (height - scale(27)) / 2, scale(47), (height - scale(27)) / 2 + scale(27)};
  addAppIcon(iconBox);
  const std::wstring title = L"SkyLoader";
  RECT titleBox{scale(56), 0, scale(56) + measureText(gFonts.logo, title) + scale(4), height};
  addText(title, titleBox, gFonts.logo, gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT versionBox{titleBox.right + scale(8), 0, titleBox.right + scale(120), height};
  addText(L"v" + std::wstring(kAppVersion), versionBox, gFonts.tiny, gTheme->dim,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  const int closeWidth = scale(44);
  const int buttonWidth = scale(42);
  const int margin = scale(6);
  RECT closeRect{width - margin - closeWidth, scale(6), width - margin, scale(38)};
  RECT maxRect{closeRect.left - buttonWidth - scale(2), scale(6), closeRect.left - scale(2), scale(38)};
  RECT minRect{maxRect.left - buttonWidth, scale(6), maxRect.left, scale(38)};
  RECT themeRect{minRect.left - scale(44), scale(6), minRect.left - scale(8), scale(38)};

  if (isHot(UI_THEME_TOGGLE)) fillRound(graphics, themeRect, scale(7), gTheme->cardHover);
  addGlyph(gTheme->dark ? GLYPH_SUN : GLYPH_MOON, themeRect, gFonts.iconMd,
           isHot(UI_THEME_TOGGLE) ? gTheme->text : gTheme->muted);
  addHotspot(UI_THEME_TOGGLE, themeRect);

  const RECT* controlRects[] = {&minRect, &maxRect, &closeRect};
  for (const RECT* rect : controlRects) {
    if (rect == &closeRect && isHot(UI_CLOSE)) {
      fillRound(graphics, *rect, scale(7), gTheme->accent);
    } else if (isHot(rect == &minRect ? UI_MINIMIZE : rect == &maxRect ? UI_MAXIMIZE : UI_CLOSE)) {
      fillRound(graphics, *rect, scale(7), gTheme->cardHover);
    }
  }
  const bool closeHot = isHot(UI_CLOSE);
  addGlyph(GLYPH_MINIMIZE, minRect, gFonts.iconSm,
           isHot(UI_MINIMIZE) ? gTheme->text : gTheme->muted);
  addGlyph(maximized ? GLYPH_RESTORE : GLYPH_MAXIMIZE, maxRect, gFonts.iconSm,
           isHot(UI_MAXIMIZE) ? gTheme->text : gTheme->muted);
  addGlyph(GLYPH_CLOSE, closeRect, gFonts.iconSm,
           closeHot ? RGB(255, 255, 255) : gTheme->muted);
  addHotspot(UI_MINIMIZE, minRect);
  addHotspot(UI_MAXIMIZE, maxRect);
  addHotspot(UI_CLOSE, closeRect);
}

void drawSidebar(Gdiplus::Graphics& graphics, int height) {
  const int titleHeight = scale(62);
  const int sidebarWidth = scale(236);
  fillRect(graphics, {0, titleHeight, sidebarWidth, height}, gTheme->sidebar);
  drawLine(graphics, sidebarWidth - 1, titleHeight, sidebarWidth - 1, height, gTheme->borderSoft);

  struct NavItem {
    const wchar_t* label;
    wchar_t glyph;
    Page page;
    int id;
  };
  const NavItem items[] = {
    {L"Home", GLYPH_HOME, Page::Home, UI_NAV_HOME},
    {L"Plugins", GLYPH_PUZZLE, Page::Plugins, UI_NAV_PLUGINS},
    {L"Plugin Marketplace", GLYPH_SHOP, Page::Marketplace, UI_NAV_MARKETPLACE},
    {L"Settings", GLYPH_SETTINGS, Page::Settings, UI_NAV_SETTINGS},
  };

  int top = titleHeight + scale(24);
  const int itemHeight = scale(48);
  for (const NavItem& item : items) {
    const RECT rect{scale(14), top, sidebarWidth - scale(14), top + itemHeight};
    const bool active = gPage == item.page;
    const bool hover = isHot(item.id);
    const bool muted = item.page == Page::Marketplace;
    if (active) {
      if (muted) {
        fillRound(graphics, rect, scale(10), gTheme->cardAlt);
        strokeRound(graphics, rect, scale(10), gTheme->borderSoft);
      } else {
        fillRound(graphics, rect, scale(10), gTheme->accentSoft);
        strokeRound(graphics, rect, scale(10), gTheme->accentBorder);
        RECT bar{rect.left + scale(1), rect.top + scale(12), rect.left + scale(4), rect.bottom - scale(12)};
        fillRound(graphics, bar, scale(2), gTheme->accent);
      }
    } else if (hover) {
      fillRound(graphics, rect, scale(10), gTheme->cardHover);
    }
    RECT iconBox{rect.left + scale(16), rect.top, rect.left + scale(40), rect.bottom};
    addGlyph(item.glyph, iconBox, gFonts.iconMd,
             muted ? gTheme->dim : (active ? gTheme->accent : (hover ? gTheme->text : gTheme->muted)));
    RECT textBox{rect.left + scale(48), rect.top, rect.right - scale(12), rect.bottom};
    RECT soonBox{};
    if (muted) {
      soonBox = RECT{rect.right - scale(52), rect.top + scale(16), rect.right - scale(12),
                     rect.top + scale(32)};
      fillRound(graphics, soonBox, scale(8), gTheme->card);
      strokeRound(graphics, soonBox, scale(8), gTheme->borderSoft);
      addText(L"SOON", soonBox, gFonts.tiny, gTheme->dim,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      textBox.right = soonBox.left - scale(4);
    }
    addText(item.label, textBox, muted ? gFonts.small : gFonts.semibold,
             muted ? gTheme->dim : (active ? gTheme->text : (hover ? gTheme->text : gTheme->muted)),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    addHotspot(item.id, rect);
    top += itemHeight + scale(4);
  }

  const int statusTop = height - scale(126);
  RECT statusCard{scale(14), statusTop, sidebarWidth - scale(14), statusTop + scale(62)};
  fillRound(graphics, statusCard, scale(10), gTheme->cardAlt);
  strokeRound(graphics, statusCard, scale(10), gTheme->borderSoft);
  RECT statusIcon{statusCard.left + scale(12), statusCard.top + scale(14),
                  statusCard.left + scale(44), statusCard.top + scale(46)};
  fillCircle(graphics, (statusIcon.left + statusIcon.right) / 2, (statusIcon.top + statusIcon.bottom) / 2,
             scale(16), gTheme->card);
  strokeCircle(graphics, (statusIcon.left + statusIcon.right) / 2,
               (statusIcon.top + statusIcon.bottom) / 2, scale(16), gTheme->borderSoft);
  addGlyph(GLYPH_GLOBE, statusIcon, gFonts.iconMd, gTheme->muted);

  RECT statusLabel{statusCard.left + scale(54), statusCard.top + scale(10),
                   statusCard.right - scale(8), statusCard.top + scale(30)};
  addText(L"Sky Status", statusLabel, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT statusValue{statusCard.left + scale(54), statusCard.top + scale(30),
                   statusCard.right - scale(8), statusCard.top + scale(50)};
  fillCircle(graphics, statusValue.left + scale(4), statusCard.top + scale(40), scale(4),
             gGameRunning ? gTheme->green : gTheme->dim);
  statusValue.left += scale(13);
  addText(gGameRunning ? L"Running" : L"Idle", statusValue, gFonts.small,
          gGameRunning ? gTheme->green : gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  RECT brandBox{scale(18), height - scale(58), sidebarWidth - scale(18), height - scale(38)};
  addText(L"SkyLoader v" + std::wstring(kAppVersion), brandBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT taglineBox{scale(18), height - scale(38), sidebarWidth - scale(18), height - scale(18)};
  addText(L"Plugin loader for your game", taglineBox, gFonts.tiny, gTheme->dim,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawHero(Gdiplus::Graphics& graphics, const RECT& rect) {
  fillRound(graphics, rect, scale(14), gTheme->card);
  strokeRound(graphics, rect, scale(14), gTheme->border);

  RECT iconBox{rect.left + scale(30), rect.top + scale(28), rect.left + scale(30) + scale(52),
               rect.top + scale(28) + scale(52)};
  addAppIcon(iconBox);

  const int textLeft = rect.left + scale(98);
  const int textRight = rect.right - scale(28);
  RECT titleBox{textLeft, rect.top + scale(20), textRight, rect.top + scale(62)};
  addText(L"Welcome to SkyLoader", titleBox, gFonts.h1, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT subtitleBox{textLeft, rect.top + scale(63), textRight - scale(8), rect.top + scale(109)};
  addText(L"Manage your plugins easily.",
          subtitleBox, gFonts.bodySm, gTheme->muted, DT_LEFT | DT_TOP | DT_WORDBREAK);

  RECT launchRect{textLeft, rect.top + scale(124), textLeft + scale(168), rect.top + scale(166)};
  drawPrimaryButton(graphics, launchRect, L"Launch Game", GLYPH_PLAY, UI_LAUNCH);
  RECT settingsRect{launchRect.right + scale(12), launchRect.top, launchRect.right + scale(152),
                    launchRect.bottom};
  drawSecondaryButton(graphics, settingsRect, L"Settings", GLYPH_SETTINGS, UI_HERO_SETTINGS);
}

void drawStatCard(Gdiplus::Graphics& graphics, const RECT& rect, wchar_t glyph, int accentIndex,
                  const std::wstring& label, const std::wstring& value, const std::wstring& caption,
                  int id, bool arrow, bool statusGreen) {
  const bool hover = isHot(id);
  drawCardBg(graphics, rect, scale(12), hover);
  AccentStyle style = accentForIndex(static_cast<size_t>(accentIndex));
  RECT tile{rect.left + scale(15), rect.top + scale(14), rect.left + scale(53), rect.top + scale(52)};
  style.glyph = glyph;
  drawIconTile(graphics, tile, style, gFonts.iconMd);

  RECT labelBox{rect.left + scale(15), rect.top + scale(64), rect.right - scale(14), rect.top + scale(82)};
  addText(label, labelBox, gFonts.bodySm, gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT valueBox{rect.left + scale(15), rect.top + scale(82), rect.right - scale(14), rect.top + scale(124)};
  HFONT valueFont = gFonts.stat;
  if (measureText(valueFont, value) > valueBox.right - valueBox.left) valueFont = gFonts.h2;
  addText(value, valueBox, valueFont, statusGreen ? gTheme->green : gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT captionBox{rect.left + scale(15), rect.top + scale(128), rect.right - scale(14), rect.top + scale(146)};
  if (statusGreen) {
    fillCircle(graphics, captionBox.left + scale(4), rect.top + scale(137), scale(4), gTheme->green);
    captionBox.left += scale(13);
  }
  addText(caption, captionBox, gFonts.small, statusGreen ? gTheme->green : gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  if (arrow) {
    RECT arrowBox{rect.right - scale(31), rect.top + scale(127), rect.right - scale(14), rect.top + scale(145)};
    addGlyph(GLYPH_CHEVRON_RIGHT, arrowBox, gFonts.iconSm, gTheme->dim);
  }
  addHotspot(id, rect);
}

void drawPluginRow(Gdiplus::Graphics& graphics, const RECT& row, size_t index, const RECT& clip,
                   bool detailed) {
  const PluginView& plugin = gPluginViews[index];
  const bool hover = isHot(static_cast<int>(UI_ROW_HOVER + index));
  fillRound(graphics, row, scale(9), hover ? gTheme->cardHover : gTheme->cardAlt);
  strokeRound(graphics, row, scale(9), hover ? gTheme->border : gTheme->borderSoft);
  addHotspot(static_cast<int>(UI_ROW_HOVER + index), row);

  const int tileSize = detailed ? scale(36) : scale(34);
  const int tileTop = row.top + ((row.bottom - row.top) - tileSize) / 2;
  RECT tile{row.left + scale(12), tileTop, row.left + scale(12) + tileSize, tileTop + tileSize};
  drawIconTile(graphics, tile, accentForIndex(0), gFonts.iconMd);

  RECT kebab{row.right - scale(42), row.top + (row.bottom - row.top) / 2 - scale(15),
             row.right - scale(12), row.top + (row.bottom - row.top) / 2 + scale(15)};
  RECT pill{};
  pill.right = kebab.left - scale(8);
  pill.left = pill.right - scale(96);
  pill.top = row.top + (row.bottom - row.top) / 2 - scale(12);
  pill.bottom = pill.top + scale(24);
  drawStatusPill(graphics, pill, plugin.enabled, static_cast<int>(UI_ROW_TOGGLE + index), &clip);

  const int textLeft = tile.right + scale(12);
  if (detailed) {
    RECT versionBox{pill.left - scale(150), row.top + scale(11), pill.left - scale(14), row.top + scale(28)};
    addText(versionLabel(plugin.version), versionBox, gFonts.semiboldSm, gTheme->text,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);
    RECT authorBox{versionBox.left, row.top + scale(29), versionBox.right, row.top + scale(46)};
    addText(plugin.author.empty() ? L"Unknown author" : plugin.author, authorBox, gFonts.tiny,
            gTheme->dim, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);

    RECT nameBox{textLeft, row.top + scale(9), versionBox.left - scale(16), row.top + scale(29)};
    addText(plugin.name, nameBox, gFonts.semibold, gTheme->text,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);
    RECT descBox{textLeft, row.top + scale(30), versionBox.left - scale(16), row.top + scale(50)};
    addText(plugin.description.empty() ? L"No description provided." : plugin.description, descBox,
            gFonts.small, gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);
  } else {
    RECT nameBox{textLeft, row.top + scale(6), pill.left - scale(18), row.top + scale(27)};
    addText(plugin.name, nameBox, gFonts.semibold, gTheme->text,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);
    RECT metaBox{textLeft, row.top + scale(26), pill.left - scale(18), row.top + scale(45)};
    addText(pluginMetaLine(plugin), metaBox, gFonts.small, gTheme->muted,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &clip);
  }

  drawKebab(graphics, kebab, static_cast<int>(UI_ROW_MENU + index));
}

void drawQuickActions(Gdiplus::Graphics& graphics, const RECT& rect) {
  drawCardBg(graphics, rect, scale(12), false);
  RECT iconBox{rect.left + scale(16), rect.top + scale(16), rect.left + scale(38), rect.top + scale(38)};
  addGlyph(GLYPH_BOLT, iconBox, gFonts.iconMd, gTheme->accent);
  RECT titleBox{rect.left + scale(42), rect.top + scale(13), rect.right - scale(16), rect.top + scale(40)};
  addText(L"Quick Actions", titleBox, gFonts.h3, gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  struct QuickAction {
    const wchar_t* label;
    wchar_t glyph;
    int accentIndex;
    int id;
  };
  const QuickAction actions[] = {
    {L"Add Plugin", GLYPH_PUZZLE, 0, UI_QUICK_ADD},
    {L"Open Folder", GLYPH_FOLDER, 1, UI_QUICK_FOLDER},
    {L"Check Updates", GLYPH_SYNC, 4, UI_QUICK_UPDATES},
  };

  int top = rect.top + scale(50);
  const int actionHeight = scale(52);
  for (const QuickAction& action : actions) {
    const RECT row{rect.left + scale(14), top, rect.right - scale(14), top + actionHeight};
    const bool hover = isHot(action.id);
    fillRound(graphics, row, scale(9), hover ? gTheme->cardHover : gTheme->cardAlt);
    strokeRound(graphics, row, scale(9), hover ? gTheme->border : gTheme->borderSoft);
    AccentStyle style = accentForIndex(static_cast<size_t>(action.accentIndex));
    style.glyph = action.glyph;
    RECT tile{row.left + scale(9), row.top + scale(9), row.left + scale(43), row.top + scale(43)};
    drawIconTile(graphics, tile, style, gFonts.icon);
    RECT labelBox{rect.left + scale(64), row.top, row.right - scale(36), row.bottom};
    addText(action.label, labelBox, gFonts.bodySm, hover ? gTheme->text : gTheme->text,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT chevron{row.right - scale(30), row.top, row.right - scale(12), row.bottom};
    addGlyph(GLYPH_CHEVRON_RIGHT, chevron, gFonts.iconSm, gTheme->dim);
    addHotspot(action.id, row);
    top += actionHeight + scale(8);
  }
}

void drawHome(Gdiplus::Graphics& graphics, int width, int height) {
  const int contentLeft = scale(236) + scale(24);
  const int contentRight = width - scale(24);
  const int contentTop = scale(62) + scale(16);
  const int contentBottom = height - scale(18);
  const int gap = scale(18);
  const int rightWidth = scale(304);
  const int rightX = contentRight - rightWidth;
  const int mainRight = rightX - gap;
  const int heroHeight = scale(200);
  const int statsHeight = scale(156);

  RECT hero{contentLeft, contentTop, mainRight, contentTop + heroHeight};
  drawHero(graphics, hero);

  const int statsTop = hero.bottom + scale(16);
  const int statGap = scale(12);
  const int statWidth = (mainRight - contentLeft - statGap * 3) / 4;
  for (int i = 0; i < 4; ++i) {
    RECT card{contentLeft + i * (statWidth + statGap), statsTop,
              contentLeft + i * (statWidth + statGap) + statWidth, statsTop + statsHeight};
    if (i == 0) {
      drawStatCard(graphics, card, GLYPH_PUZZLE, 0, L"Plugins",
                   std::to_wstring(gPluginViews.size()), L"In your library", UI_STAT_PLUGINS, true, false);
    } else if (i == 1) {
      size_t enabled = 0;
      for (const PluginView& plugin : gPluginViews) if (plugin.enabled) ++enabled;
      drawStatCard(graphics, card, GLYPH_CHECK, 5, L"Active Plugins", std::to_wstring(enabled),
                   L"Enabled", UI_STAT_ACTIVE, true, false);
    } else if (i == 2) {
      drawStatCard(graphics, card, GLYPH_SYNC, 4, L"Updates", gUpdateAvailable ? L"1" : L"0",
                   gUpdateAvailable ? L"Available" : L"Up to date", UI_STAT_UPDATES, true, false);
    } else {
      drawStatCard(graphics, card, GLYPH_BOLT, 3, L"Game Status",
                   gGameRunning ? L"Running" : L"Offline",
                   gGameRunning ? L"Connected" : L"Not running", UI_STAT_GAME, false, gGameRunning);
    }
  }

  const int recentTop = statsTop + statsHeight + scale(16);
  RECT recent{contentLeft, recentTop, mainRight, contentBottom};
  drawCardBg(graphics, recent, scale(12), false);
  RECT headerIcon{recent.left + scale(16), recent.top + scale(13), recent.left + scale(42), recent.top + scale(39)};
  fillCircle(graphics, (headerIcon.left + headerIcon.right) / 2, (headerIcon.top + headerIcon.bottom) / 2,
             scale(13), gTheme->cardAlt);
  strokeCircle(graphics, (headerIcon.left + headerIcon.right) / 2, (headerIcon.top + headerIcon.bottom) / 2,
               scale(13), gTheme->borderSoft);
  addGlyph(GLYPH_HISTORY, headerIcon, gFonts.icon, gTheme->muted);
  RECT recentTitle{recent.left + scale(44), recent.top + scale(10), recent.right - scale(100), recent.top + scale(42)};
  addText(L"Recently Used", recentTitle, gFonts.h3, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT viewAll{recent.right - scale(102), recent.top + scale(11), recent.right - scale(14), recent.top + scale(41)};
  drawViewAll(viewAll, L"View all", UI_VIEW_PLUGINS);
  drawDivider(graphics, recent.left + scale(14), recent.right - scale(14), recent.top + scale(48));

  RECT listArea{recent.left + scale(12), recent.top + scale(56), recent.right - scale(12),
                recent.bottom - scale(12)};
  if (gPluginViews.empty()) {
    drawEmptyState(graphics, listArea, L"No plugins installed",
                   L"Drag and drop a .dll here or add one to get started.", L"Add Plugin", UI_EMPTY_ADD);
  } else {
    const int rowHeight = scale(52);
    const int rowGap = scale(6);
    int top = listArea.top;
    size_t shown = 0;
    for (size_t i = 0; i < gPluginViews.size() && shown < 5; ++i, ++shown) {
      if (top + rowHeight > listArea.bottom) break;
      RECT row{listArea.left, top, listArea.right, top + rowHeight};
      drawPluginRow(graphics, row, i, recent, false);
      top += rowHeight + rowGap;
    }
  }

  const int quickHeight = scale(236);
  RECT quick{rightX, contentTop, contentRight, contentTop + quickHeight};
  drawQuickActions(graphics, quick);
}

void drawPageHeader(Gdiplus::Graphics& graphics, int width, const std::wstring& title,
                    const std::wstring& subtitle, wchar_t glyph = 0) {
  const int left = scale(236) + scale(24);
  const int right = width - scale(24);
  const int top = scale(62) + scale(16);
  int textLeft = left;
  if (glyph) {
    RECT tile{left, top - scale(2), left + scale(40), top + scale(38)};
    fillRound(graphics, tile, scale(11), gTheme->accent);
    addGlyph(glyph, tile, gFonts.iconMd, RGB(255, 255, 255));
    textLeft = tile.right + scale(14);
  }
  RECT titleBox{textLeft, top, right - scale(320), top + scale(32)};
  addText(title, titleBox, gFonts.h2, gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT subtitleBox{textLeft, top + scale(30), right - scale(320), top + scale(50)};
  addText(subtitle, subtitleBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void drawPluginsPage(Gdiplus::Graphics& graphics, int width, int height) {
  drawPageHeader(graphics, width, L"Plugins", L"Manage the plugins that are injected into your game.",
                 GLYPH_PUZZLE);
  const int left = scale(236) + scale(24);
  const int right = width - scale(24);
  const int top = scale(62) + scale(16);

  RECT addRect{right - scale(126), top + scale(4), right, top + scale(40)};
  drawPrimaryButton(graphics, addRect, L"Add Plugin", GLYPH_ADD, UI_ADD_PLUGIN);
  RECT folderRect{addRect.left - scale(10) - scale(132), addRect.top, addRect.left - scale(10), addRect.bottom};
  drawSecondaryButton(graphics, folderRect, L"Open Folder", GLYPH_FOLDER, UI_OPEN_FOLDER);

  RECT card{left, top + scale(58), right, height - scale(18)};
  drawCardBg(graphics, card, scale(12), false);
  RECT header{card.left + scale(18), card.top + scale(12), card.right - scale(20), card.top + scale(44)};
  size_t enabled = 0;
  for (const PluginView& plugin : gPluginViews) if (plugin.enabled) ++enabled;
  addText(L"Installed Plugins", header, gFonts.h3, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT countBox{header.left + scale(170), header.top, header.right - scale(4), header.bottom};
  addText(std::to_wstring(gPluginViews.size()) + L" plugins  \u2022  " + std::to_wstring(enabled) +
          L" enabled", countBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  drawDivider(graphics, card.left + scale(16), card.right - scale(16), card.top + scale(52));

  RECT listArea{card.left + scale(12), card.top + scale(60), card.right - scale(12),
                card.bottom - scale(44)};
  if (gPluginViews.empty()) {
    drawEmptyState(graphics, listArea, L"No plugins installed",
                   L"Drag and drop a .dll file here, or add a plugin to get started.",
                   L"Add Plugin", UI_EMPTY_ADD);
    gPluginScroll = 0;
    gPluginScrollMax = 0;
  } else {
    const int rowHeight = scale(62);
    const int rowGap = scale(6);
    const int totalHeight = static_cast<int>(gPluginViews.size()) * (rowHeight + rowGap) - rowGap;
    gPluginScrollMax = totalHeight > (listArea.bottom - listArea.top)
      ? totalHeight - (listArea.bottom - listArea.top) + scale(8) : 0;
    gPluginScroll = clampInt(gPluginScroll, 0, gPluginScrollMax);
    Gdiplus::GraphicsState state = graphics.Save();
    graphics.SetClip(gpRect(listArea));
    int rowTop = listArea.top - gPluginScroll;
    for (size_t i = 0; i < gPluginViews.size(); ++i) {
      RECT row{listArea.left, rowTop, listArea.right, rowTop + rowHeight};
      if (row.bottom > listArea.top && row.top < listArea.bottom)
        drawPluginRow(graphics, row, i, listArea, true);
      rowTop += rowHeight + rowGap;
    }
    graphics.Restore(state);
    drawScrollbar(graphics, listArea, gPluginScroll, gPluginScrollMax);
  }

  RECT hint{card.left + scale(20), card.bottom - scale(34), card.right - scale(150), card.bottom - scale(8)};
  addText(L"Tip: drag and drop .dll files directly into this window.", hint, gFonts.tiny, gTheme->dim,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT footerCount{card.right - scale(150), card.bottom - scale(34), card.right - scale(20), card.bottom - scale(8)};
  addText(std::to_wstring(gPluginViews.size()) + L" installed", footerCount, gFonts.tiny, gTheme->dim,
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void drawMarketplace(Gdiplus::Graphics& graphics, int width, int height) {
  drawPageHeader(graphics, width, L"Plugin Marketplace",
                 L"Discover community plugins and install them with SkyLoader.");
  const int left = scale(236) + scale(24);
  const int right = width - scale(24);
  const int top = scale(62) + scale(16);

  RECT area{left, top + scale(58), right, height - scale(18)};
  drawCardBg(graphics, area, scale(12), false);

  const int centerX = (area.left + area.right) / 2;
  const int centerY = area.top + (area.bottom - area.top) / 2;
  RECT tile{centerX - scale(34), centerY - scale(112), centerX + scale(34), centerY - scale(44)};
  fillRound(graphics, tile, scale(16), gTheme->cardAlt);
  strokeRound(graphics, tile, scale(16), gTheme->borderSoft);
  addGlyph(GLYPH_SHOP, tile, gFonts.iconXl, gTheme->dim);

  RECT soon{centerX - scale(40), tile.bottom + scale(16), centerX + scale(40), tile.bottom + scale(46)};
  fillRound(graphics, soon, scale(15), gTheme->cardAlt);
  strokeRound(graphics, soon, scale(15), gTheme->borderSoft);
  addText(L"SOON", soon, gFonts.semiboldSm, gTheme->muted,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  RECT titleBox{centerX - scale(260), soon.bottom + scale(20), centerX + scale(260),
                soon.bottom + scale(52)};
  addText(L"Plugin Marketplace is coming soon", titleBox, gFonts.h3, gTheme->muted,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  RECT descBox{centerX - scale(300), titleBox.bottom + scale(6), centerX + scale(300),
               titleBox.bottom + scale(48)};
  addText(L"Browse, download, and install community plugins without leaving SkyLoader.",
          descBox, gFonts.bodySm, gTheme->dim, DT_CENTER | DT_TOP | DT_WORDBREAK);

  RECT button{centerX - scale(82), descBox.bottom + scale(22), centerX + scale(82),
              descBox.bottom + scale(58)};
  fillRound(graphics, button, scale(9), gTheme->cardAlt);
  strokeRound(graphics, button, scale(9), gTheme->borderSoft);
  addText(L"Coming soon", button, gFonts.button, gTheme->dim,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  RECT hint{area.left + scale(24), area.bottom - scale(44), area.right - scale(24),
            area.bottom - scale(18)};
  addText(L"For now, add plugins from the Plugins page or drag and drop a DLL into this window.",
          hint, gFonts.small, gTheme->dim, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}


constexpr int kSettingsNavWidth = 288;
constexpr int kSettingsGap = 14;
constexpr int kSettingsGeneralHeight = 218;
constexpr int kSettingsUpdatesHeight = 252;
constexpr int kSettingsGameHeight = 176;
constexpr int kSettingsPluginsHeight = 234;
constexpr int kSettingsDownloadsHeight = 160;
constexpr int kSettingsAppearanceHeight = 160;
constexpr int kSettingsAdvancedHeight = 160;

int settingsLeft() {
  return scale(236) + scale(24);
}

int settingsRight(int width) {
  return width - scale(24);
}

int settingsHeaderTop() {
  return scale(62) + scale(16);
}

int settingsCardsTop() {
  return settingsHeaderTop() + scale(72);
}

int settingsActionsBottom(int height) {
  return height - scale(18);
}

int settingsActionsTop(int height) {
  return settingsActionsBottom(height) - scale(40);
}

int settingsNavRight() {
  return settingsLeft() + scale(kSettingsNavWidth);
}

int settingsCardLeft() {
  return settingsNavRight() + scale(16);
}

int settingsViewBottom(int height) {
  return settingsActionsTop(height) - scale(12);
}

int settingsSectionHeight(int index) {
  switch (index) {
    case 0: return scale(kSettingsGeneralHeight);
    case 1: return scale(kSettingsUpdatesHeight);
    case 2: return scale(kSettingsGameHeight);
    case 3: return scale(kSettingsPluginsHeight);
    case 4: return scale(kSettingsDownloadsHeight);
    case 5: return scale(kSettingsAppearanceHeight);
    default: return scale(kSettingsAdvancedHeight);
  }
}

int settingsSectionTop(int index) {
  int top = 0;
  for (int i = 0; i < index; ++i) top += settingsSectionHeight(i) + scale(kSettingsGap);
  return top;
}

int settingsContentHeight() {
  int total = 0;
  for (int i = 0; i < 7; ++i) {
    total += settingsSectionHeight(i);
    if (i < 6) total += scale(kSettingsGap);
  }
  return total;
}

RECT settingsCardRect(int width, int, int index) {
  const int top = settingsCardsTop() + settingsSectionTop(index) - gSettingsScroll;
  return RECT{settingsCardLeft(), top, settingsRight(width), top + settingsSectionHeight(index)};
}

RECT settingsGamePathRect(int width) {
  const RECT card = settingsCardRect(width, 0, 2);
  const int rowTop = card.top + scale(50);
  return RECT{card.left + scale(216), rowTop + scale(14),
              card.right - scale(120), rowTop + scale(50)};
}

RECT settingsDownloadPathRect(int width) {
  const RECT card = settingsCardRect(width, 0, 3);
  const int rowTop = card.top + scale(154);
  return RECT{card.left + scale(216), rowTop + scale(14),
              card.right - scale(120), rowTop + scale(50)};
}

void drawSettingsGroupTitle(Gdiplus::Graphics&, const RECT& card, wchar_t glyph,
                            const wchar_t* title) {
  RECT iconBox{card.left + scale(18), card.top + scale(14), card.left + scale(40), card.top + scale(36)};
  addGlyph(glyph, iconBox, gFonts.iconMd, gTheme->muted);
  RECT titleBox{card.left + scale(50), card.top + scale(10), card.right - scale(20), card.top + scale(42)};
  addText(title, titleBox, gFonts.h3, gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawSettingsDivider(Gdiplus::Graphics& graphics, const RECT& card, int y) {
  drawDivider(graphics, card.left + scale(18), card.right - scale(18), y);
}

void drawSettingsToggleRow(Gdiplus::Graphics& graphics, const RECT& card, int top, const wchar_t* label,
                           const wchar_t* description, bool value, int id) {
  RECT row{card.left + scale(18), top, card.right - scale(18), top + scale(52)};
  RECT labelBox{row.left, row.top + scale(4), row.right - scale(130), row.top + scale(24)};
  addText(label, labelBox, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT descBox{row.left, row.top + scale(23), row.right - scale(130), row.top + scale(43)};
  addText(description, descBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

  RECT statusBox{card.right - scale(82), row.top, card.right - scale(18), row.bottom};
  RECT switchRect{statusBox.left - scale(54), row.top + scale(15), statusBox.left - scale(10),
                  row.top + scale(37)};
  drawSwitch(graphics, switchRect, value, id);
  addText(value ? L"Enabled" : L"Disabled", statusBox, gFonts.small,
          value ? gTheme->green : gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawSettingsField(Gdiplus::Graphics& graphics, const RECT& rect) {
  fillRound(graphics, rect, scale(8), gTheme->input);
  strokeRound(graphics, rect, scale(8), gTheme->inputBorder);
}

void drawSettingsNav(Gdiplus::Graphics& graphics, int, int height) {
  RECT nav{settingsLeft(), settingsCardsTop(), settingsNavRight(), settingsActionsBottom(height)};
  drawCardBg(graphics, nav, scale(12), false);

  struct SettingsNavItem {
    const wchar_t* label;
    wchar_t glyph;
    int id;
  };
  const SettingsNavItem items[] = {
    {L"General", GLYPH_SETTINGS, UI_SETTINGS_GENERAL},
    {L"Updates", GLYPH_SYNC, UI_SETTINGS_UPDATES},
    {L"Game", GLYPH_GAME, UI_SETTINGS_GAME},
    {L"Plugins", GLYPH_PUZZLE, UI_SETTINGS_PLUGINS},
    {L"Downloads", GLYPH_DOWNLOAD, UI_SETTINGS_DOWNLOADS},
    {L"Appearance", GLYPH_PALETTE, UI_SETTINGS_APPEARANCE},
    {L"Advanced", GLYPH_CODE, UI_SETTINGS_ADVANCED},
  };

  int top = nav.top + scale(12);
  const int itemHeight = scale(52);
  for (int i = 0; i < 7; ++i) {
    RECT row{nav.left + scale(12), top, nav.right - scale(12), top + itemHeight};
    const bool active = gSettingsCategory == i;
    const bool hover = isHot(items[i].id);
    if (active) {
      fillRound(graphics, row, scale(9), gTheme->cardHover);
      strokeRound(graphics, row, scale(9), gTheme->borderSoft);
      RECT bar{row.left + scale(1), row.top + scale(11), row.left + scale(4), row.bottom - scale(11)};
      fillRound(graphics, bar, scale(2), gTheme->accent);
    } else if (hover) {
      fillRound(graphics, row, scale(9), gTheme->cardHover);
    }
    RECT iconBox{row.left + scale(18), row.top, row.left + scale(40), row.bottom};
    addGlyph(items[i].glyph, iconBox, gFonts.iconMd,
             active ? gTheme->text : (hover ? gTheme->text : gTheme->muted));
    RECT labelBox{row.left + scale(54), row.top, row.right - scale(40), row.bottom};
    addText(items[i].label, labelBox, gFonts.semibold,
             active ? gTheme->text : (hover ? gTheme->text : gTheme->muted),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (active) {
      RECT chevron{row.right - scale(30), row.top, row.right - scale(12), row.bottom};
      addGlyph(GLYPH_CHEVRON_RIGHT, chevron, gFonts.iconSm, gTheme->muted);
    }
    addHotspot(items[i].id, row);
    top += itemHeight + scale(4);
  }

  RECT versionBox{nav.left + scale(22), nav.bottom - scale(34), nav.right - scale(20), nav.bottom - scale(12)};
  addText(L"SkyLoader v" + std::wstring(kAppVersion), versionBox, gFonts.tiny, gTheme->dim,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawGeneralSettings(Gdiplus::Graphics& graphics, const RECT& card) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_SETTINGS, L"General Settings");

  RECT languageRow{card.left + scale(18), card.top + scale(50), card.right - scale(18), card.top + scale(104)};
  RECT langLabel{languageRow.left, languageRow.top + scale(5), languageRow.right - scale(300), languageRow.top + scale(27)};
  addText(L"Language", langLabel, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT langDesc{languageRow.left, languageRow.top + scale(25), languageRow.right - scale(300), languageRow.top + scale(45)};
  addText(L"Select the language for the application.", langDesc, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT select{card.right - scale(18) - scale(280), languageRow.top + scale(3),
              card.right - scale(18), languageRow.top + scale(39)};
  fillRound(graphics, select, scale(8), gTheme->input);
  strokeRound(graphics, select, scale(8), gTheme->inputBorder);
  RECT selectText{select.left + scale(13), select.top, select.right - scale(34), select.bottom};
  addText(L"English", selectText, gFonts.bodySm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT selectChevron{select.right - scale(30), select.top, select.right - scale(10), select.bottom};
  addGlyph(GLYPH_CHEVRON_DOWN, selectChevron, gFonts.iconSm, gTheme->muted);
  addHotspot(UI_SETTINGS_LANGUAGE, select);

  drawSettingsDivider(graphics, card, card.top + scale(104));
  drawSettingsToggleRow(graphics, card, card.top + scale(104), L"Start with Windows",
                        L"Launch SkyLoader when your computer starts.", gStartWithWindows,
                        UI_TOGGLE_STARTUP);
  drawSettingsDivider(graphics, card, card.top + scale(156));
  drawSettingsToggleRow(graphics, card, card.top + scale(156), L"Minimize to tray",
                        L"Keep the app running in the background when closed.", gMinimizeToTray,
                        UI_TOGGLE_MINIMIZE);
}

void drawGameSettings(Gdiplus::Graphics& graphics, const RECT& card, int width) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_GAME, L"Game Settings");

  const int rowOne = card.top + scale(50);
  RECT pathLabel{card.left + scale(18), rowOne + scale(4), card.left + scale(210), rowOne + scale(24)};
  addText(L"Game path", pathLabel, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT pathDesc{card.left + scale(18), rowOne + scale(23), card.left + scale(210), rowOne + scale(41)};
  addText(L"Select the Sky executable.", pathDesc, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT detectLink{card.left + scale(18), rowOne + scale(40), card.left + scale(110), rowOne + scale(60)};
  addText(L"Auto-detect", detectLink, gFonts.tiny, isHot(UI_AUTODETECT_GAME) ? gTheme->accentHover : gTheme->accent,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  addHotspot(UI_AUTODETECT_GAME, detectLink);

  const RECT pathEdit = settingsGamePathRect(width);
  drawSettingsField(graphics, pathEdit);
  RECT browse{pathEdit.right + scale(8), pathEdit.top, card.right - scale(18), pathEdit.bottom};
  drawSecondaryButton(graphics, browse, L"Browse", GLYPH_FOLDER, UI_BROWSE_GAME);

  drawSettingsDivider(graphics, card, rowOne + scale(64));
  drawSettingsToggleRow(graphics, card, rowOne + scale(64), L"Launch through Steam",
                        L"Use the Steam client when starting the game.", gLaunchViaSteam,
                        UI_TOGGLE_STEAM);
}

void drawUpdatesSettings(Gdiplus::Graphics& graphics, const RECT& card) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_SYNC, L"Updates");

  const int top = card.top + scale(50);
  RECT versionLabel{card.left + scale(18), top, card.left + scale(230), top + scale(18)};
  addText(L"INSTALLED VERSION", versionLabel, gFonts.tiny, gTheme->dim,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT versionBox{card.left + scale(18), top + scale(16), card.left + scale(230), top + scale(40)};
  addText(L"SkyLoader v" + std::wstring(kAppVersion), versionBox, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT latestBox{card.left + scale(18), top + scale(40), card.left + scale(230), top + scale(60)};
  addText(gLatestVersion.empty() ? L"No release checked yet." : L"Latest release " + gLatestVersion,
          latestBox, gFonts.small, gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  RECT checkRect{card.right - scale(160), top, card.right - scale(18), top + scale(36)};
  drawSecondaryButton(graphics, checkRect, L"Check Now", GLYPH_SYNC, UI_CHECK_UPDATES);

  RECT messagePanel{card.left + scale(18), top + scale(72), card.right - scale(18), top + scale(130)};
  fillRound(graphics, messagePanel, scale(9), gTheme->cardAlt);
  strokeRound(graphics, messagePanel, scale(9), gTheme->borderSoft);
  RECT statusBox{messagePanel.left + scale(12), messagePanel.top + scale(7),
                 messagePanel.right - scale(12), messagePanel.top + scale(27)};
  fillCircle(graphics, statusBox.left + scale(4), messagePanel.top + scale(17), scale(4),
             gUpdateCheckInProgress ? gTheme->muted
               : (gUpdateAvailable ? gTheme->accent : (gUpdateFailed ? gTheme->red : gTheme->green)));
  statusBox.left += scale(13);
  addText(gUpdateCheckInProgress ? L"Checking for updates..."
            : (gUpdateAvailable ? L"Update available"
               : (gUpdateFailed ? L"Could not check for updates"
                  : (gUpdateMessage.empty() ? L"Not checked yet" : L"Up to date"))),
          statusBox, gFonts.small,
          gUpdateCheckInProgress ? gTheme->muted
            : (gUpdateAvailable ? gTheme->accent : (gUpdateFailed ? gTheme->red : gTheme->green)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT detailBox{messagePanel.left + scale(12), messagePanel.top + scale(27),
                 messagePanel.right - scale(12), messagePanel.bottom - scale(7)};
  addText(gUpdateCheckInProgress ? L"Contacting GitHub Releases..."
            : (gUpdateMessage.empty() ? L"Check now to look for a newer SkyLoader build." : gUpdateMessage),
          detailBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

  drawSettingsDivider(graphics, card, top + scale(140));
  drawSettingsToggleRow(graphics, card, top + scale(140), L"Check for updates on startup",
                        L"Automatically look for a newer version when SkyLoader opens.",
                        gCheckUpdatesStartup, UI_TOGGLE_CHECK);
}

void drawPluginsSettings(Gdiplus::Graphics& graphics, const RECT& card, int width) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_PUZZLE, L"Plugin Settings");

  drawSettingsToggleRow(graphics, card, card.top + scale(50), L"Auto load plugins",
                        L"Automatically enable plugins when they are added.", gAutoLoadMods,
                        UI_TOGGLE_AUTOLOAD);
  drawSettingsDivider(graphics, card, card.top + scale(102));
  drawSettingsToggleRow(graphics, card, card.top + scale(102), L"Confirm removal",
                        L"Ask before removing a plugin from your library.", gConfirmRemove,
                        UI_TOGGLE_CONFIRM);

  drawSettingsDivider(graphics, card, card.top + scale(154));
  const int rowTop = card.top + scale(154);
  RECT labelBox{card.left + scale(18), rowTop + scale(4), card.left + scale(210), rowTop + scale(24)};
  addText(L"Plugin download location", labelBox, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT descBox{card.left + scale(18), rowTop + scale(23), card.left + scale(210), rowTop + scale(43)};
  addText(L"Where downloaded plugins go.", descBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

  const RECT downloadEdit = settingsDownloadPathRect(width);
  drawSettingsField(graphics, downloadEdit);
  RECT browse{downloadEdit.right + scale(8), downloadEdit.top, card.right - scale(18), downloadEdit.bottom};
  drawSecondaryButton(graphics, browse, L"Browse", GLYPH_FOLDER, UI_SETTINGS_BROWSE_DOWNLOAD);
}

void drawDownloadsSettings(Gdiplus::Graphics& graphics, const RECT& card) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_DOWNLOAD, L"Downloads");

  RECT desc{card.left + scale(18), card.top + scale(52), card.right - scale(18), card.top + scale(102)};
  addText(L"Marketplace project pages open in your browser. Download the DLL from the project, "
          L"then add it from the Plugins page. SkyLoader never downloads third-party DLLs on its own.",
          desc, gFonts.small, gTheme->muted, DT_LEFT | DT_TOP | DT_WORDBREAK);
  RECT openButton{card.left + scale(18), card.top + scale(108), card.left + scale(198), card.top + scale(144)};
  drawSecondaryButton(graphics, openButton, L"Open Downloads", GLYPH_FOLDER, UI_SETTINGS_OPEN_DOWNLOAD);
}

void drawAppearanceSettings(Gdiplus::Graphics& graphics, const RECT& card) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_PALETTE, L"Appearance");

  RECT labelBox{card.left + scale(18), card.top + scale(52), card.right - scale(360), card.top + scale(72)};
  addText(L"Theme", labelBox, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT descBox{card.left + scale(18), card.top + scale(72), card.right - scale(360), card.top + scale(92)};
  addText(L"Choose the look that suits your setup.", descBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

  RECT darkRect{card.right - scale(18) - scale(320), card.top + scale(56),
                card.right - scale(18) - scale(164), card.top + scale(102)};
  RECT lightRect{card.right - scale(18) - scale(154), darkRect.top,
                 card.right - scale(18), darkRect.bottom};
  auto themeButton = [&](const RECT& rect, const wchar_t* label, wchar_t glyph, bool active, int id) {
    fillRound(graphics, rect, scale(9), active ? gTheme->accentSoft : gTheme->cardAlt);
    strokeRound(graphics, rect, scale(9), active ? gTheme->accentBorder : gTheme->borderSoft);
    RECT iconBox{rect.left + scale(12), rect.top, rect.left + scale(34), rect.bottom};
    addGlyph(glyph, iconBox, gFonts.icon, active ? gTheme->accent : gTheme->muted);
    RECT textBox{rect.left + scale(34), rect.top, rect.right - scale(8), rect.bottom};
    addText(label, textBox, gFonts.bodySm, active ? gTheme->text : gTheme->muted,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    addHotspot(id, rect);
  };
  themeButton(darkRect, L"Dark", GLYPH_MOON, gTheme->dark, UI_THEME_DARK);
  themeButton(lightRect, L"Light", GLYPH_SUN, !gTheme->dark, UI_THEME_LIGHT);
}

void drawAdvancedSettings(Gdiplus::Graphics& graphics, const RECT& card) {
  drawCardBg(graphics, card, scale(12), false);
  drawSettingsGroupTitle(graphics, card, GLYPH_CODE, L"Advanced");

  RECT labelBox{card.left + scale(18), card.top + scale(52), card.right - scale(220), card.top + scale(72)};
  addText(L"Data folder", labelBox, gFonts.semiboldSm, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT descBox{card.left + scale(18), card.top + scale(72), card.right - scale(220), card.top + scale(92)};
  addText(L"Settings, plugins, and update downloads.", descBox, gFonts.small, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT openButton{card.right - scale(18) - scale(150), card.top + scale(54),
                  card.right - scale(18), card.top + scale(90)};
  drawSecondaryButton(graphics, openButton, L"Open Data", GLYPH_FOLDER, UI_SETTINGS_OPEN_DATA);

  drawSettingsDivider(graphics, card, card.top + scale(104));
  RECT versionBox{card.left + scale(18), card.top + scale(112), card.right - scale(18), card.top + scale(138)};
  addText(L"SkyLoader v" + std::wstring(kAppVersion) + L"  \u2022  64-bit", versionBox, gFonts.small,
          gTheme->muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawSettings(Gdiplus::Graphics& graphics, int width, int height) {
  const int headerTop = settingsHeaderTop();
  const int contentLeft = settingsLeft();
  const int right = settingsRight(width);

  RECT headerIcon{contentLeft, headerTop, contentLeft + scale(56), headerTop + scale(56)};
  fillRound(graphics, headerIcon, scale(14), gTheme->accent);
  addGlyph(GLYPH_SETTINGS, headerIcon, gFonts.iconXl, RGB(255, 255, 255));
  RECT titleBox{headerIcon.right + scale(16), headerTop - scale(4), right, headerTop + scale(32)};
  addText(L"Settings", titleBox, gFonts.h1, gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT subtitleBox{headerIcon.right + scale(16), headerTop + scale(32), right, headerTop + scale(54)};
  addText(L"Customize your SkyLoader experience.", subtitleBox, gFonts.bodySm, gTheme->muted,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  drawSettingsNav(graphics, width, height);

  const int viewTop = settingsCardsTop();
  const int viewBottom = settingsViewBottom(height);
  const RECT view{settingsCardLeft(), viewTop, right, viewBottom};
  const int contentHeight = settingsContentHeight();
  gSettingsScrollMax = contentHeight > (viewBottom - viewTop)
    ? contentHeight - (viewBottom - viewTop) + scale(8) : 0;
  gSettingsScroll = clampInt(gSettingsScroll, 0, gSettingsScrollMax);

  gTextClip = view;
  gTextClipActive = true;
  gHotspotClip = view;
  gHotspotClipActive = true;
  Gdiplus::GraphicsState state = graphics.Save();
  graphics.SetClip(gpRect(view));

  if (settingsCardRect(width, height, 0).bottom > viewTop)
    drawGeneralSettings(graphics, settingsCardRect(width, height, 0));
  if (settingsCardRect(width, height, 1).top < viewBottom &&
      settingsCardRect(width, height, 1).bottom > viewTop)
    drawUpdatesSettings(graphics, settingsCardRect(width, height, 1));
  if (settingsCardRect(width, height, 2).top < viewBottom &&
      settingsCardRect(width, height, 2).bottom > viewTop)
    drawGameSettings(graphics, settingsCardRect(width, height, 2), width);
  if (settingsCardRect(width, height, 3).top < viewBottom &&
      settingsCardRect(width, height, 3).bottom > viewTop)
    drawPluginsSettings(graphics, settingsCardRect(width, height, 3), width);
  if (settingsCardRect(width, height, 4).top < viewBottom &&
      settingsCardRect(width, height, 4).bottom > viewTop)
    drawDownloadsSettings(graphics, settingsCardRect(width, height, 4));
  if (settingsCardRect(width, height, 5).top < viewBottom &&
      settingsCardRect(width, height, 5).bottom > viewTop)
    drawAppearanceSettings(graphics, settingsCardRect(width, height, 5));
  if (settingsCardRect(width, height, 6).top < viewBottom &&
      settingsCardRect(width, height, 6).bottom > viewTop)
    drawAdvancedSettings(graphics, settingsCardRect(width, height, 6));

  graphics.Restore(state);
  gTextClipActive = false;
  gHotspotClipActive = false;
  drawScrollbar(graphics, view, gSettingsScroll, gSettingsScrollMax);

  const int actionTop = settingsActionsTop(height);
  const int actionBottom = settingsActionsBottom(height);
  RECT saveRect{right - scale(160), actionTop, right, actionBottom};
  RECT resetRect{saveRect.left - scale(10) - scale(176), actionTop, saveRect.left - scale(10), actionBottom};
  drawSecondaryButton(graphics, resetRect, L"Reset to Default", GLYPH_REFRESH, UI_SETTINGS_RESET);
  drawPrimaryButton(graphics, saveRect, L"Save Changes", GLYPH_SAVE, UI_SETTINGS_SAVE);
}

void drawToast(Gdiplus::Graphics& graphics, int width, int height) {
  if (gStatusText.empty()) return;
  if (GetTickCount64() - gStatusTick > 9000) return;
  const int maxWidth = scale(470);
  const int contentWidth = measureText(gFonts.small, gStatusText);
  const int toastWidth = clampInt(contentWidth + scale(58), scale(270), maxWidth);
  const int toastTop = gPage == Page::Settings
    ? scale(62) + scale(10) : height - scale(18) - scale(48);
  RECT toast{width - scale(24) - toastWidth, toastTop, width - scale(24),
             toastTop + scale(48)};
  fillRound(graphics, {toast.left + scale(2), toast.top + scale(3), toast.right + scale(2), toast.bottom + scale(3)},
            scale(10), RGB(0, 0, 0), gTheme->dark ? 90 : 30);
  fillRound(graphics, toast, scale(10), gTheme->card);
  strokeRound(graphics, toast, scale(10), gTheme->border);
  RECT iconBox{toast.left + scale(10), toast.top, toast.left + scale(36), toast.bottom};
  fillCircle(graphics, (iconBox.left + iconBox.right) / 2, (toast.top + toast.bottom) / 2, scale(11),
             gTheme->accentSoft);
  addGlyph(GLYPH_INFO, iconBox, gFonts.iconSm, gTheme->accent);
  RECT textBox{toast.left + scale(42), toast.top, toast.right - scale(12), toast.bottom};
  addText(gStatusText, textBox, gFonts.small, gTheme->text,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void drawPopupMenu(Gdiplus::Graphics& graphics, int width, int height) {
  if (gMenuIndex < 0 || static_cast<size_t>(gMenuIndex) >= gPluginViews.size()) return;
  const int menuWidth = scale(206);
  const int itemHeight = scale(36);
  const int padding = scale(6);
  const int menuHeight = padding * 2 + itemHeight * 3;
  int left = gMenuAnchor.x;
  int top = gMenuAnchor.y;
  if (left + menuWidth > width - scale(10)) left = width - scale(10) - menuWidth;
  if (top + menuHeight > height - scale(10)) top = height - scale(10) - menuHeight;
  if (left < scale(10)) left = scale(10);
  if (top < scale(10)) top = scale(10);
  RECT menu{left, top, left + menuWidth, top + menuHeight};
  fillRound(graphics, {menu.left + scale(3), menu.top + scale(4), menu.right + scale(3), menu.bottom + scale(4)},
            scale(10), RGB(0, 0, 0), gTheme->dark ? 110 : 35);
  fillRound(graphics, menu, scale(10), gTheme->card);
  strokeRound(graphics, menu, scale(10), gTheme->border);

  const PluginView& plugin = gPluginViews[gMenuIndex];
  struct MenuItem {
    const wchar_t* label;
    wchar_t glyph;
    bool danger;
  };
  const MenuItem items[] = {
    {plugin.enabled ? L"Disable" : L"Enable", plugin.enabled ? GLYPH_CLOSE : GLYPH_CHECK, false},
    {L"Show in folder", GLYPH_FOLDER, false},
    {L"Remove from list", GLYPH_DELETE, true},
  };
  for (int i = 0; i < 3; ++i) {
    RECT row{menu.left + padding, menu.top + padding + i * itemHeight, menu.right - padding,
             menu.top + padding + (i + 1) * itemHeight};
    const int id = UI_MENU_ITEM + i;
    if (isHot(id)) fillRound(graphics, row, scale(7), items[i].danger ? gTheme->redBg : gTheme->cardHover);
    RECT iconBox{row.left + scale(10), row.top, row.left + scale(32), row.bottom};
    addGlyph(items[i].glyph, iconBox, gFonts.iconSm,
             items[i].danger ? gTheme->red : gTheme->muted);
    RECT labelBox{row.left + scale(38), row.top, row.right - scale(8), row.bottom};
    addText(items[i].label, labelBox, gFonts.small,
            items[i].danger ? gTheme->red : gTheme->text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    addHotspot(id, row);
  }
}

void paintInterface(HWND window, HDC dc) {
  RECT client{};
  GetClientRect(window, &client);
  gHotspots.clear();
  gTexts.clear();
  gIconDraws.clear();
  {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    fillRect(graphics, client, gTheme->bg);
    drawSidebar(graphics, client.bottom);
    switch (gPage) {
      case Page::Home: drawHome(graphics, client.right, client.bottom); break;
      case Page::Plugins: drawPluginsPage(graphics, client.right, client.bottom); break;
      case Page::Marketplace: drawMarketplace(graphics, client.right, client.bottom); break;
      case Page::Settings: drawSettings(graphics, client.right, client.bottom); break;
    }
    drawTitlebar(graphics, client.right, IsZoomed(window) != FALSE);
  }
  flushText(dc);
  flushIcons(dc);

  const bool statusVisible = !gStatusText.empty() && GetTickCount64() - gStatusTick <= 9000;
  if (statusVisible || gMenuIndex >= 0) {
    gTexts.clear();
    gIconDraws.clear();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    if (gMenuIndex >= 0) drawPopupMenu(graphics, client.right, client.bottom);
    drawToast(graphics, client.right, client.bottom);
    flushText(dc);
    flushIcons(dc);
  }
}

void layoutSettingsEdit(HWND edit, const RECT& rect, const RECT& view) {
  if (!edit) return;
  if (rect.left < view.left || rect.right > view.right ||
      rect.top < view.top || rect.bottom > view.bottom) {
    ShowWindow(edit, SW_HIDE);
    return;
  }
  SetWindowPos(edit, nullptr, rect.left + scale(9), rect.top + scale(5),
               rect.right - rect.left - scale(18), rect.bottom - rect.top - scale(10),
               SWP_NOZORDER | SWP_NOACTIVATE);
  ShowWindow(edit, SW_SHOW);
}

void layoutNativeControls(HWND window) {
  if (!gGamePathEdit && !gDownloadPathEdit) return;
  if (gPage != Page::Settings) {
    if (gGamePathEdit) ShowWindow(gGamePathEdit, SW_HIDE);
    if (gDownloadPathEdit) ShowWindow(gDownloadPathEdit, SW_HIDE);
    return;
  }
  RECT client{};
  GetClientRect(window, &client);
  const RECT view{settingsCardLeft(), settingsCardsTop(), settingsRight(client.right),
                  settingsViewBottom(client.bottom)};
  layoutSettingsEdit(gGamePathEdit, settingsGamePathRect(client.right), view);
  layoutSettingsEdit(gDownloadPathEdit, settingsDownloadPathRect(client.right), view);
}

void scrollToSettingsSection(int index) {
  gSettingsCategory = clampInt(index, 0, 6);
  RECT client{};
  if (gMainWindow) GetClientRect(gMainWindow, &client);
  const int viewHeight = settingsViewBottom(client.bottom) - settingsCardsTop();
  const int maximum = settingsContentHeight() > viewHeight
    ? settingsContentHeight() - viewHeight + scale(8) : 0;
  gSettingsScrollMax = maximum;
  gSettingsScroll = clampInt(settingsSectionTop(index), 0, maximum);
  layoutNativeControls(gMainWindow);
  InvalidateRect(gMainWindow, nullptr, FALSE);
}

void updateSettingsCategoryFromScroll() {
  if (gSettingsScroll >= gSettingsScrollMax - scale(2)) {
    gSettingsCategory = 6;
    return;
  }
  int active = 0;
  for (int i = 0; i < 7; ++i) {
    if (settingsSectionTop(i) <= gSettingsScroll + scale(80)) active = i;
  }
  gSettingsCategory = active;
}

void changePage(Page page) {
  gPage = page;
  gMenuIndex = -1;
  gPressedId = 0;
  if (gRememberPage && !gLoadingSettings) saveSettings();
  layoutNativeControls(gMainWindow);
  InvalidateRect(gMainWindow, nullptr, FALSE);
}

void openPluginMenuAt(size_t index) {
  if (index >= gPluginViews.size()) return;
  gMenuIndex = static_cast<int>(index);
  gMenuAnchor = gMousePoint;
  InvalidateRect(gMainWindow, nullptr, FALSE);
}

void activateHotspot(int id) {
  if (id >= UI_MENU_ITEM && id < UI_MENU_ITEM + 3) {
    const size_t index = static_cast<size_t>(gMenuIndex < 0 ? 0 : gMenuIndex);
    if (gMenuIndex >= 0 && index < gPluginViews.size()) {
      if (id == UI_MENU_ITEM) togglePluginAt(index);
      else if (id == UI_MENU_ITEM + 1) openPluginLocationAt(index);
      else removePluginAt(index);
    }
    gMenuIndex = -1;
    InvalidateRect(gMainWindow, nullptr, FALSE);
    return;
  }
  if (id >= UI_ROW_TOGGLE && id < UI_ROW_TOGGLE + 1000) {
    togglePluginAt(static_cast<size_t>(id - UI_ROW_TOGGLE));
    return;
  }
  if (id >= UI_ROW_MENU && id < UI_ROW_MENU + 1000) {
    openPluginMenuAt(static_cast<size_t>(id - UI_ROW_MENU));
    return;
  }

  switch (id) {
    case UI_NAV_HOME: changePage(Page::Home); break;
    case UI_NAV_PLUGINS: changePage(Page::Plugins); break;
    case UI_NAV_MARKETPLACE: changePage(Page::Marketplace); break;
    case UI_NAV_SETTINGS: changePage(Page::Settings); break;
    case UI_THEME_TOGGLE: setThemeMode(!gDarkMode); break;
    case UI_THEME_DARK: setThemeMode(true); break;
    case UI_THEME_LIGHT: setThemeMode(false); break;
    case UI_MINIMIZE: ShowWindow(gMainWindow, SW_MINIMIZE); break;
    case UI_MAXIMIZE:
      ShowWindow(gMainWindow, IsZoomed(gMainWindow) ? SW_RESTORE : SW_MAXIMIZE);
      break;
    case UI_CLOSE: PostMessageW(gMainWindow, WM_CLOSE, 0, 0); break;
    case UI_LAUNCH: launchGameFlow(); break;
    case UI_HERO_SETTINGS: changePage(Page::Settings); break;
    case UI_STAT_PLUGINS:
    case UI_STAT_ACTIVE:
      changePage(Page::Plugins);
      break;
    case UI_STAT_UPDATES:
      changePage(Page::Settings);
      scrollToSettingsSection(1);
      break;
    case UI_STAT_GAME: if (!gGameRunning) launchGameFlow(); break;
    case UI_QUICK_ADD:
    case UI_ADD_PLUGIN:
    case UI_EMPTY_ADD:
      addPluginFlow();
      break;
    case UI_QUICK_FOLDER:
    case UI_OPEN_FOLDER:
      openFolderFlow();
      break;
    case UI_QUICK_UPDATES:
    case UI_CHECK_UPDATES:
      checkUpdatesFlow();
      break;
    case UI_VIEW_PLUGINS: changePage(Page::Plugins); break;
    case UI_BROWSE_GAME: browseGameFlow(); break;
    case UI_AUTODETECT_GAME: autoDetectGameFlow(); break;
    case UI_SETTINGS_GENERAL: scrollToSettingsSection(0); break;
    case UI_SETTINGS_UPDATES: scrollToSettingsSection(1); break;
    case UI_SETTINGS_GAME: scrollToSettingsSection(2); break;
    case UI_SETTINGS_PLUGINS: scrollToSettingsSection(3); break;
    case UI_SETTINGS_DOWNLOADS: scrollToSettingsSection(4); break;
    case UI_SETTINGS_APPEARANCE: scrollToSettingsSection(5); break;
    case UI_SETTINGS_ADVANCED: scrollToSettingsSection(6); break;
    case UI_SETTINGS_SAVE: saveSettingsFlow(); break;
    case UI_SETTINGS_RESET: resetSettingsFlow(); break;
    case UI_SETTINGS_OPEN_DOWNLOAD: openDownloadsFolderFlow(); break;
    case UI_SETTINGS_OPEN_DATA: openDataFolderFlow(); break;
    case UI_SETTINGS_LANGUAGE:
      setStatus(L"English is the only language available right now.");
      break;
    case UI_SETTINGS_BROWSE_DOWNLOAD: {
      std::wstring folder;
      if (chooseFolder(gMainWindow, folder)) {
        gLoadingSettings = true;
        gDownloadPath = folder;
        if (gDownloadPathEdit) SetWindowTextW(gDownloadPathEdit, gDownloadPath.c_str());
        gLoadingSettings = false;
        saveSettings();
        layoutNativeControls(gMainWindow);
        InvalidateRect(gMainWindow, nullptr, FALSE);
        setStatus(L"Plugin download location updated.");
      }
      break;
    }
    case UI_TOGGLE_STARTUP:
      gStartWithWindows = !gStartWithWindows;
      applyStartWithWindows();
      saveSettings();
      break;
    case UI_TOGGLE_MINIMIZE:
      gMinimizeToTray = !gMinimizeToTray;
      if (gMinimizeToTray) addTrayIcon();
      else removeTrayIcon();
      saveSettings();
      break;
    case UI_TOGGLE_CHECK:
      gCheckUpdatesStartup = !gCheckUpdatesStartup;
      saveSettings();
      break;
    case UI_TOGGLE_REMEMBER:
      gRememberPage = !gRememberPage;
      saveSettings();
      break;
    case UI_TOGGLE_AUTOLOAD:
      gAutoLoadMods = !gAutoLoadMods;
      saveSettings();
      break;
    case UI_TOGGLE_CONFIRM:
      gConfirmRemove = !gConfirmRemove;
      saveSettings();
      break;
    case UI_TOGGLE_STEAM:
      gLaunchViaSteam = !gLaunchViaSteam;
      saveSettings();
      break;
    default: break;
  }
}

void updateHover(int x, int y) {
  gMousePoint.x = x;
  gMousePoint.y = y;
  const int id = hotspotAt(x, y);
  if (id != gHoverId) {
    gHoverId = id;
    InvalidateRect(gMainWindow, nullptr, FALSE);
  }
}

void handleWheel(int delta) {
  int* scroll = nullptr;
  int maximum = 0;
  if (gPage == Page::Plugins) {
    scroll = &gPluginScroll;
    maximum = gPluginScrollMax;
  } else if (gPage == Page::Settings) {
    scroll = &gSettingsScroll;
    maximum = gSettingsScrollMax;
  }
  if (!scroll) return;
  const int step = delta > 0 ? -scale(64) : scale(64);
  const int next = clampInt(*scroll + step, 0, maximum);
  if (next != *scroll) {
    *scroll = next;
    if (gPage == Page::Settings) {
      updateSettingsCategoryFromScroll();
      layoutNativeControls(gMainWindow);
    }
    InvalidateRect(gMainWindow, nullptr, FALSE);
  }
}
