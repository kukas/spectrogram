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
#include <algorithm>
#include <numeric>



#define BUFFER_LEN 1024

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

class WindowFunction
{
protected:
	int windowSize = 0;
public:
	virtual double apply(double value, int i) = 0;
	virtual double setWindowSize(int windowsize){
		windowSize = windowsize;
	};
};

class RectangleWindowFunction : public WindowFunction
{
	virtual double apply(double value, int i) {
		return value;
	}
};

class ImageUtils
{
public:
	static void hline(image<rgb_pixel>& img, int x, int y, int size){
		rgb_pixel p(255, 255, 255);
		for (int i = y; i < y+size; ++i)
		{
			img.set_pixel(x, i, p);
		}
	}

	static void vline(image<rgb_pixel>& img, int x, int y, int size){
		rgb_pixel p(255, 255, 255);
		for (int i = x; i < x+size; ++i)
		{
			img.set_pixel(i, y, p);
		}
	}

	static void rectangle(image<rgb_pixel>& img, int x, int y, int width, int height){
		rgb_pixel p(255, 255, 255);
		for (int x_ = x; x_ < x+width; ++x_)
		{
			img.set_pixel(x_,y, p);
			img.set_pixel(x_,y+height-1, p);
		}
		for (int y_ = y; y_ < y+height; ++y_)
		{
			img.set_pixel(x,y_, p);
			img.set_pixel(x+width-1,y_, p);
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

class AveragesRenderer : public ImageBlock {
	vector<double> spectrumSums;
	int width = 100;
public:
	void addFrame(vector<double>& column){
		if(spectrumSums.size() == 0){
			spectrumSums = column;
		}
		else {
			for (int i = 0; i < spectrumSums.size(); ++i)
			{
				spectrumSums[i] += column[i];
			}
		}
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());

		double maxValue = *max_element(spectrumSums.begin(), spectrumSums.end());

		// empty spectrum
		if(maxValue == 0)
			return;

		for (int i = 0; i < spectrumSums.size(); ++i)
		{
			int value = spectrumSums[i]/maxValue*width;
			ImageUtils::vline(img, tx, ty+i, value);
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
public:
	void addFrame(vector<double>& column){
		spectrum.push_back(column);
	}

	virtual void render(image<rgb_pixel>& img, int tx, int ty){
		tx += x;
		ty += y;
		// find maxValue;
		double maxValue = 0;
		for (int x_ = 0; x_ < spectrum.size(); ++x_)
		{
			for (int y_ = 0; y_ < spectrum[y_].size(); ++y_)
			{
				maxValue = max(maxValue, spectrum[x_][y_]);
			}
		}

		// empty spectrum
		if(maxValue == 0)
			return;

		int bufferSize = getHeight();

		for (int x_ = 0; x_ < spectrum.size(); ++x_)
		{
			for (int y_ = 0; y_ < spectrum[x_].size(); ++y_)
			{
				// int yy = round(bufferSize*log(y)/log(bufferSize));
				double value = spectrum[x_][y_]/maxValue;
				value = 1-min(-log(value), 12.0)/12.0;
				value *= 255;
				img.set_pixel(tx+x_, ty+y_, rgb_pixel(value, value, value));
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
		tx += x;
		ty += y;
		ImageUtils::rectangle(img, tx, ty, getWidth(), getHeight());
		double halfheight = 0.5*height;
		for (int i = 0; i < wave.size(); ++i)
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

		cout << endl << maxx << "x" << maxy << endl;

		render(img, margin, margin);

		img.write(outputFilename);
	}
};

void printhelp(char* scriptName){
	cout << "Použití: " << string(scriptName) << "[PŘEPÍNAČE] VSTUPNÍ_SOUBOR" << endl;
	cout << "Generuje spektrogram ve formátu png ze VSTUPNÍHO SOUBORU." << endl;
	cout << endl;
	cout << "  -c KANÁL\t\t\tze VSTUPNÍHO SOUBORU čte KANÁL. Výchozí hodnota je 0 (1. kanál). Týká se pouze stereo nahrávek." << endl;
	cout << "  -o VÝSTUPNÍ_SOUBOR\t\tspecifikuje název výstupní bitmapy. Výchozí název je output.png" << endl;
	cout << "  -t VELIKOST\t\t\tnastaví velikost rámce pro FFT. Výchozí hodnota je 1024. VELIKOST musí být mocnina 2" << endl;
	cout << "  -s DÉLKA\t\t\tnastaví délku posunutí rámce FFT. Výchozí hodnota je 128. Ovlivňuje výslednou šířku spektrogramu" << endl;
	cout << "  -w WINDOW_FUNKCE\t\tpoužije vybranou window funkci" << endl;
	cout << endl;
	cout << "Seznam window funkcí:" << endl;
	cout << "  rect\t\t Obdélníková window funkce" << endl;
	cout << "  hann\t\t Hann window funkce" << endl;
	cout << "  hamming\t Hamming window funkce" << endl;
	cout << "  blackmann\t Blackman window funkce" << endl;
}

class Options
{
public:
	string input = "";
	string output = "output.png";
	int channel = 0;
	int windowSize = 1024;
	int windowSlide = 128;
	string windowFunction = "hann";
	void process(char** argv) {
		char* scriptName = argv[0];
		while (*++argv && **argv == '-')
		{
			switch (argv[0][1]) {
			case 'h':
				printhelp(scriptName);
				break;
			case 'c':
				if (*++argv)
					channel = stoi(string(argv[0]));
				else
					error();
				break;
			case 't':
				if (*++argv)
					windowSize = stoi(string(argv[0]));
				else
					error();
				break;
			case 's':
				if (*++argv)
					windowSlide = stoi(string(argv[0]));
				else
					error();
				break;
			case 'w':
				if (*++argv)
					windowFunction = string(argv[0]);
				else
					error();
				break;
			case 'o':
				if (*++argv)
					output = string(argv[0]);
				else
					error();
				break;
			default:
				error();
			}
		}

		// vstup
		if (argv[0]) {
			input = string(argv[0]);
		}

		cout << input << endl;
		cout << output << endl;
		cout << channel << endl;
		cout << windowSize << endl;
		cout << windowSlide << endl;
		cout << windowFunction << endl;
	}
private:
	void error() const {
		throw invalid_argument("invalid parameters");
	}
};

int main(int argc, char** argv)
{
	if(argc == 1){
		printhelp(argv[0]);
		return 0;
	}

	Options options;
	try {
		options.process(argv);
	}
	catch (invalid_argument e) {
		cout << "chyba v přepínačích"<<endl;
		return 1;
	}

	if(options.input == ""){
		cout << "chybějící vstupní soubor"<<endl;
		return 1;
	}

	cout << "Vstupní soubor: " << options.input << endl;

	SndfileHandle file;
	file = SndfileHandle(options.input);

	if(file.samplerate() == 0 || file.channels() == 0 || file.frames() == 0){
		cout << "chyba vstupního souboru"<<endl;
		return 1;
	}

	cout << "  Sample rate: " << file.samplerate() << endl;
	cout << "  Channels: " << file.channels() << endl;
	cout << "  Frames: " << file.frames() << endl;

	ChannelReader cr(file);
	try {
		cr.setChannel(options.channel);
	}
	catch (invalid_argument e) {
		cout << "neplatný kanál"<<endl;
		return 1;
	}
	SlidingWindow sw(cr);
	unique_ptr<WindowFunction> windowf;

	if(options.windowFunction == "rect")
		windowf = make_unique<RectangleWindowFunction>();
	else if(options.windowFunction == "hann")
		windowf = make_unique<RectangleWindowFunction>();
	else if(options.windowFunction == "hamming")
		windowf = make_unique<RectangleWindowFunction>();
	else if(options.windowFunction == "blackmann")
		windowf = make_unique<RectangleWindowFunction>();
	else {
		cout << "neplatná window funkce"<<endl;
		return 1;
	}
	int windowSize = options.windowSize;
	if((windowSize & (windowSize - 1)) != 0){
		cout << "neplatná velikost rámce"<<endl;
		return 1;
	}

	windowf->setWindowSize(windowSize);

	vector<double> buffer;
	buffer.resize(windowSize);
	
	int slide = options.windowSlide;
	if(slide <= 0){
		cout << "neplatná délka posunutí rámce" << endl;
		return 1;
	}
	sw.setWindow(windowSize, slide);

	Complex fourierBuffer2[windowSize];

	unique_ptr<FFTRenderer> fftrender = make_unique<FFTRenderer>();
	unique_ptr<WaveRenderer> waverender = make_unique<WaveRenderer>();
	unique_ptr<AveragesRenderer> averagesrender = make_unique<AveragesRenderer>();

	while(sw.read(buffer, windowSize)){
		for (int i = 0; i < windowSize; i++){
			fourierBuffer2[i] = windowf->apply(buffer[i], i);
			// fourierBuffer2[i] = buffer[i]*0.5*(1-cos(2*PI*i/(BUFFER_LEN-1)));
			// fourierBuffer2[i] = buffer[i];
		}

		waverender->addFrame(buffer, slide);
		
    	CArray data(fourierBuffer2, windowSize);
	    fft(data);

	    vector<double> values(windowSize/2);
		for (int i = 0; i < windowSize/2; i++){
			Complex c = data[windowSize/2+i];
			values[i] = abs(c);
		}

		fftrender->addFrame(values);
		averagesrender->addFrame(values);
	}

	ImageOutput imageOut;
	waverender->y = fftrender->getHeight()+10;
	averagesrender->x = fftrender->getWidth()+10;

	imageOut.addBlock(move(fftrender));
	imageOut.addBlock(move(waverender));
	imageOut.addBlock(move(averagesrender));

	imageOut.renderImage("out.png");

	return 0;
}
