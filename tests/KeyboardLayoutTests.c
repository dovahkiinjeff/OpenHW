#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
typedef unsigned udword;typedef int bool32,SDL_Keycode,SDL_Scancode;
#define TRUE 1
#define FALSE 0
#define KEY_TOTAL_KEYS 256
#define SDL_NUM_SCANCODES 512
enum {SDL_SCANCODE_A=4,SDL_SCANCODE_B,SDL_SCANCODE_C,SDL_SCANCODE_D,SDL_SCANCODE_E,SDL_SCANCODE_F,SDL_SCANCODE_G,SDL_SCANCODE_H,SDL_SCANCODE_I,SDL_SCANCODE_J,SDL_SCANCODE_K,SDL_SCANCODE_L,SDL_SCANCODE_M,SDL_SCANCODE_N,SDL_SCANCODE_O,SDL_SCANCODE_P,SDL_SCANCODE_Q,SDL_SCANCODE_R,SDL_SCANCODE_S,SDL_SCANCODE_T,SDL_SCANCODE_U,SDL_SCANCODE_V,SDL_SCANCODE_W,SDL_SCANCODE_X,SDL_SCANCODE_Y,SDL_SCANCODE_Z};
enum {SDL_SCANCODE_MINUS=45,SDL_SCANCODE_EQUALS,SDL_SCANCODE_LEFTBRACKET,SDL_SCANCODE_RIGHTBRACKET,SDL_SCANCODE_BACKSLASH,SDL_SCANCODE_UNUSED,SDL_SCANCODE_SEMICOLON,SDL_SCANCODE_APOSTROPHE,SDL_SCANCODE_GRAVE,SDL_SCANCODE_COMMA,SDL_SCANCODE_PERIOD,SDL_SCANCODE_SLASH};
static udword strKeyboardLayout;static int regKeysFocussed;
static SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scan){return scan==SDL_SCANCODE_Q?'a':scan==SDL_SCANCODE_A?'q':0;}
#include "../src/SDL/KeyboardLayout.inc"
int main(void)
{
    strKeyboardLayout=0;assert(mainLayoutKey(SDL_SCANCODE_Q)==SDL_SCANCODE_A);
    strKeyboardLayout=1;assert(mainLayoutKey(SDL_SCANCODE_Q)==SDL_SCANCODE_Q);
    strKeyboardLayout=2;assert(mainLayoutKey(SDL_SCANCODE_A)==SDL_SCANCODE_Q);assert(mainLayoutKey(SDL_SCANCODE_SEMICOLON)==SDL_SCANCODE_M);
    assert(mainLayoutKey(30)==30);assert(mainLayoutKey(58)==58);assert(mainLayoutKey(225)==225);
    strKeyboardLayout=3;assert(mainLayoutKey(SDL_SCANCODE_Y)==SDL_SCANCODE_Z);assert(mainLayoutKey(SDL_SCANCODE_Z)==SDL_SCANCODE_Y);
    strKeyboardLayout=6;assert(mainLayoutKey(SDL_SCANCODE_Q)==SDL_SCANCODE_APOSTROPHE);assert(mainLayoutKey(SDL_SCANCODE_E)==SDL_SCANCODE_PERIOD);
    regKeysFocussed=1;assert(mainLayoutKey(SDL_SCANCODE_Q)==SDL_SCANCODE_Q);regKeysFocussed=0;
    strKeyboardLayout=2;assert(mainLayoutEventKey(SDL_SCANCODE_Q,FALSE)==SDL_SCANCODE_A);
    strKeyboardLayout=3;assert(mainLayoutEventKey(SDL_SCANCODE_Q,TRUE)==SDL_SCANCODE_A);
    assert(mainLayoutEventKey(SDL_SCANCODE_Q,FALSE)==SDL_SCANCODE_Q);assert(mainLayoutEventKey(SDL_SCANCODE_Q,TRUE)==SDL_SCANCODE_Q);
    puts("Keyboard layout mapping, number/modifier stability, text entry and key-release pairing passed");return 0;
}
