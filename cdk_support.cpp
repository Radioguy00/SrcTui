#include "cdk_support.h"
#include "mutex" // Needed for the once_flag

// Definition of the static variables for the CdkApp class

// Pointer to the singleton
tui::CdkApp * tui::CdkApp::app = nullptr;
// Map relating the original CDK object pointers with the C++ object pointer
// This map maintains a list of all the object which are in the application
// irrespective of the screen in which they are located.
std::unordered_map< void *, tui::CdkWidget *> tui::CdkApp::objectMap;
// Flag used to make sure that CdkApp is only called once
//std::once_flag tui::CdkApp::alreadyCreated;


/******************************************************************************

  CDK Application

******************************************************************************/


/******************************************************************************

  CDK Screen

******************************************************************************/

void tui::CdkScreen::drawTitle(const std::string & str)
{
	titleWidget = std::unique_ptr<CdkLabel>(new CdkLabel(*this, w()/2 - str.size() /2, 0, str.c_str(), false));
	titleWidget->draw();
}



/// Unregister a widget from the screen so that it is not refreshed anymore
void tui::CdkScreen::unregisterWidget(CdkWidget * pWidget)
{
	unregisterCDKObject(pWidget->getObjType(),  pWidget->getCDKObject());

}
/// Register the widget with the screen. This enables the redrawing of the object 
/// when the screen is refreshed.
void tui::CdkScreen::registerWidget(CdkWidget * pWidget)
{
	registerCDKObject(pObj,pWidget->getObjType() , pWidget->getCDKObject());
}

/******************************************************************************

  CDK Widget

******************************************************************************/

/******************************************************************************

  CDK  Entry Widget

******************************************************************************/


