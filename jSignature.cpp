#pragma once
#define _USE_MATH_DEFINES // for C++
#include <cmath>
#include <iostream>
#include <vector>
#include <math.h>
#include <algorithm>
#include <string>
#include "jSignature.h"
#include "Point.h"
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::min;
using std::max;

JSIGVector::JSIGVector(double x, double y) : x(x), y(y) { };
JSIGVector::JSIGVector() { };
JSIGVector JSIGVector::reverse() {
	JSIGVector newVec;
	newVec.x = x * -1;
	newVec.y = y * -1;
	return newVec;
}
double JSIGVector::getLength() {
	_length = sqrt( pow(x, 2) + pow(y, 2) );
	return _length;
}
int JSIGVector::polarity(double e) {
	return round(e / abs(e));
}
JSIGVector &JSIGVector::resizeTo(double length) {
	if (x == 0 && y == 0) {
		_length = 0;
	} else if (x == 0) {
		_length = length;
		y = length * polarity(y);
	} else if (y == 0) {
		_length = length;
		x = length * polarity(x);
	} else {
		double proportion = abs(y / x);
		double newX = sqrt(pow(length, 2) / (1 + pow(proportion, 2)));
		double newY = proportion * newX;
		_length = length;
		x = newX * polarity(x);
		y = newY * polarity(y);
	}
	return *this;
}
	
double JSIGVector::angleTo(JSIGVector &vectorB) {
	double divisor = getLength() * vectorB.getLength();
	if (divisor == 0) {
		return 0;
	} else {
		return acos(
			min(
				max(
					( x * vectorB.x + y * vectorB.y ) / divisor
					, -1.0
				)
				, 1.0
			)
		) / M_PI;
	}
}

JSIGPoint::JSIGPoint(double x, double y) : x(x), y(y) { };
JSIGPoint::JSIGPoint() { };
JSIGVector JSIGPoint::getVectorToCoordinates(double toX, double toY) {
	JSIGVector newVec;
	newVec.x = toX - x;
	newVec.y = toY - y;
	return newVec;
}
JSIGVector JSIGPoint::getVectorFromCoordinates(double fromX, double fromY) {
	JSIGVector newVec;
	newVec = getVectorToCoordinates(fromX, fromY).reverse();
	return newVec;
}	
JSIGVector JSIGPoint::getVectorToPoint(JSIGPoint &point) {
	JSIGVector newVec;
	newVec.x = point.x - x;
	newVec.y = point.y - y;
	return newVec;
}	
JSIGVector JSIGPoint::getVectorFromPoint(JSIGPoint &point) {
	JSIGVector newVec = getVectorToPoint(point);
	return newVec.reverse();
}

Primitive::Primitive(string type, vector<Point>& points) : type(type), points(points) { };

double round(double number, int position) {
	double tmp = pow(10, position);
	return round( number * tmp ) / tmp;
}

Primitive segmentToCurve(Stroke &stroke, int positionInStroke, double lineCurveThreshold) {
	positionInStroke += 1;
	
	JSIGPoint Cpoint(stroke.x[positionInStroke-1], stroke.y[positionInStroke-1]);
	JSIGPoint Dpoint(stroke.x[positionInStroke], stroke.y[positionInStroke]);
	JSIGVector CDvector = Cpoint.getVectorToPoint(Dpoint);
	
	JSIGPoint Bpoint(stroke.x[positionInStroke-2], stroke.y[positionInStroke-2]);
	JSIGVector BCvector = Bpoint.getVectorToPoint(Cpoint);
	JSIGVector ABvector;
	int rounding = 2;
	
	if ( BCvector.getLength() > lineCurveThreshold ) {
		if (positionInStroke > 2) {
			ABvector = JSIGPoint(stroke.x[positionInStroke-3], stroke.y[positionInStroke-3]).getVectorToPoint(Bpoint);
		} else {
			ABvector = JSIGVector(0,0);
		}
		double minlenfraction = 0.05;
		double maxlen = BCvector.getLength() * 0.35;
		JSIGVector BAvector = ABvector.reverse();
		JSIGVector CBvector = BCvector.reverse();
		double ABCangle = BCvector.angleTo(BAvector);
		double BCDangle = CDvector.angleTo(CBvector);
		JSIGVector BtoCP1vector = JSIGVector(ABvector.x + BCvector.x, ABvector.y + BCvector.y).resizeTo(
			max(minlenfraction, ABCangle) * maxlen
		);
		JSIGVector CtoCP2vector = JSIGVector(BCvector.x + CDvector.x, BCvector.y + CDvector.y).reverse().resizeTo(
			max(minlenfraction, BCDangle) * maxlen
		);
		JSIGVector BtoCP2vector = JSIGVector(BCvector.x + CtoCP2vector.x, BCvector.y + CtoCP2vector.y);
		vector<Point> BezierPoints = {
			Point(Bpoint.x, Bpoint.y),
			Point(round( Bpoint.x + BtoCP1vector.x, rounding ), round( Bpoint.y + BtoCP1vector.y, rounding )),
			Point(round( Bpoint.x + BtoCP2vector.x, rounding ), round( Bpoint.y + BtoCP2vector.y, rounding )),
			Point(round( Bpoint.x + BCvector.x, rounding ), round( Bpoint.y + BCvector.y, rounding )),
		};
		return Primitive("BezierCurve", BezierPoints);
	} else {
		vector<Point> LinePoints = {
			Point(round( Bpoint.x + BCvector.x, rounding ), round( Bpoint.y + BCvector.y, rounding ))
		};
		return Primitive("Lineto", LinePoints);
	}
}

Primitive lastSegmentToCurve(Stroke &stroke, int positionInStroke, double lineCurveThreshold) {
	positionInStroke = stroke.x.size() - 1;
	
	JSIGPoint Cpoint(stroke.x[positionInStroke], stroke.y[positionInStroke]);
	JSIGPoint Bpoint(stroke.x[positionInStroke-1], stroke.y[positionInStroke-1]);
	JSIGVector BCvector = Bpoint.getVectorToPoint(Cpoint);
	
	int rounding = 2;
	
	if (positionInStroke > 1 && BCvector.getLength() > lineCurveThreshold) {
		JSIGVector ABvector = JSIGPoint(stroke.x[positionInStroke-2], stroke.y[positionInStroke-2]).getVectorToPoint(Bpoint);
		JSIGVector BAvector = ABvector.reverse();
		double ABCangle = BCvector.angleTo(BAvector);
		double minlenfraction = 0.05;
		double maxlen = BCvector.getLength() * 0.35;
		JSIGVector BtoCP1vector = JSIGVector(ABvector.x + BCvector.x, ABvector.y + BCvector.y).resizeTo(
			max(minlenfraction, ABCangle) * maxlen
		);
		
		vector<Point> BezierPoints = {
			Point(Bpoint.x, Bpoint.y),
			Point(round( Bpoint.x + BtoCP1vector.x, rounding ), round( Bpoint.y + BtoCP1vector.y, rounding )),
			Point(round( Bpoint.x + BCvector.x, rounding ), round( Bpoint.y + BCvector.y, rounding )),
			Point(round( Bpoint.x + BCvector.x, rounding ), round( Bpoint.y + BCvector.y, rounding ))
		};
		return Primitive("BezierCurve", BezierPoints);
	} else {
		vector<Point> LinePoints = {
			Point(round( Bpoint.x + BCvector.x, rounding ), round( Bpoint.y + BCvector.y, rounding ))
		};
		return Primitive("Lineto", LinePoints);
	}
}

vector<Primitive> addstroke(Stroke &stroke, double shiftx, double shifty) {
	vector<Point> MoveToPoints = {
		Point(round( (stroke.x[0] - shiftx), 2), round( (stroke.y[0] - shifty), 2))
	};
	vector<Primitive> lines = {
		Primitive("Moveto", MoveToPoints)
	};
	int i = 1;
	int l = stroke.x.size() - 1;
	double lineCurveThreshold = 0.001;
	
	for (; i < l; i++) {
		lines.push_back(segmentToCurve(stroke, i, lineCurveThreshold));
	}
	if (l > 0) {
		lines.push_back(lastSegmentToCurve(stroke, i, lineCurveThreshold));
	} else if (l == 0) {
		vector<Point> LineToPoints = {
			Point(stroke.x[0], stroke.y[0])
		};
		lines.push_back(Primitive("Lineto", LineToPoints));
	}
	
	/*for (i = 0, l = lines.size(); i < l; i++) {
		cout << i << ": " << lines[i].type << endl;
		for (int j = 0, jLen = lines[i].points.size(); j < jLen; j++) {
			cout << "(" << lines[i].points[j].x << ", " << lines[i].points[j].y << ")" << endl;
		}
	}*/
	
	return lines;
}

/*int main() {
	Stroke myStroke;
	myStroke.x = {305,302,290,283,266,261,261,263};
	myStroke.y = {294,287,255,236,191,180,181,183};
	//myStroke.x = {338,330,321,297,261,251,239,233,231,242,256,289,326,356,370,375,374,355,317,280,220};
	//myStroke.y = {125,98,88,75,71,73,88,104,137,169,188,214,231,252,275,294,337,358,373,380,377};
	
	addstroke(myStroke, 0, 0);

	return 0;
}*/