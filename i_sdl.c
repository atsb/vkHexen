#include "h2stdinc.h"
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "h2def.h"
#include "r_local.h"

// Public Data

int DisplayTicker = 0;

boolean useexterndriver;
boolean mousepresent;


// Extern Data

extern int usemouse, usejoystick;

// Private Data

static SDL_Window	*sdl_window 	= NULL;
static SDL_Renderer	*sdl_renderer 	= NULL;
static SDL_Surface	*screen_surface = NULL; /* 8-bit paletted surface */
static SDL_Surface	*buffer_surface = NULL; /* 32-bit intermediate RGBA buffer */
static SDL_Texture	*render_texture = NULL;
static SDL_Rect		blit_rect	= { 0, 0, SCREENWIDTH, SCREENHEIGHT };

static int screenWidth = SCREENWIDTH;
static int screenHeight = SCREENHEIGHT;

static Uint32 sdl_pixel_format;
static boolean vid_initialized = false;
static int grabMouse;

// Vulkan Setup
static VkInstance				vulkan_instance;
static VkPhysicalDevice			vulkan_device;
static VkApplicationInfo		vulkan_application_info;
static VkInstanceCreateInfo		vulkan_instance_creation;
static VkDeviceCreateInfo		vulkan_device_info;
static VkDevice					vulkan_logical_device;
static VkDeviceQueueCreateInfo  vulkan_queue_creation_info;

/*
============================================================================

								USER INPUT

============================================================================
*/

//--------------------------------------------------------------------------
//
// PROC I_WaitVBL
//
//--------------------------------------------------------------------------

void I_WaitVBL(int vbls)
{
	if (!vid_initialized)
	{
		return;
	}
	while (vbls--)
	{
		SDL_Delay (16667 / 1000);
	}
}

//--------------------------------------------------------------------------
//
// PROC I_SetPalette
//
// Palette source must use 8 bit RGB elements.
//
//--------------------------------------------------------------------------

void I_SetPalette(byte *palette)
{
	if ( !vid_initialized )
		return;

	SDL_Color colormap[256];
	
	for ( int i = 0; i < 256; i++ )
	{
		colormap[i].r = gammatable[usegamma][*palette++];
		colormap[i].g = gammatable[usegamma][*palette++];
		colormap[i].b = gammatable[usegamma][*palette++];
	}
	
	// TODO: VULKANISE!
	//SDL_SetPaletteColors(screen_surface->format->palette, colormap, 0, 256);
}

/*
============================================================================

							GRAPHICS MODE

============================================================================
*/

/*
==============
=
= I_Update
=
==============
*/

int UpdateState;

void I_Update (void)
{
	if (!vid_initialized)
		return;

	if (UpdateState == I_NOUPDATE)
		return;

	/*
	 * Blit from paletted 8-bit RGB surface to the intermediate 32-bit 
	 * RGBA surface 
	 */
	SDL_BlitSurface(screen_surface, &blit_rect, buffer_surface, &blit_rect);

	// TODO: VULKANISE!
	/* Update SDL texture with the contents of our 32-bit RGBA buffer */
	//SDL_UpdateTexture (render_texture, NULL, buffer_surface->pixels,
		//buffer_surface->pitch);

	SDL_RenderClear (sdl_renderer);
	SDL_RenderCopy (sdl_renderer, render_texture, NULL, NULL);
	
	SDL_RenderPresent (sdl_renderer);
	UpdateState = I_NOUPDATE;
}

//--------------------------------------------------------------------------
//
// PROC I_InitGraphics
//
//--------------------------------------------------------------------------

void I_InitGraphics(void)
{
	char text[20];
	int p, bpp;
	Uint32 flags = SDL_WINDOW_VULKAN;
	Uint32 rmask, gmask, bmask, amask;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		I_Error("Couldn't init video: %s", SDL_GetError());
	}
	else {
		ST_Message("SDL2: Initialising SubSystem\n");
	}

	// Needs some work to get screenHeight and screenWidth working - S.A.
	sdl_window = SDL_CreateWindow(text, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, flags);

	if (sdl_window == NULL)
	{
		I_Error("Could not initialise SDL2: %s\n", SDL_GetError());
	}
	else {
		ST_Message("SDL2: Initialised\n");
	}

	// Create the Vulkan instance
	ST_Message("Vulkan: Creating Instance\n");
	vulkan_application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vulkan_application_info.pNext = NULL;
	char app_name[VK_MAX_EXTENSION_NAME_SIZE];
	strcpy(app_name, "vkHexen");
	vulkan_application_info.pApplicationName = app_name;
	vulkan_application_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	char app_engine_name[VK_MAX_EXTENSION_NAME_SIZE];
	strcpy(app_engine_name, "vkHexen");
	vulkan_application_info.pEngineName = app_engine_name;
	vulkan_application_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	vulkan_application_info.apiVersion = VK_API_VERSION_1_0;

	vulkan_instance_creation.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vulkan_instance_creation.pNext = NULL;
	vulkan_instance_creation.flags = 0;
	vulkan_instance_creation.pApplicationInfo = &vulkan_application_info;
	vulkan_instance_creation.enabledLayerCount = 0;
	vulkan_instance_creation.ppEnabledLayerNames = NULL;

	const char* extensionNames[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	vulkan_instance_creation.enabledExtensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);
	vulkan_instance_creation.ppEnabledExtensionNames = extensionNames;

	if (vkCreateInstance(&vulkan_instance_creation, NULL, &vulkan_instance) != VK_SUCCESS) {
		I_Error(stderr, "Failed to create Vulkan instance\n");
		exit(EXIT_FAILURE);
	}
	else {
		ST_Message("Vulkan: Created Instance\n");
	}

	// Create the Vulkan device
	ST_Message("Vulkan: Creating Device\n");
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vulkan_instance, &deviceCount, NULL);

	if (deviceCount == 0) {
		fprintf(stderr, "Vulkan: Compatible GPU not found\n");
		return EXIT_FAILURE;
	}
	else {
		ST_Message("Vulkan: Compatible GPU found\n");
	}

	VkPhysicalDevice* physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(vulkan_instance, &deviceCount, physicalDevices);

	VkPhysicalDevice selectedDevice = physicalDevices[0];

	// Query queue family properties
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, queueFamilies);

	uint32_t queueFamilyIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queueFamilyIndex = i;
			break;
		}
	}

	if (queueFamilyIndex == UINT32_MAX) {
		fprintf(stderr, "Vulkan: No suitable queue family found\n");
		return EXIT_FAILURE;
	}
	else {
		ST_Message("Vulkan: Queue Found\n");
	}

	vulkan_queue_creation_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	vulkan_queue_creation_info.queueFamilyIndex = queueFamilyIndex;
	vulkan_queue_creation_info.queueCount = 1;

	float queuePriorities[] = { 1.0f };

	vulkan_queue_creation_info.pQueuePriorities = queuePriorities;

	vulkan_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vulkan_device_info.pQueueCreateInfos = &vulkan_queue_creation_info;
	vulkan_device_info.queueCreateInfoCount = 1;

	VkDevice vulkan_device;

	if (vkCreateDevice(selectedDevice, &vulkan_device_info, NULL, &vulkan_device) != VK_SUCCESS) {
		fprintf(stderr, "Vulkan: Failed to create Vulkan device\n");
		return EXIT_FAILURE;
	}
	else {
		ST_Message("Vulkan: Created Device\n");
	}

	// Create a Vulkan surface from the SDL window
	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(sdl_window, vulkan_instance, &surface))
	{
		I_Error("Vulkan: Could not create Vulkan Surface from SDL2: %s\n", SDL_GetError());
	}
	else {
		ST_Message("Vulkan: Surface created\n");
	}

	// Initialize other Vulkan-related objects here

	vid_initialized = true;

	// Only grab if we want to
	if (!M_CheckParm("--nograb") && !M_CheckParm("-g"))
	{
		grabMouse = 1;
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	// We like to be clean
	free(physicalDevices);
	free(queueFamilies);

	SDL_ShowCursor(SDL_DISABLE);

	// TODO: VULKANISE!
	//screen = (byte*)screen_surface->pixels;

	I_SetPalette((byte*)W_CacheLumpName("PLAYPAL", PU_CACHE));
}

//--------------------------------------------------------------------------
//
// PROC I_ShutdownGraphics
//
//--------------------------------------------------------------------------

void I_ShutdownGraphics(void)
{
	if (!vid_initialized)
		return;

	if (screen_surface != NULL)
		SDL_FreeSurface (screen_surface);

	if (buffer_surface != NULL)
		SDL_FreeSurface (buffer_surface);

	if (render_texture != NULL)
		SDL_DestroyTexture (render_texture);

	if (sdl_renderer != NULL)
		SDL_DestroyRenderer (sdl_renderer);

	if (sdl_window != NULL)
		SDL_DestroyWindow (sdl_window);

	vid_initialized = false;
}

//===========================================================================

//
//  Translates the key
//
static int xlatekey (SDL_Keysym *key)
{
	switch (key->sym)
	{
	// S.A.
	case SDLK_LEFTBRACKET:	return KEY_LEFTBRACKET;
	case SDLK_RIGHTBRACKET:	return KEY_RIGHTBRACKET;
	case SDLK_BACKQUOTE:	return KEY_BACKQUOTE;
	case SDLK_QUOTEDBL:	return KEY_QUOTEDBL;
	case SDLK_QUOTE:	return KEY_QUOTE;
	case SDLK_SEMICOLON:	return KEY_SEMICOLON;
	case SDLK_PERIOD:	return KEY_PERIOD;
	case SDLK_COMMA:	return KEY_COMMA;
	case SDLK_SLASH:	return KEY_SLASH;
	case SDLK_BACKSLASH:	return KEY_BACKSLASH;

	case SDLK_LEFT:		return KEY_LEFTARROW;
	case SDLK_RIGHT:	return KEY_RIGHTARROW;
	case SDLK_DOWN:		return KEY_DOWNARROW;
	case SDLK_UP:		return KEY_UPARROW;
	case SDLK_ESCAPE:	return KEY_ESCAPE;
	case SDLK_RETURN:	return KEY_ENTER;

	case SDLK_F1:		return KEY_F1;
	case SDLK_F2:		return KEY_F2;
	case SDLK_F3:		return KEY_F3;
	case SDLK_F4:		return KEY_F4;
	case SDLK_F5:		return KEY_F5;
	case SDLK_F6:		return KEY_F6;
	case SDLK_F7:		return KEY_F7;
	case SDLK_F8:		return KEY_F8;
	case SDLK_F9:		return KEY_F9;
	case SDLK_F10:		return KEY_F10;
	case SDLK_F11:		return KEY_F11;
	case SDLK_F12:		return KEY_F12;

	case SDLK_INSERT:	return KEY_INS;
	case SDLK_DELETE:	return KEY_DEL;
	case SDLK_PAGEUP:	return KEY_PGUP;
	case SDLK_PAGEDOWN:	return KEY_PGDN;
	case SDLK_HOME:		return KEY_HOME;
	case SDLK_END:		return KEY_END;
	case SDLK_BACKSPACE:	return KEY_BACKSPACE;
	case SDLK_PAUSE:	return KEY_PAUSE;
	case SDLK_EQUALS:	return KEY_EQUALS;
	case SDLK_MINUS:	return KEY_MINUS;

	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		return KEY_RSHIFT;

	case SDLK_LCTRL:
	case SDLK_RCTRL:
		return KEY_RCTRL;

	case SDLK_LALT:
		return KEY_LALT;
	case SDLK_RALT:
		return KEY_RALT;

	case SDLK_KP_0:
		if (key->mod & KMOD_NUM)
			return SDLK_0;
		else
			return KEY_INS;
	case SDLK_KP_1:
		if (key->mod & KMOD_NUM)
			return SDLK_1;
		else
			return KEY_END;
	case SDLK_KP_2:
		if (key->mod & KMOD_NUM)
			return SDLK_2;
		else
			return KEY_DOWNARROW;
	case SDLK_KP_3:
		if (key->mod & KMOD_NUM)
			return SDLK_3;
		else
			return KEY_PGDN;
	case SDLK_KP_4:
		if (key->mod & KMOD_NUM)
			return SDLK_4;
		else
			return KEY_LEFTARROW;
	case SDLK_KP_5:
		return SDLK_5;
	case SDLK_KP_6:
		if (key->mod & KMOD_NUM)
			return SDLK_6;
		else
			return KEY_RIGHTARROW;
	case SDLK_KP_7:
		if (key->mod & KMOD_NUM)
			return SDLK_7;
		else
			return KEY_HOME;
	case SDLK_KP_8:
		if (key->mod & KMOD_NUM)
			return SDLK_8;
		else
			return KEY_UPARROW;
	case SDLK_KP_9:
		if (key->mod & KMOD_NUM)
			return SDLK_9;
		else
			return KEY_PGUP;

	case SDLK_KP_PERIOD:
		if (key->mod & KMOD_NUM)
			return SDLK_PERIOD;
		else
			return KEY_DEL;
	case SDLK_KP_DIVIDE:	return SDLK_SLASH;
	case SDLK_KP_MULTIPLY:	return SDLK_ASTERISK;
	case SDLK_KP_MINUS:	return KEY_MINUS;
	case SDLK_KP_PLUS:	return SDLK_PLUS;
	case SDLK_KP_ENTER:	return KEY_ENTER;
	case SDLK_KP_EQUALS:	return KEY_EQUALS;

	default:
		return key->sym;
	}
}


/* Shamelessly stolen from PrBoom+ */
static int I_SDLtoHexenMouseState(Uint8 buttonstate)
{
	return 0
		| ((buttonstate & SDL_BUTTON(1)) ? 1 : 0)
		| ((buttonstate & SDL_BUTTON(2)) ? 2 : 0)
		| ((buttonstate & SDL_BUTTON(3)) ? 4 : 0);
}

/* This processes SDL events */
void I_GetEvent(SDL_Event *Event)
{
	event_t event;
	SDL_Keymod mod;

	switch (Event->type)
	{
	case SDL_KEYDOWN:
		mod = SDL_GetModState ();
		if (mod & (KMOD_RCTRL|KMOD_LCTRL))
		{
			if (Event->key.keysym.sym == 'g')
			{
				if (SDL_GetRelativeMouseMode () == SDL_FALSE)
				{
					grabMouse = 1;
					SDL_SetRelativeMouseMode (SDL_TRUE);
				}
				else
				{
					grabMouse = 0;
					SDL_SetRelativeMouseMode (SDL_FALSE);
				}
				break;
			}
		}
		else if (mod & (KMOD_RALT|KMOD_LALT))
		{
			if (Event->key.keysym.sym == SDLK_RETURN)
			{
				SDL_SetWindowFullscreen(sdl_window, 
					SDL_WINDOW_FULLSCREEN_DESKTOP);
				break;
			}
		}
		event.type = ev_keydown;
		event.data1 = xlatekey(&Event->key.keysym);
		H2_PostEvent(&event);
		break;

	case SDL_KEYUP:
		event.type = ev_keyup;
		event.data1 = xlatekey(&Event->key.keysym);
		H2_PostEvent(&event);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		event.type = ev_mouse;
		event.data1 = I_SDLtoHexenMouseState(SDL_GetMouseState(NULL,NULL));
		event.data2 = event.data3 = 0;
		H2_PostEvent(&event);
		break;

	case SDL_MOUSEMOTION:
		/* Ignore mouse warp events */
		if ((Event->motion.x != SCREENWIDTH/2) ||
		    (Event->motion.y != SCREENHEIGHT/2) )
		{
			/* Warp the mouse back to the center */
			/*
			if (grabMouse) {
				SDL_WarpMouse(SCREENWIDTH/2, SCREENHEIGHT/2);
			}
			*/
			event.type = ev_mouse;
			event.data1 = I_SDLtoHexenMouseState(Event->motion.state);
			event.data2 = Event->motion.xrel << 3;
			event.data3 = -Event->motion.yrel << 3;
			H2_PostEvent(&event);
		}
		break;

	case SDL_QUIT:
		I_Quit();
		break;

	default:
		break;
	}
}

//
// I_StartTic
//
void I_StartTic (void)
{
	SDL_Event Event;
	while ( SDL_PollEvent(&Event) )
		I_GetEvent(&Event);
}


/*
============================================================================

							MOUSE

============================================================================
*/


/*
================
=
= StartupMouse
=
================
*/

void I_StartupMouse (void)
{
	mousepresent = 1;
}
