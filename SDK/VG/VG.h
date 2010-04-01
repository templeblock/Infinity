#pragma once

#include "impl\SharedResources.h"
#include "SUI.h"

namespace vg
{
	void init();
	void cleanup();
}

//struct TextAlign
//{
//	enum
//	{
//		Top = 1, VCenter = 2, Bottom = 3,
//		Left = 1<<2, HCenter = 2<<2, Right = 3<<2,
//		VMask = 3, HMask = 3<<2
//	};
//};
//
struct Point
{
    Point() : x(0), y(0) {}
    Point(int ix, int iy) : x(ix), y(iy) {}
    Point(const Point & p) : x(p.x), y(p.y) {}

    const Point& operator= (const Point & p) { this->x = p.x; this->y = p.y; return *this; }

    int x, y;
};

struct Rect
{
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(const Point & p) : x(p.x), y(p.y), w(0), h(0) {}
    Rect(int ix, int iy, int iw = 0, int ih = 0) : x(ix), y(iy), w(iw), h(ih) {}
    Rect(const Rect & r) : x(r.x), y(r.y), w(r.w), h(r.h) {}

    const Rect& operator= (const Rect & r) { this->x = r.x; this->y = r.y; this->w = r.w; this->h = r.h; return *this; }

    int x, y;
    int w, h;

    //static const Rect null;
};

enum Color
{
    cBase = 0,
    cBool = 4,
    cOutline = 8,
    cFont = 12,
    cFontBack = 16,
    cTranslucent = 20,
    cNbColors = 24,
};


//Fix me!!!!: take into account font metrics for font rendering
class VG
{
public:
	void setSize(GLuint width, GLuint height) {mWidth = width; mHeight = height;}

	void begin();
	void end();

	//void drawText(FTFont* font, GLint x, GLint y, const char* text)	{if (font) drawText(*font, x, y, text);}
	//void drawText(FTFont* font, glm::ivec2 pos, const char* text)	{if (font) drawText(*font, pos, text);}
	//void drawText(FTFont& font, glm::ivec2 pos, const char* text)	{drawText(font, pos.x, pos.y, text);}
	//void drawText(FTFont& font, GLint x, GLint y, const char* text) {drawText(font, x, y, TextAlign::Top|TextAlign::Left, text);}
	//void drawText(FTFont& font, glm::ivec2 pos, int flags, const char* text) {drawText(font, pos.x, pos.y, flags, text);}
	//void drawText(FTFont& font, GLint x, GLint y, int flags, const char* text);

private:
	void setOrthoProj();
	void setPerspectiveProj();

private:
	GLuint	mWidth;
	GLuint	mHeight;
	//ProgramRef	mWidgetProgram;

	//unsigned int mFillColorUniform;
	//unsigned int mBorderColorUniform;
	//unsigned int mZonesUniform;
};