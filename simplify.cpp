#pragma once
#include <iostream>
#include <vector>
#include <math.h>
#include "IntPoint.h"
using std::vector;
using std::cout;

double getSqDist(IntPoint &p1, IntPoint &p2) {
	double dx = p1.x - p2.x;
	double dy = p1.y - p2.y;
	
	return dx * dx + dy * dy;
}

double getSqSegDist(IntPoint &p, IntPoint &p1, IntPoint &p2) {
	double x = p1.x;
	double y = p1.y;
	double dx = p2.x - x;
	double dy = p2.y - y;
	
	if (dx != 0 || dy != 0) {
		double t = ((p.x - x) * dx + (p.y - y) * dy) / (dx * dx + dy * dy);
		if (t > 1) {
			x = p2.x;
			y = p2.y;
		} else if (t > 0) {
			x += dx * t;
			y += dy * t;
		}
	}
	dx = p.x - x;
	dy = p.y - y;
	
	return dx * dx + dy * dy;
}

vector<IntPoint> simplifyRadialDist(vector<IntPoint> &points, int sqTolerance) {
	IntPoint prevIntPoint = points[0];
	vector<IntPoint> newIntPoints = { prevIntPoint };
	IntPoint point = points[0];
	
	for (int i = 1, len = points.size(); i < len; i++) {
		point = points[i];
		
		if (getSqDist(point, prevIntPoint) > sqTolerance) {
			newIntPoints.push_back(point);
			prevIntPoint = point;
		}
	}
	
	if (!(prevIntPoint.x == point.x && prevIntPoint.y == point.y)) {
		newIntPoints.push_back(point);
	}
	
	return newIntPoints;
}

void simplifyDPStep(vector<IntPoint> &points, int first, int last, int sqTolerance, vector<IntPoint> &simplified) {
	int maxSqDist = sqTolerance;
	int index = -1;
	
	for (int i = first + 1; i < last; i++) {
		int sqDist = getSqSegDist(points[i], points[first], points[last]);
		if (sqDist > maxSqDist) {
			index = i;
			maxSqDist = sqDist;
		}
	}
	
	if (maxSqDist > sqTolerance) {
		if (index - first > 1) {
			simplifyDPStep(points, first, index, sqTolerance, simplified);
		}
		simplified.push_back(points[index]);
		if (last - index > 1) {
			simplifyDPStep(points, index, last, sqTolerance, simplified);
		}
	}
}

vector<IntPoint> simplifyDouglasPeucker(vector<IntPoint> &points, int sqTolerance) {
	int last = points.size() - 1;
	
	vector<IntPoint> simplified = { points[0] };
	simplifyDPStep(points, 0, last, sqTolerance, simplified);
	simplified.push_back(points[last]);
	
	return simplified;
}

vector<IntPoint> simplify(vector<IntPoint> &points, int tolerance, bool highestQuality) {
	vector<IntPoint> newIntPoints;
	
	if (points.size() <= 2) {
		return points;
	}
	
	int sqTolerance = tolerance * tolerance;
	
	newIntPoints = highestQuality ? points : simplifyRadialDist(points, sqTolerance);
	newIntPoints = simplifyDouglasPeucker(points, sqTolerance);
	
	return newIntPoints;
}