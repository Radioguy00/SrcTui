#include <cdk_test.h>
#include <cassert>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <mutex>


namespace tui

{


/***************************************************************************//*
Representation of a ncurse WINDOW

******************************************************************************/
class Window
{
	public:
		/// Create a Window or a subWindow. If parent is not null
		/// it is a subWindow. If relative is true, the coordinates for the
		/// subWindow are relative to those of the parent Window
		Window(int lines, int cols, int begin_y, int begin_x, Window* parent = nullptr, bool relative = true);
		/// Create the stdscr curses window
		Window();
		/// Create a Window object from an existing WINDOW *
		Window(WINDOW * pWin):ptr(pWin){};
		/// Assign a different WINDOW* to this object. Returns the old pointer
		WINDOW * assign(WINDOW * newWin)
			{
				auto tmp = ptr;
				ptr = newWin;
				return tmp;
			}
		/// Destrow the Window
		~Window();
		/// Create a box around the Window
		void box();
		/// Move the Window to a different location. If it is a subWindow, it is moved
		/// within the parent Window.
		void move(int y, int x, bool relative = false);
		/// Update the screen with the content of the Window
		void update();
		/// Get a character from the Window
		int getchar();
		/// Get the pointer to the ncurses Window
		WINDOW* getPtr()
			{return ptr;} 
		/// Return the x position of the top left corner of the window
		int x()
		{
			return x_pos;
		}
		/// Return the y  position of the top left corner of the window
		int y()
		{
			return y_pos;
		}
		/// Return the width  of the window
		int w()
		{
			return width;
		}
		/// Return the height of the window
		int h()
		{
			return height;
		}
	private:
		WINDOW * ptr = nullptr;
		bool subWindow = false;
		// The following window position and size are relative to the terminal whether this is a
		// subwindow or not
		int x_pos{}; // Location of the top left corner related to the terminal
		int y_pos{}; //< Location of the top left corner related to the terminal
		int height{};	//< Height of the window
		int width{};	//< Width of the window

};

} // end of namespace
