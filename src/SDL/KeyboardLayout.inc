/* Convert physical SDL keys into the selected shortcut layout. Do not change
   Windows' active layout, text input, scan-code storage or numeric/function rows. */
static udword mainAsciiShortcut(SDL_Keycode symbol,udword fallback)
{
    if(symbol>='a' && symbol<='z')return SDL_SCANCODE_A+symbol-'a';
    switch(symbol) {
        case '-':return SDL_SCANCODE_MINUS;case '=':return SDL_SCANCODE_EQUALS;
        case '[':return SDL_SCANCODE_LEFTBRACKET;case ']':return SDL_SCANCODE_RIGHTBRACKET;
        case ';':return SDL_SCANCODE_SEMICOLON;case 39:return SDL_SCANCODE_APOSTROPHE;
        case ',':return SDL_SCANCODE_COMMA;case '.':return SDL_SCANCODE_PERIOD;
        case '/':return SDL_SCANCODE_SLASH;case 92:return SDL_SCANCODE_BACKSLASH;
        case '`':return SDL_SCANCODE_GRAVE;
    }
    return fallback;
}
static udword mainLayoutKey(udword scan)
{
    if(scan>=KEY_TOTAL_KEYS || strKeyboardLayout==1 || regKeysFocussed)return scan;
    /* Number rows, function keys, keypad and modifiers retain their physical
       identity. This also keeps control groups usable on AZERTY keyboards. */
    if((scan<SDL_SCANCODE_A || scan>SDL_SCANCODE_Z) &&
       (scan<SDL_SCANCODE_MINUS || scan>SDL_SCANCODE_SLASH))return scan;
    if(strKeyboardLayout==0 || strKeyboardLayout>6)
        return mainAsciiShortcut(SDL_GetKeyFromScancode((SDL_Scancode)scan),scan);
    if(strKeyboardLayout==2) switch(scan) {
        case SDL_SCANCODE_A:return SDL_SCANCODE_Q;case SDL_SCANCODE_Q:return SDL_SCANCODE_A;
        case SDL_SCANCODE_W:return SDL_SCANCODE_Z;case SDL_SCANCODE_Z:return SDL_SCANCODE_W;
        case SDL_SCANCODE_M:return SDL_SCANCODE_COMMA;case SDL_SCANCODE_SEMICOLON:return SDL_SCANCODE_M;
        case SDL_SCANCODE_COMMA:return SDL_SCANCODE_SEMICOLON;
    }
    if(strKeyboardLayout==3) {
        if(scan==SDL_SCANCODE_Y)return SDL_SCANCODE_Z;
        if(scan==SDL_SCANCODE_Z)return SDL_SCANCODE_Y;
    }
    if(strKeyboardLayout==6) {
        static const char letters[]="axje.uidchtnmbrl'poygk,qf;";
        if(scan>=SDL_SCANCODE_A && scan<=SDL_SCANCODE_Z)return mainAsciiShortcut(letters[scan-SDL_SCANCODE_A],scan);
        if(scan==SDL_SCANCODE_SEMICOLON)return SDL_SCANCODE_S;
        if(scan==SDL_SCANCODE_COMMA)return SDL_SCANCODE_W;
        if(scan==SDL_SCANCODE_PERIOD)return SDL_SCANCODE_V;
        if(scan==SDL_SCANCODE_SLASH)return SDL_SCANCODE_Z;
    }
    return scan;
}

static udword mainLayoutEventKey(udword scan, bool32 release)
{
    static udword pressed[SDL_NUM_SCANCODES];
    udword translated;
    if (scan >= SDL_NUM_SCANCODES) return 0;
    if (release) { translated=pressed[scan]; pressed[scan]=0; return translated ? translated-1 : mainLayoutKey(scan); }
    if (!pressed[scan]) pressed[scan]=mainLayoutKey(scan)+1;
    return pressed[scan]-1;
}
