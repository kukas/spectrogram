#include <cmath>
#include "png++/png.hpp"
#include <memory>
#include <algorithm>

using namespace std;
using namespace png;

class ImageUtils
{
public:
	static void hline(image<rgb_pixel>& img, int x, int y, int size){
		rgb_pixel white(255, 255, 255);
		for (int i = y; i < y+size; ++i)
		{
			img[i][x] = white;
		}
	}

	static void vline(image<rgb_pixel>& img, int x, int y, int size){
		rgb_pixel white(255, 255, 255);
		for (int i = x; i < x+size; ++i)
		{
			img[y][i] = white;
		}
	}

	static void rectangle(image<rgb_pixel>& img, int x, int y, int width, int height){
		rgb_pixel white(255, 255, 255);
		for (int x_ = x; x_ < x+width; ++x_)
		{
			img[y][x_] = white;
			img[y+height-1][x_] = white;
		}
		for (int y_ = y; y_ < y+height; ++y_)
		{
			img[y_][x] = white;
			img[y_][x+width-1] = white;
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

		// double maxValue = *max_element(spectrumSums.begin(), spectrumSums.end());

		// // empty spectrum
		// if(maxValue == 0)
		// 	return;

		// for (size_t i = 0; i < spectrumSums.size(); ++i)
		// {
		// 	int value = spectrumSums[i]/maxValue*width;
		// 	ImageUtils::vline(img, tx, ty+i, value);
		// }
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
	double linear(double x, double start, double width = 0.2) {
		if (x < start)
			return 0;
		else if (x > start+width)
			return 1;
		else
			return (x-start) / width;
	}

	double getR(double value){
		return linear(value, 0.2);
	}
	double getG(double value){
		return linear(value, 0.6);
	}
	double getB(double value){
		return linear(value, 0) - linear(value, 0.4) + linear(value, 0.8);
	}

	vector<rgb_pixel> palette;
public:
	FFTRenderer(){
		for (int i = 0; i < 256; ++i)
		{
			rgb_pixel p(i, 0, 0);
			palette.push_back(p);
		}
		cout << palette.size();
	}
	void addFrame(vector<double> column){
		spectrum.push_back(column);
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		// find maxValue;
		double maxValue = 0;
		for (size_t x_ = 0; x_ < spectrum.size(); ++x_)
		{
			for (size_t y_ = 0; y_ < spectrum[y_].size(); ++y_)
			{
				maxValue = max(maxValue, spectrum[x_][y_]);
			}
		}

		// empty spectrum
		if(maxValue == 0)
			return;

		// int bufferSize = getHeight();

		for (size_t x_ = 0; x_ < spectrum.size(); ++x_)
		{
			for (size_t y_ = 0; y_ < spectrum[x_].size(); ++y_)
			{
				// int yy = round(bufferSize*log(y)/log(bufferSize));
				double value = spectrum[x_][y_]/maxValue;
				// value = 1-min(-log(value), 12.0)/12.0;
				value *= 255;
				// img[ty+y_][tx+x_] = palette[(size_t)value];
				img[ty+y_][tx+x_] = rgb_pixel(value, value, value);
				// img.set_pixel(tx+x_, ty+y_, rgb_pixel(value, value, value));
			}
		}

		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());
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
		// double maxValue = accumulate(column.begin(), column.end(), 0)/((double)column.size());
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
			ImageUtils::hline(img, tx+x+i, ty+halfheight-size, size*2);
		}
	}

	virtual int getWidth(){
		return wave.size();
	};
	virtual int getHeight(){
		return height;
	};
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