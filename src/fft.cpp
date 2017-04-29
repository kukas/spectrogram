#include <complex>
#include <valarray>
#include <cmath>

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;
// Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
void fft(std::valarray<Complex>& x)
{
    const size_t N = x.size();
    if (N <= 1) return;
 
    // divide
    std::valarray<Complex> even = x[std::slice(0, N/2, 2)];
    std::valarray<Complex>  odd = x[std::slice(1, N/2, 2)];
 
    // conquer
    fft(even);
    fft(odd);
 
    // combine
    for (size_t k = 0; k < N/2; ++k)
    {
        Complex t = std::polar(1.0, -2 * M_PI * k / N) * odd[k];
        x[k    ] = even[k] + t;
        x[k+N/2] = even[k] - t;
    }
}
