#include <cmath>
#include "png++/png.hpp"
#include <memory>
#include <algorithm>

#include "window_functions.cpp"

using namespace std;
using namespace png;

class ImageUtils
{
public:
	static void vline(image<rgb_pixel>& img, int x, int y, int size){
		rgb_pixel white(255, 255, 255);
		for (int i = y; i < y+size; ++i)
		{
			img[i][x] = white;
		}
	}

	static void hline(image<rgb_pixel>& img, int x, int y, int size, bool red = false){
		rgb_pixel p(255, 255, 255);
		if(red)
			p = rgb_pixel(255, 20, 0);

		for (int i = x; i < x+size; ++i)
		{
			img[y][i] = p;
		}
	}

	static void rectangle(image<rgb_pixel>& img, int x, int y, int width, int height){
		rgb_pixel white(255, 255, 255);
		for (int x_ = x; x_ < x+width; ++x_)
		{
			img[y][x_] = white;
			img[y+height][x_] = white;
		}
		for (int y_ = y; y_ < y+height; ++y_)
		{
			img[y_][x] = white;
			img[y_][x+width] = white;
		}
	}
};

class ImageBlock
{
protected:
	int width;
	int height;
public:
	int x = 0;
	int y = 0;
	virtual void render(image<rgb_pixel>& img, int tx, int ty) = 0;
	virtual int getWidth(){
		return width;
	};
	virtual int getHeight(){
		return height;
	};
};

class AveragesRenderer : public ImageBlock {
	vector<double> spectrumSums;
	int width = 100;
public:
	void addFrame(vector<double>& column){
		if(spectrumSums.size() == 0){
			spectrumSums = column;
		}
		else {
			for (size_t i = 0; i < spectrumSums.size(); ++i)
			{
				spectrumSums[i] += column[i];
			}
		}
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		if(spectrumSums.size() == 0)
			return;
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());

		double maxValue = *max_element(spectrumSums.begin(), spectrumSums.end());

		// empty spectrum
		if(maxValue == 0)
			return;

		int height = getHeight();
		for (size_t i = 0; i < spectrumSums.size(); ++i)
		{
			int value = spectrumSums[i]/maxValue*width;
			if(spectrumSums[i] == maxValue)
				ImageUtils::hline(img, tx, ty+height-i, value, true);
			else
				ImageUtils::hline(img, tx, ty+height-i, value, false);
		}
	}

	virtual int getWidth(){
		return width;
	};
	virtual int getHeight(){
		return spectrumSums.size();
	};
};

class FFTRenderer : public ImageBlock {
	vector<vector<double>> spectrum;

	// http://stackoverflow.com/questions/15868234/map-a-value-0-0-1-0-to-color-gain
	// paleta z: http://4.bp.blogspot.com/-d96rd-cACn0/TdUINqcBxuI/AAAAAAAAA9I/nGDXL7ksxAc/s1600/01-Deep_Rumba-A_Calm_in_the_Fire_of_Dances_2496-Cubana.flac.png
	double linear(double x, double start, double end) {
		if (x < start)
			return 0;
		else if (x > end)
			return 1;
		else
			return (x-start) / (end-start);
	}

	double getR(double value){
		return linear(value, 25.0/200.0, 140.0/200.0);
	}
	double getG(double value){
		return linear(value, 120.0/200.0, 180.0/200.0);
	}
	double getB(double value){
		return linear(value, 0.75, 1.0) + (linear(value, 0, 57.0/200.0) - linear(value, 63.0/200.0, 120.0/200.0))*0.5;
		return 1.0-linear(value, 0, 0.5);
	}

	vector<rgb_pixel> palette;
public:
	FFTRenderer(){
		int paletteSize = 512;
		double ps = paletteSize;
		for (int i = 0; i < paletteSize; ++i)
		{
			rgb_pixel p(getR(i/ps)*255, getG(i/ps)*255, getB(i/ps)*255);
			palette.push_back(p);
		}
	}
	void addFrame(vector<double> column){
		spectrum.push_back(column);
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		int width = getWidth();
		int height = getHeight();

		// find maxValue
		double maxValue = 0;
		for (int x_ = 0; x_ < width; ++x_)
		{
			for (int y_ = 0; y_ < height; ++y_)
			{

				maxValue = max(maxValue, spectrum[x_][y_]);
			}
		}

		// empty spectrum
		if(maxValue == 0)
			return;

		for (int x_ = 0; x_ < width; ++x_)
		{
			for (int y_ = 0; y_ < height; ++y_)
			{
				double value = max(0.0, spectrum[x_][y_]/maxValue);
				value = 1-min(-log(value), 12.0)/12.0;
				value *= palette.size();
				img[ty+height-y_][tx+x_] = palette[(size_t)value];
			}
		}

		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());

		renderScale(img, tx + getWidth() + 10, ty + getHeight() + 10, 100, 20);
		ImageUtils::rectangle(img, tx + getWidth() + 10, ty + getHeight() + 10, 100, 20);
	}

	void renderScale(image<rgb_pixel>& img, int tx, int ty, int widthscale, int heightscale){
		for (int x_ = 0; x_ < widthscale; ++x_)
		{
			for (int y_ = 0; y_ < heightscale; ++y_)
			{
				double value = x_/(double)widthscale;
				value *= palette.size();
				img[ty+y_][tx+x_] = palette[(size_t)value];
			}
		}
	}

	virtual int getWidth(){
		return spectrum.size();
	};
	virtual int getHeight(){
		if(spectrum.size() == 0)
			return 0;
		return spectrum[0].size();
	};
};

class WaveRenderer : public ImageBlock {
	vector<double> wave;
	int height = 100;
public:
	void addFrame(vector<double>& column, int slide){
		slide = min(slide, (int)column.size());
		double maxValue = *max_element(column.begin(), column.begin()+slide);
		wave.push_back(maxValue);
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		if(wave.size() == 0)
			return;
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());
		double halfheight = 0.5*height;
		for (size_t i = 0; i < wave.size(); ++i)
		{
			int size = halfheight*wave[i];
			ImageUtils::vline(img, tx+x+i, ty+halfheight-size, size*2);
		}
	}

	virtual int getWidth(){
		return wave.size();
	};
	virtual int getHeight(){
		return height;
	};
};

class ScaleRenderer : public ImageBlock {
public:
	double rangex, rangey, dx, dy;
	ScaleRenderer(int x, int y, int width, int height, double rangex, double rangey, double dx, double dy) : 
		rangex(rangex), rangey(rangey), dx(dx), dy(dy) {
			this->x = x;
			this->y = y;
			this->width = width;
			this->height = height;
		}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());

		if(rangex > 0){
			for (double x_ = 0; x_ <= rangex; x_ += dx)
			{
				ImageUtils::vline(img, tx+width*x_/rangex, ty-2 + height, 5);
				ImageUtils::vline(img, tx+width*x_/rangex, ty-2, 5);
			}
		}

		if(rangey > 0){
			for (double y_ = 0; y_ <= rangey; y_ += dy)
			{
				ImageUtils::hline(img, tx-2 + width, ty + height-height*y_/rangey, 5);
				ImageUtils::hline(img, tx-2, ty + height-height*y_/rangey, 5);
			}
		}
	}
};

class WindowRenderer : public ImageBlock {
	unique_ptr<WindowFunction> windowf;
	int windowSize;
public:
	double rangex, rangey, dx, dy;
	WindowRenderer(int x, int y, int width, int height, unique_ptr<WindowFunction> windowf, int windowSize) : 
		windowf(move(windowf)), windowSize(windowSize) {
			this->x = x;
			this->y = y;
			this->width = width;
			this->height = height;
		}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());

		for (double x_ = 0; x_ <= width; ++x_)
		{
			double h = windowf->apply(height, x_*windowSize/width);
			ImageUtils::vline(img, tx+x_, ty+height-h+1, h);
		}
	}
};

class ImageOutput
{
	vector<unique_ptr<ImageBlock>> blocks;
	int margin = 10;
public:
	void addBlock(unique_ptr<ImageBlock> block){
		blocks.push_back(move(block));
	}

	void render(image<rgb_pixel>& img, int tx, int ty){
		for(size_t i = 0; i < blocks.size(); ++i){
			blocks[i]->render(img, tx, ty);
		}
	}

	void renderImage(string outputFilename){
		int maxx = 1;
		int maxy = 1;
		for(size_t i = 0; i < blocks.size(); ++i){
			maxx = max(blocks[i]->x+blocks[i]->getWidth(), maxx);
			maxy = max(blocks[i]->y+blocks[i]->getHeight(), maxy);
		}

		maxx += margin*2;
		maxy += margin*2;

		image<rgb_pixel> img(maxx, maxy);

		render(img, margin, margin);

		img.write(outputFilename);
	}
};