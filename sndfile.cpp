// dependencies:
// png++
// libsndfile
// compilation:
// g++ -lsndfile -o sndfile sndfile.cpp

#include <fstream>
#include <string>
#include <iostream>
#include <cmath>
#include <sndfile.hh>
#include "png++/png.hpp"
#include <memory>


#define BUFFER_LEN 2048

using namespace std;
using namespace png;

#include <complex>
#include <iostream>
#include <valarray>
 
const double PI = 3.141592653589793238460;
 
typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;
 
// Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
void fft(CArray& x)
{
    const size_t N = x.size();
    if (N <= 1) return;
 
    // divide
    CArray even = x[std::slice(0, N/2, 2)];
    CArray  odd = x[std::slice(1, N/2, 2)];
 
    // conquer
    fft(even);
    fft(odd);
 
    // combine
    for (size_t k = 0; k < N/2; ++k)
    {
        Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
        x[k    ] = even[k] + t;
        x[k+N/2] = even[k] - t;
    }
}

class ChannelReader
{
	int channels;
	int channel;
	SndfileHandle& handle;
	vector<double> buffer;
public:
	ChannelReader(SndfileHandle& handle_) : handle(handle_) {
		channels = handle.channels();
		setChannel(0);
	}

	int read(vector<double>& outBuffer, int size){
		int bufferSize = size*channels;
		buffer.resize(bufferSize);
		int readBytes = handle.read(buffer.data(), bufferSize);
		int readFrames = readBytes/channels; // readFrames <= size
		for (int i = 0; i < readFrames; ++i)
		{
			outBuffer[i] = buffer[i*channels + channel];
		}
		return readFrames;
	}

	void setChannel(int channel_){
		if(channel_ >= 0 && channel_ < channels)
			channel = channel_;
		else
			throw invalid_argument("invalid channel");
	}
};

class SlidingWindow
{
	int position = 0;
	int bufferSize;

	int windowSize;
	int windowSlide;

	vector<double> buffer;
	vector<double> window;
	ChannelReader& reader;

	bool readTo(vector<double>& buf, int size){
		int readBytes = reader.read(buf, size);
		position += readBytes;
		return readBytes == size;
	}

	bool readWindow(){
		return readTo(window, windowSize);
	}

	bool readBuffer(){
		return readTo(buffer, bufferSize);
	}
	bool next(){
		if(position == 0){
			return readWindow();
		}

		if(windowSlide < windowSize){
			// slide existing data
			copy(window.begin()+windowSlide, window.end(), window.begin());
			// read and copy new data to the end
			bool notEnd = readBuffer();
			copy(buffer.begin(), buffer.end(), window.end()-windowSlide);
			return notEnd;
		}
		else {
			// posun
			readBuffer();
			// čtení
			return readWindow();
		}
	}

public:
	SlidingWindow(ChannelReader reader) : reader(reader) {
		setWindow(128, 64);
	}

	void setWindow(int size, int slide){
		windowSize = size;
		windowSlide = slide;

		if(slide <= size){
			bufferSize = slide;
		}
		else {
			bufferSize = slide - size;
		}

		buffer.resize(bufferSize);
		window.resize(windowSize);
	}

	bool read(vector<double>& outBuffer, int size){
		if (next()) {
			copy_n(window.begin(), size, outBuffer.begin());
			return true;
		}
		else {
			return false;
		}
	}
};

class ImageBlock
{
protected:
	int width;
	int height;
public:
	int x;
	int y;
	virtual void render(image<rgb_pixel>& img, int tx, int ty) = 0;
	virtual int getWidth(){
		return width;
	};
	virtual int getHeight(){
		return height;
	};
};

class Rectangle : public ImageBlock
{
public:
	Rectangle(int x_, int y_, int width_, int height_){
		width = width_+1;
		height = height_+1;
		x = x_;
		y = y_;
	}
	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		cout << "rendering";
		rgb_pixel p(255, 255, 255);
		for (int x_ = tx+x; x_ < tx+x+width; ++x_)
		{
			img.set_pixel(x_,ty+y, p);
			img.set_pixel(x_,ty+y+height-1, p);
		}
		for (int y_ = ty+y; y_ < ty+y+height; ++y_)
		{
			img.set_pixel(tx+x,y_, p);
			img.set_pixel(tx+x+width-1,y_, p);
		}
	};
};

class FFTRenderer : public ImageBlock {

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

static void
read_file (const char * fname)
{

	SndfileHandle file;

	file = SndfileHandle(fname);
	ChannelReader cr(file);
	SlidingWindow sw(cr);

	printf ("Opened file '%s'\n", fname) ;
	printf ("    Sample rate : %d\n", file.samplerate ()) ;
	printf ("    Channels    : %d\n", file.channels ()) ;
	printf ("    Frames    : %d\n", file.frames()) ;

	// sw.readWindow();
	vector<double> buffer;
	buffer.resize(BUFFER_LEN);
	
	int slide = 128;
	sw.setWindow(BUFFER_LEN, slide);

	ImageOutput imageOut;
	// ImageBlock* rr = new Rectangle(0, 0, 100, 100);
	imageOut.addBlock(make_unique<Rectangle>(10, 20, 100, 100));

	imageOut.renderImage("out.png");


	// int height = BUFFER_LEN;
	// int width = ceil(file.frames()/slide);
		
	// Complex fourierBuffer2[BUFFER_LEN];

	// int x = 0;
	// while(sw.read(buffer, BUFFER_LEN)){
	// 	for (int i = 0; i < BUFFER_LEN; i++){
	// 		fourierBuffer2[i] = buffer[i]*0.5*(1-cos(2*PI*i/(BUFFER_LEN-1)));
	// 		// fourierBuffer2[i] = buffer[i];
	// 	}
		
 //    	CArray data(fourierBuffer2, BUFFER_LEN);
	//     fft(data);

	// 	int y = 0;
	// 	for (int i = BUFFER_LEN/2; i < BUFFER_LEN; i++){
	// 		Complex c = data[min(BUFFER_LEN, i)];
	// 		double val3 = log(abs(c))/log(100);
	// 		val3 = min(1.0, max(0.0, val3))*255;
	// 		img.set_pixel(x,y, rgb_pixel(val3, val3, val3));
	// 		y++;
	// 	}

	// 	x++;
	// }


	// int readBytes = file.read (buffer, BUFFER_LEN);
	// int x = 0;
	// file.read(buffer, BUFFER_LEN);
	// for (int i = 0; i < BUFFER_LEN; i+=1){
	// 	cout << fourierBuffer[i] << endl;
	// 	int val = fourierBuffer[i]*halfheight;
	// 	int y = min(height-2, max(0, val+halfheight));
	// 	img.set_pixel(x,y, rgb_pixel(255, 255, 255));
		
	// 	int val2 = buffer[min(BUFFER_LEN, i*2)]/32768.0*halfheight;
	// 	int y2 = val2+halfheight;
	// 	img.set_pixel(x,y2, rgb_pixel(255, 0, 0));

	// 	x++;
	// 	// cout << x << " " << y << endl;
	// }
	// img.write("output.png");



	// while(file.read (buffer, BUFFER_LEN) > 0){
	// 	for (int i = 0; i < BUFFER_LEN; i+=channels)
	// 	{
	// 	cout << buffer[0]/scale << endl;

	// 	}
	// 	int val = buffer[0]/scale;
	// 	int start = halfheight 
	// 	for (size_t y = 0; y < val; ++y){
	// 		img[y][x] = rgb_pixel(255, 255, 255);
	// 	}
	// 	x++;
	// }
}


int main (void)
{	const char * fname = "input/test.wav" ;

	puts ("\nSimple example showing usage of the C++ SndfileHandle object.\n") ;

	read_file (fname) ;

	puts ("Done.\n") ;
	return 0 ;
} /* main */

