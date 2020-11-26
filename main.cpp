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
#include "Point.h"
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
	int frameIndex = 2000;
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

struct rgba {
	int r;
	int g;
	int b;
	int a;
	rgba(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {};
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

bool similarColor2(char* imd, int x, int y, rgb color) {
	int curpx = (y)* frameWidth * 4 + (x) * 4;
	return (UINT8)imd[curpx + 3] == 255;// && !noticeablyDifferent(rgb((UINT8)imd[curpx], (UINT8)imd[curpx + 1], (UINT8)imd[curpx + 2]), color);
}

bool sameColor(rgba px1, rgba px2) {
	return (UINT8)px1.r == (UINT8)px2.r &&
		   (UINT8)px1.g == (UINT8)px2.g &&
		   (UINT8)px1.b == (UINT8)px2.b &&
		   (UINT8)px1.a == (UINT8)px2.a;
}

rgba getPixelColor(char* imd, int x, int y) {
	return rgba(
		(UINT8)imd[y * 980 * 4 + x * 4],
		(UINT8)imd[y * 980 * 4 + x * 4 + 1],
		(UINT8)imd[y * 980 * 4 + x * 4 + 2],
		(UINT8)imd[y * 980 * 4 + x * 4 + 3]
	);
}

vector<IntPoint> checkAbove(char* imd, int x, int y, rgba targetColor) {
	vector<IntPoint> retval;
	if (!sameColor(getPixelColor(imd, x - 1, y), targetColor)) {
		retval.push_back(IntPoint(x - 1, y));
	}
	if (!sameColor(getPixelColor(imd, x, y), targetColor)) {
		retval.push_back(IntPoint(x, y));
	}
	if (!sameColor(getPixelColor(imd, x + 1, y), targetColor)) {
		retval.push_back(IntPoint(x + 1, y));
	}
	return retval;
}

struct scanLineRetval {
	int minX;
	int maxX;
	vector<IntPoint> _abv;
	vector<IntPoint> _blw;
};

scanLineRetval scanLine(char* imd, int px, int py, rgba targetColor, bool _not) {
	int x = px;
	int y = py;
	int minX;
	int maxX;
	bool eq;
	vector<IntPoint> _abv;
	vector<IntPoint> _blw;

	eq = sameColor(getPixelColor(imd, x, y), targetColor);
	while (x > 0 && (_not ? !eq : eq)) {
		if (_not) {
			vector<IntPoint> abv = checkAbove(imd, x, y - 1, targetColor);
			if (abv.size()) {
				for (int i = 0, len = abv.size(); i < len; i++) {
					_abv.push_back(abv[i]);
				}
			}
			vector<IntPoint> blw = checkAbove(imd, x, y + 1, targetColor);
			if (blw.size()) {
				for (int i = 0, len = blw.size(); i < len; i++) {
					_blw.push_back(blw[i]);
				}
			}
		}
		x--;
		rgba color = getPixelColor(imd, x, y);
		eq = sameColor(getPixelColor(imd, x, y), targetColor);
	}
	minX = x + 1;

	x = px + 1;

	eq = sameColor(getPixelColor(imd, x, y), targetColor);
	while (x < 980 && (_not ? !eq : eq)) {
		if (_not) {
			vector<IntPoint> abv = checkAbove(imd, x, y - 1, targetColor);
			if (abv.size()) {
				for (int i = 0, len = abv.size(); i < len; i++) {
					_abv.push_back(abv[i]);
				}
			}
			vector<IntPoint> blw = checkAbove(imd, x, y + 1, targetColor);
			if (blw.size()) {
				for (int i = 0, len = blw.size(); i < len; i++) {
					_blw.push_back(blw[i]);
				}
			}
		}
		x++;
		eq = sameColor(getPixelColor(imd, x, y), targetColor);
	}
	maxX = x - 1;

	scanLineRetval retval;
	retval.minX = minX;
	retval.maxX = maxX;
	retval._abv = _abv;
	retval._blw = _blw;

	return retval;
}

map<int, map<int, bool>> floodFillMat;
vector<IntPoint> floodFillKnots;

bool sortAB(IntPoint a, IntPoint b) { return a.x - b.x; }
bool sortBA(IntPoint a, IntPoint b) { return b.x - a.x; }

void floodFill(char *imd, int px, int py, rgba targetColor, rgba fillColor, string dir) {
	if (floodFillMat.find(py) != floodFillMat.end() && floodFillMat[py].find(px) != floodFillMat[py].end()) {
		return;
	}
	if (px <= 0 || py <= 0 || px >= 980 || py >= 420 || sameColor(getPixelColor(imd, px, py), fillColor)) {
		return;
	}

	int y = py;

	scanLineRetval r = scanLine(imd, px, py, targetColor, true);
	int minX = r.minX;
	int maxX = r.maxX;

	vector<IntPoint> newKnots;
	for (int i = minX; i <= maxX; i++) {
		if (floodFillMat.find(y) == floodFillMat.end()) {
			floodFillMat[y] = {};
		}
		floodFillMat[y][i] = true;
		newKnots.push_back(IntPoint(i, y));
	}
	if (abs(px - minX) > abs(px - maxX)) {
		std::reverse(newKnots.begin(), newKnots.end());
	}
	for (int i = 0, len = newKnots.size(); i < len; i++) {
		floodFillKnots.push_back(newKnots[i]);
	}
	if ((r._abv.size() + r._blw.size()) > 2) {
		fillColor = rgba((UINT8)0, (UINT8)255, (UINT8)0, (UINT8)255);
	}
	if (r._abv.size()) {
		sort(r._abv.begin(), r._abv.end(), sortAB);
		for (int i = 0, len = r._abv.size(); i < len; i++) {
			floodFill(imd, r._abv[i].x, y - 1, targetColor, fillColor, "UP");
		}
	}
	if (r._blw.size()) {
		sort(r._blw.begin(), r._blw.end(), sortBA);
		for (int i = 0, len = r._blw.size(); i < len; i++) {
			floodFill(imd, r._blw[i].x, y + 1, targetColor, fillColor, "DOWN");
		}
	}
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

		int realFrameCount = mov.get(cv::CAP_PROP_FRAME_COUNT);
		int frameDiff = 1;// realFrameCount - mov.get(cv::CAP_PROP_POS_FRAMES);
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
			char* interp_pixels4 = new char[(frameWidth+20) * (frameHeight+20) * 4];
			char* interp_pixels5 = new char[frameWidth * frameHeight * 4];

			mov >> src;

			vector<IntPoint> motionVectors;
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

						x++;
					}
				}
			}

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

			map<int, vector<IntPoint>> clrmap;
			map<int, int> totalPixelCountPerColor;
			vector<IntPoint> startPoints;
			map<int, vector<IntPoint>> startKnotsByColor;

			{
				typedef cv::Point3_<uint8_t> Pixel;
				int px_idx = 0;
				rows = frameHeight;
				for (y = 0, rows = frameHeight; y < rows; y += 1) {
					for (x = 0, cols = frameWidth; x < cols - 1; x += 1) {
						int curpx = (y)* frameWidth * 4 + (x) * 4;
						int nextpx = (y)* frameWidth * 4 + (x + 1) * 4;
						if ((UINT8)interp_pixels2[curpx + 3] == 255 || (UINT8)interp_pixels2[nextpx + 3] == 255 || noticeablyDifferent(rgb((UINT8)interp_pixels[nextpx], (UINT8)interp_pixels[nextpx + 1], (UINT8)interp_pixels[nextpx + 2]), rgb((UINT8)interp_pixels[curpx], (UINT8)interp_pixels[curpx + 1], (UINT8)interp_pixels[curpx + 2]))) {
							continue;
						}
						interp_pixels2[curpx + 3] = 255;
						interp_pixels2[nextpx + 3] = 255;
						interp_pixels2[curpx + 2] = interp_pixels[curpx];
						interp_pixels2[curpx + 1] = interp_pixels[curpx + 1];
						interp_pixels2[curpx] = interp_pixels[curpx + 2];
						interp_pixels2[nextpx + 2] = interp_pixels[nextpx];
						interp_pixels2[nextpx + 1] = interp_pixels[nextpx + 1];
						interp_pixels2[nextpx] = interp_pixels[nextpx + 2];

						rgb fillColor = rgb((UINT8)interp_pixels[nextpx], (UINT8)interp_pixels[nextpx + 1], (UINT8)interp_pixels[nextpx + 2]);
						std::deque<IntPoint> q;
						std::vector<IntPoint> qq;
						q.push_back(IntPoint(x + 1, y));
						int sum_red = 0;
						int sum_green = 0;
						int sum_blue = 0;
						while (!q.empty()) {
							IntPoint n = q.front();
							curpx = (n.y) * frameWidth * 4 + (n.x) * 4;
							rgb compColor = rgb((UINT8)interp_pixels[curpx], (UINT8)interp_pixels[curpx + 1], (UINT8)interp_pixels[curpx + 2]);
							compColor = fillColor;
							q.pop_front();
							if (n.x > 0 && similarColor(interp_pixels, interp_pixels2, n.x - 1, n.y, compColor)) {
								interp_pixels2[(n.y) * frameWidth * 4 + (n.x - 1) * 4 + 3] = (UINT8)255;
								sum_blue += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x - 1) * 4 + 2];
								sum_green += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x - 1) * 4 + 1];
								sum_red += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x - 1) * 4];
								q.push_back(IntPoint(n.x - 1, n.y));
								qq.push_back(IntPoint(n.x - 1, n.y));
							}
							if (n.x < cols - 1 && similarColor(interp_pixels, interp_pixels2, n.x + 1, n.y, compColor)) {
								interp_pixels2[(n.y) * frameWidth * 4 + (n.x + 1) * 4 + 3] = (UINT8)255;
								sum_blue += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x + 1) * 4 + 2];
								sum_green += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x + 1) * 4 + 1];
								sum_red += (UINT8)interp_pixels[(n.y) * frameWidth * 4 + (n.x + 1) * 4];
								q.push_back(IntPoint(n.x + 1, n.y));
								qq.push_back(IntPoint(n.x + 1, n.y));
							}
							if (n.y > 0 && similarColor(interp_pixels, interp_pixels2, n.x, n.y - 1, compColor)) {
								interp_pixels2[(n.y - 1) * frameWidth * 4 + (n.x) * 4 + 3] = (UINT8)255;
								sum_blue += (UINT8)interp_pixels[(n.y - 1) * frameWidth * 4 + (n.x) * 4 + 2];
								sum_green += (UINT8)interp_pixels[(n.y - 1) * frameWidth * 4 + (n.x) * 4 + 1];
								sum_red += (UINT8)interp_pixels[(n.y - 1) * frameWidth * 4 + (n.x) * 4];
								q.push_back(IntPoint(n.x, n.y - 1));
								qq.push_back(IntPoint(n.x, n.y - 1));
							}
							if (n.y < rows - 1 && similarColor(interp_pixels, interp_pixels2, n.x, n.y + 1, compColor)) {
								interp_pixels2[(n.y + 1) * frameWidth * 4 + (n.x) * 4 + 3] = (UINT8)255;
								sum_blue += (UINT8)interp_pixels[(n.y + 1) * frameWidth * 4 + (n.x) * 4 + 2];
								sum_green += (UINT8)interp_pixels[(n.y + 1) * frameWidth * 4 + (n.x) * 4 + 1];
								sum_red += (UINT8)interp_pixels[(n.y + 1) * frameWidth * 4 + (n.x) * 4];
								q.push_back(IntPoint(n.x, n.y + 1));
								qq.push_back(IntPoint(n.x, n.y + 1));
							}
						}
						int qq_len = qq.size();
						if (qq_len) {
							int avg_red = sum_red / qq_len;
							int avg_green = sum_green / qq_len;
							int avg_blue = sum_blue / qq_len;
							for (auto qqq : qq) {
								int pxidx = (qqq.y) * frameWidth * 4 + (qqq.x) * 4;
								interp_pixels2[pxidx] = (UINT8)avg_blue;
								interp_pixels2[pxidx + 1] = (UINT8)avg_green;
								interp_pixels2[pxidx + 2] = (UINT8)avg_red;
								interp_pixels2[pxidx + 3] = (UINT8)255;
							}
						}
						/*curpx = (y)* frameWidth * 4 + (x + 1) * 4;
						int color_value = (UINT8)interp_pixels[curpx] << 16 | (UINT8)interp_pixels[curpx+1] << 8 | (UINT8)interp_pixels[curpx+2];
						if (startKnotsByColor.find(color_value) == startKnotsByColor.end()) {
							startKnotsByColor[color_value] = {};
						}
						startKnotsByColor[color_value].push_back(IntPoint(x + 1, y));*/
					}
				}

				for (y = 0, rows = frameHeight; y < rows; y += 1) {
					for (x = 0, cols = frameWidth; x < cols; x += 1) {
						int pxidx = (y)* frameWidth * 4 + (x) * 4;
						if ((UINT8)interp_pixels2[pxidx + 3] != 255) {
							interp_pixels2[pxidx + 2] = (UINT8)interp_pixels[pxidx];
							interp_pixels2[pxidx + 1] = (UINT8)interp_pixels[pxidx + 1];
							interp_pixels2[pxidx] = (UINT8)interp_pixels[pxidx + 2];
							interp_pixels2[pxidx + 3] = 255;
						}
						int color_value = (UINT8)interp_pixels2[pxidx + 2] << 16 | (UINT8)interp_pixels2[pxidx + 1] << 8 | (UINT8)interp_pixels2[pxidx];
						if (x > 0) {
							int pxidx_prev = (y)* frameWidth * 4 + (x - 1) * 4;
							if (interp_pixels2[pxidx + 2] == interp_pixels2[pxidx_prev + 2] && interp_pixels2[pxidx + 1] == interp_pixels2[pxidx_prev + 1] && interp_pixels2[pxidx] == interp_pixels2[pxidx_prev]) {
								interp_pixels2[pxidx_prev + 3] = 255;
							}
							else {
								clrmap[color_value].push_back(IntPoint(x, y));
							}
						}
						else {
							clrmap[color_value].push_back(IntPoint(x, y));
						}
						if (totalPixelCountPerColor.find(color_value) == totalPixelCountPerColor.end()) {
							totalPixelCountPerColor[color_value] = 1;
						}
						else {
							totalPixelCountPerColor[color_value]++;
						}
					}
				}
			}

			if (true || i == 1) {
				int contourCount = 0;
				bytecount += 2.0;
				int colorCount = 0;

				for (auto it = clrmap.begin(); it != clrmap.end(); it++)
				{
					if (totalPixelCountPerColor[it->first] >= 1) {
						colorCount++;
					}
				}

				string bits = bitify(colorCount >> 8) + bitify(colorCount & 0xFF);
				for (auto bit : bits) {
					addBit(bit == '1');
				}
				for (auto it = clrmap.begin(); it != clrmap.end(); it++)
				{
					int clr = it->first;
					int red = clr >> 16 & 0xFF;
					int green = clr >> 8 & 0xFF;
					int blue = clr & 0xFF;
					if (totalPixelCountPerColor[clr] >= 1) {
						bytecount += 3;

						for (int i = 0, len = (frameWidth + 20) * (frameHeight + 20) * 4; i < len; i++) {
							interp_pixels4[i] = (UINT8)255;
						}
						for (auto pt : it->second)
						{
							interp_pixels4[(pt.y+10) * 980 * 4 + (pt.x+10) * 4] = (UINT8)0;
							interp_pixels4[(pt.y+10) * 980 * 4 + (pt.x+10) * 4 + 1] = (UINT8)0;
							interp_pixels4[(pt.y+10) * 980 * 4 + (pt.x+10) * 4 + 2] = (UINT8)0;
							interp_pixels4[(pt.y+10) * 980 * 4 + (pt.x+10) * 4 + 3] = (UINT8)255;
						}

						floodFillMat.clear();
						floodFillKnots.clear();

						floodFill(interp_pixels4, it->second[0].x + 10, it->second[0].y + 10, rgba((UINT8)255, (UINT8)255, (UINT8)255, (UINT8)255), rgba((UINT8)255, (UINT8)0, (UINT8)0, (UINT8)255), "");

						/*for (int i = 0, len = floodFillKnots.size(); i < len; i++) {
							IntPoint k = floodFillKnots[i];
							floodFillKnots[i].x -= 10;
							floodFillKnots[i].y -= 10;
						}

						//findContours(canny_output, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE, Point(0, 0));
						bytecount += 2.0;
						int contourCount = floodFillKnots.size();

						string s1 = "contourCount: " + to_string(contourCount) + "\n";
						std::wstring widestr = std::wstring(s1.begin(), s1.end());
						OutputDebugStringW(widestr.c_str());

						string bits = bitify((UINT8)red) + bitify((UINT8)green) + bitify((UINT8)blue);
						for (auto bit : bits) {
							addBit(bit == '1');
						}
						bits = bitify(contourCount >> 8) + bitify(contourCount & 0xFF);
						for (auto bit : bits) {
							addBit(bit == '1');
						}
						IntPoint firstPoint(it->second[0].x + 10, it->second[0].y + 10);
						bytecount += 6.0;
						bits = bitify(firstPoint.x >> 8) + bitify(firstPoint.x & 0xFF) + bitify(firstPoint.y >> 8) + bitify(firstPoint.y & 0xFF);
						for (auto bit : bits) {
							addBit(bit == '1');
						}
						//for (int i = 0, len = floodFillKnots.size(); i < len; i++) {
							int innerContourSize = floodFillKnots.size();
							bits = bitify(innerContourSize >> 8) + bitify(innerContourSize & 0xFF);
							for (auto bit : bits) {
								addBit(bit == '1');
							}
						IntPoint lastDiff(0, 0);
						int idx = it->second[0].y * frameWidth * 4 + it->second[0].x * 4;
						interp_pixels5[idx + 2] = (UINT8)red;
						interp_pixels5[idx + 1] = (UINT8)green;
						interp_pixels5[idx] = (UINT8)blue;
						interp_pixels5[idx + 3] = (UINT8)255;
						for (int j = 0, jLen = floodFillKnots.size(); j < jLen; j++) {
							int idx = floodFillKnots[j].y * frameWidth * 4 + floodFillKnots[j].x * 4;
							interp_pixels5[idx + 2] = (UINT8)red;
							interp_pixels5[idx + 1] = (UINT8)green;
							interp_pixels5[idx] = (UINT8)blue;
							interp_pixels5[idx + 3] = (UINT8)255;
							IntPoint newDiff(floodFillKnots[j].x - floodFillKnots[j - 1].x, floodFillKnots[j].y - floodFillKnots[j - 1].y);
							if (newDiff.x == lastDiff.x && newDiff.y == lastDiff.y) {
								addBit(0);
								bytecount += 0.125;
							}
							else if (floodFillKnots[j].x - floodFillKnots[j - 1].x >= -1 && floodFillKnots[j].x - floodFillKnots[j - 1].x <= 1 && floodFillKnots[j].y - floodFillKnots[j - 1].y >= -1 && floodFillKnots[j].y - floodFillKnots[j - 1].y <= 1) {
								addBit(1);
								addShortDelta(newDiff);
								bytecount += 0.125 + 0.5;
							}
							lastDiff.x = floodFillKnots[j].x - floodFillKnots[j - 1].x;
							lastDiff.y = floodFillKnots[j].y - floodFillKnots[j - 1].y;
						}*/
						//}
						for (auto pt : it->second)
						{
							int idx = pt.y * frameWidth * 4 + pt.x * 4;
							interp_pixels5[idx + 2] = (UINT8)red;
							interp_pixels5[idx + 1] = (UINT8)green;
							interp_pixels5[idx] = (UINT8)blue;
							interp_pixels5[idx + 3] = (UINT8)255;
							//startPoints.push_back(IntPoint(pt.x, pt.y));
							bytecount += 0.5;
						}
					}
				}

				ID2D1Bitmap* pBitmap = NULL;
				hr = pRenderTarget->CreateBitmap(D2D1::SizeU(frameWidth, frameHeight), interp_pixels5, frameWidth * 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &pBitmap);

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
				prevFrame = interp_pixels2;
			}

			delete[] interp_pixels5;
			delete[] interp_pixels4;
			delete[] interp_pixels;
			delete[] interp_pixels2;
			break;
		}

		string s1 = "bytecount: " + to_string(bytecount) + "\n";
		std::wstring widestr = std::wstring(s1.begin(), s1.end());
		OutputDebugStringW(widestr.c_str());

		s1 = "frameEdgeBytes.size(): " + to_string(frameEdgeBytes.size()) + "\n";
		widestr = std::wstring(s1.begin(), s1.end());
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