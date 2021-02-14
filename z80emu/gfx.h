// Graphics functions prototypes
// Functions are in z80emu.cpp to reduce dependencies on Windows headers

class Gfx
{
public:
	enum { Black, DkGreen, LtGreen };
	void Rect(int x,int y,int cx,int cy,int col);
	void Line(int x1,int y1,int x2,int y2,int col);
	void Frame(int x,int y,int cx,int cy,int col);
	void Text(int x,int y,int cx,int cy,char *text,int col,int mode);
	void Clear();
	void setClient(int x,int y);
};

extern Gfx theGfx;

