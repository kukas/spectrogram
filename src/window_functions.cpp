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