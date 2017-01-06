#include "curses_support.h"
#include "mutex"

// Creates a curses window with the desired characteristics
tui::Window::Window(int lines, int cols, int begin_y, int begin_x, Window * parent , bool relative )
	:x_pos(begin_x), y_pos(begin_y), height(lines), width(cols)
{
	if(!parent)
	{
		subWindow = false;
		ptr = newwin(lines, cols , begin_y, begin_x);
	}
	else
	{
		subWindow = true;
		if (!relative)
		{
			ptr = subwin(parent->ptr, lines, cols , begin_y, begin_x);
		}
		else
		{
			ptr = derwin(parent->ptr, lines, cols, begin_y, begin_x);
			// We adjust the terminal coordinates of the subwindow
			x_pos += parent->x();
			y_pos += parent->y();
		}
	}	
	
}

// Create the main curses window
tui::Window::Window()
{
	ptr = initscr();
	// This creates the main 
	//std::once_flag mainWindowCreated;
	//std::call_once(mainWindowCreated, [this](){ptr = initscr();});
}


tui::Window::~Window()
{

	delwin(ptr);

}

void tui::Window::move(int y, int x, bool relative)
{
	
	if(!subWindow)
		mvwin(ptr, y,x);
	else
		mvderwin(ptr,y,x);

	// We adjust the terminal coordinates of the window	
	x_pos += x;
	y_pos += y;
	
}

void tui::Window::update()
{
	wrefresh(ptr);
}

void tui::Window::box()
{
	chtype ls, rs, ts, bs, tl, tr, bl, br;
	ls = rs = ts = bs = tl = tr = bl = br = 0;
	wborder(ptr , ls, rs, ts, bs, tl, tr, bl,  br);
	wrefresh(ptr);
}


int tui::Window::getchar()
{
	return wgetch(ptr);
}

