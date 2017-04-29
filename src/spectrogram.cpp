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

using namespace std;
using namespace png;

#include "image_output.cpp"
#include "fft.cpp"
#include "window_functions.cpp"

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
	}
private:
	void error() const {
		throw invalid_argument("invalid parameters");
	}
};

int main(int argc, char** argv)
{
	// zpracování vstupních argumentů
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

	// načtení souboru
	SndfileHandle file;
	file = SndfileHandle(options.input);

	// kontrola, zda-li je soubor platný
	if(file.samplerate() == 0 || file.channels() == 0 || file.frames() == 0){
		cout << "chyba vstupního souboru"<<endl;
		return 1;
	}

	cout << "  Sample rate: " << file.samplerate() << endl;
	cout << "  Channels: " << file.channels() << endl;
	cout << "  Frames: " << file.frames() << endl;

	cout << "Výstupní soubor: " << options.output << endl;
	cout << "  Rozměr spektrogramu: " << file.frames()/options.windowSlide << "x" << options.windowSize/2 << endl;

	// nastavení čtení zvoleného kanálu
	ChannelReader cr(file);
	try {
		cr.setChannel(options.channel);
	}
	catch (invalid_argument e) {
		cout << "neplatný kanál"<<endl;
		return 1;
	}

	// nastavení window funkce
	unique_ptr<WindowFunction> windowf;
	if(options.windowFunction == "rect")
		windowf = make_unique<RectangleWindowFunction>();
	else if(options.windowFunction == "hann")
		windowf = make_unique<HannWindowFunction>();
	else if(options.windowFunction == "hamming")
		windowf = make_unique<HammingWindowFunction>();
	else if(options.windowFunction == "blackmann")
		windowf = make_unique<BlackmannWindowFunction>();
	else {
		cout << "neplatná window funkce"<<endl;
		return 1;
	}

	// nastavení velikosti rámce
	int windowSize = options.windowSize;
	if((windowSize & (windowSize - 1)) != 0 || windowSize < 8){
		cout << "neplatná velikost rámce"<<endl;
		return 1;
	}
	windowf->setWindowSize(windowSize);
	
	// nastavení délky posouvání rámce
	int slide = options.windowSlide;
	if(slide <= 0){
		cout << "neplatná délka posunutí rámce" << endl;
		return 1;
	}

	// čtení souboru posuvným oknem
	SlidingWindow sw(cr);
	sw.setWindow(windowSize, slide);

	// zobrazovací komponenty
	unique_ptr<FFTRenderer> fftrender = make_unique<FFTRenderer>();
	unique_ptr<WaveRenderer> waverender = make_unique<WaveRenderer>();
	unique_ptr<AveragesRenderer> averagesrender = make_unique<AveragesRenderer>();

	FFT fft;
	fft.setTransformSize(windowSize);

	vector<double> buffer;
	vector<double> fourierBuffer;
	buffer.resize(windowSize);

	fourierBuffer.resize(windowSize*2);

	while(sw.read(buffer, windowSize)){
		fill(fourierBuffer.begin()+windowSize, fourierBuffer.end(), 0);

		for (int i = 0; i < windowSize; i++){
			fourierBuffer[i] = windowf->apply(buffer[i], i);
		}

		vector<double> mag = fft.getMagnitudes(fourierBuffer);

		waverender->addFrame(buffer, slide);
		averagesrender->addFrame(mag);
		fftrender->addFrame(mag);
	}

	// obrázkový výstup
	ImageOutput imageOut;

	// umístění komponent
	waverender->y = fftrender->getHeight()+10;
	averagesrender->x = fftrender->getWidth()+10;

	double timescale = file.frames()/((double)file.samplerate());
	double freqscale = file.samplerate()/2.0;
	unique_ptr<ScaleRenderer> fftscale = make_unique<ScaleRenderer>(0, 0, fftrender->getWidth(), fftrender->getHeight(), timescale, freqscale, 0.5, 1000);
	unique_ptr<ScaleRenderer> wavescale = make_unique<ScaleRenderer>(0, waverender->y, waverender->getWidth(), waverender->getHeight(), timescale, -1, 0.5, -1);
	unique_ptr<ScaleRenderer> averagesscale = make_unique<ScaleRenderer>(averagesrender->x, 0, averagesrender->getWidth(), averagesrender->getHeight(), -1, freqscale, -1, 1000);

	imageOut.addBlock(make_unique<WindowRenderer>(averagesrender->x+15, waverender->y+30, 70, 70, move(windowf), windowSize));

	imageOut.addBlock(move(fftrender));
	imageOut.addBlock(move(waverender));
	imageOut.addBlock(move(averagesrender));

	imageOut.addBlock(move(fftscale));
	imageOut.addBlock(move(wavescale));
	imageOut.addBlock(move(averagesscale));

	

	// výstup
	imageOut.renderImage(options.output);

	return 0;
}
