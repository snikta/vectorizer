#pragma once
#include <iostream>
#include <vector>
#include <math.h>
#include "IntPoint.h"
using std::vector;
using std::cout;

double getSqDist(IntPoint& p1, IntPoint& p2);
double getSqSegDist(IntPoint& p, IntPoint& p1, IntPoint& p2);
vector<IntPoint> simplifyRadialDist(vector<IntPoint>& points, int sqTolerance);
void simplifyDPStep(vector<IntPoint>& points, int first, int last, int sqTolerance, vector<IntPoint>& simplified);
vector<IntPoint> simplifyDouglasPeucker(vector<IntPoint>& points, int sqTolerance);
vector<IntPoint> simplify(vector<IntPoint>& points, int tolerance, bool highestQuality);