#include <fstream>
#include <string>
#include <iostream>
#include <cmath>
#include <sndfile.hh>
#include "png++/png.hpp"
#include <memory>
#include <algorithm>
#include <numeric>

#include "input.hpp"
#include "image_output.hpp"
#include "fft.hpp"
#include "window_functions.hpp"

using namespace std;
using namespace png;

void printhelp(char* scriptName){
	cout << "Použití: " << string(scriptName) << " [PŘEPÍNAČE] VSTUPNÍ_SOUBOR" << endl;
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
		throw invalid_argument("chyba v přepínačích");
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
	catch (const invalid_argument & e) {
		cout << e.what() <<endl;
		return 1;
	}

	if(options.input == ""){
		cout << "chybějící vstupní soubor"<<endl;
		return 1;
	}

	// nastavení délky posouvání rámce
	int slide = options.windowSlide;
	if(slide <= 0){
		cout << "neplatná délka posunutí rámce" << endl;
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
	catch (const invalid_argument &) {
		cout << "neplatný kanál" << endl;
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

	// čtení souboru posuvným oknem
	SlidingWindow sw(cr);
	sw.setWindow(windowSize, slide);

	// zobrazovací komponenty
	unique_ptr<FFTRenderer> fftrender = make_unique<FFTRenderer>();
	unique_ptr<WaveRenderer> waverender = make_unique<WaveRenderer>();
	unique_ptr<AveragesRenderer> averagesrender = make_unique<AveragesRenderer>();

	FFT fft;
	fft.setTransformSize(windowSize);

	// buffer pro čtení zvukového souboru
	vector<double> buffer;
	// buffer pro výsledky fourierovy transformace
	vector<double> fourierBuffer;
	buffer.resize(windowSize);
	// fourierBuffer má dvojnásobnou velikost kvůli imaginárním částem
	fourierBuffer.resize(windowSize*2);

	while(sw.read(buffer, windowSize)){
		// smazání imaginárních částí z minulého výpočtu
		fill(fourierBuffer.begin()+windowSize, fourierBuffer.end(), 0);
		// přepis z bufferu do fourierBufferu + aplikace window funkce
		for (int i = 0; i < windowSize; i++){
			fourierBuffer[i] = windowf->apply(buffer[i], i);
		}

		vector<double> mag = fft.getMagnitudes(fourierBuffer);

		// předání hodnot do tříd zajišťujících grafický výstup
		waverender->addFrame(buffer, slide);
		averagesrender->addFrame(mag);
		fftrender->addFrame(mag);
	}

	// grafický výstup
	ImageOutput imageOut;

	// umístění komponent
	waverender->y = fftrender->getHeight()+10; // pod FFT
	averagesrender->x = fftrender->getWidth()+10; // napravo od FFT

	// měřítko os
	double timescale = file.frames()/((double)file.samplerate());
	double freqscale = file.samplerate()/2.0;
	unique_ptr<ScaleRenderer> fftscale = make_unique<ScaleRenderer>(0, 0, fftrender->getWidth(), fftrender->getHeight(), timescale, freqscale, 0.5, 1000);
	unique_ptr<ScaleRenderer> wavescale = make_unique<ScaleRenderer>(0, waverender->y, waverender->getWidth(), waverender->getHeight(), timescale, -1, 0.5, -1);
	unique_ptr<ScaleRenderer> averagesscale = make_unique<ScaleRenderer>(averagesrender->x, 0, averagesrender->getWidth(), averagesrender->getHeight(), -1, freqscale, -1, 1000);

	// zobrazení window funkce
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
