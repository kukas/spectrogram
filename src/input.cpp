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