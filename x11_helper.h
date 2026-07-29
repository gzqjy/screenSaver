#ifndef X11_HELPER_H
#define X11_HELPER_H

#ifdef __linux__
void reparentToLockScreen(unsigned long myWinId);
void reparentToRoot(unsigned long myWinId);
#endif

#endif // X11_HELPER_H
