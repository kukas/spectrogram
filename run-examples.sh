#!/bin/sh

mkdir -p output

echo ""
echo "demonstrace nastavení slide parametru"
./spectrogram -s 300 -o output/amen_break.png input/amen_break.wav
./spectrogram -s 400 -o output/aphex.png input/aphex.wav

echo ""
echo "slide parametr větší než velikost rámce"
./spectrogram -s 600 -t 512 -o output/amen_break-600-slide.png input/amen_break.wav

echo ""
echo "demonstrace změny window funkce"
./spectrogram -w rect -o output/cello-rect.png input/cello.wav
./spectrogram -w hann -o output/cello-hann.png input/cello.wav
./spectrogram -w hamming -o output/cello-hamming.png input/cello.wav
./spectrogram -w blackmann -o output/cello-blackmann.png input/cello.wav

echo ""
echo "demonstrace nastavení velikosti rámce"
./spectrogram -t 4096 -o output/square_sweep-4096.png input/square_sweep.wav
./spectrogram -t 128 -o output/square_sweep-128.png input/square_sweep.wav

echo ""
echo "demonstrace čtení kanálů"
echo ""
echo " čtení levého kanálu"
./spectrogram -c 0 -o output/880hz.png input/880hz_noise.wav
echo ""
echo " čtení pravého kanálu"
./spectrogram -c 1 -o output/noise.png input/880hz_noise.wav