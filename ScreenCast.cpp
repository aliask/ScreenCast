// ScreenCast.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "ScreenCast.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
DWORD ipAddress = MAKEIPADDRESS(192, 168, 0, 26);
int udpPort = 20304;
int panelWidth = 32;
int panelHeight = 16;
int FPS = 10;
bool running = false;
SOCKET s = WSANOTINITIALISED;

// Debug output wrapper
inline void logA(const char* format, ...)
{
	char buf[1024];
	wvsprintfA(buf, format, ((char*)&format) + sizeof(void*));
	OutputDebugStringA(buf);
}

int sendUDP(char* pkt, int size) {

	sockaddr_in dest;
	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);
	ULONG* srcAddr = new ULONG;
	ULONG* destAddr = new ULONG;
	int written = 0;

	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl(ipAddress);
	dest.sin_port = htons(udpPort);

	// Happens on first run, or on a previous sendUDP failing
	if (s == WSANOTINITIALISED)
		s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	// Can't write to a socket which isn't initialized
	if (s != WSANOTINITIALISED)
		written = sendto(s, pkt, size, 0, (sockaddr *)&dest, sizeof(dest));

	delete srcAddr;
	delete destAddr;

	return written;
}

bool SendBMPFile(HBITMAP bitmap, HDC bitmapDC, int width, int height){
	bool Success = false;
	HDC SurfDC = NULL;        // GDI-compatible device context for the surface
	HBITMAP OffscrBmp = NULL; // bitmap that is converted to a DIB
	HDC OffscrDC = NULL;      // offscreen DC that we can select OffscrBmp into
	LPBITMAPINFO lpbi = NULL; // bitmap format info; used by GetDIBits
	LPVOID lpvBits = NULL;    // pointer to bitmap bits array

	// We need an HBITMAP to convert it to a DIB:
	if ((OffscrBmp = CreateCompatibleBitmap(bitmapDC, width, height)) == NULL) {
		logA("Couldn't create offscreen bitmap");
		return false;
	}
	// The bitmap is empty, so let's copy the contents of the surface to it.
	// For that we need to select it into a device context. We create one.
	if ((OffscrDC = CreateCompatibleDC(bitmapDC)) == NULL) {
		DeleteObject(OffscrBmp);
		logA("Couldn't create DC");
		return false;
	}
	// Select OffscrBmp into OffscrDC:
	HBITMAP OldBmp = (HBITMAP)SelectObject(OffscrDC, OffscrBmp);
	// Now we can copy the contents of the surface to the offscreen bitmap:
	BitBlt(OffscrDC, 0, 0, width, height, bitmapDC, 0, 0, SRCCOPY);
	// GetDIBits requires format info about the bitmap. We can have GetDIBits
	// fill a structure with that info if we pass a NULL pointer for lpvBits:
	// Reserve memory for bitmap info (BITMAPINFOHEADER + largest possible
	// palette):
	if ((lpbi = (LPBITMAPINFO)(new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)])) == NULL) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		logA("Couldn't allocate bitmapinfo");
		return false;
	}
	ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	// Get info but first de-select OffscrBmp because GetDIBits requires it:
	SelectObject(OffscrDC, OldBmp);
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, NULL, lpbi, DIB_RGB_COLORS)) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		logA("Couldn't get bitmap info");
		return false;
	}
	// Abort if we don't have enough data to fill a frame
	if (lpbi->bmiHeader.biSizeImage < 4 * height * width) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		logA("Not enough data to make a frame");
		return false;
	}
	// Reserve memory for bitmap bits:
	if ((lpvBits = new char[lpbi->bmiHeader.biSizeImage]) == NULL) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		logA("Couldn't allocate bitmap");
		return false;
	}
	// Have GetDIBits convert OffscrBmp to a DIB (device-independent bitmap):
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS)) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		delete[]lpvBits;
		logA("Couldn't get bitmap data");
		return false;
	}

	// Convert byte order of pixel from BGRA to RGBA
	unsigned char temp[4];
	unsigned char* pixelOffset;
	for (int i = 0; i < lpbi->bmiHeader.biSizeImage/4; i++)
	{
		pixelOffset = (unsigned char*)lpvBits + (4 * i);
		temp[0] = *(pixelOffset + 2);
		temp[1] = *(pixelOffset + 1);
		temp[2] = *(pixelOffset + 0);
		temp[3] = *(pixelOffset + 3);
		*((u_long*)lpvBits + i) = *(u_long*)temp;
	}

	size_t headerSize = offsetof(struct Frame, data);
	size_t dataSize = lpbi->bmiHeader.biSizeImage;
	struct Frame *sendFrame = (struct Frame*)malloc(headerSize + dataSize);
	sendFrame->ident = FRAME_IDENT;
	sendFrame->height = height;
	sendFrame->width = width;
	sendFrame->length = dataSize;
	memcpy_s(sendFrame->data, dataSize, (char*)lpvBits, dataSize);

	// Write frame to the socket:
	unsigned int written = sendUDP((char*)sendFrame, headerSize + dataSize);
	if (written == SOCKET_ERROR) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		delete[]lpvBits;
		free(sendFrame);
		logA("Couldn't send UDP packet: Socket error %d\n", WSAGetLastError());
		return false;
	}

	if (written < lpbi->bmiHeader.biSizeImage) {
		DeleteObject(OffscrBmp);
		DeleteObject(OffscrDC);
		delete[]lpbi;
		delete[]lpvBits;
		free(sendFrame);
		logA("Couldn't send UDP packet: %d / %d bytes sent\n", written, lpbi->bmiHeader.biSizeImage);
		return false;
	}

	// Free up memory allocated in this function
	DeleteObject(OffscrBmp);
	DeleteObject(OffscrDC);
	delete[]lpbi;
	delete[]lpvBits;
	free(sendFrame);

	return true;
}

bool ScreenCaptureUDP(int x, int y, int width, int height){
	// get a DC compat. w/ the screen
	HDC hDc = CreateCompatibleDC(0);

	// make a bmp in memory to store the capture in
	HDC hBmpDc = GetDC(0);
	HBITMAP hBmp = CreateCompatibleBitmap(hBmpDc, width, height);

	// join em up
	SelectObject(hDc, hBmp);

	// copy from the screen to my bitmap
	HDC hDcResized = GetDC(0);
	SetStretchBltMode(hDc, HALFTONE);
	StretchBlt(hDc, 0, 0, panelWidth, panelHeight, hDcResized, x, y, width, height, SRCCOPY);

	// Send UDP STREAM
	bool ret = SendBMPFile(hBmp, hDc, panelWidth, panelHeight);

	// free the bitmap memory
	DeleteObject(hBmp);
	DeleteDC(hDc);
	DeleteDC(hBmpDc);
	DeleteDC(hDcResized);

	return ret;
}

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	Connect(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SCREENCAST, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SCREENCAST));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SCREENCAST));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_SCREENCAST);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   // Set Window as always-on-top
   SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

   // Make Client Area transparent
   SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
   SetLayeredWindowAttributes(hWnd, RGB(255, 0, 0), 0, LWA_COLORKEY); // Use this rather than LWA_ALPHA to avoid clickthrough
   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_STARTSTOP:
			if (running)
			{
				SetWindowText(hWnd, L"ScreenCast");
				running = false;
			}
			else
			{
				SetWindowText(hWnd, L"ScreenCast (Casting)");
				running = true;
			}
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_CONNECT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_CONNECT), hWnd, Connect);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(255, 0, 0)));
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		if (!running)
			break;

		RECT rect;
		GetClientRect(hWnd, &rect);
		ClientToScreen(hWnd, reinterpret_cast<POINT*>(&rect.left)); // convert top-left
		ClientToScreen(hWnd, reinterpret_cast<POINT*>(&rect.right)); // convert bottom-right

		if (!ScreenCaptureUDP(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top))
		{
			// On error, stop the stream
			SetWindowText(hWnd, L"ScreenCast");
			running = false;
			MessageBox(hWnd, L"Error sending screen!", L"Error", MB_OK | MB_ICONEXCLAMATION);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK Connect(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		//SetDlgItemText(hDlg, IDC_IPADDR, ipAddr);
		SendMessage(GetDlgItem(hDlg, IDC_IPADDRESS), IPM_SETADDRESS, 0, ipAddress);
		SetDlgItemInt(hDlg, IDC_UDPPORT, udpPort, false);
		SetDlgItemInt(hDlg, IDC_HEIGHT, panelHeight, false);
		SetDlgItemInt(hDlg, IDC_WIDTH, panelWidth, false);
		SetDlgItemInt(hDlg, IDC_FPS, FPS, false);
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			SendMessage(GetDlgItem(hDlg, IDC_IPADDRESS), IPM_GETADDRESS, 0, (LPARAM)&ipAddress);

			udpPort = GetDlgItemInt(hDlg, IDC_UDPPORT, false, false);
			if (udpPort < 1024 || udpPort > 65535)
				return MessageBox(hDlg, L"Port value invalid, please specify between 1024 and 65535", L"Error connecting", MB_ICONEXCLAMATION);

			panelHeight = GetDlgItemInt(hDlg, IDC_HEIGHT, false, false);
			panelWidth = GetDlgItemInt(hDlg, IDC_WIDTH, false, false);
			//if (panelHeight * panelWidth * 4 > 1470)
			//	return MessageBox(hDlg, L"Panel resolution too big to send via network", L"Error connecting", MB_ICONEXCLAMATION);
			if (!panelHeight || !panelWidth)
				return MessageBox(hDlg, L"Can't have zero sized panel", L"Error connecting", MB_ICONEXCLAMATION);

			FPS = GetDlgItemInt(hDlg, IDC_FPS, false, false);
			if (!FPS || FPS >= 100)
				return MessageBox(hDlg, L"FPS value invalid, please specify between 1 and 100 FPS", L"Error connecting", MB_ICONEXCLAMATION);

			running = true;
			SetTimer(GetParent(hDlg), 0, 1 / FPS, NULL); // Set timer on the main thread
			SetWindowText(GetParent(hDlg), L"ScreenCast (Casting)");

		}

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
