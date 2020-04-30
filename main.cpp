#include <windows.h>
#include <stdio.h>

#include "resource.h"


const char outOfMem[] = "Nicht genügend freier Speicher! Programm wird beendet.";
const char validValues[] = "Bitte gültige Werte eingeben";
const char nonstandard[] = "Das Standardprofil kann nicht überschrieben werden. Bitte legen Sie ein neues Profil an.";
const char nodel[] = "Das Standardprofil kann nicht gelöscht werden.";
const char err[] = "Fehler";

#define MAX_PROFILE_NAME_LENGTH 30

#define STATE_RUN		0
#define STATE_HALT		1
#define STATE_PAUSE		2

unsigned char cMeterState = STATE_HALT;


#define GET_SECONDS(x)			(x%10)
#define GET_TEN_SECONDS(x)		((x%60)/10)
#define GET_MINUTES(x)			((x/60)%10)
#define GET_TEN_MINUTES(x)		(((x/60)%60)/10)
#define GET_HOURS(x)			((x/3600)%10)
#define GET_TEN_HOURS(x)		((x/3600)/10)

const char szConfigFile[] = "config.ini";
const int iDigitWidth = 26;
const int iColonWidth = 10;
const int iHeight = 39;
const int iBmpWidth = 178;
const int iBmpHeight = 39;

const UINT uTimerID  = 9999; 

unsigned int iTime = 0;
float iCost = 0.0f;

unsigned int iLevel1 = 0;
unsigned int iLevel2 = 0;
unsigned int iLevel3 = 0;

unsigned int iCountLevel1 = 0;
unsigned int iCountLevel2 = 0;
unsigned int iCountLevel3 = 0;

char *chosenProfile = NULL;

char *SettingsFile = NULL;


HINSTANCE hAppInstance = NULL;
HWND hDialogWindow = NULL;
HBITMAP hBmpDigits = NULL;
HBITMAP hBmp = NULL;
HDC hOffscreenDc = NULL;
HDC hBitmapDc = NULL;
HBRUSH hBlackBrush = NULL;
HFONT hSumFont = NULL;
HFONT hProfileFont = NULL;
HICON hAppIcon = NULL;
BOOL bInfoVisible = true;


/* fill the combobox with the profile names*/
void fillComboBox(HWND hWnd) {
	SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_RESETCONTENT, 0, 0);
	char buf[5000] = {'\0'}; 
	char *bp = buf;
	FILE *f = NULL;
	f = fopen(SettingsFile, "r");
	if(f == NULL) {
		SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_ADDSTRING, 0, (LPARAM)"Standard");
		return;
	}
	SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_ADDSTRING, 0, (LPARAM)"Standard");
	GetPrivateProfileSectionNames(buf, 5000, SettingsFile);
	while(strlen(bp)) {
		if(stricmp(bp, "LastProfile")) {
			SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_ADDSTRING, 0, (LPARAM)bp);
		}
		while(*bp != '\0') {
			bp++;
		}
		bp++; // skip \0 char
	}
}

/* deletes a section from an ini file */
void DeleteSection(char *Section) {
	WritePrivateProfileString(Section, "", NULL, SettingsFile);
}

/* saves a profile to the settings file */
void SaveProfile(char *Name, char *l1, char *l2, char *l3) {
	WritePrivateProfileString(Name, "Level1", l1, SettingsFile);
	WritePrivateProfileString(Name, "Level2", l2, SettingsFile);
	WritePrivateProfileString(Name, "Level3", l3, SettingsFile);
}

/* shows a profile in the profile edit dialog */
void showProfile(HWND hWnd, char *Profile) {
	SetDlgItemInt(hWnd, IDC_LEVEL1, GetPrivateProfileInt(Profile, "Level1", 0, SettingsFile), false);
	SetDlgItemInt(hWnd, IDC_LEVEL2, GetPrivateProfileInt(Profile, "Level2", 0, SettingsFile), false);
	SetDlgItemInt(hWnd, IDC_LEVEL3, GetPrivateProfileInt(Profile, "Level3", 0, SettingsFile), false);
}

/* loads a profile from the program ini file */
void LoadProfile() {
	FILE *f = NULL;
	f = fopen(SettingsFile, "r");
	if(f == NULL) { // make chosen profile standard if configuration file does not exist
		iLevel1 = 200;
		iLevel2 = 150;
		iLevel3 = 100;
		if(chosenProfile != NULL) {
			HeapFree(GetProcessHeap(), 0, chosenProfile);
		}
		chosenProfile = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PROFILE_NAME_LENGTH+1*sizeof(char));
		if(chosenProfile == NULL) {
			MessageBox(NULL, outOfMem, err, MB_OK | MB_ICONERROR);
			PostQuitMessage(-1);
		}
		strcpy(chosenProfile, "Standard");
		return;
	}

	chosenProfile = (char*)HeapAlloc(GetProcessHeap(), 0, MAX_PROFILE_NAME_LENGTH+1*sizeof(char));
	if(chosenProfile == NULL) {
		MessageBox(NULL, outOfMem, err, MB_OK | MB_ICONERROR);
		PostQuitMessage(-1);
	}
	// check if profile to be loaded is present, if not use standard profile
	GetPrivateProfileString("LastProfile", "Value", "Not found", chosenProfile, MAX_PROFILE_NAME_LENGTH+1, SettingsFile);
	if(!stricmp(chosenProfile, "Not found") || !stricmp(chosenProfile, "Standard")) {
		iLevel1 = 200;
		iLevel2 = 150;
		iLevel3 = 100;
		strcpy(chosenProfile, "Standard");
		return;
	}
	// anderes profil soll geladen werden
	iLevel1 = GetPrivateProfileInt(chosenProfile, "Level1", 0, SettingsFile);
	iLevel2 = GetPrivateProfileInt(chosenProfile, "Level2", 0, SettingsFile);
	iLevel3 = GetPrivateProfileInt(chosenProfile, "Level3", 0, SettingsFile);

	if(!(iLevel1 && iLevel2 && iLevel3)) {
		iLevel1 = 200;
		iLevel2 = 150;
		iLevel3 = 100;
		strcpy(chosenProfile, "Standard");
		return;
	}
}

/* draw the clock */
void drawClock() {
	int iDigitIdx = 0;
	int iLeft = 0;
	SelectObject(hOffscreenDc, hBlackBrush); 
	Rectangle(hOffscreenDc, 0, 0, iBmpWidth, iBmpHeight); // clear image
	iDigitIdx = GET_TEN_HOURS(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw tenth hour
	iLeft += iDigitWidth;
	iDigitIdx = GET_HOURS(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw hour
	iLeft += iDigitWidth;
	iDigitIdx = 10;
	BitBlt(hOffscreenDc, iLeft, 0, iColonWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw colon
	iLeft += iColonWidth;
	iDigitIdx = GET_TEN_MINUTES(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw tenth minute
	iLeft += iDigitWidth;
	iDigitIdx = GET_MINUTES(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw minute
	iLeft += iDigitWidth;
	iDigitIdx = 10;
	BitBlt(hOffscreenDc, iLeft, 0, iColonWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw colon
	iLeft += iColonWidth;
	iDigitIdx = GET_TEN_SECONDS(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw tenth second
	iLeft += iDigitWidth;
	iDigitIdx = GET_SECONDS(iTime);
	BitBlt(hOffscreenDc, iLeft, 0, iDigitWidth, iHeight, hBitmapDc, iDigitIdx*iDigitWidth, 0, SRCCOPY); // draw second
}

/* free resources */
void cleanUp() {
	if(hBmpDigits != NULL) {
		DeleteObject(hBmpDigits);
	}
	if(hOffscreenDc != NULL) {
		DeleteDC(hOffscreenDc);
	}
	if(hBitmapDc != NULL) {
		DeleteDC(hBitmapDc);
	}
	if(hBmp != NULL) {
		DeleteObject(hBmp);
	}
	if(hBlackBrush != NULL) {
		DeleteObject(hBlackBrush);
	}
	if(hSumFont != NULL) {
		DeleteObject(hSumFont);
	}
	if(hAppIcon != NULL) {
		DestroyIcon(hAppIcon);
	}
}

/* load resources, init DCs */
bool initApplication() {
	LOGBRUSH lbBrush;

	LOGFONT lfArial;
	lfArial.lfHeight = -48;
	lfArial.lfWidth = 0;
	lfArial.lfEscapement = 0;
	lfArial.lfOrientation = 0;
	lfArial.lfWeight = 700;
	lfArial.lfItalic = 0;
	lfArial.lfUnderline = 0;
	lfArial.lfStrikeOut = 0;
	lfArial.lfCharSet = 0;
	lfArial.lfOutPrecision = 3;
	lfArial.lfClipPrecision = 2;
	lfArial.lfQuality = 1;
	lfArial.lfPitchAndFamily = 34;
	strcpy(lfArial.lfFaceName,"Arial");

	hSumFont = CreateFontIndirect(&lfArial); // create font for sum display
	if(hSumFont == NULL) {
		return false;
	}

	lfArial.lfHeight = -18;
	hProfileFont = CreateFontIndirect(&lfArial); // create font for profile display
	if(hProfileFont == NULL) {
		return false;
	}
	
	hBmpDigits = (HBITMAP)LoadImage(hAppInstance, MAKEINTRESOURCE(IDB_DIGITS), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR); // load digit bitmap
	if(hBmpDigits == NULL) {
		return false;
	}
	hOffscreenDc = CreateCompatibleDC(GetDC(NULL)); // create offscreen dc that holds the time
	if(hOffscreenDc == NULL) {
		return false;
	}
	hBmp = CreateCompatibleBitmap(GetDC(NULL), iBmpWidth, iBmpHeight); // create bitmap for drawing the time on it
	if(hBmp == NULL) {
		return false;
	}
	SelectObject(hOffscreenDc, hBmp);
	hBitmapDc = CreateCompatibleDC(GetDC(NULL)); // create dc for holding the digit bitmap
	if(hBitmapDc == NULL) {
		return false;
	}
	SelectObject(hBitmapDc, hBmpDigits);
	lbBrush.lbColor = RGB(0, 0, 0);
	lbBrush.lbStyle = BS_SOLID;
	hBlackBrush = CreateBrushIndirect(&lbBrush); // create background brush
	if(hBlackBrush == NULL) {
		return false;
	}
	SelectObject(hOffscreenDc, hBlackBrush);

	hAppIcon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_DOLLAR));	

	return true;
}

/* centers a window on screen */
void CenterhWnd(HWND hWnd){
	int cx = GetSystemMetrics(SM_CXFULLSCREEN);
	int cy = GetSystemMetrics(SM_CYFULLSCREEN);
	RECT rect;
	GetWindowRect(hWnd,&rect);
	int width = rect.right-rect.left;
	int height = rect.bottom-rect.top;
	SetWindowPos(hWnd,NULL,(cx-width)/2,(cy-height)/2,width,height,NULL);

}

/* centers a window ontop of another */
void CenterOnParent(HWND hWnd, HWND hWndParent) {
	RECT rChild, rParent;
	GetWindowRect(hWnd, &rChild);
	GetWindowRect(hWndParent, &rParent);
	POINT pParentMiddle, pChildMiddle;
	pParentMiddle.x = rParent.right-((rParent.right-rParent.left)/2);
	pParentMiddle.y = rParent.bottom-((rParent.bottom-rParent.top)/2);
	pChildMiddle.x = rChild.right-((rChild.right-rChild.left)/2);
	pChildMiddle.y = rChild.bottom-((rChild.bottom-rChild.top)/2);
	OffsetRect(&rChild, -(pChildMiddle.x-pParentMiddle.x), -(pChildMiddle.y-pParentMiddle.y));
	SetWindowPos(hWnd, NULL, rChild.left, rChild.top, 0, 0, SWP_NOSIZE);
}

/* calculates the current cost */
void calculateCost(HWND hWnd){
	char tmp[128];
	iCost = (((float)((iCountLevel1 * iLevel1) + (iCountLevel2 * iLevel2) + (iCountLevel3 * iLevel3))) / 3600.0f) * iTime;
	sprintf(tmp, "%.2f €", iCost);
	char *foo = strstr(tmp, ".");
	if(foo) *foo = ',';
	SetDlgItemText(hWnd, IDC_SUM, tmp);
};

/* enables the box where the user can edit the number of persons currently present */
void AnzahlBoxActivate(bool state, HWND hWnd){
	EnableWindow(GetDlgItem(hWnd, IDC_ANZ_1), state);
	EnableWindow(GetDlgItem(hWnd, IDC_ANZ_2), state);
	EnableWindow(GetDlgItem(hWnd, IDC_ANZ_3), state);
};

/* shows the levels in the edit windows */
void KostenBoxUpdate(HWND hWnd){
	SetDlgItemInt(hWnd, IDC_KOSTEN_1, iLevel1, false);
	SetDlgItemInt(hWnd, IDC_KOSTEN_2, iLevel2, false);
	SetDlgItemInt(hWnd, IDC_KOSTEN_3, iLevel3, false);
};


/* profile dlg proc */
BOOL CALLBACK ProfileProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			if(stricmp(chosenProfile, "Standard")) {
				iLevel1 = GetPrivateProfileInt(chosenProfile, "Level1", 0, SettingsFile);
				iLevel2 = GetPrivateProfileInt(chosenProfile, "Level2", 0, SettingsFile);
				iLevel3 = GetPrivateProfileInt(chosenProfile, "Level3", 0, SettingsFile);
			}
			WritePrivateProfileString("LastProfile", "Value", chosenProfile, SettingsFile);
			EndDialog(hWnd, 0);
			break;
		case IDC_SAVE:
			char txt[MAX_PROFILE_NAME_LENGTH+1];
			GetDlgItemText(hWnd, IDC_PROFILENAME, txt, MAX_PROFILE_NAME_LENGTH);
			if(stricmp(txt, "Standard")) {
				int l1= 0, l2= 0, l3= 0;
				l1 = GetDlgItemInt(hWnd, IDC_LEVEL1, NULL, false);
				l2 = GetDlgItemInt(hWnd, IDC_LEVEL2, NULL, false);
				l3 = GetDlgItemInt(hWnd, IDC_LEVEL3, NULL, false);
				if((l1 && l2 && l3) && (l1 > 0 && l2 > 0 && l3 > 0)) {
					iLevel1 = l1;
					iLevel2 = l2;
					iLevel3 = l3;
					char txt1[10];
					GetDlgItemText(hWnd, IDC_LEVEL1, txt1, 9);
					WritePrivateProfileString(txt, "Level1", txt1, SettingsFile);
					GetDlgItemText(hWnd, IDC_LEVEL2, txt1, 9);
					WritePrivateProfileString(txt, "Level2", txt1, SettingsFile);
					GetDlgItemText(hWnd, IDC_LEVEL3, txt1, 9);
					WritePrivateProfileString(txt, "Level3", txt1, SettingsFile);
				} else {
					MessageBox(hWnd, validValues, err, MB_ICONERROR | MB_OK);
				}
			} else {
				MessageBox(hWnd, nonstandard, err, MB_OK | MB_ICONERROR);
			}
			fillComboBox(hWnd);
			// find string select the profile and show it
			int sel;
			sel = SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_FINDSTRING, 0, (LPARAM)txt);
			SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_SETCURSEL, sel, 0);
			break;
		case IDC_DELPROFILE:
			if(stricmp(chosenProfile, "Standard")) {
				WritePrivateProfileSection(chosenProfile, NULL, SettingsFile);
				fillComboBox(hWnd);
				SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_SETCURSEL, 0, 0);
				DWORD lp = 0; 
				lp = ((CBN_SELCHANGE & 0xFFFF) << 16);
				SendMessage(hWnd, WM_COMMAND, (LPARAM)lp, 0);
			} else {
				MessageBox(hWnd, nodel, err, MB_ICONERROR | MB_OK);
			}
			break;
		}
		switch(HIWORD(wParam)) {
		case CBN_SELCHANGE:
			int sel, len;
			char txt[MAX_PROFILE_NAME_LENGTH+1];
			len = SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_GETLBTEXTLEN, 0, 0);
			if(len>MAX_PROFILE_NAME_LENGTH) {
				break;
			}
			sel = SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_GETCURSEL, 0, 0);
			SendMessage(GetDlgItem(hWnd, IDC_PROFILENAME), CB_GETLBTEXT, sel, (LPARAM)txt);
			if(!stricmp(txt, "Standard")) {
				iLevel1 = 200;
				iLevel2 = 150;
				iLevel3 = 100;
				SetDlgItemInt(hWnd, IDC_LEVEL1, iLevel1, false);
				SetDlgItemInt(hWnd, IDC_LEVEL2, iLevel2, false);
				SetDlgItemInt(hWnd, IDC_LEVEL3, iLevel3, false);
			} else {
				showProfile(hWnd, txt);
			}
			strcpy(chosenProfile, txt);
			break;
		}
	break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		fillComboBox(hWnd);
		CenterOnParent(hWnd, hDialogWindow);
		SetDlgItemText(hWnd, IDC_PROFILENAME, chosenProfile);
		SetDlgItemInt(hWnd, IDC_LEVEL1, iLevel1, false);
		SetDlgItemInt(hWnd, IDC_LEVEL2, iLevel2, false);
		SetDlgItemInt(hWnd, IDC_LEVEL3, iLevel3, false);
		break;
	}
	// for compiler warning level 4
	UNREFERENCED_PARAMETER(lParam);
	return false;
}


/* main dlg proc */
BOOL CALLBACK MeetometerProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
	case WM_TIMER:
		iTime++;
		if(iTime > 359999) {
			iTime = 0;
		}
		drawClock();
		calculateCost(hWnd);
		SendMessage(hWnd, WM_PAINT, 0, (LPARAM)true);
		break;
	case WM_INITDIALOG:
		if(!initApplication()) {
			MessageBox(hWnd, outOfMem, err, MB_ICONERROR|MB_OK);
			cleanUp();
			EndDialog(hWnd, 0);
			return -1;
		}
		drawClock();

		AnzahlBoxActivate(true, hWnd);	
		SetDlgItemText(hWnd, IDC_ANZ_1, "0");
		SetDlgItemText(hWnd, IDC_ANZ_2, "0");
		SetDlgItemText(hWnd, IDC_ANZ_3, "0");
		
		KostenBoxUpdate(hWnd);

		CenterhWnd(hWnd);

		SendMessage(GetDlgItem(hWnd, IDC_SUM), WM_SETFONT, (WPARAM)hSumFont, (LPARAM)true);
		SendMessage(GetDlgItem(hWnd, WM_PROFILE), WM_SETFONT, (WPARAM)hProfileFont, (LPARAM)true);
		SetDlgItemText(hWnd, IDC_PROFLABEL, chosenProfile);
		UpdateWindow(hWnd);
		hDialogWindow = hWnd;

		SendMessage(hWnd, WM_SETICON, 0, (LPARAM)hAppIcon);
		SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);

		RegisterHotKey(hWnd, 0xAAAA, MOD_ALT, VK_F5);
		break;
	case WM_PAINT:
		drawClock();
		BitBlt(GetDC(GetDlgItem(hWnd, IDC_CLOCK)), 0, 0, iBmpWidth, iBmpHeight, hOffscreenDc, 0, 0, SRCCOPY); 
		break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_START:
			if(cMeterState == STATE_HALT) iTime = 0;
			drawClock();
			SendMessage(hWnd, WM_PAINT, 0, (LPARAM)true);
			SetTimer(hWnd, uTimerID, 1000, NULL);
			cMeterState = STATE_RUN;
			EnableWindow(GetDlgItem(hWnd, IDC_START), false);
			EnableWindow(GetDlgItem(hWnd, IDC_STOP), true);
			EnableWindow(GetDlgItem(hWnd, IDC_PAUSE), true);
			AnzahlBoxActivate(false, hWnd);
			iCountLevel1 = GetDlgItemInt(hWnd, IDC_ANZ_1, NULL, false);
			iCountLevel2 = GetDlgItemInt(hWnd, IDC_ANZ_2, NULL, false);
			iCountLevel3 = GetDlgItemInt(hWnd, IDC_ANZ_3, NULL, false);
			EnableWindow(GetDlgItem(hWnd, IDC_EDITPROFILES), false);
			break;
		case IDC_PAUSE:
			KillTimer(hWnd, uTimerID);
			cMeterState = STATE_PAUSE;
			EnableWindow(GetDlgItem(hWnd, IDC_START), true);
			EnableWindow(GetDlgItem(hWnd, IDC_STOP), true);
			EnableWindow(GetDlgItem(hWnd, IDC_PAUSE), false);
			break;
		case IDC_STOP:
			KillTimer(hWnd, uTimerID);
			cMeterState = STATE_HALT;
			EnableWindow(GetDlgItem(hWnd, IDC_START), true);
			EnableWindow(GetDlgItem(hWnd, IDC_STOP), false);
			EnableWindow(GetDlgItem(hWnd, IDC_PAUSE), false);
			AnzahlBoxActivate(true, hWnd);
			EnableWindow(GetDlgItem(hWnd, IDC_EDITPROFILES), true);
			break;
		case IDC_EDITPROFILES:
			int Profile;
			Profile = DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_PROFILE), hWnd, ProfileProc, 0);
			KostenBoxUpdate(hDialogWindow);
			SetDlgItemText(hWnd, IDC_PROFLABEL, chosenProfile);
			break;
		}
		break;
	case WM_HOTKEY:
		if(wParam == 0xAAAA) {
			// hide or show controls and move buttons up or down, then resize window
			if(bInfoVisible) {
				RECT rPos;
				POINT rP;
				ShowWindow(GetDlgItem(hWnd, IDC_STATICP), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL1), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL2), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL3), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC1), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC2), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC3), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_1), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_2), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_3), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_1), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_2), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_3), SW_HIDE);
				GetWindowRect(GetDlgItem(hWnd, IDC_START), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_START), NULL, rP.x, rP.y-120, 0, 0, SWP_NOSIZE);
				GetWindowRect(GetDlgItem(hWnd, IDC_PAUSE), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_PAUSE), NULL, rP.x, rP.y-120, 0, 0, SWP_NOSIZE);
				GetWindowRect(GetDlgItem(hWnd, IDC_STOP), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_STOP), NULL, rP.x, rP.y-120, 0, 0, SWP_NOSIZE);
				GetWindowRect(hWnd, &rPos);
				SetWindowPos(hWnd, NULL, 0, 0, rPos.right-rPos.left,rPos.bottom-rPos.top-120, SWP_NOMOVE);
				bInfoVisible = !bInfoVisible;
			} else {
				RECT rPos;
				POINT rP;
				ShowWindow(GetDlgItem(hWnd, IDC_STATICP), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL1), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL2), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICL3), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC1), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC2), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_STATICC3), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_1), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_2), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_ANZ_3), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_1), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_2), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_KOSTEN_3), SW_SHOW);
				GetWindowRect(GetDlgItem(hWnd, IDC_START), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_START), NULL, rP.x, rP.y+120, 0, 0, SWP_NOSIZE);
				GetWindowRect(GetDlgItem(hWnd, IDC_PAUSE), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_PAUSE), NULL, rP.x, rP.y+120, 0, 0, SWP_NOSIZE);
				GetWindowRect(GetDlgItem(hWnd, IDC_STOP), &rPos);
				rP.x = rPos.left;
				rP.y = rPos.top;
				ScreenToClient(hWnd, &rP);
				SetWindowPos(GetDlgItem(hWnd, IDC_STOP), NULL, rP.x, rP.y+120, 0, 0, SWP_NOSIZE);
				GetWindowRect(hWnd, &rPos);
				SetWindowPos(hWnd, NULL, 0, 0, rPos.right-rPos.left,rPos.bottom-rPos.top+120, SWP_NOMOVE);
				bInfoVisible = !bInfoVisible;
			}
		}
		break;
	}
	// for compiler warning level 4
	UNREFERENCED_PARAMETER(lParam);
	return false;
}



/* program entry point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	HMODULE hModule = NULL;
	// alloc mem for settings file path
	SettingsFile = (char*)HeapAlloc(GetProcessHeap(), 0, 500*sizeof(char));
	if(SettingsFile == NULL) {
		MessageBox(NULL, outOfMem, err, MB_ICONERROR | MB_OK);
		return -1;
	}
	// craft settings file path
	hModule = GetModuleHandle(NULL);
	GetModuleFileName(hModule, SettingsFile, 500);
	int i = strlen(SettingsFile);
	while(i >= 0 && SettingsFile[i] != '\\') {
		i--;
	}
	SettingsFile[i+1] = '\0';
	strcat(SettingsFile, "Settings.ini");
	// load profile
	hAppInstance = hInstance;
	LoadProfile();
	// start app dlg wnd
	DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_MEETOMETER), NULL, MeetometerProc, 0);
	// free ressources
	cleanUp();
	// for compiler warning level 4
	UNREFERENCED_PARAMETER(nShowCmd);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(hPrevInstance);
	return 0;
}