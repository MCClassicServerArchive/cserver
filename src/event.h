#ifndef EVENT_H
#define EVENT_H
#include "core.h"
#include "types/event.h"

#define Event_DeclareBunch(N) static EventRegBunch N[] =
#define EVENT_BUNCH_ADD(R, E, F) {R, E, (void*)F},
#define EVENT_BUNCH_END {0, 0, NULL}

API cs_bool Event_RegisterVoid(EventTypes type, evtVoidCallback func);
API cs_bool Event_RegisterBool(EventTypes type, evtBoolCallback func);
API cs_bool Event_RegisterBunch(EventRegBunch *bunch);
API cs_bool Event_Unregister(EventTypes type, cs_uintptr evtFuncPtr);
API cs_bool Event_UnregisterBunch(EventRegBunch *bunch);

NOINL void Event_UnregisterAll(void);
NOINL cs_bool Event_Call(EventTypes type, void *param);
#endif // EVENT_H
