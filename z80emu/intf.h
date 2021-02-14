#include <vector>
using namespace std;
typedef unsigned char uchar;

/*
Base class: interface
click: called when the user clicks the mouse, if 1 returned the screen is redrawn
redraw: just draw self
*/

class InterfaceObject
{
private:
	int id;
	int mem_addr;

protected:
	InterfaceObject():id(-1),dirty(1){}
	int x,y,cx,cy;
	int dirty;

public:
	virtual int click(int xx,int yy){return xx>=x && xx <(x+cx) && yy>=y && yy<(y+cy);}
	int isdirty()
	{
		int ret=0;
		if (dirty)
		{
			ret=1;
		}
		dirty=0;
		return ret;
	}
	virtual void redraw()=0;
	virtual void io_write(char val)=0;
	virtual char io_read()=0;
	void setid(int n){id=n;}
	int getid(){return id;}
	void setmem(int addr){mem_addr=addr;}
	int getmem(){return mem_addr;}
};

class ioButton : public InterfaceObject
{
private:
	char label[32];
	char pressed;

public:
	ioButton(int _x,int _y,int wid,int hei);
	void setLabel(char *t){strncpy_s(label,32,t,31);label[31]=0;dirty=1;}
	void redraw();
	// void press(){pressed=1;}
	int click(int xx,int yy)
	{
		if (xx>=x && xx <(x+cx) && yy>=y && yy<(y+cy))
		{
			pressed=1;
			return 1;
		}
		return 0;
	}
	void io_write(char){} // irrelevant - can't write to a button (what about label?)
	char io_read() // returns true if button has been pressed since last time we checked
	{
		if (pressed)
		{
			// a read resets the button
			pressed=0;
			return 1;
		}
		else
		{
			return 0;
		}
	}
};

class ioSevenSeg : public InterfaceObject
{
private:
	char value;

public:
	ioSevenSeg(int _x,int _y,int wid);
	void set(char n);
	char getvalue(){return value;}
	void redraw();
	void io_write(char val){set(val);}
	char io_read(){return getvalue();}
};

class Interface
{
private:
	void report(char *rep,char *mem);
	vector<InterfaceObject*> ifObjects;
	int regs_enabled,regx,regy;
	int mem_enabled,memx,memy;
	uchar cache[32];
	uchar out0cacheidx;
	uchar out0max;

public:
	Interface();
	~Interface();
	void add(InterfaceObject *i);
	int hasChanged()
	{
		int ret=0;
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end() && !ret; i++)
		{
			ret=(*i)->isdirty();
		}
		return ret;
	}
	void redraw(char *rep,char *mem)
	{
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
		{
			(*i)->redraw();
		}
		report(rep,mem);
	}
	InterfaceObject *click(int x,int y)
	{
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
		{
			if ((*i)->click(x,y))
				return *i;

		}
		return 0;
	}
	void io_write(int addr,char val)
	{
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
		{
			if ((*i)->getmem()==addr)
				(*i)->io_write(val);
		}
	}
	char io_read(int addr)
	{
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
		{
			if ((*i)->getmem()==addr)
				return (*i)->io_read();
		}
		return 0x00;
	}
	void Interface::reset()
	{
		for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
		{
			delete *i;
		}
		ifObjects.clear();
	}
	void show_regs(int x,int y){regs_enabled=1;regx=x;regy=y;}
	void hide_regs(){regs_enabled=0;}
	void show_memdump(int x,int y){mem_enabled=1;memx=x;memy=y;}
	void hide_memdump(){mem_enabled=0;}
	void handle_out(uchar port,uchar byte);
};
