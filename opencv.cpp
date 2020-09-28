#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
using namespace cv;
Mat src, src_gray;
Mat dst, detected_edges;
int lowThreshold = 0;
const int max_lowThreshold = 100;
const int ratio = 3;
const int kernel_size = 3;
const char* window_name = "Edge Map";
static void CannyThreshold(int, void*)
{
	blur(src_gray, detected_edges, Size(3, 3));
	Canny(detected_edges, detected_edges, lowThreshold, lowThreshold * ratio, kernel_size);
	dst = Scalar::all(0);
	src.copyTo(dst, detected_edges);
	typedef cv::Point3_<uint8_t> Pixel;
	dst.forEach<Pixel>([](Pixel& p, const int* position) -> void {
		if (!(p.x == 0 && p.y == 0 && p.z == 0))
		{
			p.x = 255;
			p.y = 255;
			p.z = 255;
		}
	});
	imshow(window_name, dst);
}
int main(int argc, char** argv)
{
	CommandLineParser parser(argc, argv, "{@input | snikta.jpg | input image}");
	src = imread(parser.get<String>("@input"), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image!\n" << std::endl;
		std::cout << "Usage: " << argv[0] << " <Input image>" << std::endl;
		return -1;
	}
	dst.create(src.size(), src.type());
	cvtColor(src, src_gray, COLOR_BGR2GRAY);
	namedWindow(window_name, WINDOW_AUTOSIZE);
	createTrackbar("Min Threshold:", window_name, &lowThreshold, max_lowThreshold, CannyThreshold);
	CannyThreshold(0, 0);
	waitKey(0);
	return 0;
}