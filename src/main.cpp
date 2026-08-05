#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "GameState.h"
#include "Log.h"
#include "scheme/SchemeInfo.h"

using namespace Gdiplus;

namespace {
struct Row { const wchar_t* key; const wchar_t* action; };
const Row kRows[] = {
 {L"Left/Right", L"Moves selected worm in respective direction."},
 {L"Shift + Left/Right", L"Forces worm to face one direction when moving."},
 {L"Shift or Middle Mouse Button", L"Speeds up mouse cursor movement by 4x separately or 16x together."},
 {L"Right Mouse Button", L"Opens and closes the weapon panel."},
 {L"Up/Down", L"Adjusts aim for the selected weapon."},
 {L"Spacebar", L"Fires or activates the selected weapon."},
 {L"Tab", L"Selects next worm on team when Worm Select is active."},
 {L"Enter", L"Performs a forward jump."},
 {L"Enter, Enter", L"Performs a backjump."},
 {L"Backspace", L"Jumps vertically."},
 {L"Backspace, Backspace", L"Performs a backflip."},
 {L"Backspace, Enter", L"Performs a backjump."},
 {L"Backspace, Backspace, Enter", L"Performs a small backflip."},
 {L"F1 - F12", L"Cycles through available weapons in weapon groups."},
 {L"` or ~", L"Cycles through available utilities."},
 {L"Pause/Break", L"Takes a screenshot, saved in the Capture folder."},
 {L"Alt + Pause", L"Saves a PNG snapshot of the current map."},
 {L"Shift + Escape", L"Minimizes the game."},
 {L"Insert", L"Cycles background graphics."},
 {L"Home", L"Centres view on the active worm."},
 {L"Ctrl + Home", L"Locks view on the active worm; press Home to disable."},
 {L"Scroll Lock", L"Toggles automatic camera following."},
 {L"Ctrl + Scroll Lock", L"Also prevents mouse movement from moving the camera."},
 {L"Page Up/Page Down", L"Hides/reveals the chat window respectively."},
 {L"Ctrl + Up/Down", L"Resizes the chat window when open."},
 {L"Ctrl + Page Up/Page Down", L"Unlocks/locks the chat window open."},
 {L"Delete", L"Cycles through above-worm information displays."},
 {L"Alt + Delete", L"Toggles opacity of above-worm information displays."},
 {L"Shift + Delete", L"Toggles autohide for the team status bar."},
 {L"T", L"Displays a thought bubble above the selected worm."},
 {L"R", L"Replays the last shot when pressed immediately afterwards."},
 {L"WEAPON-SPECIFIC CONTROLS", L""},
 {L"-", L"Sets Min Bounce for Grenades and Cluster Bombs."},
 {L"+", L"Sets Max Bounce for Grenades and Cluster Bombs."},
 {L"1 - 5", L"Sets fuse or Mad Cow herd size; RubberWorm supports 1 - 0."},
 {L"REPLAY PLAYBACK CONTROLS", L""},
 {L"Spacebar", L"Fast-forwards to the next turn or the end of an instant replay."},
 {L"M", L"Sets a bookmark in the current replay."},
 {L"R", L"Restarts from the beginning or bookmark."},
 {L"Shift + R", L"Removes the bookmark."},
 {L"S", L"Pauses playback or advances one frame when paused."},
 {L"1 - 9", L"Sets playback speeds from 1x through 16x."},
 {L"0 + [1 - 9]", L"Sets playback speeds from 24x through 384x."},
 {L"Shift + [1 - 9]", L"Sets slow-motion speeds from 1x through 1/16x."},
 {L"Shift + 0 + [1 - 9]", L"Sets slow-motion speeds from 1/24x through 1/384x."},
 {L"Shift + Alt + Delete", L"Shows extra normally hidden information during replay playback."}
};

constexpr const wchar_t* kWeaponNames[64] = {
 L"Bazooka",L"Homing Missile",L"Mortar",L"Grenade",L"Cluster Bomb",L"Skunk",
 L"Petrol Bomb",L"Banana Bomb",L"Handgun",L"Shotgun",L"Uzi",L"Minigun",
 L"Longbow",L"Air Strike",L"Napalm Strike",L"Mine",L"Fire Punch",L"Dragon Ball",
 L"Kamikaze",L"Prod",L"Battle Axe",L"Blowtorch",L"Pneumatic Drill",L"Girder",
 L"Ninja Rope",L"Parachute",L"Bungee",L"Teleport",L"Dynamite",L"Sheep",
 L"Baseball Bat",L"Flame Thrower",L"Homing Pigeon",L"Mad Cow",L"Holy Hand Grenade",
 L"Old Woman",L"Sheep Launcher",L"Super Sheep",L"Mole Bomb",L"Jet Pack",
 L"Low Gravity",L"Laser Sight",L"Fast Walk",L"Invisibility",L"Damage x2",
 L"Freeze",L"Super Banana Bomb",L"Mine Strike",L"Girder Starter Pack",L"Earthquake",
 L"Scales of Justice",L"Ming Vase",L"Mike's Carpet Bomb",L"Magic Bullet",
 L"Nuclear Test",L"Select Worm",L"Salvation Army",L"Mole Squadron",
 L"MB Bomb",L"Concrete Donkey",L"Suicide Bomber",L"Sheep Strike",L"Mail Strike",
 L"Armageddon"
};

struct Config {
 int anim=260,poll=10,scenarioReadyDelay=2500,widthPct=55,heightPct=90,topPct=5,right=0,padding=28,bar=14,wheel=5;
 int opacity=245,title=28,heading=20,body=16,spacing=7,rowPad=8,keyPct=28;
 COLORREF bg=RGB(0,0,0), text=RGB(255,255,255), muted=RGB(185,185,185), track=RGB(105,105,105), thumb=RGB(220,220,220);
 std::wstring font=L"Worms Armageddon", fallback=L"Arial"; int helpHotkey='H',schemeHotkey='I'; bool startOpen=false;
};
HMODULE g_module{}; HANDLE g_stop{},g_thread{}; HWND g_panel{},g_game{}; Config g_cfg; ULONG_PTR g_gdip{};
enum class PanelMode { Help, Scheme };
std::unique_ptr<Image> g_img1,g_img2,g_img3,g_img4,g_logo; int g_scroll=0,g_contentHeight=1,g_viewHeight=1; bool g_open=false,g_chat=false;PanelMode g_mode=PanelMode::Help;

std::wstring modulePath() { wchar_t p[MAX_PATH]{}; GetModuleFileNameW(g_module,p,MAX_PATH); std::wstring s=p; auto n=s.find_last_of(L"\\/"); return s.substr(0,n+1); }
int iniInt(const wchar_t* s,const wchar_t* k,int d,const std::wstring& p){return GetPrivateProfileIntW(s,k,d,p.c_str());}
std::wstring iniStr(const wchar_t*s,const wchar_t*k,const wchar_t*d,const std::wstring&p){wchar_t b[256]{};GetPrivateProfileStringW(s,k,d,b,256,p.c_str());return b;}
COLORREF color(const wchar_t*s,const wchar_t*k,COLORREF d,const std::wstring&p){auto v=iniStr(s,k,L"",p);int r,g,b;if(swscanf_s(v.c_str(),L"%d,%d,%d",&r,&g,&b)==3)return RGB(std::clamp(r,0,255),std::clamp(g,0,255),std::clamp(b,0,255));return d;}
int parseHotkey(std::wstring value,int fallback){
 for(auto&c:value)c=(wchar_t)towupper(c);value.erase(std::remove_if(value.begin(),value.end(),[](wchar_t c){return iswspace(c)!=0;}),value.end());
 if(value.size()==1){SHORT vk=VkKeyScanW(value[0]);if(vk!=-1)return LOBYTE(vk);}
 if(value.size()>=2&&value[0]==L'F'){wchar_t*end=nullptr;long n=wcstol(value.c_str()+1,&end,10);if(end&&*end==0&&n>=1&&n<=24)return VK_F1+(int)n-1;}
 const std::pair<const wchar_t*,int> names[]={{L"SPACE",VK_SPACE},{L"TAB",VK_TAB},{L"ENTER",VK_RETURN},{L"RETURN",VK_RETURN},{L"ESC",VK_ESCAPE},{L"ESCAPE",VK_ESCAPE},{L"BACKSPACE",VK_BACK},{L"INSERT",VK_INSERT},{L"DELETE",VK_DELETE},{L"HOME",VK_HOME},{L"END",VK_END},{L"PAGEUP",VK_PRIOR},{L"PAGEDOWN",VK_NEXT},{L"PGUP",VK_PRIOR},{L"PGDN",VK_NEXT},{L"UP",VK_UP},{L"DOWN",VK_DOWN},{L"LEFT",VK_LEFT},{L"RIGHT",VK_RIGHT},{L"PAUSE",VK_PAUSE},{L"SCROLLLOCK",VK_SCROLL}};
 for(const auto&n:names)if(value==n.first)return n.second;return fallback;
}
void loadConfig(){
 auto p=modulePath()+L"wkHelp.ini";
 g_cfg.anim=iniInt(L"General",L"AnimationDurationMs",260,p);g_cfg.poll=iniInt(L"General",L"PollIntervalMs",10,p);g_cfg.scenarioReadyDelay=iniInt(L"General",L"ScenarioReadyDelayMs",2500,p);g_cfg.startOpen=iniInt(L"General",L"StartOpen",0,p)!=0;
 g_cfg.helpHotkey=parseHotkey(iniStr(L"General",L"HelpHotkey",L"H",p),'H');g_cfg.schemeHotkey=parseHotkey(iniStr(L"General",L"SchemeHotkey",L"I",p),'I');
 g_cfg.widthPct=iniInt(L"Window",L"WidthPercent",55,p);g_cfg.heightPct=iniInt(L"Window",L"HeightPercent",90,p);g_cfg.topPct=iniInt(L"Window",L"TopPercent",5,p);g_cfg.right=iniInt(L"Window",L"RightMargin",0,p);g_cfg.padding=iniInt(L"Window",L"Padding",28,p);g_cfg.bar=iniInt(L"Window",L"ScrollbarWidth",14,p);g_cfg.opacity=iniInt(L"Window",L"Opacity",245,p);g_cfg.wheel=iniInt(L"Window",L"MouseWheelLines",5,p);
 g_cfg.bg=color(L"Window",L"BackgroundColor",g_cfg.bg,p);g_cfg.track=color(L"Window",L"ScrollbarColor",g_cfg.track,p);g_cfg.thumb=color(L"Window",L"ScrollbarThumbColor",g_cfg.thumb,p);
 g_cfg.font=iniStr(L"Text",L"FontName",L"Worms Armageddon",p);g_cfg.fallback=iniStr(L"Text",L"FallbackFontName",L"Arial",p);g_cfg.title=iniInt(L"Text",L"TitleSize",28,p);g_cfg.heading=iniInt(L"Text",L"HeadingSize",20,p);g_cfg.body=iniInt(L"Text",L"BodySize",16,p);g_cfg.spacing=iniInt(L"Text",L"LineSpacing",7,p);g_cfg.rowPad=iniInt(L"Text",L"TableRowPadding",8,p);g_cfg.keyPct=iniInt(L"Text",L"KeyColumnPercent",28,p);g_cfg.text=color(L"Text",L"TextColor",g_cfg.text,p);g_cfg.muted=color(L"Text",L"MutedTextColor",g_cfg.muted,p);
}
std::unique_ptr<Image> imageFromResource(int id){
 HRSRC r=FindResourceW(g_module,MAKEINTRESOURCEW(id),RT_RCDATA);if(!r)return{};HGLOBAL h=LoadResource(g_module,r);DWORD n=SizeofResource(g_module,r);void* q=LockResource(h);
 HGLOBAL copy=GlobalAlloc(GMEM_MOVEABLE,n);void* dst=GlobalLock(copy);memcpy(dst,q,n);GlobalUnlock(copy);IStream* stream{};CreateStreamOnHGlobal(copy,TRUE,&stream);auto im=std::make_unique<Image>(stream);stream->Release();return im;
}
HWND findGame(){
 struct S{DWORD pid;HWND h;};S s{GetCurrentProcessId(),nullptr};
 EnumWindows([](HWND h,LPARAM l)->BOOL{auto*s=(S*)l;DWORD p{};GetWindowThreadProcessId(h,&p);if(p==s->pid&&IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)){wchar_t c[100]{};GetClassNameW(h,c,100);if(wcsstr(c,L"wkHelp")==nullptr)s->h=h;}return TRUE;},(LPARAM)&s);return s.h;
}
bool activeGame(){if(!g_game||!IsWindow(g_game))g_game=findGame();if(!g_game||GetForegroundWindow()!=g_game||IsIconic(g_game))return false;RECT r{};GetClientRect(g_game,&r);return r.right>=640&&r.bottom>=400;}
Font makeFont(float sz,int style=FontStyleRegular){auto f=std::make_unique<FontFamily>(g_cfg.font.c_str());if(f->GetLastStatus()!=Ok)f=std::make_unique<FontFamily>(g_cfg.fallback.c_str());return Font(f.get(),sz,(INT)style,UnitPixel);}
void drawWrapped(Graphics&gr,const std::wstring&t,Font&f,Brush&b,RectF box,float&y){RectF bound;gr.MeasureString(t.c_str(),-1,&f,box,&bound);gr.DrawString(t.c_str(),-1,&f,box,nullptr,&b);y+=bound.Height+g_cfg.spacing;}
void drawCentered(Graphics&gr,const std::wstring&t,Font&f,Brush&b,float x,float width,float&y){
 RectF bounds;gr.MeasureString(t.c_str(),-1,&f,PointF(0,0),&bounds);
 StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentNear);
 RectF box(x,y,width,bounds.Height+8);gr.DrawString(t.c_str(),-1,&f,box,&format,&b);y+=bounds.Height+g_cfg.spacing;
}
void paint(HDC dc,RECT rc){
 Graphics gr(dc);float uiScale=std::clamp((float)rc.bottom/972.0f,0.80f,2.25f);rc.right=(LONG)(rc.right/uiScale);rc.bottom=(LONG)(rc.bottom/uiScale);gr.ScaleTransform(uiScale,uiScale);gr.SetCompositingQuality(CompositingQualityHighQuality);gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);gr.SetPixelOffsetMode(PixelOffsetModeHighQuality);gr.SetSmoothingMode(SmoothingModeHighQuality);gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);SolidBrush bg(Color(255,GetRValue(g_cfg.bg),GetGValue(g_cfg.bg),GetBValue(g_cfg.bg)));gr.FillRectangle(&bg,0,0,rc.right,rc.bottom);
 int usable=rc.right-g_cfg.padding*2-g_cfg.bar-8;float y=(float)g_cfg.padding-g_scroll;SolidBrush white(Color(255,GetRValue(g_cfg.text),GetGValue(g_cfg.text),GetBValue(g_cfg.text)));SolidBrush muted(Color(255,GetRValue(g_cfg.muted),GetGValue(g_cfg.muted),GetBValue(g_cfg.muted)));
 Font ft=makeFont((float)g_cfg.title,FontStyleBold),fh=makeFont((float)g_cfg.heading,FontStyleBold),fb=makeFont((float)g_cfg.body);
 if(g_logo&&g_logo->GetLastStatus()==Ok){float logoWidth=usable*0.50f;float logoScale=logoWidth/(float)g_logo->GetWidth();float logoHeight=g_logo->GetHeight()*logoScale;float logoX=g_cfg.padding+(usable-logoWidth)/2.0f;gr.DrawImage(g_logo.get(),RectF(logoX,y,logoWidth,logoHeight));y+=logoHeight+18;}
 if(g_mode==PanelMode::Help){
  drawCentered(gr,L"Game Controls",ft,white,(REAL)g_cfg.padding,(REAL)usable,y);y+=12;
  for(Image* im:{g_img1.get(),g_img2.get(),g_img3.get(),g_img4.get()})if(im&&im->GetLastStatus()==Ok){float scale=usable/(float)im->GetWidth();float h=im->GetHeight()*scale;gr.DrawImage(im,RectF((REAL)g_cfg.padding,y,(REAL)usable,h));y+=h+18;}
  drawCentered(gr,L"In-game Controls",fh,white,(REAL)g_cfg.padding,(REAL)usable,y);
  int keyW=usable*g_cfg.keyPct/100;Pen line(Color(100,255,255,255),1);
  for(const auto&r:kRows){RectF kb((REAL)g_cfg.padding,y,(REAL)keyW,2000),ab((REAL)(g_cfg.padding+keyW+12),y,(REAL)(usable-keyW-12),2000),kbo,abo;gr.MeasureString(r.key,-1,&fb,kb,&kbo);gr.MeasureString(r.action,-1,&fb,ab,&abo);float h=std::max(kbo.Height,abo.Height)+g_cfg.rowPad*2;gr.DrawLine(&line,(REAL)g_cfg.padding,y+h,(REAL)(g_cfg.padding+usable),y+h);gr.DrawString(r.key,-1,&fb,RectF(kb.X,y+g_cfg.rowPad,kb.Width,h),nullptr,&white);gr.DrawString(r.action,-1,&fb,RectF(ab.X,y+g_cfg.rowPad,ab.Width,h),nullptr,&white);y+=h;}
  y+=14;drawWrapped(gr,L"For more information please visit \"worms2d.info/Worms_Armageddon\".",fb,white,RectF((REAL)g_cfg.padding,y,(REAL)usable,1000),y);
 }else{
  drawCentered(gr,L"Current Scheme",ft,white,(REAL)g_cfg.padding,(REAL)usable,y);y+=16;
  const auto s=SchemeInfo::snapshot();
  const int labelW=usable*40/100;Pen grid(Color(125,255,255,255),1);SolidBrush categoryBg(Color(255,45,45,45));bool alternate=false;
  auto onOff=[](bool value){return value?std::wstring(L"On"):std::wstring(L"Off");};
  auto seconds=[](int value){return std::to_wstring(value)+L" s";};
  auto percent=[](int value){return std::to_wstring(value)+L"%";};
  auto fixed2=[](float value){wchar_t b[64]{};swprintf_s(b,L"%.2f",value);return std::wstring(b);};
  auto category=[&](const wchar_t*title){y+=8;float h=g_cfg.heading+g_cfg.rowPad*2;gr.FillRectangle(&categoryBg,(REAL)g_cfg.padding,y,(REAL)usable,h);StringFormat f;f.SetAlignment(StringAlignmentCenter);f.SetLineAlignment(StringAlignmentCenter);gr.DrawString(title,-1,&fh,RectF((REAL)g_cfg.padding,y,(REAL)usable,h),&f,&white);y+=h;alternate=false;};
  auto row=[&](const std::wstring&label,const std::wstring&value){RectF lb((REAL)(g_cfg.padding+8),y,(REAL)(labelW-16),2000),vb((REAL)(g_cfg.padding+labelW+8),y,(REAL)(usable-labelW-16),2000),lbo,vbo;gr.MeasureString(label.c_str(),-1,&fb,lb,&lbo);gr.MeasureString(value.c_str(),-1,&fb,vb,&vbo);float h=std::max(lbo.Height,vbo.Height)+g_cfg.rowPad*2;if(alternate){SolidBrush shade(Color(255,20,20,20));gr.FillRectangle(&shade,(REAL)g_cfg.padding,y,(REAL)usable,h);}gr.DrawLine(&grid,(REAL)g_cfg.padding,y+h,(REAL)(g_cfg.padding+usable),y+h);gr.DrawLine(&grid,(REAL)(g_cfg.padding+labelW),y,(REAL)(g_cfg.padding+labelW),y+h);gr.DrawString(label.c_str(),-1,&fb,RectF(lb.X,y+g_cfg.rowPad,lb.Width,h),nullptr,&white);gr.DrawString(value.c_str(),-1,&fb,RectF(vb.X,y+g_cfg.rowPad,vb.Width,h),nullptr,&white);y+=h;alternate=!alternate;};
  if(!s.valid){row(L"Status",L"No readable scheme is currently available.");}
  else{
   category(L"General");row(L"Version",std::to_wstring(s.version));row(L"Source",std::wstring(s.sourceKind.begin(),s.sourceKind.end()));row(L"Turn time",s.turnTimeInfinite?L"Infinite":seconds(s.turnTime));row(L"Round time",s.roundTimeSeconds?seconds(s.roundTimeSeconds):std::to_wstring(s.roundTimeMinutes)+L" min");row(L"Hot-seat time",seconds(s.hotSeatTime));row(L"Retreat time",seconds(s.retreatTime));row(L"Rope retreat time",seconds(s.retreatTimeRope));row(L"Rounds to win",std::to_wstring(s.numberOfWins));
   category(L"Worms and Map");row(L"Worm energy",std::to_wstring(s.wormEnergy));row(L"Manual placement",onOff(s.manualWormPlacement));row(L"Objects",std::to_wstring(s.objectCount));const wchar_t*objectTypes[]={L"None",L"Mines",L"Oil drums",L"Mines and oil drums"};row(L"Object types",objectTypes[s.objectTypes&3]);row(L"Mine fuse",s.mineDelayRandom?L"Random (1-3 s)":seconds(s.mineDelay));row(L"Dud mines",onOff(s.dudMines));row(L"Fall damage",percent(s.fallDamage));const wchar_t*sd[]={L"Round end",L"Nuclear strike",L"Health drop",L"Water rise"};row(L"Sudden death",sd[std::min<int>(s.suddenDeathEvent,3)]);row(L"Water rise",std::to_wstring(s.waterRiseRate)+L" px/turn");const wchar_t*stock[]={L"Off",L"On",L"Anti"};row(L"Stockpiling",stock[std::min<int>(s.stockpiling,2)]);const wchar_t*select[]={L"Sequential",L"Manual",L"Random"};row(L"Worm select",select[std::min<int>(s.wormSelect,2)]);
   category(L"Crates");row(L"Weapon crate probability",percent(s.weaponCrateProb));row(L"Utility crate probability",percent(s.utilityCrateProb));row(L"Health crate probability",percent(s.healthCrateProb));row(L"Health crate energy",std::to_wstring(s.healthCrateEnergy));row(L"Donor cards",onOff(s.donorCards));
   category(L"Game Flags");row(L"Artillery mode",onOff(s.artilleryMode));row(L"Blood",onOff(s.blood));row(L"Aqua sheep",onOff(s.aquaSheep));row(L"Sheep heaven",onOff(s.sheepHeaven));row(L"God worms",onOff(s.godWorms));row(L"Indestructible land",onOff(s.indiLand));row(L"Team weapons",onOff(s.teamWeapons));row(L"Super weapons",onOff(s.superWeapons));row(L"Replays",onOff(s.replays));row(L"Show round time",onOff(s.showRoundTime));
   category(L"Weapon Upgrades");row(L"Grenade",onOff(s.upgradeGrenade));row(L"Shotgun",onOff(s.upgradeShotgun));row(L"Cluster bomb",onOff(s.upgradeCluster));row(L"Longbow",onOff(s.upgradeLongbow));
   category(L"Weapons");
   const int weaponColumns[6]={0,40,55,70,82,100};const wchar_t*weaponHeaders[5]={L"Weapon",L"Ammo",L"Power",L"Delay",L"Probability"};StringFormat cellCenter;cellCenter.SetAlignment(StringAlignmentCenter);cellCenter.SetLineAlignment(StringAlignmentCenter);
   float weaponHeaderHeight=g_cfg.body*2.6f;SolidBrush headerBg(Color(255,32,32,32));gr.FillRectangle(&headerBg,(REAL)g_cfg.padding,y,(REAL)usable,weaponHeaderHeight);
   for(int c=0;c<5;c++){float x=(REAL)(g_cfg.padding+usable*weaponColumns[c]/100),w=(REAL)(usable*(weaponColumns[c+1]-weaponColumns[c])/100);gr.DrawString(weaponHeaders[c],-1,&fb,RectF(x,y,w,weaponHeaderHeight),&cellCenter,&white);if(c>0)gr.DrawLine(&grid,x,y,x,y+weaponHeaderHeight);}gr.DrawLine(&grid,(REAL)g_cfg.padding,y+weaponHeaderHeight,(REAL)(g_cfg.padding+usable),y+weaponHeaderHeight);y+=weaponHeaderHeight;alternate=false;
   for(int i=0;i<s.weaponCount&&i<64;i++){const auto&w=s.weapons[i];std::wstring values[5]={kWeaponNames[i],(w.ammo==10||w.ammo<0)?L"Infinite":std::to_wstring((int)w.ammo),std::to_wstring((unsigned)w.power),std::to_wstring((int)w.delay),std::to_wstring((int)w.probability)};RectF nameBox((REAL)(g_cfg.padding+8),y,(REAL)(usable*40/100-16),1000),nameBounds;gr.MeasureString(values[0].c_str(),-1,&fb,nameBox,&nameBounds);float h=std::max((float)(g_cfg.body+g_cfg.rowPad*2),nameBounds.Height+g_cfg.rowPad*2);if(alternate){SolidBrush shade(Color(255,20,20,20));gr.FillRectangle(&shade,(REAL)g_cfg.padding,y,(REAL)usable,h);}for(int c=0;c<5;c++){float x=(REAL)(g_cfg.padding+usable*weaponColumns[c]/100),cw=(REAL)(usable*(weaponColumns[c+1]-weaponColumns[c])/100);if(c==0)gr.DrawString(values[c].c_str(),-1,&fb,RectF(x+8,y+g_cfg.rowPad,cw-16,h),nullptr,&white);else gr.DrawString(values[c].c_str(),-1,&fb,RectF(x,y,cw,h),&cellCenter,&white);if(c>0)gr.DrawLine(&grid,x,y,x,y+h);}gr.DrawLine(&grid,(REAL)g_cfg.padding,y+h,(REAL)(g_cfg.padding+usable),y+h);y+=h;alternate=!alternate;}
   for(const wchar_t*command:{L"Skip Turn",L"Surrender"}){std::wstring values[5]={command,L"Infinite",L"N/A",L"N/A",L"N/A"};float h=(float)(g_cfg.body+g_cfg.rowPad*2);if(alternate){SolidBrush shade(Color(255,20,20,20));gr.FillRectangle(&shade,(REAL)g_cfg.padding,y,(REAL)usable,h);}for(int c=0;c<5;c++){float x=(REAL)(g_cfg.padding+usable*weaponColumns[c]/100),cw=(REAL)(usable*(weaponColumns[c+1]-weaponColumns[c])/100);if(c==0)gr.DrawString(values[c].c_str(),-1,&fb,RectF(x+8,y+g_cfg.rowPad,cw-16,h),nullptr,&white);else gr.DrawString(values[c].c_str(),-1,&fb,RectF(x,y,cw,h),&cellCenter,&white);if(c>0)gr.DrawLine(&grid,x,y,x,y+h);}gr.DrawLine(&grid,(REAL)g_cfg.padding,y+h,(REAL)(g_cfg.padding+usable),y+h);y+=h;alternate=!alternate;}

   auto optionalOnOff=[&](const std::optional<bool>&v){return v.has_value()?onOff(*v):std::wstring(L"Default");};
   auto byteOrDefault=[](uint8_t v,const wchar_t*suffix=L""){return v==255?std::wstring(L"Default"):std::to_wstring(v)+suffix;};
   const auto&e=s.extended;
   category(L"Extended Game Options");
   row(L"Data version",std::to_wstring(e.dataVersion));
   category(L"Physics");
   row(L"Constant wind",onOff(e.constantWind));row(L"Wind",std::to_wstring(e.wind));row(L"Wind bias",std::to_wstring(e.windBias));row(L"Gravity",fixed2(e.gravity));row(L"Terrain friction",fixed2(e.friction));row(L"Rope knocking force",byteOrDefault(e.ropeKnockForce,L"%"));row(L"Maximum projectile speed",fixed2(e.projectileMaxSpeed));row(L"Maximum rope speed",fixed2(e.ropeMaxSpeed));row(L"Maximum Jet Pack speed",fixed2(e.jetpackMaxSpeed));row(L"Game engine speed",fixed2(e.gameSpeed));
   category(L"Gameplay 1");
   row(L"Unrestrict Rope",onOff(e.ropeUpgrade));row(L"Maximum crate count",std::to_wstring(e.crateLimit));row(L"No-crate probability",byteOrDefault(e.noCrateProbability,L"%"));row(L"Sudden Death disables Worm Select",onOff(e.suddenDeathNoWormSelect));row(L"Sudden Death worm damage",std::to_wstring(e.suddenDeathTurnDamage));row(L"Batty Rope",onOff(e.battyRope));const wchar_t*roll[]={L"Disabled",L"As from rope only",L"As from rope or jump"};row(L"Rope-roll drops",roll[std::min<int>(e.ropeRollDrops,2)]);row(L"X-impact loss of control",e.keepControlXImpact==255?L"Keep control":L"Lose control");row(L"Keep control after head bump",onOff(e.keepControlHeadBump));const wchar_t*skim[]={L"Lose control",L"Keep control",L"Keep control and rope"};row(L"Keep control after skimming",skim[std::min<int>(e.keepControlSkim,2)]);row(L"Fall damage after explosions",onOff(e.explosionFallDamage));row(L"Explosions push all objects",optionalOnOff(e.objectPushByExplosion));row(L"Pneumatic Drill imparts velocity",optionalOnOff(e.drillImpartsVelocity));row(L"Petrol turn decay",fixed2(e.flameTurnDecay));row(L"Petrol touch decay",std::to_wstring(e.flameTouchDecay));row(L"Maximum flamelet count",std::to_wstring(e.flameLimit));
   category(L"Gameplay 2");
   row(L"Undetermined crates",optionalOnOff(e.undeterminedCrates));row(L"Undetermined mine fuses",optionalOnOff(e.undeterminedMineFuse));row(L"Pause timer while firing",onOff(e.firingPausesTimer));row(L"Loss of control doesn't end turn",onOff(e.loseControlDoesntEndTurn));row(L"Weapon use doesn't end turn",onOff(e.shotDoesntEndTurn));row(L"Above option allows all weapons",onOff(e.shotDoesntEndTurnAll));row(L"Fractional round timer",onOff(e.roundTimeFractional));row(L"Automatic end-of-turn retreat",onOff(e.autoRetreat));const wchar_t*cure[]={L"Collecting worm",L"Whole team",L"All allied teams"};row(L"Health crates cure poison",e.healthCure==255?L"Nobody":cure[std::min<int>(e.healthCure,2)]);row(L"Sheep Heaven effects",std::to_wstring(e.sheepHeavenFlags)+L" / 7");row(L"Conserve instant utilities",onOff(e.conserveUtilities));row(L"Expedite instant utilities",onOff(e.expediteUtilities));row(L"Double Time stack limit",e.doubleTimeCount?std::to_wstring(e.doubleTimeCount):L"Unlimited");
   category(L"Glitch Emulation");
   row(L"Indian Rope glitch",optionalOnOff(e.indianRopeGlitch));row(L"Herd-doubling glitch",optionalOnOff(e.herdDoublingGlitch));row(L"Jet Pack Bungee glitch",onOff(e.jetpackBungeeGlitch));row(L"Angle cheat glitch",onOff(e.angleCheatGlitch));row(L"Glide glitch",onOff(e.glideGlitch));row(L"Skip-walking",e.skipWalk==255?L"Disabled":(e.skipWalk==1?L"Facilitated":L"Possible"));const wchar_t*roof[]={L"Allow roofing",L"Block above",L"Block everywhere"};row(L"Block roofing",roof[std::min<int>(e.roofing,2)]);row(L"Floating weapon glitch",onOff(e.floatingWeaponGlitch));row(L"Terrain overlap phasing glitch",optionalOnOff(e.terrainOverlapGlitch));
   category(L"Input");
   row(L"Auto-place worms by ally",onOff(e.groupPlaceAllies));row(L"Circular Aim",onOff(e.circularAim));row(L"Anti-lock Aim",onOff(e.antiLockAim));row(L"Anti-lock Power",onOff(e.antiLockPower));row(L"Worm Selection keeps Hot Seat",onOff(e.wormSelectKeepHotSeat));row(L"Worm Selection is never cancelled",onOff(e.wormSelectAnytime));row(L"Girder Radius Assist",onOff(e.girderRadiusAssist));
   category(L"Visual");row(L"Blood level",byteOrDefault(e.bloodAmount,L"%"));
   category(L"RubberWorm");
   row(L"Bounciness",fixed2(e.wormBounce));row(L"Air viscosity",fixed2(e.viscosity));row(L"Air viscosity applies to worms",onOff(e.viscosityWorms));row(L"Wind influence",fixed2(e.rwWind));row(L"Wind influence applies to worms",onOff(e.rwWindWorms));const wchar_t*rwGravity[]={L"Unmodified",L"Standard",L"Black hole (constant)",L"Black hole (linear)"};row(L"Gravity type",rwGravity[std::min<int>(e.rwGravityType,3)]);row(L"Gravity strength",fixed2(e.rwGravity));row(L"Crate rate",std::to_wstring(e.crateRate));row(L"Crate shower",onOff(e.crateShower));row(L"Anti-sink",onOff(e.antiSink));row(L"Remember weapons",onOff(e.weaponsDontChange));row(L"Extended fuses/herds",onOff(e.extendedFuse));row(L"Anti-lock aim",onOff(e.autoReaim));row(L"Kaos mod preset",std::to_wstring(e.kaosMod));
  }
 }
 g_contentHeight=(int)(y+g_scroll+g_cfg.padding);g_viewHeight=rc.bottom;int maxScroll=std::max(0,g_contentHeight-g_viewHeight);g_scroll=std::clamp(g_scroll,0,maxScroll);
 SolidBrush tr(Color(255,GetRValue(g_cfg.track),GetGValue(g_cfg.track),GetBValue(g_cfg.track))),th(Color(255,GetRValue(g_cfg.thumb),GetGValue(g_cfg.thumb),GetBValue(g_cfg.thumb)));int bx=(int)rc.right-g_cfg.bar;gr.FillRectangle(&tr,bx,0,g_cfg.bar,(int)rc.bottom);int vh=(int)rc.bottom;int hh=std::max(28,vh*vh/std::max(vh,g_contentHeight));int by=maxScroll?g_scroll*(vh-hh)/maxScroll:0;gr.FillRectangle(&th,bx+2,by,g_cfg.bar-4,hh);
}
void scrollBy(int d){g_scroll=std::clamp(g_scroll+d,0,std::max(0,g_contentHeight-g_viewHeight));InvalidateRect(g_panel,nullptr,FALSE);}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_PAINT){PAINTSTRUCT ps{};HDC d=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);HDC mem=CreateCompatibleDC(d);HBITMAP bmp=CreateCompatibleBitmap(d,r.right,r.bottom);HGDIOBJ old=SelectObject(mem,bmp);paint(mem,r);BitBlt(d,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
 if(m==WM_MOUSEWHEEL){scrollBy(-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*g_cfg.wheel*(g_cfg.body+g_cfg.spacing));return 0;}
 if(m==WM_ERASEBKGND)return 1;if(m==WM_NCHITTEST)return HTCLIENT;return DefWindowProcW(h,m,w,l);
}
void positionPanel(float progress){
 RECT cr{};GetClientRect(g_game,&cr);POINT p{0,0};ClientToScreen(g_game,&p);int w=cr.right*g_cfg.widthPct/100,h=cr.bottom*g_cfg.heightPct/100,y=p.y+cr.bottom*g_cfg.topPct/100;int shown=p.x+cr.right-g_cfg.right-w,hidden=p.x+cr.right;int x=(int)(hidden+(shown-hidden)*progress);
 RECT old{};GetWindowRect(g_panel,&old);bool resized=(old.right-old.left)!=w||(old.bottom-old.top)!=h;
 if(old.left!=x||old.top!=y||resized)SetWindowPos(g_panel,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 if(resized)InvalidateRect(g_panel,nullptr,FALSE);
}
DWORD WINAPI worker(LPVOID){
 Log::initialize(g_module);
 Log::write("Worker started; process=%lu module=0x%p", GetCurrentProcessId(), g_module);
 loadConfig();
 Log::write("Config loaded: panel=%d%%x%d%% top=%d%% opacity=%d animation=%dms scenarioDelay=%dms", g_cfg.widthPct, g_cfg.heightPct, g_cfg.topPct, g_cfg.opacity, g_cfg.anim, g_cfg.scenarioReadyDelay);
 const bool stateHookInstalled=GameState::install();
 Log::write("Game-state detection installed=%d", stateHookInstalled ? 1 : 0);
 bool schemeInfoInstalled=false;try{SchemeInfo::install();schemeInfoInstalled=true;Log::write("Scheme-info detection installed=1");}catch(const std::exception&e){Log::write("Scheme-info install failed: %s",e.what());}catch(...){Log::write("Scheme-info install failed: unknown exception");}
 GdiplusStartupInput in;const auto gdipStatus=GdiplusStartup(&g_gdip,&in,nullptr);g_img1=imageFromResource(101);g_img2=imageFromResource(102);g_img3=imageFromResource(103);g_img4=imageFromResource(104);g_logo=imageFromResource(105);
 Log::write("GDI+ status=%d; resources=%d,%d,%d,%d logo=%d", (int)gdipStatus, g_img1?1:0, g_img2?1:0, g_img3?1:0, g_img4?1:0, g_logo?1:0);
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=g_module;wc.lpszClassName=L"wkHelpOverlay";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 g_panel=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED,L"wkHelpOverlay",L"wkHelp",WS_POPUP,0,0,1,1,nullptr,nullptr,g_module,nullptr);
 Log::write("Overlay window creation: hwnd=0x%p error=%lu", g_panel, g_panel ? 0 : GetLastError());
 SetLayeredWindowAttributes(g_panel,0,(BYTE)std::clamp(g_cfg.opacity,20,255),LWA_ALPHA);
 g_open=g_cfg.startOpen;bool helpPrev=false,schemePrev=false,puPrev=false,pdPrev=false;DWORD animStart=GetTickCount();bool target=g_open;
 bool previousActive=false, previousMatch=false;DWORD matchStartedTick=0;
 while(WaitForSingleObject(g_stop,g_cfg.poll)==WAIT_TIMEOUT){
  MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  const DWORD nowTick=GetTickCount();const bool foreground=activeGame(),match=GameState::inMatch();
  if(match&&!previousMatch){matchStartedTick=nowTick;target=false;g_scroll=0;animStart=nowTick;ShowWindow(g_panel,SW_HIDE);Log::write("New match detected: panel reset; waiting %dms for scenario",g_cfg.scenarioReadyDelay);}
  if(!match&&previousMatch){target=false;g_scroll=0;ShowWindow(g_panel,SW_HIDE);Log::write("Match ended: panel reset");}
  const bool scenarioReady=match&&matchStartedTick!=0&&(nowTick-matchStartedTick)>=(DWORD)std::max(0,g_cfg.scenarioReadyDelay);
  bool active=foreground&&scenarioReady,helpDown=(GetAsyncKeyState(g_cfg.helpHotkey)&0x8000)!=0,schemeDown=(GetAsyncKeyState(g_cfg.schemeHotkey)&0x8000)!=0,pu=(GetAsyncKeyState(VK_PRIOR)&0x8000)!=0,pd=(GetAsyncKeyState(VK_NEXT)&0x8000)!=0;
  if(active!=previousActive||match!=previousMatch){
   Log::write("State changed: foreground=%d match=%d scenarioReady=%d active=%d gameHwnd=0x%p", foreground?1:0, match?1:0, scenarioReady?1:0, active?1:0, g_game);
   previousActive=active;previousMatch=match;
  }
  if(active&&pd&&!pdPrev){g_chat=true;if(target){target=false;animStart=nowTick;Log::write("Chat opened: overlay closing");}}
  if(active&&pu&&!puPrev){g_chat=false;Log::write("Chat closed");}
  auto handleModeKey=[&](PanelMode requested,const char*name){Log::write("%s hotkey pressed: active=%d chat=%d target=%d",name,active?1:0,g_chat?1:0,target?1:0);if(!active||g_chat){Log::write("%s toggle rejected",name);return;}if(target&&g_mode==requested){target=false;}else{g_mode=requested;g_scroll=0;target=true;InvalidateRect(g_panel,nullptr,FALSE);ShowWindow(g_panel,SW_SHOWNOACTIVATE);}animStart=GetTickCount();Log::write("%s toggle accepted: target=%d",name,target?1:0);};
  if(helpDown&&!helpPrev)handleModeKey(PanelMode::Help,"Help");
  if(schemeDown&&!schemePrev){if(schemeInfoInstalled){const auto snap=SchemeInfo::snapshot();Log::write("Scheme snapshot: valid=%d version=%d weapons=%d extendedBytes=%zu",snap.valid?1:0,snap.version,snap.weaponCount,snap.extraOptionsSize);handleModeKey(PanelMode::Scheme,"Scheme");}else Log::write("Scheme hotkey rejected: scheme detection unavailable");}
  helpPrev=helpDown;schemePrev=schemeDown;puPrev=pu;pdPrev=pd;
  if(!active){ShowWindow(g_panel,SW_HIDE);continue;} DWORD elapsed=GetTickCount()-animStart;float t=std::min(1.0f,elapsed/(float)std::max(1,g_cfg.anim));t=1-(1-t)*(1-t)*(1-t);float pr=target?t:1-t;positionPanel(pr);if(!target&&elapsed>=(DWORD)g_cfg.anim)ShowWindow(g_panel,SW_HIDE);
  if(target){if(GetAsyncKeyState(VK_UP)&1)scrollBy(-(g_cfg.body+g_cfg.spacing));if(GetAsyncKeyState(VK_DOWN)&1)scrollBy(g_cfg.body+g_cfg.spacing);if(GetAsyncKeyState(VK_PRIOR)&1)scrollBy(-g_viewHeight*8/10);if(GetAsyncKeyState(VK_NEXT)&1)scrollBy(g_viewHeight*8/10);if(GetAsyncKeyState(VK_HOME)&1){g_scroll=0;InvalidateRect(g_panel,nullptr,FALSE);}if(GetAsyncKeyState(VK_END)&1){g_scroll=std::max(0,g_contentHeight-g_viewHeight);InvalidateRect(g_panel,nullptr,FALSE);}}
 }
 Log::write("Worker shutting down");
 DestroyWindow(g_panel);GdiplusShutdown(g_gdip);return 0;
}
}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID reserved){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);g_module=h;g_stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);g_thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);}
 else if(reason==DLL_PROCESS_DETACH){if(g_stop)SetEvent(g_stop);if(!reserved&&g_thread)WaitForSingleObject(g_thread,1000);if(g_thread)CloseHandle(g_thread);if(g_stop)CloseHandle(g_stop);}return TRUE;
}
