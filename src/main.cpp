#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <uxtheme.h>
#include <winver.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cwctype>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "loader_controller.h"

namespace {

constexpr UINT WM_INJECTION_COMPLETE = WM_USER + 101;
constexpr UINT WM_UPDATE_CHECK_COMPLETE = WM_USER + 102;

constexpr int kIdGamePath = 1001;
constexpr int kIdDownloadPath = 1002;
constexpr UINT_PTR kAutoInjectTimer = 1;
constexpr UINT_PTR kGameStatusTimer = 2;
constexpr UINT_PTR kStatusTimer = 3;
constexpr UINT kAutoInjectPollMs = 500;
constexpr UINT kGameStatusPollMs = 2500;
constexpr UINT kTrayMessage = WM_USER + 110;
constexpr int kTrayOpen = 40001;
constexpr int kTrayExit = 40002;

constexpr wchar_t kAppVersion[] = L"0.1.1";
constexpr wchar_t kReleasePageUrl[] = L"https://github.com/HerokeyVN/SkyLoader/releases";
constexpr wchar_t kSkySteamUri[] = L"steam://run/2325290";

struct PluginItem {
  std::wstring path;
  bool enabled{true};
};

struct PluginMetadata {
  std::wstring id;
  std::wstring version;
  std::wstring name;
  std::wstring author;
  std::wstring description;
};

struct PluginView {
  std::wstring name;
  std::wstring version;
  std::wstring author;
  std::wstring description;
  bool enabled{true};
};

enum class Page {
  Home,
  Plugins,
  Marketplace,
  Settings,
};

struct UpdateCheckResult {
  bool manual{};
  bool ok{};
  bool updateAvailable{};
  std::wstring latestVersion;
  std::wstring releaseUrl;
  std::wstring assetUrl;
  std::wstring downloadedPath;
  std::wstring message;
};

HWND gMainWindow = nullptr;
HWND gGamePathEdit = nullptr;
HWND gDownloadPathEdit = nullptr;
HICON gWindowIcon = nullptr;
ULONG_PTR gGdiplusToken = 0;
int gDpi = 96;

std::wstring gIniPath;
std::wstring gBootstrapPath;
std::wstring gGamePathValue;
std::wstring gStatusText;
ULONGLONG gStatusTick = 0;
std::wstring gLatestVersion;
std::wstring gUpdateDownloadedPath;
std::wstring gUpdateMessage;
std::wstring gDownloadPath;
bool gDarkMode = true;
bool gThemeReady = false;
bool gGameRunning = false;
bool gUpdateAvailable = false;
bool gUpdateCheckInProgress = false;
bool gUpdateFailed = false;
bool gAutoInjectPending = false;
bool gLoadingSettings = false;
bool gStartWithWindows = false;
bool gMinimizeToTray = false;
bool gCheckUpdatesStartup = true;
bool gRememberPage = true;
bool gAutoLoadMods = true;
bool gLaunchViaSteam = true;
bool gConfirmRemove = true;
bool gTrayIconAdded = false;
bool gExiting = false;
DWORD gPendingSkyPid = 0;

Page gPage = Page::Home;
std::vector<PluginItem> gPlugins;
std::vector<PluginView> gPluginViews;

void refreshPluginViews();
bool chooseFolder(HWND owner, std::wstring& output);
void saveSettings();
void setThemeMode(bool dark);
void applyStartWithWindows();
void addTrayIcon();
void removeTrayIcon();
void openDownloadsFolderFlow();
void openDataFolderFlow();
void resetSettingsFlow();
void saveSettingsFlow();
void addPluginFlow();
void browseGameFlow();
void autoDetectGameFlow();
void launchGameFlow();
void checkUpdatesFlow();
void openFolderFlow();
void removePluginAt(size_t index);
void togglePluginAt(size_t index);
void openPluginLocationAt(size_t index);

#include "utils/utils.inl"
#include "plugins/plugins.inl"
#include "update/update.inl"
#include "ui/theme.inl"
#include "ui/drawing.inl"
#include "ui/interface.inl"

void refreshPluginViews() {
  gPluginViews.clear();
  gPluginViews.reserve(gPlugins.size());
  for (const PluginItem& item : gPlugins) {
    PluginView view;
    PluginMetadata metadata;
    const bool hasMetadata = readPluginMetadata(item.path, metadata);
    view.name = hasMetadata && !metadata.name.empty() ? metadata.name : fileNameOnly(item.path);
    view.version = hasMetadata ? metadata.version : L"";
    view.author = hasMetadata ? metadata.author : L"";
    view.description = hasMetadata ? metadata.description : L"";
    view.enabled = item.enabled;
    gPluginViews.push_back(std::move(view));
  }
  if (gMainWindow) InvalidateRect(gMainWindow, nullptr, FALSE);
}

bool fileExists(const std::wstring& path) {
  if (path.empty()) return false;
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring getSteamPathFromRegistry() {
  wchar_t buffer[MAX_PATH]{};
  DWORD size = sizeof(buffer);
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath",
                   RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS && buffer[0]) {
    std::wstring path(buffer);
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
  }
  size = sizeof(buffer);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath",
                   RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS && buffer[0]) {
    std::wstring path(buffer);
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
  }
  size = sizeof(buffer);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath",
                   RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS && buffer[0]) {
    std::wstring path(buffer);
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
  }
  return L"";
}

std::vector<std::wstring> getSteamLibraryFolders(const std::wstring& steamPath) {
  std::vector<std::wstring> folders;
  if (steamPath.empty()) return folders;
  folders.push_back(steamPath);

  const std::wstring vdfPath = steamPath + L"\\steamapps\\libraryfolders.vdf";
  HANDLE file = CreateFileW(vdfPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    const DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize != INVALID_FILE_SIZE && fileSize > 0 && fileSize < 5 * 1024 * 1024) {
      std::string content(fileSize, '\0');
      DWORD readBytes = 0;
      if (ReadFile(file, &content[0], fileSize, &readBytes, nullptr) && readBytes > 0) {
        size_t pos = 0;
        while ((pos = content.find("\"path\"", pos)) != std::string::npos) {
          pos += 6;
          const size_t startQuote = content.find('"', pos);
          if (startQuote == std::string::npos) break;
          const size_t endQuote = content.find('"', startQuote + 1);
          if (endQuote == std::string::npos) break;

          const std::string rawPath = content.substr(startQuote + 1, endQuote - startQuote - 1);
          std::string unescaped;
          for (size_t i = 0; i < rawPath.size(); ++i) {
            if (rawPath[i] == '\\' && i + 1 < rawPath.size() && rawPath[i + 1] == '\\') {
              unescaped += '\\';
              ++i;
            } else {
              unescaped += rawPath[i];
            }
          }
          if (!unescaped.empty()) {
            wchar_t wide[MAX_PATH]{};
            if (MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, wide, MAX_PATH)) {
              std::wstring folder(wide);
              std::replace(folder.begin(), folder.end(), L'/', L'\\');
              if (std::find_if(folders.begin(), folders.end(), [&](const std::wstring& existing) {
                    return samePath(existing, folder);
                  }) == folders.end()) {
                folders.push_back(folder);
              }
            }
          }
          pos = endQuote + 1;
        }
      }
    }
    CloseHandle(file);
  }
  return folders;
}

std::wstring autoDetectSkyGamePath() {
  const std::wstring relative = L"\\steamapps\\common\\Sky Children of the Light\\Sky.exe";
  const std::wstring steamPath = getSteamPathFromRegistry();
  if (!steamPath.empty()) {
    for (const std::wstring& library : getSteamLibraryFolders(steamPath)) {
      const std::wstring candidate = library + relative;
      if (fileExists(candidate)) return normalizedPath(candidate);
    }
  }

  wchar_t drives[512]{};
  if (GetLogicalDriveStringsW(512, drives)) {
    const wchar_t* drive = drives;
    while (*drive) {
      std::wstring root(drive);
      if (!root.empty() && root.back() == L'\\') root.pop_back();
      const std::wstring prefixes[] = {
        L"", L"\\SteamLibrary", L"\\Steam", L"\\Program Files (x86)\\Steam",
        L"\\Program Files\\Steam", L"\\Games\\SteamLibrary", L"\\Games\\Steam", L"\\Games",
      };
      for (const std::wstring& prefix : prefixes) {
        const std::wstring candidate = root + prefix + relative;
        if (fileExists(candidate)) return normalizedPath(candidate);
      }
      drive += wcslen(drive) + 1;
    }
  }
  return L"";
}

void setEditTextStart(HWND edit, const std::wstring& text) {
  if (!edit) return;
  SetWindowTextW(edit, text.c_str());
  SendMessageW(edit, EM_SETSEL, 0, 0);
}

void setGamePath(const std::wstring& path) {
  gGamePathValue = path;
  if (gGamePathEdit) {
    SetWindowTextW(gGamePathEdit, path.c_str());
    SendMessageW(gGamePathEdit, EM_SETSEL, 0, 0);
  }
}

std::wstring skyLoaderRootDirectory() {
  wchar_t localAppData[MAX_PATH]{};
  if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData) != S_OK)
    return L"";
  const std::wstring root = std::wstring(localAppData) + L"\\SkyLoader";
  CreateDirectoryW(root.c_str(), nullptr);
  return root;
}

std::wstring defaultDownloadDirectory() {
  const std::wstring root = skyLoaderRootDirectory();
  return root.empty() ? L"" : root + L"\\downloads";
}

void applyStartWithWindows() {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    return;
  if (gStartWithWindows) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const std::wstring value = L"\"" + std::wstring(exePath) + L"\"";
    RegSetValueExW(key, L"SkyLoader", 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, L"SkyLoader");
  }
  RegCloseKey(key);
}

void addTrayIcon() {
  if (gTrayIconAdded || !gMainWindow) return;
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = gMainWindow;
  data.uID = 1;
  data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  data.uCallbackMessage = kTrayMessage;
  data.hIcon = gWindowIcon ? gWindowIcon : LoadIconW(nullptr, IDI_APPLICATION);
  lstrcpynW(data.szTip, L"SkyLoader", ARRAYSIZE(data.szTip));
  if (Shell_NotifyIconW(NIM_ADD, &data)) gTrayIconAdded = true;
}

void removeTrayIcon() {
  if (!gTrayIconAdded) return;
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = gMainWindow;
  data.uID = 1;
  Shell_NotifyIconW(NIM_DELETE, &data);
  gTrayIconAdded = false;
}

void saveSettings() {
  WritePrivateProfileStringW(L"SkyLoader", L"GamePath", gGamePathValue.c_str(), gIniPath.c_str());
  WritePrivateProfileStringW(L"SkyLoader", L"BootstrapPath", gBootstrapPath.c_str(), gIniPath.c_str());
  WritePrivateProfileStringW(L"SkyLoader", L"Theme", gDarkMode ? L"dark" : L"light", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"StartWithWindows", gStartWithWindows ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"MinimizeToTray", gMinimizeToTray ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"CheckUpdates", gCheckUpdatesStartup ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"RememberPage", gRememberPage ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"AutoLoadMods", gAutoLoadMods ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"LaunchViaSteam", gLaunchViaSteam ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"ConfirmRemove", gConfirmRemove ? L"1" : L"0", gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"DownloadPath", gDownloadPath.c_str(), gIniPath.c_str());
  WritePrivateProfileStringW(L"Settings", L"LastPage",
                             std::to_wstring(static_cast<int>(gPage)).c_str(), gIniPath.c_str());
  WritePrivateProfileStringW(L"Dlls", L"Count", std::to_wstring(gPlugins.size()).c_str(), gIniPath.c_str());

  for (size_t i = 0; i < gPlugins.size(); ++i) {
    const std::wstring key = L"Dll" + std::to_wstring(i);
    const std::wstring enabledKey = L"DllEnabled" + std::to_wstring(i);
    WritePrivateProfileStringW(L"Dlls", key.c_str(), gPlugins[i].path.c_str(), gIniPath.c_str());
    WritePrivateProfileStringW(L"Dlls", enabledKey.c_str(), gPlugins[i].enabled ? L"1" : L"0",
                               gIniPath.c_str());
  }
  for (size_t i = gPlugins.size(); i < 256; ++i) {
    const std::wstring key = L"Dll" + std::to_wstring(i);
    const std::wstring enabledKey = L"DllEnabled" + std::to_wstring(i);
    WritePrivateProfileStringW(L"Dlls", key.c_str(), nullptr, gIniPath.c_str());
    WritePrivateProfileStringW(L"Dlls", enabledKey.c_str(), nullptr, gIniPath.c_str());
  }
}

void loadSettings() {
  gLoadingSettings = true;
  const wchar_t* fallback =
    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Sky Children of the Light\\Sky.exe";
  wchar_t savedGamePath[MAX_PATH]{};
  GetPrivateProfileStringW(L"SkyLoader", L"GamePath", L"", savedGamePath, MAX_PATH, gIniPath.c_str());

  std::wstring gamePath = savedGamePath;
  if (gamePath.empty() || !fileExists(gamePath)) {
    const std::wstring detected = autoDetectSkyGamePath();
    if (!detected.empty()) {
      gamePath = detected;
      WritePrivateProfileStringW(L"SkyLoader", L"GamePath", gamePath.c_str(), gIniPath.c_str());
      setStatus(L"Sky.exe was auto-detected in your Steam library.");
    } else {
      gamePath = fallback;
      if (!fileExists(gamePath))
        setStatus(L"Sky.exe was not found. Choose the executable in Settings.");
    }
  }
  setGamePath(gamePath);

  const std::wstring defaultBootstrap = moduleDirectory() + L"\\SkyBootstrap.dll";
  wchar_t bootstrapPath[MAX_PATH]{};
  GetPrivateProfileStringW(L"SkyLoader", L"BootstrapPath", defaultBootstrap.c_str(),
                           bootstrapPath, MAX_PATH, gIniPath.c_str());
  gBootstrapPath = bootstrapPath;
  if (gBootstrapPath.empty()) gBootstrapPath = defaultBootstrap;
  const std::wstring legacyBootstrap = L"D:\\Develop\\Language\\C++\\SkyBootstrap\\dist\\SkyBootstrap.dll";
  if (samePath(gBootstrapPath, legacyBootstrap)) gBootstrapPath = defaultBootstrap;

  const UINT count = GetPrivateProfileIntW(L"Dlls", L"Count", 0, gIniPath.c_str());
  gPlugins.clear();
  for (UINT i = 0; i < count && i < 256; ++i) {
    const std::wstring key = L"Dll" + std::to_wstring(i);
    const std::wstring enabledKey = L"DllEnabled" + std::to_wstring(i);
    wchar_t value[MAX_PATH]{};
    GetPrivateProfileStringW(L"Dlls", key.c_str(), L"", value, MAX_PATH, gIniPath.c_str());
    if (value[0] && GetFileAttributesW(value) != INVALID_FILE_ATTRIBUTES) {
      const UINT enabled = GetPrivateProfileIntW(L"Dlls", enabledKey.c_str(), 1, gIniPath.c_str());
      gPlugins.push_back({value, enabled != 0});
    }
  }

  wchar_t theme[32]{};
  GetPrivateProfileStringW(L"SkyLoader", L"Theme", L"dark", theme, 32, gIniPath.c_str());
  gDarkMode = CompareStringOrdinal(theme, -1, L"light", -1, TRUE) != CSTR_EQUAL;

  gStartWithWindows = GetPrivateProfileIntW(L"Settings", L"StartWithWindows", 0, gIniPath.c_str()) != 0;
  gMinimizeToTray = GetPrivateProfileIntW(L"Settings", L"MinimizeToTray", 0, gIniPath.c_str()) != 0;
  gCheckUpdatesStartup = GetPrivateProfileIntW(L"Settings", L"CheckUpdates", 1, gIniPath.c_str()) != 0;
  gRememberPage = GetPrivateProfileIntW(L"Settings", L"RememberPage", 1, gIniPath.c_str()) != 0;
  gAutoLoadMods = GetPrivateProfileIntW(L"Settings", L"AutoLoadMods", 1, gIniPath.c_str()) != 0;
  gLaunchViaSteam = GetPrivateProfileIntW(L"Settings", L"LaunchViaSteam", 1, gIniPath.c_str()) != 0;
  gConfirmRemove = GetPrivateProfileIntW(L"Settings", L"ConfirmRemove", 1, gIniPath.c_str()) != 0;

  wchar_t downloadPath[MAX_PATH]{};
  const std::wstring defaultDownloads = defaultDownloadDirectory();
  GetPrivateProfileStringW(L"Settings", L"DownloadPath", defaultDownloads.c_str(), downloadPath,
                           MAX_PATH, gIniPath.c_str());
  gDownloadPath = downloadPath;
  if (gDownloadPath.empty()) gDownloadPath = defaultDownloads;

  setEditTextStart(gDownloadPathEdit, gDownloadPath);

  if (gRememberPage) {
    const int lastPage = GetPrivateProfileIntW(L"Settings", L"LastPage", 0, gIniPath.c_str());
    if (lastPage >= 0 && lastPage <= static_cast<int>(Page::Settings))
      gPage = static_cast<Page>(lastPage);
  }

  refreshPluginViews();
  gLoadingSettings = false;
}

void setThemeMode(bool dark) {
  gDarkMode = dark;
  applyThemePalette(dark);
  WritePrivateProfileStringW(L"SkyLoader", L"Theme", dark ? L"dark" : L"light", gIniPath.c_str());
}

void saveSettingsFlow() {
  applyStartWithWindows();
  saveSettings();
  setStatus(L"Settings saved.");
}

void openDownloadsFolderFlow() {
  if (gDownloadPath.empty()) return;
  CreateDirectoryW(gDownloadPath.c_str(), nullptr);
  if (GetFileAttributesW(gDownloadPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wstring error = L"Could not create the plugin download folder.";
    MessageBoxW(gMainWindow, error.c_str(), L"SkyLoader", MB_ICONWARNING);
    return;
  }
  ShellExecuteW(gMainWindow, L"open", gDownloadPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openDataFolderFlow() {
  const std::wstring root = skyLoaderRootDirectory();
  if (!root.empty()) ShellExecuteW(gMainWindow, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void resetSettingsFlow() {
  if (MessageBoxW(gMainWindow,
                  L"Reset all SkyLoader settings to their defaults?\n\nYour installed plugins are kept.",
                  L"Reset Settings", MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;

  gStartWithWindows = false;
  gMinimizeToTray = false;
  gCheckUpdatesStartup = true;
  gRememberPage = true;
  gAutoLoadMods = true;
  gLaunchViaSteam = true;
  gConfirmRemove = true;
  gDownloadPath = defaultDownloadDirectory();

  gLoadingSettings = true;
  setEditTextStart(gDownloadPathEdit, gDownloadPath);
  const std::wstring detected = autoDetectSkyGamePath();
  setGamePath(detected.empty() ? L"" : detected);
  gLoadingSettings = false;

  setThemeMode(true);
  applyStartWithWindows();
  removeTrayIcon();
  saveSettings();
  setStatus(L"Settings restored to defaults.");
}

bool chooseFile(HWND owner, const wchar_t* filter, std::wstring& output) {
  wchar_t path[MAX_PATH]{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = owner;
  dialog.lpstrFilter = filter;
  dialog.lpstrFile = path;
  dialog.nMaxFile = MAX_PATH;
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&dialog)) return false;
  output = normalizedPath(path);
  return true;
}

bool chooseFolder(HWND owner, std::wstring& output) {
  BROWSEINFOW info{};
  info.hwndOwner = owner;
  info.lpszTitle = L"Select the plugin download folder";
  info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  LPITEMIDLIST item = SHBrowseForFolderW(&info);
  if (!item) return false;
  wchar_t path[MAX_PATH]{};
  const bool ok = SHGetPathFromIDListW(item, path) != FALSE;
  CoTaskMemFree(item);
  if (!ok) return false;
  output = path;
  return true;
}

DWORD findSkyProcess(const std::wstring& expectedPath, std::wstring* actualDetectedPath = nullptr) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;

  PROCESSENTRY32W process{};
  process.dwSize = sizeof(process);
  DWORD pid = 0;
  if (Process32FirstW(snapshot, &process)) {
    do {
      if (CompareStringOrdinal(process.szExeFile, -1, L"Sky.exe", -1, TRUE) != CSTR_EQUAL) continue;
      HANDLE candidate = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.th32ProcessID);
      if (!candidate) continue;
      wchar_t imagePath[MAX_PATH]{};
      DWORD imageLength = MAX_PATH;
      const bool queried = QueryFullProcessImageNameW(candidate, 0, imagePath, &imageLength) != FALSE;
      CloseHandle(candidate);
      if (queried) {
        const std::wstring normalizedImage = normalizedPath(imagePath);
        if (expectedPath.empty() || samePath(normalizedImage, normalizedPath(expectedPath))) {
          pid = process.th32ProcessID;
          if (actualDetectedPath) *actualDetectedPath = normalizedImage;
          break;
        }
        if (pid == 0) {
          pid = process.th32ProcessID;
          if (actualDetectedPath) *actualDetectedPath = normalizedImage;
        }
      }
    } while (Process32NextW(snapshot, &process));
  }
  CloseHandle(snapshot);
  return pid;
}

void requestAutoInject() {
  gAutoInjectPending = true;
  gPendingSkyPid = 0;
}

void autoInjectViaController(HWND hwnd, DWORD pid) {
  std::vector<std::wstring> activeDlls;
  for (const PluginItem& item : gPlugins) {
    if (item.enabled) activeDlls.push_back(item.path);
  }
  const std::wstring bootstrap = gBootstrapPath;
  std::thread([pid, bootstrap, activeDlls, hwnd]() {
    LoaderController controller(bootstrap, activeDlls);
    const LoaderResult result = controller.installBootstrapAndPlugins(pid);
    auto* message = new std::wstring(result.message + L" (PID " + std::to_wstring(pid) + L").");
    PostMessageW(hwnd, WM_INJECTION_COMPLETE, reinterpret_cast<WPARAM>(message), result.ok ? 1 : 0);
  }).detach();
}

void addPluginFlow() {
  std::wstring path;
  if (!chooseFile(gMainWindow, L"Dynamic-link libraries (*.dll)\0*.dll\0\0", path)) return;
  std::wstring message;
  const PluginImportAction action = importAndRegisterPlugin(gMainWindow, path, message);
  if (action == PluginImportAction::Added || action == PluginImportAction::Updated) {
    refreshPluginViews();
    saveSettings();
  }
  setStatus(message);
}

void browseGameFlow() {
  std::wstring path;
  if (chooseFile(gMainWindow, L"Sky executable\0Sky.exe\0Executables\0*.exe\0\0", path)) {
    setGamePath(path);
    saveSettings();
    setStatus(L"Sky executable path updated.");
  }
}

void autoDetectGameFlow() {
  const std::wstring detected = autoDetectSkyGamePath();
  if (detected.empty()) {
    setStatus(L"Sky.exe could not be found in your Steam libraries.");
    return;
  }
  setGamePath(detected);
  saveSettings();
  setStatus(L"Sky.exe was auto-detected in your Steam library.");
}

void launchGameFlow() {
  std::wstring actualPath;
  const DWORD runningPid = findSkyProcess(gGamePathValue, &actualPath);
  requestAutoInject();
  if (runningPid) {
    if (!actualPath.empty() && !samePath(gGamePathValue, actualPath)) {
      setGamePath(actualPath);
      saveSettings();
    }
    gAutoInjectPending = false;
    setStatus(L"Sky.exe is already running. Loading the bootstrap now.");
    autoInjectViaController(gMainWindow, runningPid);
    return;
  }

  HINSTANCE result = nullptr;
  if (gLaunchViaSteam) {
    result = ShellExecuteW(gMainWindow, L"open", kSkySteamUri, nullptr, nullptr, SW_SHOWNORMAL);
    setStatus(reinterpret_cast<INT_PTR>(result) <= 32
      ? L"Could not open Steam. Start Steam and try again."
      : L"Steam is starting Sky. Waiting for Sky.exe...");
  } else {
    if (gGamePathValue.empty() || !fileExists(gGamePathValue)) {
      gAutoInjectPending = false;
      setStatus(L"Sky.exe was not found. Choose the executable in Settings.");
      return;
    }
    const size_t slash = gGamePathValue.find_last_of(L'\\');
    const std::wstring directory = slash == std::wstring::npos
      ? std::wstring() : gGamePathValue.substr(0, slash);
    result = ShellExecuteW(gMainWindow, L"open", gGamePathValue.c_str(), nullptr,
                           directory.empty() ? nullptr : directory.c_str(), SW_SHOWNORMAL);
    setStatus(reinterpret_cast<INT_PTR>(result) <= 32
      ? L"Could not start Sky.exe."
      : L"Sky is starting. Waiting for Sky.exe...");
  }
  saveSettings();
}

void checkUpdatesFlow() {
  startUpdateCheck(gMainWindow, true);
}

void openFolderFlow() {
  const std::wstring directory = managedPluginDirectory();
  if (!directory.empty())
    ShellExecuteW(gMainWindow, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openPluginLocationAt(size_t index) {
  if (index >= gPlugins.size()) return;
  const std::wstring arguments = L"/select,\"" + gPlugins[index].path + L"\"";
  ShellExecuteW(gMainWindow, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

void removePluginAt(size_t index) {
  if (index >= gPlugins.size()) return;
  if (gConfirmRemove) {
    const std::wstring name = index < gPluginViews.size() ? gPluginViews[index].name : fileNameOnly(gPlugins[index].path);
    const std::wstring prompt = L"Remove " + name + L" from your plugin list?\n\nThe original file is not deleted.";
    if (MessageBoxW(gMainWindow, prompt.c_str(), L"Remove Plugin", MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;
  }
  const std::wstring removedPath = gPlugins[index].path;
  gPlugins.erase(gPlugins.begin() + static_cast<std::ptrdiff_t>(index));
  refreshPluginViews();
  saveSettings();
  setStatus(L"Plugin removed from the list. The original file was not deleted.");

  const DWORD runningPid = findSkyProcess(gGamePathValue);
  if (runningPid) {
    const std::wstring bootstrap = gBootstrapPath;
    std::thread([runningPid, bootstrap, removedPath]() {
      LoaderController controller(bootstrap, {});
      const LoaderResult result = controller.unloadPlugin(runningPid, removedPath);
      auto* message = new std::wstring(result.message);
      PostMessageW(gMainWindow, WM_INJECTION_COMPLETE, reinterpret_cast<WPARAM>(message), result.ok ? 1 : 0);
    }).detach();
  }
}

void togglePluginAt(size_t index) {
  if (index >= gPlugins.size()) return;
  const bool enabled = !gPlugins[index].enabled;
  gPlugins[index].enabled = enabled;
  if (index < gPluginViews.size()) gPluginViews[index].enabled = enabled;
  saveSettings();
  InvalidateRect(gMainWindow, nullptr, FALSE);
  setStatus(enabled ? L"Plugin enabled." : L"Plugin disabled.");

  const DWORD runningPid = findSkyProcess(gGamePathValue);
  if (!runningPid) return;
  const std::wstring pluginPath = gPlugins[index].path;
  const std::wstring bootstrap = gBootstrapPath;
  std::thread([runningPid, bootstrap, pluginPath, enabled]() {
    LoaderController controller(bootstrap, {});
    const LoaderResult result = enabled ? controller.loadPlugin(runningPid, pluginPath)
                                        : controller.unloadPlugin(runningPid, pluginPath);
    auto* message = new std::wstring(result.message);
    PostMessageW(gMainWindow, WM_INJECTION_COMPLETE, reinterpret_cast<WPARAM>(message), result.ok ? 1 : 0);
  }).detach();
}

void paintWindow(HWND window) {
  PAINTSTRUCT paint{};
  HDC dc = BeginPaint(window, &paint);
  RECT client{};
  GetClientRect(window, &client);
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  if (width > 0 && height > 0) {
    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
    HGDIOBJ previous = SelectObject(memory, bitmap);
    paintInterface(window, memory);
    BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
  }
  EndPaint(window, &paint);
}

void layoutWindow(HWND window) {
  RECT client{};
  GetClientRect(window, &client);
  layoutNativeControls(window);
  InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      gMainWindow = window;
      gDpi = GetDeviceCaps(GetDC(nullptr), LOGPIXELSX);
      initThemes();
      createUiFonts();
      gMeasureDC = CreateCompatibleDC(nullptr);
      gThemeReady = true;

      HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
      gWindowIcon = static_cast<HICON>(LoadImageW(instance, L"IDI_SKYLOADER_ICON", IMAGE_ICON, 64, 64,
                                                  LR_DEFAULTCOLOR));
      if (!gWindowIcon) gWindowIcon = LoadIconW(nullptr, IDI_APPLICATION);
      gAppIcon = gWindowIcon;

      DragAcceptFiles(window, TRUE);

      gGamePathEdit = CreateWindowExW(0, L"EDIT", L"",
                                      WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(kIdGamePath), instance, nullptr);
      SendMessageW(gGamePathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(gFonts.bodySm), TRUE);
      SendMessageW(gGamePathEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));

      gDownloadPathEdit = CreateWindowExW(0, L"EDIT", L"",
                                          WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                                          0, 0, 0, 0, window, reinterpret_cast<HMENU>(kIdDownloadPath),
                                          instance, nullptr);
      SendMessageW(gDownloadPathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(gFonts.bodySm), TRUE);
      SendMessageW(gDownloadPathEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));

      loadSettings();
      applyThemePalette(gDarkMode);
      applyStartWithWindows();
      if (gMinimizeToTray) addTrayIcon();
      layoutNativeControls(window);
      requestAutoInject();
      if (gCheckUpdatesStartup) startUpdateCheck(window, false);
      SetTimer(window, kAutoInjectTimer, kAutoInjectPollMs, nullptr);
      SetTimer(window, kGameStatusTimer, kGameStatusPollMs, nullptr);
      return 0;
    }

    case WM_NCACTIVATE:
      return TRUE;

    case WM_NCPAINT:
      return 0;

    case WM_NCCALCSIZE:
      if (wParam) {
        if (IsZoomed(window)) {
          auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
          const int frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
          const int frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
          params->rgrc[0].left += frameX;
          params->rgrc[0].right -= frameX;
          params->rgrc[0].top += frameY;
          params->rgrc[0].bottom -= frameY;
        }
        return 0;
      }
      break;

    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(window, &point);
      RECT client{};
      GetClientRect(window, &client);
      const int border = scale(7);
      if (!IsZoomed(window)) {
        const bool left = point.x < border;
        const bool right = point.x >= client.right - border;
        const bool top = point.y < border;
        const bool bottom = point.y >= client.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
      }
      if (point.y >= 0 && point.y < scale(62) && point.x < client.right - scale(190)) return HTCAPTION;
      return HTCLIENT;
    }

    case WM_SIZE:
      layoutWindow(window);
      return 0;

    case WM_DPICHANGED: {
      gDpi = HIWORD(wParam);
      destroyUiFonts();
      createUiFonts();
      SendMessageW(gGamePathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(gFonts.bodySm), TRUE);
      SendMessageW(gDownloadPathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(gFonts.bodySm), TRUE);
      const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
      SetWindowPos(window, nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left, suggested->bottom - suggested->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      layoutWindow(window);
      return 0;
    }

    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<LPMINMAXINFO>(lParam);
      info->ptMinTrackSize.x = scale(1180);
      info->ptMinTrackSize.y = scale(720);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT:
      paintWindow(window);
      return 0;

    case WM_PRINTCLIENT:
      paintInterface(window, reinterpret_cast<HDC>(wParam));
      return 0;

    case WM_MOUSEMOVE: {
      if (!gMouseTracked) {
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = window;
        TrackMouseEvent(&track);
        gMouseTracked = true;
      }
      updateHover(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      return 0;
    }

    case WM_MOUSELEAVE:
      gMouseTracked = false;
      if (gHoverId != 0) {
        gHoverId = 0;
        InvalidateRect(window, nullptr, FALSE);
      }
      return 0;

    case WM_LBUTTONDOWN: {
      SetFocus(window);
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);
      gMousePoint = {x, y};
      const int hit = hotspotAt(x, y);
      gPressedId = hit;
      SetCapture(window);
      if (gMenuIndex >= 0) {
        if (!(hit >= UI_MENU_ITEM && hit < UI_MENU_ITEM + 3)) {
          gMenuIndex = -1;
          gPressedId = 0;
        }
      }
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }

    case WM_LBUTTONUP: {
      ReleaseCapture();
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);
      const int hit = hotspotAt(x, y);
      const int pressed = gPressedId;
      gPressedId = 0;
      if (gMenuIndex >= 0) {
        if (hit >= UI_MENU_ITEM && hit < UI_MENU_ITEM + 3) activateHotspot(hit);
        else gMenuIndex = -1;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
      }
      if (hit != 0 && hit == pressed) activateHotspot(hit);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }

    case WM_SETCURSOR:
      if (LOWORD(lParam) == HTCLIENT && gHoverId != UI_NONE) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
      }
      break;

    case WM_MOUSEWHEEL:
      if (gMenuIndex >= 0) {
        gMenuIndex = -1;
        InvalidateRect(window, nullptr, FALSE);
      }
      handleWheel(GET_WHEEL_DELTA_WPARAM(wParam));
      return 0;

    case WM_CTLCOLOREDIT: {
      HDC dc = reinterpret_cast<HDC>(wParam);
      SetTextColor(dc, gTheme->text);
      SetBkColor(dc, gTheme->input);
      return reinterpret_cast<INT_PTR>(gInputBrush);
    }

    case WM_COMMAND:
      if (HIWORD(wParam) == EN_CHANGE) {
        const int id = LOWORD(wParam);
        if (id == kIdGamePath) {
          wchar_t path[MAX_PATH]{};
          GetWindowTextW(gGamePathEdit, path, MAX_PATH);
          gGamePathValue = path;
        } else if (id == kIdDownloadPath) {
          wchar_t path[MAX_PATH]{};
          GetWindowTextW(gDownloadPathEdit, path, MAX_PATH);
          gDownloadPath = path;
        }
        if (!gLoadingSettings) saveSettings();
        return 0;
      }
      break;

    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wParam);
      const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
      bool added = false;
      for (UINT i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH]{};
        if (!DragQueryFileW(drop, i, path, MAX_PATH)) continue;
        const std::wstring file = normalizedPath(path);
        if (file.size() < 4 || CompareStringOrdinal(file.c_str() + file.size() - 4, -1, L".dll", -1, TRUE) != CSTR_EQUAL)
          continue;
        std::wstring message;
        const PluginImportAction action = importAndRegisterPlugin(window, file, message);
        if (action == PluginImportAction::Added || action == PluginImportAction::Updated) added = true;
        else if (action == PluginImportAction::Failed) setStatus(message);
      }
      DragFinish(drop);
      if (added) {
        refreshPluginViews();
        saveSettings();
        setStatus(L"Plugins added by drag and drop.");
      }
      return 0;
    }

    case WM_TIMER:
      if (wParam == kAutoInjectTimer && gAutoInjectPending) {
        std::wstring actualPath;
        const DWORD pid = findSkyProcess(gGamePathValue, &actualPath);
        if (!pid) return 0;
        if (!actualPath.empty() && !samePath(gGamePathValue, actualPath)) {
          setGamePath(actualPath);
          saveSettings();
        }
        gAutoInjectPending = false;
        if (pid != gPendingSkyPid) gPendingSkyPid = pid;
        setStatus(L"Sky.exe detected. Loading the bootstrap...");
        autoInjectViaController(window, pid);
        return 0;
      }
      if (wParam == kGameStatusTimer) {
        const bool running = findSkyProcess(gGamePathValue) != 0;
        if (running != gGameRunning) {
          gGameRunning = running;
          InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
      }
      if (wParam == kStatusTimer) {
        KillTimer(window, kStatusTimer);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
      }
      return 0;

    case WM_INJECTION_COMPLETE: {
      std::unique_ptr<std::wstring> message(reinterpret_cast<std::wstring*>(wParam));
      if (message) setStatus(*message);
      return 0;
    }

    case WM_UPDATE_CHECK_COMPLETE: {
      std::unique_ptr<UpdateCheckResult> result(reinterpret_cast<UpdateCheckResult*>(wParam));
      gUpdateCheckInProgress = false;
      if (!result) return 0;
      gUpdateAvailable = result->ok && result->updateAvailable;
      gUpdateFailed = !result->ok;
      gLatestVersion = result->latestVersion;
      gUpdateDownloadedPath = result->downloadedPath;
      gUpdateMessage = result->message.empty() ? L"Could not check for updates." : result->message;
      InvalidateRect(window, nullptr, FALSE);
      if (gUpdateAvailable) {
        setStatus(result->message);
        const std::wstring prompt = L"SkyLoader " + result->latestVersion +
          L" has been downloaded.\n\nRun the downloaded updater now?";
        if (MessageBoxW(window, prompt.c_str(), L"Update Available",
                        MB_YESNO | MB_ICONINFORMATION) == IDYES) {
          const HINSTANCE launched = ShellExecuteW(window, L"open", result->downloadedPath.c_str(),
                                                   nullptr, nullptr, SW_SHOWNORMAL);
          if (reinterpret_cast<INT_PTR>(launched) <= 32) {
            setStatus(L"The downloaded update could not be started.");
            MessageBoxW(window, L"The update was downloaded, but Windows could not start it.",
                        L"SkyLoader Updates", MB_ICONWARNING);
          }
        }
      } else if (result->manual) {
        const std::wstring text = result->message.empty() ? L"Could not check for updates." : result->message;
        setStatus(text);
        MessageBoxW(window, text.c_str(), L"SkyLoader Updates",
                    result->ok ? MB_ICONINFORMATION : MB_ICONWARNING);
      }
      return 0;
    }

    case kTrayMessage:
      if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
        ShowWindow(window, SW_SHOW);
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
        return 0;
      }
      if (lParam == WM_RBUTTONUP) {
        POINT point{};
        GetCursorPos(&point);
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kTrayOpen, L"Open SkyLoader");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayExit, L"Exit");
        SetForegroundWindow(window);
        const int command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                           point.x, point.y, 0, window, nullptr);
        DestroyMenu(menu);
        if (command == kTrayOpen) {
          ShowWindow(window, SW_SHOW);
          ShowWindow(window, SW_RESTORE);
          SetForegroundWindow(window);
        } else if (command == kTrayExit) {
          gExiting = true;
          removeTrayIcon();
          DestroyWindow(window);
        }
        PostMessageW(window, WM_NULL, 0, 0);
        return 0;
      }
      return 0;

    case WM_CLOSE:
      if (gMinimizeToTray && !gExiting) {
        ShowWindow(window, SW_HIDE);
        addTrayIcon();
        return 0;
      }
      DestroyWindow(window);
      return 0;

    case WM_DESTROY:
      KillTimer(window, kAutoInjectTimer);
      KillTimer(window, kGameStatusTimer);
      KillTimer(window, kStatusTimer);
      removeTrayIcon();
      saveSettings();
      destroyUiFonts();
      if (gMeasureDC) {
        DeleteDC(gMeasureDC);
        gMeasureDC = nullptr;
      }
      if (gInputBrush) {
        DeleteObject(gInputBrush);
        gInputBrush = nullptr;
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
  if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
    using SetDpiContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if (auto setDpi = reinterpret_cast<SetDpiContext>(reinterpret_cast<void*>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext")))) {
      setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
      SetProcessDPIAware();
    }
  }

  Gdiplus::GdiplusStartupInput input;
  if (Gdiplus::GdiplusStartup(&gGdiplusToken, &input, nullptr) != Gdiplus::Ok) return 1;

  const std::wstring directory = moduleDirectory();
  gIniPath = directory + L"\\SkyLoader.ini";
  gDpi = GetDeviceCaps(GetDC(nullptr), LOGPIXELSX);

  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = L"SkyLoaderWindow";
  windowClass.lpfnWndProc = windowProc;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hbrBackground = nullptr;
  windowClass.hIcon = static_cast<HICON>(LoadImageW(instance, L"IDI_SKYLOADER_ICON", IMAGE_ICON,
                                                   GetSystemMetrics(SM_CXICON),
                                                   GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
  windowClass.hIconSm = static_cast<HICON>(LoadImageW(instance, L"IDI_SKYLOADER_ICON", IMAGE_ICON,
                                                     GetSystemMetrics(SM_CXSMICON),
                                                     GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  if (!windowClass.hIcon) windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  if (!windowClass.hIconSm) windowClass.hIconSm = windowClass.hIcon;
  if (!RegisterClassExW(&windowClass)) return 1;

  HWND window = CreateWindowExW(WS_EX_APPWINDOW, windowClass.lpszClassName, L"SkyLoader",
                                WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                                WS_SYSMENU | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(1400, gDpi, 96),
                                MulDiv(900, gDpi, 96), nullptr, nullptr, instance, nullptr);
  if (!window) return 1;

  BOOL darkTitle = TRUE;
  DwmSetWindowAttribute(window, 20, &darkTitle, sizeof(darkTitle));
  int corners = 2;
  DwmSetWindowAttribute(window, 33, &corners, sizeof(corners));

  ShowWindow(window, showCommand);
  UpdateWindow(window);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  Gdiplus::GdiplusShutdown(gGdiplusToken);
  return static_cast<int>(message.wParam);
}
