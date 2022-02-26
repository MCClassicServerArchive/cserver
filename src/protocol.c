#include "core.h"
#include "str.h"
#include "log.h"
#include "block.h"
#include "client.h"
#include "event.h"
#include "server.h"
#include "growingbuffer.h"
#include "protocol.h"
#include "platform.h"
#include "command.h"
#include "heartbeat.h"
#include "strstor.h"
#include "compr.h"
#include "world.h"
#include "config.h"

static cs_uint16 extensionsCount = 0;
static CPEExt *headExtension = NULL;

void Proto_WriteString(cs_char **dataptr, cs_str string) {
	cs_size size = 0;
	if(string)
		size = min(String_Length(string), 64);
	for(cs_byte i = 0; i < 64; i++)
		*(*dataptr)++ = i < size ? string[i] : ' ';
}

void Proto_WriteFlVec(cs_char **dataptr, const Vec *vec) {
	cs_char *data = *dataptr;
	*(cs_int32 *)data = htonl((cs_uint32)(vec->x * 32)); data += 4;
	*(cs_int32 *)data = htonl((cs_uint32)(vec->y * 32)); data += 4;
	*(cs_int32 *)data = htonl((cs_uint32)(vec->z * 32)); data += 4;
	*dataptr = data;
}

void Proto_WriteFlSVec(cs_char **dataptr, const Vec *vec) {
	cs_char *data = *dataptr;
	*(cs_int16 *)data = htons((cs_uint16)(vec->x * 32)); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)(vec->y * 32)); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)(vec->z * 32)); data += 2;
	*dataptr = data;
}

void Proto_WriteSVec(cs_char **dataptr, const SVec *vec) {
	cs_char *data = *dataptr;
	*(cs_int16 *)data = htons((cs_uint16)vec->x); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)vec->y); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)vec->z); data += 2;
	*dataptr = data;
}

void Proto_WriteAng(cs_char **dataptr, const Ang *ang) {
	cs_char *data = *dataptr;
	*(cs_byte *)data++ = (cs_byte)((ang->yaw / 360) * 256);
	*(cs_byte *)data++ = (cs_byte)((ang->pitch / 360) * 256);
	*dataptr = data;
}

void Proto_WriteColor3(cs_char **dataptr, const Color3* color) {
	cs_char *data = *dataptr;
	*(cs_int16 *)data = htons((cs_uint16)color->r); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)color->g); data += 2;
	*(cs_int16 *)data = htons((cs_uint16)color->b); data += 2;
	*dataptr = data;
}

void Proto_WriteColor4(cs_char **dataptr, const Color4* color) {
	cs_char *data = *dataptr;
	*(cs_int16 *)data = (cs_int16)htons(color->r); data += 2;
	*(cs_int16 *)data = (cs_int16)htons(color->g); data += 2;
	*(cs_int16 *)data = (cs_int16)htons(color->b); data += 2;
	*(cs_int16 *)data = (cs_int16)htons(color->a); data += 2;
	*dataptr = data;
}

void Proto_WriteByteColor3(cs_char **dataptr, const Color3* color) {
	cs_char *data = *dataptr;
	*data++ = (cs_int8)color->r;
	*data++ = (cs_int8)color->g;
	*data++ = (cs_int8)color->b;
	*dataptr = data;
}

void Proto_WriteByteColor4(cs_char **dataptr, const Color4* color) {
	cs_char *data = *dataptr;
	*data++ = (cs_char)color->r;
	*data++ = (cs_char)color->g;
	*data++ = (cs_char)color->b;
	*data++ = (cs_char)color->a;
	*dataptr = data;
}

NOINL static void WriteExtEntityPos(cs_char **dataptr, Vec *vec, Ang *ang, cs_bool extended) {
	if(extended)
		Proto_WriteFlVec(dataptr, vec);
	else
		Proto_WriteFlSVec(dataptr, vec);
	Proto_WriteAng(dataptr, ang);
}

cs_byte Proto_ReadString(cs_char **dataptr, cs_str *dstptr) {
	cs_str data = *dataptr;
	*dataptr += 64;
	cs_byte end;

	for(end = 64; end > 0; end--)
		if(data[end - 1] != ' ') break;

	if(end > 0) {
		cs_char *str = Memory_Alloc(end + 1, 1);
		Memory_Copy(str, data, end);
		str[end] = '\0';
		if(*dstptr) Memory_Free((void *)*dstptr);
		*dstptr = str;
	};

	return end;
}

cs_byte Proto_ReadStringNoAlloc(cs_char **dataptr, cs_char *dst) {
	cs_str data = *dataptr;
	*dataptr += 64;
	cs_byte end;

	for(end = 64; end > 0; end--)
		if(data[end - 1] != ' ') break;

	if(end > 0) {
		Memory_Copy(dst, data, end);
		dst[end] = '\0';
	}

	return end;
}

void Proto_ReadSVec(cs_char **dataptr, SVec *vec) {
	cs_char *data = *dataptr;
	vec->x = ntohs(*(cs_int16 *)data); data += 2;
	vec->y = ntohs(*(cs_int16 *)data); data += 2;
	vec->z = ntohs(*(cs_int16 *)data); data += 2;
	*dataptr = data;
}

void Proto_ReadAng(cs_char **dataptr, Ang *ang) {
	cs_char *data = *dataptr;
	ang->yaw = (((cs_float)*data++) / 256) * 360;
	ang->pitch = (((cs_float)*data++) / 256) * 360;
	*dataptr = data;
}

void Proto_ReadFlSVec(cs_char **dataptr, Vec *vec) {
	cs_char *data = *dataptr;
	vec->x = (cs_float)ntohs(*(cs_int16 *)data) / 32; data += 2;
	vec->y = (cs_float)ntohs(*(cs_int16 *)data) / 32; data += 2;
	vec->z = (cs_float)ntohs(*(cs_int16 *)data) / 32; data += 2;
	*dataptr = data;
}

void Proto_ReadFlVec(cs_char **dataptr, Vec *vec) {
	cs_char *data = *dataptr;
	vec->x = (cs_float)ntohl(*(cs_int32 *)data) / 32; data += 4;
	vec->y = (cs_float)ntohl(*(cs_int32 *)data) / 32; data += 4;
	vec->z = (cs_float)ntohl(*(cs_int32 *)data) / 32; data += 4;
	*dataptr = data;
}

void Vanilla_WriteServerIdent(Client *client, cs_str name, cs_str motd) {
	PacketWriter_Start(client, 131);

	*data++ = 0x00;
	*data++ = PROTOCOL_VERSION;
	Proto_WriteString(&data, name);
	Proto_WriteString(&data, motd);
	*data++ = 0x00;

	PacketWriter_EndAnytime(client);
}

void Vanilla_WriteLvlInit(Client *client) {
	PacketWriter_Start(client, 1);

	*data++ = 0x02;

	PacketWriter_EndAnytime(client);
}

void Vanilla_WriteLvlFin(Client *client, SVec *dims) {
	PacketWriter_Start(client, 7);

	*data++ = 0x04;
	Proto_WriteSVec(&data, dims);

	PacketWriter_EndAnytime(client);
}

void Vanilla_WriteSetBlock(Client *client, SVec *pos, BlockID block) {
	PacketWriter_Start(client, 8);

	*data++ = 0x06;
	Proto_WriteSVec(&data, pos);
	*data++ = block;

	PacketWriter_EndIngame(client);
}

void Vanilla_WriteSpawn(Client *client, Client *other) {
	PacketWriter_Start(client, 80);

	*data++ = 0x07;
	*data++ = client == other ? CLIENT_SELF : other->id;
	Proto_WriteString(&data, other->playerData->name);
	WriteExtEntityPos(
		&data,
		&other->playerData->position,
		&other->playerData->angle,
		Client_GetExtVer(client, EXT_ENTPOS) > 0
	);

	PacketWriter_EndIngame(client);
}

void Vanilla_WriteTeleport(Client *client, Vec *pos, Ang *ang) {
	PacketWriter_Start(client, 18);

	*data++ = 0x08;
	*data++ = CLIENT_SELF;
	WriteExtEntityPos(
		&data, pos, ang,
		Client_GetExtVer(client, EXT_ENTPOS) > 0
	);

	PacketWriter_EndIngame(client);
}

void Vanilla_WritePosAndOrient(Client *client, Client *other) {
	PacketWriter_Start(client, 18);

	*data++ = 0x08;
	*data++ = client == other ? CLIENT_SELF : other->id;
	WriteExtEntityPos(
		&data,
		&other->playerData->position,
		&other->playerData->angle,
		Client_GetExtVer(client, EXT_ENTPOS) > 0
	);

	PacketWriter_EndIngame(client);
}

void Vanilla_WriteDespawn(Client *client, Client *other) {
	PacketWriter_Start(client, 2);

	*data++ = 0x0C;
	*data++ = client == other ? CLIENT_SELF : other->id;

	PacketWriter_EndIngame(client);
}

void Vanilla_WriteChat(Client *client, EMesgType type, cs_str mesg) {
	PacketWriter_Start(client, 66);

	cs_char mesg_out[65];
	String_Copy(mesg_out, 65, mesg);

	if(Client_GetExtVer(client, EXT_CP437) != 1) {
		for(cs_int32 i = 0; i < 65; i++) {
			if(mesg_out[i] == '\0') break;
			if(mesg_out[i] < ' ' || mesg_out[i] > '~')
				mesg_out[i] = '?';
		}
	}

	*data++ = 0x0D;
	*data++ = (cs_byte)type;
	Proto_WriteString(&data, mesg_out);

	PacketWriter_EndAnytime(client);
}

void Vanilla_WriteKick(Client *client, cs_str reason) {
	PacketWriter_Start(client, 65);

	*data++ = 0x0E;
	Proto_WriteString(&data, reason);

	PacketWriter_EndAnytime(client);
}

void Vanilla_WriteUserType(Client *client, cs_byte type) {
	PacketWriter_Start(client, 2);

	*data++ = 0x0F;
	*data++ = type;

	PacketWriter_EndIngame(client);
}

static cs_bool FinishHandshake(Client *client) {
	onHandshakeDone evt = {
		.client = client,
		.world = World_Main
	};

	return Event_Call(EVT_ONHANDSHAKEDONE, &evt) &&
	Client_ChangeWorld(client, evt.world);
}

cs_bool Handler_Handshake(Client *client, cs_char *data) {
	if(client->playerData) return false;
	cs_byte protoVer = *data++;
	if(protoVer != PROTOCOL_VERSION) {
		Client_KickFormat(client, Sstor_Get("KICK_PROTOVER"), PROTOCOL_VERSION, protoVer);
		return true;
	}

	client->playerData = Memory_Alloc(1, sizeof(PlayerData));
	client->playerData->firstSpawn = true;
	if(Client_IsLocal(client) && Config_GetBoolByKey(Server_Config, CFG_LOCALOP_KEY))
		client->playerData->isOP = true;

	if(!Proto_ReadStringNoAlloc(&data, client->playerData->name)) return false;
	if(!Proto_ReadStringNoAlloc(&data, client->playerData->key)) return false;
	String_Copy(client->playerData->displayname, 65, client->playerData->name);

	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *other = Clients_List[i];
		if(!other || !other->playerData || other == client) continue;
		if(String_CaselessCompare(client->playerData->name, other->playerData->name)) {
			Client_Kick(client, Sstor_Get("KICK_NAMEINUSE"));
			return true;
		}
	}

	if(Heartbeat_Validate(client)) {
		Client_SetServerIdent(client,
			Config_GetStrByKey(Server_Config, CFG_SERVERNAME_KEY),
			Config_GetStrByKey(Server_Config, CFG_SERVERMOTD_KEY)
		);
	} else {
		Client_Kick(client, Sstor_Get("KICK_AUTHFAIL"));
		return true;
	}

	if(*data == 0x42) {
		client->cpeData = Memory_Alloc(1, sizeof(CPEData));
		client->cpeData->model = 256; // Humanoid model id
		client->cpeData->clickDist = 160; // Default click distance

		CPE_WriteInfo(client);
		CPEExt *ptr = headExtension;
		while(ptr) {
			CPE_WriteExtEntry(client, ptr);
			ptr = ptr->next;
		}
		CPE_CustomBlockSupportLevel(client, 1);
	} else return FinishHandshake(client);

	return true;
}

INL static void UpdateBlock(World *world, SVec *pos, BlockID block) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *client = Clients_List[i];
		if(client && Client_IsInWorld(client, world))
			Client_SetBlock(client, pos, block);
	}
}

cs_bool Handler_SetBlock(Client *client, cs_char *data) {
	ValidateClientState(client, PLAYER_STATE_INGAME, false)

	World *world = Client_GetWorld(client);
	if(!world || !World_IsReadyToPlay(world)) return false;

	onBlockPlace params;
	params.client = client;
	Proto_ReadSVec(&data, &params.pos);
	params.mode = *data++;
	params.id = *data;

	switch(params.mode) {
		case 0x01:
			if(!Block_IsValid(world, params.id)) {
				Client_KickFormat(client, Sstor_Get("KICK_UNKBID"), params.id);
				return false;
			}
			if(Event_Call(EVT_ONBLOCKPLACE, &params) && World_SetBlock(world, &params.pos, params.id))
				UpdateBlock(world, &params.pos, params.id);
			else
				Vanilla_WriteSetBlock(client, &params.pos, World_GetBlock(world, &params.pos));
			break;
		case 0x00:
			params.id = BLOCK_AIR;
			if(Event_Call(EVT_ONBLOCKPLACE, &params) && World_SetBlock(world, &params.pos, params.id))
				UpdateBlock(world, &params.pos, params.id);
			else
				Vanilla_WriteSetBlock(client, &params.pos, World_GetBlock(world, &params.pos));
			break;
	}

	return true;
}

INL static cs_bool ReadClientPos(Client *client, cs_char *data) {
	Vec *vec = &client->playerData->position, newVec;
	Ang *ang = &client->playerData->angle, newAng;
	cs_bool changed = false;

	if(Client_GetExtVer(client, EXT_ENTPOS))
		Proto_ReadFlVec(&data, &newVec);
	else
		Proto_ReadFlSVec(&data, &newVec);

	Proto_ReadAng(&data, &newAng);

	if(newVec.x != vec->x || newVec.y != vec->y || newVec.z != vec->z) {
		client->playerData->position = newVec;
		Event_Call(EVT_ONMOVE, client);
		changed = true;
	}

	if(newAng.yaw != ang->yaw || newAng.pitch != ang->pitch) {
		client->playerData->angle = newAng;
		Event_Call(EVT_ONROTATE, client);
		changed = true;
	}

	return changed;
}

cs_bool Handler_PosAndOrient(Client *client, cs_char *data) {
	ValidateClientState(client, PLAYER_STATE_INGAME, true)

	BlockID cb = *data++;
	if(Client_GetExtVer(client, EXT_HELDBLOCK) == 1) {
		if(client->cpeData->heldBlock != cb) {
			onHeldBlockChange params = {
				.client = client,
				.prev = client->cpeData->heldBlock,
				.curr = cb
			};
			Event_Call(EVT_ONHELDBLOCKCHNG, &params);
			client->cpeData->heldBlock = cb;
		}
	}

	if(ReadClientPos(client, data)) {
		for(ClientID i = 0; i < MAX_CLIENTS; i++) {
			Client *other = Clients_List[i];
			if(other && client != other && Client_CheckState(other, PLAYER_STATE_INGAME) && Client_IsInSameWorld(client, other))
				Vanilla_WritePosAndOrient(other, client);
		}
	}
	return true;
}

cs_bool Handler_Message(Client *client, cs_char *data) {
	ValidateClientState(client, PLAYER_STATE_INGAME, true)

	cs_char message[65],
	*messptr = message;
	cs_byte partial = *data++,
	len = Proto_ReadStringNoAlloc(&data, message);
	if(len == 0) return false;

	for(cs_byte i = 0; i < len; i++) {
		if(message[i] == '%' && ISHEX(message[i + 1]))
			message[i] = '&';
	}

	if(client->cpeData && Client_GetExtVer(client, EXT_LONGMSG)) {
		if(!client->cpeData->message) client->cpeData->message = Memory_Alloc(1, 193);
		if(String_Append(client->cpeData->message, 193, message) && partial == 1) return true;
		messptr = client->cpeData->message;
	}

	onMessage params = {
		.client = client,
		.message = messptr,
		.type = 0
	};

	if(Event_Call(EVT_ONMESSAGE, &params)) {
		cs_char formatted[320];
		String_FormatBuf(formatted, 320, "<%s&f>: %s", Client_GetDisplayName(client), params.message);

		if(*params.message == '/') {
			if(!Command_Handle(params.message, client))
				Vanilla_WriteChat(client, params.type, Sstor_Get("CMD_UNK"));
		} else
			Client_Chat(Broadcast, params.type, formatted);

		Log_Chat(formatted);
	}

	if(messptr != message) *messptr = '\0';

	return true;
}

/*
** Врайтеры и хендлеры
** CPE протокола
*/
#define MODELS_COUNT 15

static cs_str validModelNames[MODELS_COUNT] = {
	"humanoid",
	"chicken",
	"creeper",
	"pig",
	"sheep",
	"skeleton",
	"sheep",
	"sheep_nofur",
	"skeleton",
	"spider",
	"zombie",
	"head",
	"sit",
	"chibi",
	NULL
};

cs_bool CPE_CheckModel(Client *client, cs_int16 model) {
	World *world = Client_GetWorld(client);
	if(world && model < 256)
		return Block_IsValid(world, (BlockID)model);
	return model - 256 < MODELS_COUNT;
}

cs_int16 CPE_GetModelNum(cs_str model) {
	cs_int16 modelnum = -1;
	for(cs_int16 i = 0; validModelNames[i]; i++) {
		cs_str cmdl = validModelNames[i];
		if(String_CaselessCompare(model, cmdl)) {
			modelnum = i + 256;
			break;
		}
	}

	if(modelnum == -1) {
		if(ISNUM(*model)) {
			cs_int32 tmp = String_ToInt(model);
			if(tmp >= 0 && tmp < 256)
				modelnum = (cs_int16)tmp;
		} else
			modelnum = 256;
	}

	return modelnum;
}

cs_uint32 CPE_GetModelStr(cs_int16 num, char *buffer, cs_uint32 buflen) {
	if(num > 255) {
		cs_str mdl = validModelNames[num % 256];
		return mdl ? (cs_uint32)String_Copy(buffer, buflen, mdl) : 0;
	}
	cs_int32 ret = String_FormatBuf(buffer, buflen, "%d", num);
	return max(0, ret);
}

void CPE_WriteInfo(Client *client) {
	PacketWriter_Start(client, 67);

	*data++ = 0x10;
	Proto_WriteString(&data, SOFTWARE_FULLNAME);
	*(cs_uint16 *)data = htons(extensionsCount);
	data += 2;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteExtEntry(Client *client, CPEExt *ext) {
	PacketWriter_Start(client, 69);

	*data++ = 0x11;
	Proto_WriteString(&data, ext->name);
	*(cs_uint32 *)data = htonl(ext->version);
	data += 4;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteClickDistance(Client *client, cs_uint16 dist) {
	PacketWriter_Start(client, 3);

	*data++ = 0x12;
	*(cs_uint16 *)data = htons(dist);
	data += 2;

	PacketWriter_EndIngame(client);
}

void CPE_CustomBlockSupportLevel(Client *client, cs_byte level) {
	PacketWriter_Start(client, 2);

	*data++ = 0x13;
	*data++ = level;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteHoldThis(Client *client, BlockID block, cs_bool preventChange) {
	PacketWriter_Start(client, 3);

	*data++ = 0x14;
	*data++ = block;
	*data++ = (cs_char)preventChange;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetHotKey(Client *client, cs_str action, cs_int32 keycode, cs_int8 keymod) {
	PacketWriter_Start(client, 134);

	*data++ = 0x15;
	Proto_WriteString(&data, NULL); // Label
	Proto_WriteString(&data, action);
	*(cs_int32 *)data = htonl(keycode); data += 4;
	*data++ = keymod;

	PacketWriter_EndIngame(client);
}

void CPE_WriteAddName(Client *client, Client *other) {
	PacketWriter_Start(client, 196);

	*data = 0x16; data += 2;
	*data++ = client == other ? CLIENT_SELF : other->id;
	Proto_WriteString(&data, Client_GetName(other));
	Proto_WriteString(&data, Client_GetDisplayName(other));
	CGroup *group = Client_GetGroup(other);
	Proto_WriteString(&data, group->name);
	*data++ = group->rank;

	PacketWriter_EndIngame(client);
}

void CPE_WriteAddEntity2(Client *client, Client *other) {
	PacketWriter_Start(client, 144);

	*data++ = 0x21;
	*data++ = client == other ? CLIENT_SELF : other->id;
	if(other->cpeData && other->cpeData->hideDisplayName)
		Proto_WriteString(&data, NULL);
	else
		Proto_WriteString(&data, Client_GetName(other));
	Proto_WriteString(&data, Client_GetSkin(other));
	WriteExtEntityPos(
		&data,
		&other->playerData->position,
		&other->playerData->angle,
		Client_GetExtVer(client, EXT_ENTPOS) > 0
	);

	PacketWriter_EndIngame(client);
}

void CPE_WriteRemoveName(Client *client, Client *other) {
	PacketWriter_Start(client, 3);

	*data = 0x18; data += 2;
	*data++ = client == other ? CLIENT_SELF : other->id;

	PacketWriter_EndIngame(client);
}

void CPE_WriteEnvColor(Client *client, cs_byte type, Color3* col) {
	PacketWriter_Start(client, 8);

	*data++ = 0x19;
	*data++ = type;
	Proto_WriteColor3(&data, col);

	PacketWriter_EndIngame(client);
}

void CPE_WriteMakeSelection(Client *client, cs_byte id, SVec *start, SVec *end, Color4* color) {
	PacketWriter_Start(client, 86);

	*data++ = 0x1A;
	*data++ = id;
	Proto_WriteString(&data, NULL); // Label
	Proto_WriteSVec(&data, start);
	Proto_WriteSVec(&data, end);
	Proto_WriteColor4(&data, color);

	PacketWriter_EndIngame(client);
}

void CPE_WriteRemoveSelection(Client *client, cs_byte id) {
	PacketWriter_Start(client, 2);

	*data++ = 0x1B;
	*data++ = id;

	PacketWriter_EndIngame(client);
}

void CPE_WriteBlockPerm(Client *client, BlockID id, cs_bool allowPlace, cs_bool allowDestroy) {
	PacketWriter_Start(client, 4);

	*data++ = 0x1C;
	*data++ = id;
	*data++ = (cs_char)allowPlace;
	*data++ = (cs_char)allowDestroy;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetModel(Client *client, Client *other) {
	PacketWriter_Start(client, 66);

	*data++ = 0x1D;
	*data++ = client == other ? CLIENT_SELF : other->id;

	cs_char model[64] = {0};
	if(CPE_GetModelStr(Client_GetModel(other), model, 64))
		Proto_WriteString(&data, model);
	else
		Proto_WriteString(&data, validModelNames[0]);

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetMapAppearanceV1(Client *client, cs_str tex, cs_byte side, cs_byte edge, cs_int16 sidelvl) {
	PacketWriter_Start(client, 69);

	*data++ = 0x1E;
	Proto_WriteString(&data, tex);
	*data++ = side;
	*data++ = edge;
	*(cs_uint16 *)data = htons(sidelvl);
	data += 2;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetMapAppearanceV2(Client *client, cs_str tex, cs_byte side, cs_byte edge, cs_int16 sidelvl, cs_int16 cllvl, cs_int16 maxview) {
	PacketWriter_Start(client, 73);

	*data++ = 0x1E;
	Proto_WriteString(&data, tex);
	*data++ = side;
	*data++ = edge;
	*(cs_uint16 *)data = htons(sidelvl); data += 2;
	*(cs_uint16 *)data = htons(cllvl); data += 2;
	*(cs_uint16 *)data = htons(maxview); data += 2;

	PacketWriter_EndIngame(client);
}

void CPE_WriteWeatherType(Client *client, cs_int8 type) {
	PacketWriter_Start(client, 2);

	*data++ = 0x1F;
	*data++ = type;

	PacketWriter_EndIngame(client);
}

void CPE_WriteHackControl(Client *client, CPEHacks *hacks) {
	PacketWriter_Start(client, 8);

	*data++ = 0x20;
	*data++ = (cs_char)hacks->flying;
	*data++ = (cs_char)hacks->noclip;
	*data++ = (cs_char)hacks->speeding;
	*data++ = (cs_char)hacks->spawnControl;
	*data++ = (cs_char)hacks->tpv;
	*(cs_int16 *)data = htons(hacks->jumpHeight);
	data += 2;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteDefineBlock(Client *client, BlockDef *block) {
	PacketWriter_Start(client, 80);

	*data++ = 0x23;
	*data++ = block->id;
	Proto_WriteString(&data, block->name);
	*(struct _BlockParams *)data = block->params.nonext;
	data += sizeof(block->params.nonext);

	PacketWriter_EndAnytime(client);
}

void CPE_WriteUndefineBlock(Client *client, BlockID id) {
	PacketWriter_Start(client, 2);

	*data++ = 0x24;
	*data++ = id;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteDefineExBlock(Client *client, BlockDef *block) {
	PacketWriter_Start(client, 88);

	*data++ = 0x25;
	*data++ = block->id;
	Proto_WriteString(&data, block->name);
	*(struct _BlockParamsExt *)data = block->params.ext;
	data += sizeof(block->params.ext);

	PacketWriter_EndAnytime(client);
}

void CPE_WriteBulkBlockUpdate(Client *client, BulkBlockUpdate *bbu) {
	PacketWriter_Start(client, 1282);

	*data++ = 0x26;
	*(struct _BBUData *)data = bbu->data;
	data += sizeof(bbu->data);

	PacketWriter_EndIngame(client);
}

void CPE_WriteFastMapInit(Client *client, cs_uint32 size) {
	PacketWriter_Start(client, 5);

	*data++ = 0x02;
	*(cs_uint32 *)data = htonl(size);
	data += 4;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteAddTextColor(Client *client, Color4* color, cs_char code) {
	PacketWriter_Start(client, 6);

	*data++ = 0x27;
	Proto_WriteByteColor4(&data, color);
	*data++ = code;

	PacketWriter_EndIngame(client);
}

void CPE_WriteTexturePack(Client *client, cs_str url) {
	PacketWriter_Start(client, 65);

	*data++ = 0x28;
	Proto_WriteString(&data, url);

	PacketWriter_EndIngame(client);
}

void CPE_WriteMapProperty(Client *client, cs_byte property, cs_int32 value) {
	PacketWriter_Start(client, 6);

	*data++ = 0x29;
	*data++ = property;
	*(cs_int32 *)data = htonl(value);
	data += 4;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetEntityProperty(Client *client, Client *other, EEntProp type, cs_int32 value) {
	PacketWriter_Start(client, 7);

	*data++ = 0x2A;
	*data++ = client == other ? CLIENT_SELF : other->id;
	*data++ = type;
	*(cs_int32 *)data = htonl(value);
	data += 4;

	PacketWriter_EndIngame(client);
}

void CPE_WriteTwoWayPing(Client *client, cs_byte direction, cs_int16 num) {
	PacketWriter_Start(client, 4);

	*data++ = 0x2B;
	*data++ = direction;
	*(cs_uint16 *)data = num;
	data += 2;

	PacketWriter_EndAnytime(client);
}

void CPE_WriteInventoryOrder(Client *client, cs_byte order, BlockID block) {
	PacketWriter_Start(client, 3);

	*data++ = 0x2C;
	*data++ = block;
	*data++ = order;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetHotBar(Client *client, cs_byte order, BlockID block) {
	PacketWriter_Start(client, 3);

	*data++ = 0x2D;
	*data++ = block;
	*data++ = order;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSetSpawnPoint(Client *client, Vec *pos, Ang *ang) {
	PacketWriter_Start(client, 15);

	*data++ = 0x2E;
	if(Client_GetExtVer(client, EXT_ENTPOS))
		Proto_WriteFlVec(&data, pos);
	else
		Proto_WriteFlSVec(&data, pos);
	Proto_WriteAng(&data, ang);

	PacketWriter_EndIngame(client);
}

void CPE_WriteVelocityControl(Client *client, Vec *velocity, cs_bool mode) {
	PacketWriter_Start(client, 16);

	*data++ = 0x2F;
	Proto_WriteFlVec(&data, velocity);
	*(cs_uint32 *)data = mode ? 0x01010101 : 0x00000000; // Why not?
	data += 4;

	PacketWriter_EndIngame(client);
}

void CPE_WriteDefineEffect(Client *client, CustomParticle *e) {
	PacketWriter_Start(client, 36);

	*data++ = 0x30;
	*(struct TextureRec *)data = e->rec;
	*data++ = e->frameCount;
	*data++ = e->particleCount;
	*data++ = (cs_byte)(e->size * 32.0f);
	*(cs_uint32 *)data = htonl((cs_uint32)(e->sizeVariation * 1000.0f)); data += 4;
	*(cs_uint16 *)data = htons((cs_uint16)(e->spread * 32.0f)); data += 2;
	*(cs_uint32 *)data = htonl((cs_uint32)(e->speed * 10000.0f)); data += 4;
	*(cs_uint32 *)data = htonl((cs_uint32)(e->gravity * 10000.0f)); data += 4;
	*(cs_uint32 *)data = htonl((cs_uint32)(e->baseLifetime * 10000.0f)); data += 4;
	*(cs_uint32 *)data = htonl((cs_uint32)(e->lifetimeVariation * 10000.0f)); data += 4;
	*data++ = e->collideFlags;
	*data++ = e->fullBright;

	PacketWriter_EndIngame(client);
}

void CPE_WriteSpawnEffect(Client *client, cs_byte id, Vec *pos, Vec *origin) {
	PacketWriter_Start(client, 26);

	*data++ = 0x31;
	*data++ = id;
	Proto_WriteFlVec(&data, pos);
	Proto_WriteFlVec(&data, origin);

	PacketWriter_EndIngame(client);
}

// void CPE_WriteDefineModel(Client *client, ...) {}
// void CPE_WriteDefineModelPart(Client *client, ...) {}
// void CPE_WriteUndefineModel(Client *client, ...) {}

void CPE_WritePluginMessage(Client *client, cs_byte channel, cs_str message) {
	PacketWriter_Start(client, 66);

	*data++ = 0x35;
	*data++ = channel;
	Proto_WriteString(&data, message);

	PacketWriter_EndAnytime(client);
}

cs_bool CPEHandler_ExtInfo(Client *client, cs_char *data) {
	ValidateCpeClient(client, false)
	ValidateClientState(client, PLAYER_STATE_INITIAL, false)

	if(!Proto_ReadStringNoAlloc(&data, client->cpeData->appName)) {
		String_Copy(client->cpeData->appName, 65, "(unknown)");
		return true;
	}
	client->cpeData->_extCount = ntohs(*(cs_uint16 *)data);
	return true;
}

cs_bool CPEHandler_ExtEntry(Client *client, cs_char *data) {
	ValidateCpeClient(client, false)
	ValidateClientState(client, PLAYER_STATE_INITIAL, false)

	CPEExt *tmp = Memory_Alloc(1, sizeof(struct _CPEExt));
	if(!Proto_ReadString(&data, &tmp->name)) {
		Memory_Free(tmp);
		return false;
	}

	tmp->version = ntohl(*(cs_int32 *)data);
	if(tmp->version < 1) {
		Memory_Free((void *)tmp->name);
		Memory_Free(tmp);
		return false;
	}

	tmp->hash = Compr_CRC32((cs_byte*)tmp->name, (cs_uint32)String_Length(tmp->name));
	tmp->next = client->cpeData->headExtension;
	client->cpeData->headExtension = tmp;

	if(--client->cpeData->_extCount == 0) {
		if(Client_GetExtVer(client, EXT_CUSTOMBLOCKS))
			return true;

		return FinishHandshake(client);
	}

	return true;
}

cs_bool CPEHandler_SetCBVer(Client *client, cs_char *data) {
	ValidateCpeClient(client, false)
	ValidateClientState(client, PLAYER_STATE_INITIAL, false)
	if(Client_GetExtVer(client, EXT_CUSTOMBLOCKS) < 1) return false;
	client->cpeData->cbLevel = *data;
	return FinishHandshake(client);
}

cs_bool CPEHandler_PlayerClick(Client *client, cs_char *data) {
	if(Client_GetExtVer(client, EXT_PLAYERCLICK) < 1) return false;
	ValidateClientState(client, PLAYER_STATE_INGAME, false)

	onPlayerClick params;
	params.client = client;
	params.button = *data++;
	params.action = *data++;
	params.angle.yaw = (cs_float)((cs_int16)ntohs(*(cs_uint16 *)data)) / 32767.0f * 360.0f; data += 2;
	params.angle.pitch = (cs_float)((cs_int16)ntohs(*(cs_uint16 *)data)) / 32767.0f * 360.0f; data += 2;
	params.tgid = *data++;
	Proto_ReadSVec(&data, &params.tgpos);
	params.tgface = *data;
	Event_Call(EVT_ONCLICK, &params);

	return true;
}

cs_bool CPEHandler_TwoWayPing(Client *client, cs_char *data) {
	if(Client_GetExtVer(client, EXT_TWOWAYPING) < 1) return false;

	cs_byte pingDirection = *data++;
	cs_uint16 pingData = *(cs_uint16 *)data;

	if(pingDirection == 0) {
		CPE_WriteTwoWayPing(client, 0, pingData);
		if(!client->cpeData->pingStarted) {
			CPE_WriteTwoWayPing(client, 1, ++client->cpeData->pingData);
			client->cpeData->pingStarted = true;
			client->cpeData->pingStart = Time_GetMSec();
		}
		return true;
	} else if(pingDirection == 1) {
		if(client->cpeData->pingStarted) {
			client->cpeData->pingStarted = false;
			if(client->cpeData->pingData == pingData) {
				client->cpeData->pingTime = (cs_uint32)((Time_GetMSec() - client->cpeData->pingStart) / 2);
				client->cpeData->pingAvgTime = (client->cpeData->_pingAvgSize * client->cpeData->pingAvgTime +
				(cs_float)client->cpeData->pingTime) / (client->cpeData->_pingAvgSize + 1);
				client->cpeData->_pingAvgSize += 1;
			}
			return true;
		}
	}
	return false;
}

cs_bool CPEHandler_PluginMessage(Client *client, cs_char *data) {
	if(Client_GetExtVer(client, EXT_PLUGINMESSAGE) < 1) return false;

	onPluginMessage pmesg = {
		.client = client,
		.channel = *data++
	};

	Proto_ReadStringNoAlloc(&data, pmesg.message);
	return Event_Call(EVT_ONPLUGINMESSAGE, &pmesg);
}

Packet *packetsList[255] = {NULL};

void Packet_Register(cs_byte id, cs_uint16 size, packetHandler handler) {
	Packet *tmp = (Packet *)Memory_Alloc(1, sizeof(Packet));
	tmp->id = id;
	tmp->size = size;
	tmp->handler = handler;
	packetsList[id] = tmp;
}

void Packet_SetCPEHandler(cs_byte id, cs_uint32 hash, cs_int32 ver, cs_uint16 size, packetHandler handler) {
	Packet *tmp = packetsList[id];
	tmp->exthash = hash;
	tmp->extVersion = ver;
	tmp->extHandler = handler;
	tmp->extSize = size;
	tmp->haveCPEImp = true;
}

void CPE_RegisterServerExtension(cs_str name, cs_int32 version) {
	CPEExt *tmp = Memory_Alloc(1, sizeof(struct _CPEExt));
	tmp->name = name;
	tmp->version = version;
	tmp->next = headExtension;
	headExtension = tmp;
	++extensionsCount;
}

static const struct extReg {
	cs_str name;
	cs_int32 version;
} serverExtensions[] = {
	{"ClickDistance", 1},
	{"CustomBlocks", 1},
	{"HeldBlock", 1},
	{"EmoteFix", 1},
	{"TextHotKey", 1},
	{"ExtPlayerList", 2},
	{"EnvColors", 1},
	{"SelectionCuboid", 1},
	{"BlockPermissions", 1},
	{"ChangeModel", 1},
	{"EnvMapAppearance", 1},
	{"EnvMapAppearance", 2},
	{"EnvWeatherType", 1},
	{"HackControl", 1},
	{"MessageTypes", 1},
	{"PlayerClick", 1},
	{"LongerMessages", 1},
	{"FullCP437", 1},
	{"BlockDefinitions", 1},
	{"BlockDefinitionsExt", 2},
	{"BulkBlockUpdate", 1},
	{"TextColors", 1},
	{"EnvMapAspect", 1},
	{"EntityProperty", 1},
	{"ExtEntityPositions", 1},
	{"TwoWayPing", 1},
	{"InventoryOrder", 1},
	// {"ExtendedBlocks", 1},
	{"FastMap", 1},
	// {"ExtendedTextures", 1},
	{"SetHotbar", 1},
	{"SetSpawnpoint", 1},
	{"VelocityControl", 1},
	{"CustomParticles", 1},
	// {"CustomModels", 2},
	{"PluginMessages", 1},

	{NULL, 0}
};

Packet *Packet_Get(cs_byte id) {
	return packetsList[id];
}

void Packet_RegisterDefault(void) {
	Packet_Register(0x00, 130, Handler_Handshake);
	Packet_Register(0x05,   8, Handler_SetBlock);
	Packet_Register(0x08,   9, Handler_PosAndOrient);
	Packet_Register(0x0D,  65, Handler_Message);

	const struct extReg *ext;
	for(ext = serverExtensions; ext->name; ext++) {
		CPE_RegisterServerExtension(ext->name, ext->version);
	}

	Packet_Register(0x10, 66, CPEHandler_ExtInfo);
	Packet_Register(0x11, 68, CPEHandler_ExtEntry);
	Packet_Register(0x13, 1, CPEHandler_SetCBVer);
	Packet_Register(0x2B,  3, CPEHandler_TwoWayPing);
	Packet_Register(0x22, 14, CPEHandler_PlayerClick);
	Packet_SetCPEHandler(0x08, EXT_ENTPOS, 1, 15, NULL);
}

void Packet_UnregisterAll(void) {
	while(headExtension != NULL) {
		CPEExt *tmp = headExtension;
		headExtension = headExtension->next;
		Memory_Free(tmp);
	}

	for(cs_int32 i = 0; i < 255; i++) {
		Packet *packet = packetsList[i];
		if(packet) {
			Memory_Free(packet);
			packetsList[i] = NULL;
		}
	}
}
