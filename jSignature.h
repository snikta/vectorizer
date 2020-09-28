#pragma once
#include <string>
#include <vector>
#include "Point.h"
using std::vector;
using std::string;

class JSIGVector
{
private:
	double _length = 0.0;
public:
	double x = 0.0;
	double y = 0.0;

	JSIGVector(double x, double y);
	JSIGVector();

	JSIGVector reverse();
	double getLength();
	int polarity(double e);
	JSIGVector& resizeTo(double length);
	double angleTo(JSIGVector& vectorB);
};

class JSIGPoint
{
public:
	double x = 0.0;
	double y = 0.0;

	JSIGPoint(double x, double y);
	JSIGPoint();

	JSIGVector getVectorToCoordinates(double toX, double toY);
	JSIGVector getVectorFromCoordinates(double fromX, double fromY);
	JSIGVector getVectorToPoint(JSIGPoint& point);
	JSIGVector getVectorFromPoint(JSIGPoint& point);
};

class Primitive
{
public:
	string type;
	vector<Point> points;

	Primitive(string type, vector<Point>& points);
};

struct Stroke
{
public:
	vector<double> x;
	vector<double> y;
};

double round(double number, int position);

Primitive segmentToCurve(Stroke& stroke, int positionInStroke, double lineCurveThreshold);
Primitive lastSegmentToCurve(Stroke& stroke, int positionInStroke, double lineCurveThreshold);

vector<Primitive> addstroke(Stroke& stroke, double shiftx, double shifty);