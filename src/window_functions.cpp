#include <vector>
#include <cmath>

using namespace std;

#ifndef WINDOWFUNCTIONS 
#define WINDOWFUNCTIONS


class WindowFunction
{
protected:
	int windowSize = 0;
public:
	virtual double apply(double value, int i) = 0;
	virtual void setWindowSize(int windowsize){
		windowSize = windowsize;
	};
};

class RectangleWindowFunction : public WindowFunction
{
	virtual double apply(double value, int i) {
		return value;
	}
};

class PrecomputedWindowFunction : public WindowFunction
{
protected:
	vector<double> window;
public:
	virtual void setWindowSize(int windowsize){
		windowSize = windowsize;
		window.resize(windowSize);
		for (int i = 0; i < windowSize; ++i)
		{
			window[i] = get(i);
		}
	};
	virtual double get(int i) = 0;
	virtual double apply(double value, int i) {
		return value*window[i];
	}
};

// vzorce čerpány z: https://en.wikipedia.org/wiki/Window_function#Spectral_analysis
class HannWindowFunction : public PrecomputedWindowFunction
{
	virtual double get(int i){
		return 0.5*(1-cos(2*M_PI*i/(windowSize-1)));
	}
};

class HammingWindowFunction : public PrecomputedWindowFunction
{
	virtual double get(int i){
		return 0.54 - 0.46 * cos(2*M_PI*i/(windowSize-1));
	}
};

class BlackmannWindowFunction : public PrecomputedWindowFunction
{
	virtual double get(int i){
		double a0 = 7938.0/18608.0;
		double a1 = 9240.0/18608.0;
		double a2 = 1430.0/18608.0;
		// double a0 = 0.42, a1 = 0.5, a2 = 0.08;
		return a0 - a1 * cos(2*M_PI*i/(windowSize-1)) + a2 * cos(4*M_PI*i/(windowSize-1));
	}
};

#endif