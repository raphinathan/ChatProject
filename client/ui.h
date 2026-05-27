#ifndef CHAT_UI_H
#define CHAT_UI_H

#include "client_mng.h"

/* Runs the text-mode UI loop: screen 1 (auth) -> screen 2 (groups) on login.
 * Returns when the user chooses exit. */
void ui_run(ClientMng* m);

#endif /* CHAT_UI_H */
