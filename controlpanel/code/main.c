#include <core.h>
#include <config.h>
#include <server.h>
#include <event.h>

#include "http.h"

CFGSTORE* cplStore;

void onStop(void* ptr) {
	if(cplStore) Config_Save(cplStore);
}

EXP int Plugin_ApiVer = 100;
EXP bool Plugin_Load() {
	cplStore = Config_Create("cp.cfg");
	if(!cplStore || !mainCfg) return false;

	Config_SetBool(cplStore, "enabled", true);
	Config_SetInt(cplStore, "port", 8080);
	Config_Load(cplStore);

	Event_RegisterVoid(EVT_ONSTOP, onStop);
	const char* ip = Config_GetStr(mainCfg, "ip");
	ushort port = Config_GetInt(cplStore, "port");

	if(Http_StartServer(ip, port))
		Log_Info("CPL http server started on %s:%d", ip, port);
	else
		return false;

	return true;
}

EXP bool Plugin_Unload() {
	Event_Unregister(EVT_ONSTOP, onStop);
	Http_CloseServer();
	if(cplStore) {
		Config_Save(cplStore);
		Config_EmptyStore(cplStore);
		Memory_Free(cplStore);
	}
	return true;
}
