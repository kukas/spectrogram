#include <iostream>
#include <cmath>
#include <vector>
using namespace std;

class FFT
{
    vector<double> cacher;
    vector<double> cachei;
    int transformSize;
public:
    void setTransformSize(int N){
        transformSize = N;
        cacher.resize(N/2);
        cachei.resize(N/2);
        for (int i = 0; i < N/2; ++i)
        {
            // jednotková kružnice
            double cr = cos(-2*M_PI*i/N);
            double ci = sin(-2*M_PI*i/N);
            cacher[i] = cr;
            cachei[i] = ci;
        }
    }
    // https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm
	void transform(vector<double>& data){
        int N = data.size()/2;
        if(N == 1) return;

        int imag = data.size()/2;

        vector<double> even(N);
        for (int i = 0; i < N; i++)
            even[i] = data[i*2];

        vector<double> odd(N);
        for (int i = 0; i < N; i++)
            odd[i] = data[i*2+1];


        transform(even);
        transform(odd);

        for (int i = 0; i < N/2; ++i)
        {
            double cr = cacher[i*transformSize/N];
            double ci = cachei[i*transformSize/N];
            // sudé
            double oddr = odd[i];
            double oddi = odd[i+imag/2];
            // liché
            double evenr = even[i];
            double eveni = even[i+imag/2];

            double tr = oddr*cr - oddi*ci;
            double ti = oddr*ci + cr*oddi;

            data[i] = evenr + tr;
            data[i+imag] = eveni + ti;

            data[i+N/2] = evenr - tr;
            data[i+N/2+imag] = eveni - ti;
        }
	}

    vector<double> getMagnitudes(vector<double>& data){
        transform(data);

        vector<double> magnitudes(data.size()/4);
        size_t N = data.size()/2;
        for (size_t i = 0; i < N/2; ++i)
        {
            magnitudes[i] = sqrt(data[i]*data[i] + data[i+N]*data[i+N]);
        }

        return magnitudes;
    }
};