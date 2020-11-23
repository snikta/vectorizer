#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <windows.h>
#include <windowsx.h>
#include <wincodec.h>
#include <d2d1.h>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <set>
#include <cctype>
#include "IntPoint.h"
//#include "simplify.h"
//#include "jSignature.h"
#include "comdef.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#pragma comment(lib, "d2d1")
#pragma comment(lib, "windowscodecs.lib")

#include "basewin.h"

using std::set;
using std::string;
using std::map;
using std::to_string;
using std::vector;
using std::queue;
using namespace cv;
using cv::Mat;
using cv::Size;
using cv::COLOR_BGR2GRAY;
typedef cv::Point3_<uint8_t> Pixel;

Mat src, src_gray;
Mat dst, detected_edges;
int lowThreshold = 30;
const int max_lowThreshold = 30;
const int ratio = 3;
const int kernel_size = 3;
const char* window_name = "Edge Map";
int maxX;
int py;
map<int, map<int, bool>> visited;
bool layoutDone = false;

int frameWidth = 960;
int frameHeight = 400;

template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

struct DecodeEl {
	int x;
	int y;
	int run_length;
	DecodeEl(int x, int y, int run_length) : x(x), y(y), run_length(run_length) {};
};

string bytes = "";
int curBitIdx = 0;
int curByteIdx = 0;
int curBitSet = 0;
string bitstream = "";

void addBit(bool bit) {
	if (bit) {
		curBitSet |= (1 << (7 - curBitIdx));
	}
	curBitIdx++;
	if (curBitIdx == 8) {
		bytes += (char)(UINT8)(curBitSet);
		curBitIdx = 0;
		curByteIdx++;
		curBitSet = 0;
	}
}
int getBit() {
	if (curBitIdx == 8) {
		curBitIdx = 0;
		curByteIdx++;
	}
	if (curByteIdx >= bytes.size()) {
		return 0;
	}
	int bit = ((UINT8)(bytes[curByteIdx]) & (1 << (7 - curBitIdx))) != 0 ? 1 : 0;
	curBitIdx++;
	return bit;
}
string bitify(char bite) {
	string bits = "";
	for (int i = 7; i >= 0; i--) {
		bits += (bite & (1 << i)) ? '1' : '0';
	}
	return bits;
}

string lastChars(string str, int count) {
	return str.substr(str.size() - count, std::string::npos);
}

void addShortDelta(IntPoint delta) {
	if (delta.x >= -1 && delta.x <= 1 && delta.y >= -1 && delta.y <= 1) {
		if (delta.x == -1) { addBit(0); addBit(0); }
		else if (delta.x == 0) { addBit(0); addBit(1); }
		else if (delta.x == 1) { addBit(1); addBit(0); }
		if (delta.y == -1) { addBit(0); addBit(0); }
		else if (delta.y == 0) { addBit(0); addBit(1); }
		else if (delta.y == 1) { addBit(1); addBit(0); }
	}
}

void addDelta(IntPoint delta) {
	if (delta.x >= -1 && delta.x <= 1 && delta.y >= -1 && delta.y <= 1) {
		addBit(0);
		if (delta.x == -1) { addBit(0); addBit(0); }
		else if (delta.x == 0) { addBit(0); addBit(1); }
		else if (delta.x == 1) { addBit(1); addBit(0); }
		if (delta.y == -1) { addBit(0); addBit(0); }
		else if (delta.y == 0) { addBit(0); addBit(1); }
		else if (delta.y == 1) { addBit(1); addBit(0); }
	}
	else {
		addBit(1);
		delta.x = delta.x; // 0xFFFF limit
		delta.y = delta.y; // 0xFFFF limit
		string bits = (delta.x < 0 ? '1' : '0') + lastChars(bitify(abs(delta.x) >> 6), 6) + lastChars(bitify(abs(delta.x) & 0xFF), 6) + (delta.y < 0 ? '1' : '0') + lastChars(bitify(abs(delta.y) >> 6), 6) + lastChars(bitify(abs(delta.y) & 0xFF), 6);
		for (auto bit : bits) {
			addBit(bit == '1');
		}
	}
}

vector<vector<IntPoint>> izePoints;

string toLowerCase(string str) {
	string lcasestr;
	for (char c : str) {
		lcasestr += tolower(c);
	}
	return lcasestr;
}

map<int, map<int, bool>> toInterpolate;
cv::VideoCapture mov("simpsons.mp4");

class MainWindow : public BaseWindow<MainWindow>
{
	ID2D1Factory* pFactory;
	ID2D1HwndRenderTarget* pRenderTarget;
	ID2D1SolidColorBrush* pBrush;

	void    CalculateLayout();
	HRESULT CreateGraphicsResources();
	void    DiscardGraphicsResources();
	void    OnPaint();
	void    Resize();
	void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);

public:

	ID2D1StrokeStyle* pStrokeStyle;
	int frameIndex = 0;
	int frameCount;

	MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL)
	{
		srand(time(NULL));
		mov.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
		mov.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
		mov.set(cv::CAP_PROP_POS_FRAMES, frameIndex);
	};

	PCWSTR  ClassName() const { return L"Circle Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// Recalculate drawing layout when the size of the window changes.

string cslice(char* carr, int start, int len) {
	string out = "";
	for (int i = start, end = start + len; i < end; i++) {
		out += carr[i];
	}
	return out;
}

struct labVal {
	double l;
	double a;
	double b;
	labVal(double l, double a, double b) : l(l), a(a), b(b) {};
};

labVal nd(double p, double s) {
	labVal A = labVal(10.16, 10.68, 10.70);
	labVal B = labVal(1.50, 3.08, 5.74);

	return labVal(
		p * (A.l + B.l / s),
		p * (A.a + B.a / s),
		p * (A.b + B.b / s)
	);
}

double K = 18.0;
double Xn = 0.96422;
double Yn = 1.0;
double Zn = 0.82521;
double t0 = 4.0 / 29.0;
double t1 = 6.0 / 29.0;
double t2 = 3.0 * t1 * t1;
double t3 = t1 * t1 * t1;

struct rgb {
	int r;
	int g;
	int b;
	rgb(int r, int g, int b) : r(r), g(g), b(b) {};
};

double rgb2lrgb(double x) {
	return (x /= 255) <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

double xyz2lab(double t) {
	if (t == 0.0) {
		return t0;
	}
	return t > t3 ? pow(t, 1.0 / 3.0) : t / t2 + t0;
}

labVal labConvert(rgb o) {
	double r = rgb2lrgb(o.r);
	double g = rgb2lrgb(o.g);
	double b = rgb2lrgb(o.b);
	double y = xyz2lab((0.2225045 * r + 0.7168786 * g + 0.0606169 * b) / Yn);
	double x;
	double z;
	if (r == g && g == b) {
		z = y;
		x = z;
	}
	else {
		x = xyz2lab((0.4360747 * r + 0.3850649 * g + 0.1430804 * b) / Xn);
		z = xyz2lab((0.0139322 * r + 0.0971045 * g + 0.7141733 * b) / Zn);
	}
	return labVal(116.0 * y - 16.0, 500.0 * (x - y), 200.0 * (y - z));
}


labVal lab(rgb l) {
	return labConvert(l);
}

bool noticeablyDifferent(rgb c1, rgb c2) {
	double s = 0.1;
	double p = 0.5;

	labVal jnd = nd(p, s);
	labVal c1_lab = lab(c1);
	labVal c2_lab = lab(c2);

	return (abs(c1_lab.l - c2_lab.l) >= jnd.l) || (abs(c1_lab.a - c2_lab.a) >= jnd.a) || (abs(c1_lab.b - c2_lab.b) >= jnd.b);
}

void MainWindow::CalculateLayout()
{
	if (pRenderTarget != NULL)
	{
		D2D1_SIZE_F size = pRenderTarget->GetSize();
		const float x = size.width / 2;
		const float y = size.height / 2;
		const float radius = min(x, y);
	}

	layoutDone = true;
}

HRESULT MainWindow::CreateGraphicsResources()
{
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
		rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
		rtProps.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

		hr = pFactory->CreateHwndRenderTarget(
			rtProps,
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			&pRenderTarget);

		if (SUCCEEDED(hr))
		{
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);

			if (SUCCEEDED(hr))
			{
				CalculateLayout();
			}
		}
	}
	return hr;
}

void MainWindow::DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

bool similarColor(char* imd, char* imd2, int x, int y, rgb color) {
	int curpx = (y)* frameWidth * 4 + (x) * 4;
	UINT8 red = (UINT8)imd[curpx];
	UINT8 green = (UINT8)imd[curpx + 1];
	UINT8 blue = (UINT8)imd[curpx + 2];
	UINT8 imd2_alpha = (UINT8)imd2[curpx + 3];
	UINT8 imd_alpha = (UINT8)imd[curpx + 3];
	if (!(imd2_alpha != 255 && imd_alpha == 255)) {
		return false;
	}
	if (red == color.r && green == color.g && blue == color.b) {
		return true;
	}
	return !noticeablyDifferent(rgb(red, green, blue), color);
}

struct PixelArea {
	map<int, int> StartXByY;
	map<int, int> EndXByY;
	rgb Color = rgb(0,0,0);
};

bool similarColor2(char* imd, int x, int y, rgb color) {
	int curpx = (y)* frameWidth * 4 + (x) * 4;
	return (UINT8)imd[curpx + 3] == 255;// && !noticeablyDifferent(rgb((UINT8)imd[curpx], (UINT8)imd[curpx + 1], (UINT8)imd[curpx + 2]), color);
}

void MainWindow::OnPaint()
{
	using namespace std::this_thread; // sleep_for, sleep_until
	using namespace std::chrono; // nanoseconds, system_clock, seconds
	HRESULT hr = CreateGraphicsResources();

	double bytecount = 0;

	if (SUCCEEDED(hr))// && layoutDone)
	{
		PAINTSTRUCT ps;
		BeginPaint(m_hwnd, &ps);

		int x = 0;
		int y = 0;

		curBitIdx = 0;
		curByteIdx = 0;
		curBitSet = 0;
		bytes = "";
		std::ofstream myfile;
		myfile.open("chroma.txt", std::ios_base::binary | std::ios_base::out);

		int realFrameCount = 3288;// mov.get(cv::CAP_PROP_FRAME_COUNT);
		int frameDiff = realFrameCount - mov.get(cv::CAP_PROP_POS_FRAMES);
		myfile << (char)(UINT8)(frameDiff >> 8 & 0xFF);
		myfile << (char)(UINT8)(frameDiff & 0xFF);

		{
			string s1 = "realFrameCount: " + to_string(realFrameCount) + " frameDiff: " + to_string(frameDiff);
			std::wstring widestr = std::wstring(s1.begin(), s1.end());
			OutputDebugStringW(widestr.c_str());
		}

		vector<int> frameEdgeBytes;
		vector<int> frameEdgeBits;
		int prevByteIdx = 0;
		int accumBytes = 0;
		int accumBits = 0;
		int i = 0;
		char* prevFrame = nullptr;

		while (true)
		{
			++i;
			pRenderTarget->BeginDraw();
			pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

			if (mov.get(cv::CAP_PROP_POS_FRAMES) >= realFrameCount) {
				break;
			}

			char* curFrame = new char[frameWidth * frameHeight * 4];

			mov >> src;

			resize(src, src, Size(frameWidth, frameHeight), 0, 0, cv::INTER_CUBIC);

			if (!src.empty())
			{
				dst.create(src.size(), src.type());

				{
					int x = 0;
					int y = 0;
					int cols = src.cols;
					for (Pixel& p : cv::Mat_<Pixel>(src)) {
						if (x == cols)
						{
							x = 0;
							y++;
						}
						int idx = (y)* frameWidth * 4 + (x) * 4;
						curFrame[idx] = src.at<Pixel>(y, x).z;
						curFrame[idx + 1] = src.at<Pixel>(y, x).y;
						curFrame[idx + 2] = src.at<Pixel>(y, x).x;
						curFrame[idx + 3] = 255;
						//}

						x++;
					}
				}
			}

			//cvtColor(src, src_gray, cv::COLOR_BGR2GRAY);
			//blur(src_gray, src_gray, Size(3, 3));

			/// Detect edges using canny
			//Canny(src_gray, canny_output, 100, 100 * 2, 3);
			/// Find contours
			//findContours(canny_output, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE, Point(0, 0));

			/*string s1 = "contour count: " + to_string(contours.size()) + "\n";
			std::wstring widestr = std::wstring(s1.begin(), s1.end());
			OutputDebugStringW(widestr.c_str());

			for (int i = 0, len = contours.size(); i < len; i++) {
				for (int j = 1, jLen = contours[i].size(); j < jLen; j++) {
					string s1 = "(" + to_string(contours[i][j].x - contours[i][j-1].x) + ", " + to_string(contours[i][j].y - contours[i][j-1].y) + ")\n";
					std::wstring widestr = std::wstring(s1.begin(), s1.end());
					OutputDebugStringW(widestr.c_str());
				}
			}*/

			frameEdgeBytes.push_back(curByteIdx - prevByteIdx);
			frameEdgeBits.push_back(curBitIdx);

			accumBytes += curByteIdx - prevByteIdx;
			accumBits += curBitIdx;

			prevByteIdx = curByteIdx;

			char* interp_pixels = curFrame;
			char* interp_pixels2 = new char[frameWidth * frameHeight * 4];

			//int y;
			//int x;
			int rows;
			int cols;

			map<int, map<int, PixelArea*>> pxAreas;

			{
				typedef cv::Point3_<uint8_t> Pixel;
				int px_idx = 0;
				rows = frameHeight;
				for (y = 0, rows = frameHeight; y < rows; y += 1) {
					pxAreas[y] = {};
					for (x = 0, cols = frameWidth; x < cols - 1; x += 1) {
						int curpx = (y)* frameWidth * 4 + (x) * 4;
						int aboveleft = (y-1)* frameWidth * 4 + (x - 1) * 4;
						int abovetop = (y - 1) * frameWidth * 4 + (x) * 4;
						int left = y * frameWidth * 4 + (x - 1) * 4;

						if (x > 0) {	
							if (noticeablyDifferent(rgb((UINT8)interp_pixels[left], (UINT8)interp_pixels[left + 1], (UINT8)interp_pixels[left + 2]), rgb((UINT8)interp_pixels[curpx], (UINT8)interp_pixels[curpx + 1], (UINT8)interp_pixels[curpx + 2]))) {
								interp_pixels2[curpx + 3] = (UINT8)255;
							}
						}

						if (y > 0) {
							if (noticeablyDifferent(rgb((UINT8)interp_pixels[abovetop], (UINT8)interp_pixels[abovetop + 1], (UINT8)interp_pixels[abovetop + 2]), rgb((UINT8)interp_pixels[curpx], (UINT8)interp_pixels[curpx + 1], (UINT8)interp_pixels[curpx + 2]))) {
								interp_pixels2[curpx + 3] = (UINT8)255;
							}
						}

						if (x > 0 && y > 0) {
							if (noticeablyDifferent(rgb((UINT8)interp_pixels[aboveleft], (UINT8)interp_pixels[aboveleft + 1], (UINT8)interp_pixels[aboveleft + 2]), rgb((UINT8)interp_pixels[curpx], (UINT8)interp_pixels[curpx + 1], (UINT8)interp_pixels[curpx + 2]))) {
								interp_pixels2[curpx + 3] = (UINT8)255;
							}
						}
					}
				}
			}

			if (true || i == 1) {
				ID2D1Bitmap* pBitmap = NULL;
				hr = pRenderTarget->CreateBitmap(D2D1::SizeU(frameWidth, frameHeight), interp_pixels2, frameWidth * 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &pBitmap);

				// Draw a bitmap.
				pRenderTarget->DrawBitmap(
					pBitmap,
					D2D1::RectF(
						0,
						0,
						frameWidth,
						frameHeight
					),
					1.0
				);

				SafeRelease(&pBitmap);

				hr = pRenderTarget->EndDraw();
			}

			delete[] interp_pixels;
			delete[] interp_pixels2;

			//break;
		}

		string s1 = "bytecount: " + to_string(bytecount) + "\n";
		std::wstring widestr = std::wstring(s1.begin(), s1.end());
		OutputDebugStringW(widestr.c_str());

		for (int i = 0, len = frameEdgeBytes.size(); i < len; i++) {
			int byt = frameEdgeBytes[i];
			int bits = frameEdgeBits[i];
			myfile << (char)(UINT8)(byt >> 16 & 0xFF);
			myfile << (char)(UINT8)(byt >> 8 & 0xFF);
			myfile << (char)(UINT8)(byt & 0xFF);
			myfile << (char)(UINT8)(bits & 0xFF);
		}

		myfile << bytes;
		myfile.close();

		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			DiscardGraphicsResources();
		}
		EndPaint(m_hwnd, &ps);
	}
}

void MainWindow::Resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);
		CalculateLayout();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	MainWindow win;

	if (!win.Create(L"Vectorizer", WS_OVERLAPPED))
	{
		return 0;
	}

	ShowWindow(win.Window(), SW_SHOWMAXIMIZED);

	// Run the message loop.

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
	SetCapture(m_hwnd);

	InvalidateRect(m_hwnd, NULL, FALSE);
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			return -1;  // Fail CreateWindowEx.
		}
		pFactory->CreateStrokeStyle(
			D2D1::StrokeStyleProperties(
				D2D1_CAP_STYLE_ROUND,
				D2D1_CAP_STYLE_ROUND,
				D2D1_CAP_STYLE_ROUND,
				D2D1_LINE_JOIN_ROUND,
				2.0f,
				D2D1_DASH_STYLE_SOLID,
				0.0f),
			NULL,
			0,
			&pStrokeStyle
		);
		return 0;

	case WM_DESTROY:
		DiscardGraphicsResources();
		SafeRelease(&pFactory);
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		OnPaint();
		return 0;

	case WM_LBUTTONDOWN:
		OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;

	case WM_SIZE:
		Resize();
		return 0;
	}
	return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}