#include "curses_support.h"
#include <cdk_test.h>
#include <cassert>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <mutex>
#include <memory>


namespace tui

{

class CdkWidget;	
class CdkLabel;
class CdkScreen;

/***************************************************************************//*
Conversion of a string into an array of pointer to char. This allow to make
the conversion between a std::string and the arguments needed by the CDK library

******************************************************************************/
using CallBack = int (*)(chtype key);
using CallBack2 = int (CdkScreen::*)(chtype key);

class ConvertToArrayCharPtr
{
public:
	ConvertToArrayCharPtr(const std::string & str)
	{
		std::string::size_type pos{};
		std::string::size_type oldPos{};
		while((pos = str.find('\n', oldPos)) != std::string::npos)
			{
				auto tmpSub = str.substr(oldPos, pos-oldPos);
				rows.push_back(std::move(tmpSub));
				oldPos = pos + 1;
			}
		// We take care of the space after the last <cr>
		auto tmpSub = str.substr(oldPos);
		rows.push_back(std::move(tmpSub));
		auto nRows = rows.size();
		ptr = new const char*[nRows];
		auto index = 0;
		for (auto &row:rows)
		{
			ptr[index] = row.c_str();
			++index;
		}
	}

	char ** getPtr()
	{
		return const_cast<char**>(ptr);
	}

	int size()
	{
		return  rows.size();
	}

	~ConvertToArrayCharPtr()
	{
		delete[] ptr;

	}

private:
	/// vector containing one string per row
	std::vector<std::string> rows{};
	/// Pointer to an array of pointers to const char
	const char ** ptr = nullptr;

};

/***************************************************************************//*
Conversion of an array of pointer to char into a string. This allow to make
the conversion between a std::string and the values returned  by the CDK library

******************************************************************************/
class ConvertFromArrayCharPtr
{
public:
	ConvertFromArrayCharPtr(chtype ** mesg, int nbr)
	{
		for (int index = 0 ; index < nbr ; ++index)
		{
			str += mesg[index];
			if(index != nbr -1)
				str += '\n';
		}
	}

	std::basic_string<chtype>  getStr()
	{
		return str;
	}

	~ConvertFromArrayCharPtr()
	{

	}

private:
	std::basic_string<chtype> str{};

};

/****************************************************************************//*
Main application to create CDK objects.

The singleton is retrieved by calling the functin getCdkApp which returns a pointer
to the singleton object.
******************************************************************************/
class CdkApp 
{
	
public:
	~CdkApp()
	{
	   	endCDK();
	}

	/// Returns the main curses window stdscr
	Window & getMainWindow()
	{
		return mainWindow;
	}

	static CdkApp * getCdkApp()
	{
		if (app == nullptr)
			app = new CdkApp;

		//std::call_once(alreadyCreated, []()mutable{app = new CdkApp;});
		return app;
	}

	/// Add a new CdkWidget to the internal application map
	static void  addObject( void * cdkPtr, CdkWidget * widgetPtr)
	{
		objectMap.insert(decltype(objectMap)::value_type({cdkPtr, widgetPtr}));
	}
	
	/// Remove a CdkWidget from the internal application map
	static void  removeObject(void * cdkPtr)
	{
		auto pos = objectMap.find(cdkPtr);
		objectMap.erase(pos);
	}

	/// Get a CdkWidget * from a CdkPtr *
	static CdkWidget*  getWidget(void * CdkPtr)
	{
		auto pos = objectMap.find(CdkPtr);
		if (pos != objectMap.end())
			return pos->second;
		else
			return nullptr;
	}

private:
	/// Constructor. It also initializes ncurses
	/// It is private because only the factory getCdkApp can call this object
 	CdkApp()
	{
	};


private:

	// This will call the default constructor which 
	// will create the main curse window by calling the default constructor of Window
	// This should not be a static member if we want the mainWindow to be deleted in the 
	// destructor of CdkApp
	Window mainWindow;
	/// Pointer to the singleton
	static CdkApp * app;
	/// Map relating the original CDK object pointers with the C++ object pointer
	/// This map maintains a list of all the object which are in the application
	/// irrespective of the screen in which they are located.
	static std::unordered_map< void *, CdkWidget *> objectMap;
	/// Flag used to make sure that CdkApp is only called once
	static std::once_flag alreadyCreated;
};

/******************************************************************************
Represents a CDK screen

There can be multiple CDK screen in an application. Each CDK screen is associated
with an ncurses window.
******************************************************************************/
class CdkScreen
{
public:

	/// Contructor - Create a CDKScreen using the stdscr curses window
	CdkScreen()
	{
		pCppCurseWin = & CdkApp::getCdkApp()->getMainWindow();
		assert(pCppCurseWin->getPtr() != nullptr);
		pObj = initCDKScreen(pCppCurseWin->getPtr());
		initCDKColor();

	}
	/// Contructor - Create a CDKScreen using a curses window defined by the parameters
	/// The curse window is automatically created and maintained by the CdkScreen object
	CdkScreen(  int x, int y, int width,int height)
	{
		// Create a new curses window
		pCppCurseWin = new Window(height ,width ,y ,x);
		assert(pCppCurseWin->getPtr() != nullptr);
		pObj = initCDKScreen(pCppCurseWin->getPtr());
		initCDKColor();
	}
		

	/// Destructor. Perform any necessary memory clean up. Because this CDK screen may be
	/// associated with the main curse window we check this condition before deleting the
	/// curse window (we do not want to delete the main window)
	~CdkScreen()
		{
		   	destroyCDKScreen(pObj);
			if(pCppCurseWin->getPtr() != CdkApp::getCdkApp()->getMainWindow().getPtr())
			{
				// This is not the main window
				delete pCppCurseWin;
			}
		}

	/// Erase all widgets associated with the screen without destroying them
	void erase()
		{ eraseCDKScreen(pObj); }

	/// Refresh widgets associated to the screen
	void refresh()
		{ refreshCDKScreen(pObj); }
	
	/// Draw a box around the window
	void box()
	{
		pCppCurseWin->box();
	}
	
	/// Add a label in the frame of the window
	void drawTitle(const std::string & str);
	
	/// Creates a popup label in the center of the string. If the input
	/// str has '\n' inside, the label will have multiple lines.
	void popupLabel(const std::string & str)
	{
		// We create a char ** from the str based on the presence of new line
		// characters.
		ConvertToArrayCharPtr convert(str);
				
		::popupLabel(pObj, convert.getPtr(), convert.size());

	}
	///  Open a dialog to choose a file. If the user presses cancel
	/// an empty file is returned
	std::string chooseFile(const std::string title)
	{
		std::string filepath{};
		auto ptr = selectFile(pObj,title.c_str());
		if (ptr != nullptr)
			filepath = ptr;
			
		return filepath;
	}
	/// Size related item
	int x()
	{
		return pCppCurseWin->x();
	}
	int y()
	{
		return pCppCurseWin->y();
	}
	int h()
	{
		return pCppCurseWin->h();
	}
	int w()
	{
		return pCppCurseWin->w();
	}

	/// Call back to process the widget in a class derived from 
	/// CdkScreen
	virtual int widgetCallback(CdkWidget * widget, chtype) {return 1;};


	/// Unregister a widget from the screen so that it is not refreshed anymore
	void unregisterWidget(CdkWidget * pWidget);
	/// Register the widget with the screen. This enables the redrawing of the object 
	/// when the screen is refreshed.
	void registerWidget(CdkWidget * pWidget);

	/// Allow to go from one widget to another within the same window
	virtual int traverse()
	{
		return traverseCDKScreen(pObj);
	}

	/// Return a pointer to the CDK object
	CDKSCREEN * getPtr() { return pObj;}

	
private:
	/// Pointer to the CDK object which is a a screen here
	CDKSCREEN * pObj;
	/// Pointer to the underlying encapsulated curses window
	Window * pCppCurseWin;
	/// Label creating the title
	std::unique_ptr<CdkLabel> titleWidget{} ;


};

/// This is the base class for all CDK widgets
class CdkWidget
{
public:
	/// Default constructor
	CdkWidget(){} ;

	/// Destructor
	virtual ~CdkWidget(){} ;

	/// Clear the widget
	virtual void clear(){};
	//
	/// Activate the widget. It should be noted that overrides in derived class
	/// may return a different value.
	virtual  EExitType activate(chtype * actions = nullptr) = 0;

	/// Draw the widget
	virtual void draw(bool box = true) = 0;
	/// Erase the widget from the screen without destroying it
	
	virtual void erase() = 0;
	
	/// Move the widget to an absolute or relative position
	virtual void move(int xpos, int ypos, bool relative = false, bool refresh = false) = 0;
	
	/// Raise this object
	virtual	void raise() = 0;

	/// Lower this object
	virtual void lower() = 0;

	/// Return the type of the object
	EObjectType getObjType()
	{
		return objType;
	}

	/// Return the underlying CDKObject pointer
	virtual void * getCDKObject() = 0;
	//
	/// Register call back. This registered call back is called in the post processing part
	/// of the widget
	void registerCallback(CallBack desiredFn)
	{
		fn = desiredFn;
	}
	void registerCallback2(CallBack2 desiredFn )
	{
		fn2 = desiredFn;
	}

protected:
	
	/// Preprocessing. Override these functions in the 
	// derived class for the desired functionality
	virtual int preProcess(chtype input)
	{
		return 1;
	}
	
	/// Postprocessing. Override these functions in the 
	/// derived class for the desired functionality.
	virtual int  postProcess(chtype input)
	{
		if (fn != nullptr)
			return fn(input);
		return 1;
	}

	/// Dispatch function to forward the preProcesssing to the
	/// class routine. The concept is based on having clientData be 
	/// a pointer to the object 
	static	int preHandler (EObjectType cdktype GCC_UNUSED, void *object ,
		       void *clientData GCC_UNUSED, chtype input )
	{
		auto cdkWidget = CdkApp::getWidget(object);
		return cdkWidget->preProcess(input);
	}

	/// Dispatch function to forward the post Processsing to the
	/// class routine. The concept is based on having clientData be 
	/// a pointer to the object 
	static	int postHandler (EObjectType cdktype GCC_UNUSED, void *object ,
		       void *clientData GCC_UNUSED, chtype input )
	{
		auto cdkWidget = CdkApp::getWidget(object);
		return cdkWidget->postProcess(input);
	}

	/// Pointer to the screen object to which this widget belongs. The knowledge of 
	/// the screen allows to convert between terminal coordinates and screen coordinates
	CdkScreen * screenPtr = nullptr;
	EObjectType objType;
	

private:
	/// Pointer to a call back function
	
	CallBack fn = nullptr;
	CallBack2 fn2 = nullptr;


};

/******************************************************************************
class CdkEntry
Representation of a widget which accepts user inputs

******************************************************************************/
class CdkEntry : public CdkWidget
{
public:
	/// Constructors
	CdkEntry(CdkScreen & screen, 
			int xpos, //< Relative position from the screen 
			int ypos, //< Relative position from the screen
			const std::string & title, 
			const std::string& label,
			EDisplayType displayType = vMIXED,
			int fieldwidth = 10, int minLength = 0, int maxLength = 10)
	{
		objType = vENTRY;
		auto termXPos = xpos + screen.x();
		auto termYPos = ypos + screen.y();
		pObj = newCDKEntry(screen.getPtr(), termXPos, termYPos, title.c_str(), label.c_str(), A_NORMAL,  ' ',
		   displayType, fieldwidth, minLength, maxLength, true, false);
		if (pObj != nullptr)
		{
			setCDKEntryPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKEntryPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			CdkApp::addObject(pObj, this);
		}
		
	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKEntry(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{cleanCDKEntry(pObj);}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{drawCDKEntry(pObj, box);}

	/// Erase from the screen without destroying it
	void erase() override
		{ eraseCDKEntry(pObj);}

	/// Get the current value from the widget
	char * getValue() const
		{return getCDKEntryValue(pObj);}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKEntry(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vENTRY, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vENTRY, ObjOf(pObj));
	}

	void* getCDKObject() override
	{
		return pObj;
	}

	/// Destructor
	~CdkEntry()
		{
			// Remove the object from the map
			CdkApp::removeObject(pObj);
			// Destroy the object
		   	destroyCDKEntry(pObj);
		}
	
protected:

	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);	
		//	return CdkWidget::postProcess(input);
	}


private:
	CDKENTRY * pObj = nullptr;

};

/******************************************************************************
class CdkMenu
Representation of a widget which accepts user inputs

******************************************************************************/
class CdkMenu : public CdkWidget
{
public:
	/// Constructors
	CdkMenu(CdkScreen & screen, 
		const char * menuList[MAX_MENU_ITEMS][MAX_SUB_ITEMS],
		int menuListLength,
		int * submenuListLength,
		int * menuLocation, 
		int menuPos,
		chtype titleAttribute = A_NORMAL,
		chtype subtitleAttribute = A_NORMAL
		)	

	{
		pObj = newCDKMenu(screen.getPtr(), menuList, menuListLength, submenuListLength, menuLocation, 
				menuPos, titleAttribute, subtitleAttribute);
		// Insert the preprocess and post process functions
		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			setCDKMenuPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKMenuPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vMENU;
	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKMenu(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{drawCDKMenu(pObj, box);}

	/// Erase from the screen without destroying it
	void erase() override
		{ eraseCDKMenu(pObj);}

	/// Get the current value from the widget
	std::pair<int,int> getValue() const
		{
			int m{}, s{};
			getCDKMenuCurrentItem(pObj, &m, &s);
			return std::make_pair(m,s);
		}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{
			moveCDKLabel(pObj, xpos, ypos, relative, refresh);
		}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vMENU, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vMENU, ObjOf(pObj));
	}
	
	/// Return a pointer to the underlying CDK Object (	CDKMENU *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Destructor
	~CdkMenu()
		{ 
			CdkApp::removeObject(pObj);
			destroyCDKMenu(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);
		//return CdkWidget::postProcess(input);
	}

private:
	CDKMENU * pObj = nullptr;


	


};

/****************************************************************************//*
class CdkLabel
Representation of a widget which display simple text\n
The size of the widget depends entirely on its content (both in width and height) and
cannot be adjusted

******************************************************************************/
class CdkLabel : public CdkWidget
{
public:
	/// Constructors
	CdkLabel(CdkScreen & screen, 
			int xrel, //< Relative x position relative to the screen
			int yrel, //< Relative y position relative to the screen
			const std::string & str,
			bool box = true,
			bool shadow = false
			)
	{
		auto xpos = xrel + screen.x();
		auto ypos = yrel + screen.y();
		auto convert = ConvertToArrayCharPtr(str);
		pObj = newCDKLabel(screen.getPtr(), xpos, ypos, convert.getPtr(), convert.size(),box, shadow);
		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			// A label is read-only and does not process the input
			//setCDKLabelPreProcess(pObj, CdkWidget::preHandler, this);
			//setCDKLabelPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vLABEL;

	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKLabel(pObj, actions); return vNORMAL;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{drawCDKLabel(pObj, box);}

	/// Erase from the screen without destroying it
	void erase() override
		{ eraseCDKLabel(pObj);}

	/// Set the text value of the label
	void setValue(const std::string & mesg)
		{
			ConvertToArrayCharPtr convert (mesg);
			setCDKLabel(pObj, convert.getPtr(), convert.size(), false);
		}

	/// Get the current value from the widget
	std::basic_string<chtype> getValue() const
		{
			int tmp{};
			auto mesg = getCDKLabelMessage(pObj, &tmp);
			ConvertFromArrayCharPtr convert(mesg, tmp);
			return convert.getStr();
		}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKLabel(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vLABEL, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vLABEL, ObjOf(pObj));
	}

	/// Return a pointer to the underlying CDK Object (	CDKLABEL *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Wait for the user to press a key to continue
	void wait(char key = 0)
		{
			waitCDKLabel(pObj, key);
		}

	/// Destructor
	~CdkLabel()
		{
			CdkApp::removeObject(pObj);
		   	destroyCDKLabel(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);
		//return CdkWidget::postProcess(input);
	}

private:
	CDKLABEL * pObj = nullptr;

};


/****************************************************************************//*
class CdkRadio
Representation of a widget which offers a choice to the user\n
The size of the widget depends entirely on its content (both in width and height) and
cannot be adjusted

******************************************************************************/
class CdkRadio : public CdkWidget
{
public:
	/// Constructors
	CdkRadio(CdkScreen & screen, //< Screen where the widget is located
			int xrel,				//< Placement of the object relative to the window
			int yrel,				//< Y Placement of the object relative to the window
			int spos,				//< Scroll bar location (LEFT, RIGHT, NONE)
			int height,				//< Widget height - if 0 full screen height
			int width,				//< Widget width - if 0 full screen width
			const std::string & title, //< Title displayed at the top of the widget
			const std::vector<std::string> & radioList, //< List of items
			chtype choiceCharacter = 'X', //< Character to highlight the current selection
			int defaultItem = 0,			//< Default selection (start at index 0)
			chtype highlight = A_BOLD,
			bool box = true,
			bool shadow = false
			)
	{
		// Converts the radio list in an array of const char *
		char ** list = new  char *[radioList.size()];
		auto tmp = list;
		for(auto &item : radioList)
		{
			*tmp = const_cast<char *>(item.c_str());
			++tmp;
		}
		// We create the object
		auto xpos = xrel + screen.x();
		auto ypos = yrel + screen.y();
		pObj = newCDKRadio(screen.getPtr(), xpos, ypos, spos , height, width, 
				title.c_str(), list,radioList.size(), choiceCharacter, defaultItem, highlight, box, shadow);
		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			setCDKRadioPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKRadioPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vRADIO;

	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKRadio(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{drawCDKRadio(pObj, box);}

	/// Erase from the screen without destroying it
	void erase() override
		{ eraseCDKRadio(pObj);}


	/// Get the index of the currently selected item
	int getValue() const
		{
			auto res  = getCDKRadioSelectedItem(pObj);
			return res;
		}
	/// Set the option currently selected.
	/// arg option Number from 0 to the number of possible options -1
	void setValue(int option)
	{
		setCDKRadioSelectedItem(pObj, option);

	}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKRadio(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vRADIO, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vRADIO, ObjOf(pObj));
	}

	/// Return a pointer to the underlying CDK Object (	CDKRADIO *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Destructor
	~CdkRadio()
		{
			CdkApp::removeObject(pObj);
		   	destroyCDKRadio(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);	
		//return CdkWidget::postProcess(input);
	}

private:
	CDKRADIO * pObj = nullptr;

};

/****************************************************************************//*
class CdkFslider
Representation of a widget which  let the user enter a floating point value and
see this value on a scale.

******************************************************************************/
class CdkFSlider : public CdkWidget
{
public:
	/// Constructors
	CdkFSlider(CdkScreen & screen, //< Screen where the widget is located
			   int xrel, //< Relative position
			   int yrel, //< Relative position
			   const std::string &title,
			   const std::string &label,
			   float start, //< Current value
			   float low,
			   float high,
			   int digits = 2,
			   float inc = 0.1,
			   float fastInc = 0.5,
			   int fieldWidth = 0,
			   chtype filler = '-' | A_REVERSE | COLOR_PAIR(29),
 			   bool box = false,
			   bool shadow = false)
	{
		// We create the object
		auto xpos = xrel + screen.x();
		auto ypos = yrel + screen.y();
		if(fieldWidth == 0)
			fieldWidth = screen.w() - label.size() - 10 ;
		pObj = newCDKFSlider(screen.getPtr(), xpos, ypos,title.c_str(), label.c_str() , filler, fieldWidth, start, 
				low, high, inc, fastInc, digits, box, shadow);
		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			setCDKFSliderPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKFSliderPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vFSLIDER;

	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKFSlider(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{
		}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{drawCDKFSlider(pObj, box);}

	/// Erase from the screen without destroying it
	void erase() override
		{ eraseCDKFSlider(pObj);}


	/// Get the index of the currently selected item
	float getValue() const
		{
			auto res  = getCDKFSliderValue(pObj);
			return res;
		}

	/// Set the current value of the object
	void setValue(float val)
	{
		setCDKFSliderValue(pObj, val);
	}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKFSlider(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vFSLIDER, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vFSLIDER, ObjOf(pObj));
	}

	/// The lower and upper limits are adjusted. The frame box remains
	/// the same
	void setLowHigh(float min, float max)
	{
		setCDKFSliderLowHigh(pObj, min, max );
	}

	/// Return a pointer to the underlying CDK Object (	CDKFSLIDER *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Destructor
	~CdkFSlider()
		{
			CdkApp::removeObject(pObj);
		   	destroyCDKFSlider(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);
		//return CdkWidget::postProcess(input);
	}

private:
	CDKFSLIDER * pObj = nullptr;


};


/****************************************************************************//*
class CdkButtonBox
Representation of a widget which show a collection of buttons. 

******************************************************************************/
class CdkButtonbox : public CdkWidget
{
public:
	/// Constructors
	CdkButtonbox(CdkScreen & screen, //< Screen where the widget is located
			   int xrel, //< Relative position
			   int yrel, //< Relative position
				int height, 
				int width, 
				const std::string title, 
				int rows, 
				int cols, 
				const std::vector<std::string> & buttons, 
				chtype highlight,
				bool box
			)
	{
		// We create the object
		auto xpos = xrel + screen.x();
		auto ypos = yrel + screen.y();
		// Converts the collection of buttons to the desired type
		char ** list = new  char *[buttons.size()];
		auto tmp = list;
		for(auto &item : buttons)
		{
			*tmp = const_cast<char *>(item.c_str());
			++tmp;
		}

		pObj = newCDKButtonbox(screen.getPtr(),
			   	xpos, ypos,
				height, width, 
				title.c_str(),
			    rows, cols, 
				list,
				buttons.size(),
				highlight,
				box,
				false);
		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			setCDKButtonboxPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKButtonboxPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vBUTTONBOX;

	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKButtonbox(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{
		}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{
			drawCDKButtonbox(pObj, box);
			drawCDKButtonboxButtons(pObj);
		}

	/// Erase from the screen without destroying it
	void erase() override
		{ 
			eraseCDKButtonbox(pObj);
		}


	/// Get the index of the currently selected button
	int getValue() const
		{
			auto res  = getCDKButtonboxCurrentButton(pObj);
			return res;
		}

	/// Set the current value of the object
	void setValue(int val)
	{
		setCDKButtonboxCurrentButton(pObj, val);
	}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKButtonbox(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vBUTTONBOX, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vBUTTONBOX, ObjOf(pObj));
	}


	/// Return a pointer to the underlying CDK Object (	CDKBUTTONBOX *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Destructor
	~CdkButtonbox()
		{
			CdkApp::removeObject(pObj);
		   	destroyCDKButtonbox(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);
		//return CdkWidget::postProcess(input);
	}

private:
	CDKBUTTONBOX * pObj = nullptr;


};


/****************************************************************************//*
class CdkSelection
Representation of a widget which offers a list of selection and allow the operator
to select one or more options

******************************************************************************/
class CdkSelection : public CdkWidget
{
public:
	/// Constructors
	CdkSelection(CdkScreen & screen, //< Screen where the widget is located
			   int xrel, //< Relative position
			   int yrel, //< Relative position
				int height, //< Height of the widget
				int width, //< Width of the widget
				int spos, //< Scroll bar location (LEFT, RIGHT, NONE)
				const std::string title, //< Title of the widget
				const std::vector<std::string> selectionList, //< List of items to display in the selection list
				chtype highlight = A_REVERSE, //< Attribute of the currently selected item
				bool box = false //< True to draw a box around the selection box
			)
	{
		// Create the string indicating the prefix to the selected items
		static const char * choices[] =
		{
			"   ",
			"-->"
		};
		// We create the object
		auto xpos = xrel + screen.x();
		auto ypos = yrel + screen.y();
		// Converts the collection of selections to the desired type
		char ** list = new  char *[selectionList.size()];
		auto tmp = list;
		for(auto &item : selectionList)
		{
			*tmp = const_cast<char *>(item.c_str());
			++tmp;
		}

		pObj = newCDKSelection(screen.getPtr(),
			   	xpos, ypos,
				spos,
				height, width, 
				title.c_str(),
				list,
				selectionList.size(),
				const_cast<char**>(choices),
				2,
				highlight,
				box,
				false);

		assert(pObj != nullptr);
		if (pObj != nullptr)
		{
			setCDKButtonboxPreProcess(pObj, CdkWidget::preHandler, this);
			setCDKButtonboxPostProcess(pObj, CdkWidget::postHandler, this);
			screenPtr = &screen;
			// Add the object to the map of CdkObj *
			CdkApp::addObject(pObj, this);
		}
		objType = vSELECTION;
		nbrChoices = selectionList.size();

	}	

	/// Activate the widget so that it is ready to accept user inputs. It will also draw the widget on the
	/// screen if if it not already drawn.
	EExitType activate(chtype * actions = nullptr) override
		{activateCDKSelection(pObj, actions); return pObj->exitType;}
 
	/// Clear the entry field of the widget
	void clear() override
   		{
		}

	/// Draw the widget. This does not give the focus to the object
	void draw(bool box = true) override
		{
			drawCDKSelection(pObj, box);
		}

	/// Erase from the screen without destroying it
	void erase() override
		{ 
			eraseCDKSelection(pObj);
		}


	/// Return a vector indicating all indexes of the list
	/// which have been selected. If nothing is selected, the
	/// vector is empty 
	std::vector<int>  getValue() const
		{
			std::vector<int> selected{};

			auto res  = getCDKSelectionChoices(pObj);
			// res is a pointer to an array of integer indicating 
			// the selection status of each choices
			for(int index = 0; index < nbrChoices; ++index)
			{
				if(*(res + index))
					selected.push_back(index);
			}
			return selected;
		}

	/// Set the current value of the object. The length of the 
	/// vector is the same as the number of selectable items. Each element 
	/// of the vector has a value zero or one.
	void setValue(std::vector<int> selected)
	{

		setCDKSelectionChoices(pObj, selected.data());	
	}

	/// Move the widget to an absolute or relative position
	void move(int xpos, int ypos, bool relative = false, bool refresh = false) override
		{moveCDKSelection(pObj, xpos, ypos, relative, refresh);}

	/// Raise this object
	void raise() override
	{
		raiseCDKObject(vSELECTION, ObjOf(pObj));
	}

	void lower() override
	{
		lowerCDKObject(vSELECTION, ObjOf(pObj));
	}


	/// Return a pointer to the underlying CDK Object (	CDKBUTTONBOX *)
	void* getCDKObject() override
	{
		return pObj;
	}
	
	/// Destructor
	~CdkSelection()
		{
			CdkApp::removeObject(pObj);
		   	destroyCDKSelection(pObj);
		}
	
protected:


	/// Preprocessing. Override this function in the 
	// derived class for the desired functionality
	// By default, this function just return 1
	//int preProcess(chtype input) override {return CdkWidget::postProcess(input);}
	
	/// Postprocessing. Override this function in the 
	/// derived class for the desired functionality.
	/// By default, this function call the callback fn if it
	/// has been registered
	int postProcess(chtype input) override 
	{
		return screenPtr->widgetCallback(this, input);
		//return CdkWidget::postProcess(input);
	}

private:
	CDKSELECTION * pObj = nullptr;
	int nbrChoices {};


};

} // end of namespace
