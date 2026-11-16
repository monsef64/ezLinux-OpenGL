#include <Foundation/FoundationPCH.h>

#if EZ_ENABLED(EZ_PLATFORM_LINUX)

#  include <Foundation/Basics/Platform/PlatformFeatures.h>

#  if EZ_ENABLED(EZ_SUPPORTS_GLFW)

#    include <Foundation/Platform/GLFW/Screen_GLFW.h>

#  else

#    include <Foundation/System/Screen.h>

// If GLFW is not available there is no way to enumerate screens, so report a single default screen,
// so that code that queries screens still works in headless setups.
ezResult ezScreen::EnumerateScreens(ezDynamicArray<ezScreenInfo>& out_Screens)
{
  out_Screens.Clear();

  ezScreenInfo& currentScreen = out_Screens.ExpandAndGetRef();
  currentScreen.m_sDisplayName = "Default Display";
  currentScreen.m_iOffsetX = 0;
  currentScreen.m_iOffsetY = 0;
  currentScreen.m_iResolutionX = 1920;
  currentScreen.m_iResolutionY = 1080;
  currentScreen.m_bIsPrimary = true;
  currentScreen.m_fContentScale = 1.0f;
  return EZ_SUCCESS;
}

#  endif
#endif
