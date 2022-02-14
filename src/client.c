#include "core.h"
#include "str.h"
#include "log.h"
#include "list.h"
#include "platform.h"
#include "block.h"
#include "server.h"
#include "protocol.h"
#include "client.h"
#include "event.h"
#include "strstor.h"
#include "compr.h"

Client *Broadcast = NULL;
Client *Clients_List[MAX_CLIENTS] = {0};
static AListField *headCGroup = NULL;

CGroup *Group_Add(cs_int16 gid, cs_str gname, cs_byte grank) {
	CGroup *gptr = Group_GetByID(gid);
	if(!gptr) {
		gptr = Memory_Alloc(1, sizeof(CGroup));
		gptr->id = gid;
		gptr->field = AList_AddField(&headCGroup, gptr);
	}

	String_Copy(gptr->name, 65, gname);
	gptr->rank = grank;
	return gptr;
}

CGroup *Group_GetByID(cs_int16 gid) {
	CGroup *gptr = NULL;
	AListField *lptr = NULL;

	List_Iter(lptr, headCGroup) {
		CGroup *tmp = lptr->value.ptr;
		if(tmp->id == gid) {
			gptr = tmp;
			break;
		}
	}

	return gptr;
}

cs_bool Group_Remove(cs_int16 gid) {
	CGroup *cg = Group_GetByID(gid);
	if(!cg) return false;

	for(ClientID id = 0; id < MAX_CLIENTS; id++) {
		Client *client = Clients_List[id];
		if(client && Client_GetGroupID(client) == gid)
			Client_SetGroup(client, -1);
	}

	AList_Remove(&headCGroup, cg->field);
	Memory_Free(cg);
	return true;
}

cs_byte Clients_GetCount(EPlayerState state) {
	cs_byte count = 0;
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *client = Clients_List[i];
		if(client && Client_CheckState(client, state)) count++;
	}
	return count;
}

void Clients_KickAll(cs_str reason) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *client = Clients_List[i];
		if(client) Client_Kick(client, reason);
	}
}

cs_str Client_GetName(Client *client) {
	if(!client->playerData) return Sstor_Get("NONAME");
	return client->playerData->name;
}

cs_str Client_GetKey(Client *client) {
	if(!client->playerData) return Sstor_Get("CL_NOKEY");
	return client->playerData->key;
}

cs_str Client_GetAppName(Client *client) {
	if(!client->cpeData) return Sstor_Get("CL_VANILLA");
	return client->cpeData->appName;
}

cs_str Client_GetSkin(Client *client) {
	if(!client->cpeData || *client->cpeData->skin == '\0')
		return NULL;
	return client->cpeData->skin;
}

Client *Client_GetByName(cs_str name) {
	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *client = Clients_List[i];
		if(client && client->playerData && String_CaselessCompare(client->playerData->name, name))
			return client;
	}
	return NULL;
}

Client *Client_GetByID(ClientID id) {
	return id >= 0 && id < MAX_CLIENTS ? Clients_List[id] : NULL;
}

ClientID Client_GetID(Client *client) {
	return client->id;
}

World *Client_GetWorld(Client *client) {
	if(!client->playerData) return NULL;
	return client->playerData->world;
}

BlockID Client_GetStandBlock(Client *client) {
	if(!client->playerData) return 0;
	SVec tpos; SVec_Copy(tpos, client->playerData->position);
	if(tpos.x < 0 || tpos.y < 0 || tpos.z < 0) return 0;
	tpos.y -= 2;

	return World_GetBlock(client->playerData->world, &tpos);
}

cs_int8 Client_GetFluidLevel(Client *client, BlockID *fluid) {
	if(!client->playerData) return 0;
	SVec tpos; SVec_Copy(tpos, client->playerData->position)
	if(tpos.x < 0 || tpos.y < 0 || tpos.z < 0) return 0;

	BlockID id;
	cs_byte level = 2;

	test_wtrlevel:
	if((id = World_GetBlock(client->playerData->world, &tpos)) > 7 && id < 12) {
		if(fluid) *fluid = id;
		return level;
	}

	if(level > 1) {
		level--;
		tpos.y--;
		goto test_wtrlevel;
	}

	return 0;
}

static CGroup dgroup = {-1, 0, "", NULL};

CGroup *Client_GetGroup(Client *client) {
	if(!client->cpeData) return &dgroup;
	CGroup *gptr = Group_GetByID(client->cpeData->group);
	return !gptr ? &dgroup : gptr;
}

cs_int16 Client_GetGroupID(Client *client) {
	if(!client->cpeData) return -1;
	return client->cpeData->group;
}

cs_int16 Client_GetModel(Client *client) {
	if(!client->cpeData) return 256;
	return client->cpeData->model;
}

BlockID Client_GetHeldBlock(Client *client) {
	if(!client->cpeData) return BLOCK_AIR;
	return client->cpeData->heldBlock;
}

cs_uint16 Client_GetClickDistance(Client *client) {
	if(!client->cpeData) return 160;
	return client->cpeData->clickDist;
}

cs_float Client_GetClickDistanceInBlocks(Client *client) {
	if(!client->cpeData) return 5.0f;
	return client->cpeData->clickDist / 32.0f;
}

cs_int32 Client_GetExtVer(Client *client, cs_uint32 exthash) {
	if(!client->cpeData) return false;

	CPEExt *ptr = client->cpeData->headExtension;
	while(ptr) {
		if(ptr->hash == exthash) return ptr->version;
		ptr = ptr->next;
	}

	return false;
}

cs_bool Client_Despawn(Client *client) {
	if(!client->playerData || !client->playerData->spawned)
		return false;
	client->playerData->spawned = false;
	for(cs_uint32 i = 0; i < MAX_CLIENTS; i++) {
		Client *other = Clients_List[i];
		if(other) Vanilla_WriteDespawn(other, client);
	}
	Event_Call(EVT_ONDESPAWN, client);
	return true;
}

cs_bool Client_ChangeWorld(Client *client, World *world) {
	if(Client_CheckState(client, PLAYER_STATE_MOTD))
		return false;

	Client_Despawn(client);
	client->playerData->world = world;
	client->playerData->state = PLAYER_STATE_MOTD;
	client->playerData->reqWorldChange = world;
	return true;
}

void Client_UpdateWorldInfo(Client *client, World *world, cs_bool updateAll) {
	if(!client->cpeData) return;

	if(Client_GetExtVer(client, EXT_MAPASPECT)) {
		if(updateAll || world->info.modval & MV_COLORS) {
			for(cs_byte color = 0; color < WORLD_COLORS_COUNT; color++) {
				if(updateAll || world->info.modclr & (1 << color))
					CPE_WriteEnvColor(client, color, World_GetEnvColor(world, color));
			}
		}
		if(updateAll || world->info.modval & MV_TEXPACK)
			CPE_WriteTexturePack(client, world->info.texturepack);
		if(updateAll || world->info.modval & MV_PROPS) {
			for(cs_byte prop = 0; prop < WORLD_PROPS_COUNT; prop++) {
				if(updateAll || world->info.modprop & (1 << prop))
					CPE_WriteMapProperty(client, prop, World_GetEnvProp(world, prop));
			}
		}
	} else if(Client_GetExtVer(client, EXT_MAPPROPS) == 2) {
		CPE_WriteSetMapAppearanceV2(
			client, world->info.texturepack,
			(cs_byte)World_GetEnvProp(world, 0),
			(cs_byte)World_GetEnvProp(world, 1),
			(cs_int16)World_GetEnvProp(world, 2),
			(cs_int16)World_GetEnvProp(world, 3),
			(cs_int16)World_GetEnvProp(world, 4)
		);
	} else if(Client_GetExtVer(client, EXT_MAPPROPS) == 1) {
		CPE_WriteSetMapAppearanceV1(
			client, world->info.texturepack,
			(cs_byte)World_GetEnvProp(world, 0),
			(cs_byte)World_GetEnvProp(world, 1),
			(cs_int16)World_GetEnvProp(world, 2)
		);
	}

	if(updateAll || world->info.modval & MV_WEATHER)
		Client_SetWeather(client, world->info.weatherType);
}

cs_bool Client_MakeSelection(Client *client, cs_byte id, SVec *start, SVec *end, Color4* color) {
	if(Client_GetExtVer(client, EXT_CUBOID)) {
		CPE_WriteMakeSelection(client, id, start, end, color);
		return true;
	}
	return false;
}

cs_bool Client_RemoveSelection(Client *client, cs_byte id) {
	if(Client_GetExtVer(client, EXT_CUBOID)) {
		CPE_WriteRemoveSelection(client, id);
		return true;
	}
	return false;
}

cs_bool Client_TeleportTo(Client *client, Vec *pos, Ang *ang) {
	if(Client_CheckState(client, PLAYER_STATE_INGAME)) {
		Vanilla_WriteTeleport(client, pos, ang);
		return true;
	}
	return false;
}

cs_bool Client_TeleportToSpawn(Client *client) {
	if(!client->playerData || !client->playerData->world) return false;
	return Client_TeleportTo(client,
		&client->playerData->world->info.spawnVec,
		&client->playerData->world->info.spawnAng
	);
}

INL static cs_uint32 copyMessagePart(cs_str msg, cs_char *part, cs_uint32 i, cs_char *color) {
	if(*msg == '\0') return 0;

	if(i > 0) {
		*part++ = '>';
		*part++ = ' ';
	}

	if(*color > 0) {
		*part++ = '&';
		*part++ = *color;
	}

	cs_uint32 len = min(60, (cs_uint32)String_Length(msg));
	if(msg[len - 1] == '&' && ISHEX(msg[len])) --len;

	for(cs_uint32 j = 0; j < len; j++) {
		cs_char prevsym = *msg++,
		nextsym = *msg;

		if(prevsym != '\r') *part++ = prevsym;
		if(nextsym == '\0' || nextsym == '\n') break;
		if(prevsym == '&' && ISHEX(nextsym)) *color = nextsym;
	}

	*part = '\0';
	return len;
}

void Client_Chat(Client *client, EMesgType type, cs_str message) {
	cs_uint32 msgLen = (cs_uint32)String_Length(message);

	if(msgLen > 62 && type == MESSAGE_TYPE_CHAT) {
		cs_char color = 0, part[65] = {0};
		cs_uint32 parts = (msgLen / 60) + 1;
		for(cs_uint32 i = 0; i < parts; i++) {
			cs_uint32 len = copyMessagePart(message, part, i, &color);
			if(len > 0) {
				Vanilla_WriteChat(client, type, part);
				message += len;
			}
		}
		return;
	}

	Vanilla_WriteChat(client, type, message);
}

NOINL static void HandlePacket(Client *client, cs_char *data, Packet *packet, cs_bool extended) {
	cs_bool ret = false;

	if(extended)
		if(packet->extHandler)
			ret = packet->extHandler(client, data);
		else
			ret = packet->handler(client, data);
	else
		if(packet->handler)
			ret = packet->handler(client, data);

	if(!ret)
		Client_KickFormat(client, Sstor_Get("KICK_PERR_UNEXP"), packet->id);
	else client->pps += 1;
}

INL static cs_uint16 GetPacketSizeFor(Packet *packet, Client *client, cs_bool *extended) {
	if(packet->haveCPEImp) {
		*extended = Client_GetExtVer(client, packet->exthash) == packet->extVersion;
		if(*extended) return packet->extSize;
	}
	return packet->size;
}

cs_bool Client_CheckState(Client *client, EPlayerState state) {
	if(!client->playerData) return false;
	return client->playerData->state == state;
}

cs_bool Client_IsLocal(Client *client) {
	return client->addr == 0x7f000001;
}

cs_bool Client_IsInSameWorld(Client *client, Client *other) {
	return Client_GetWorld(client) == Client_GetWorld(other);
}

cs_bool Client_IsInWorld(Client *client, World *world) {
	return Client_GetWorld(client) == world;
}

cs_bool Client_IsOP(Client *client) {
	return client->playerData ? client->playerData->isOP : false;
}

cs_bool Client_IsFirstSpawn(Client *client) {
	return client->playerData ? client->playerData->firstSpawn : true;
}

void Client_SetBlock(Client *client, SVec *pos, BlockID id) {
	Vanilla_WriteSetBlock(client, pos, id);
}

cs_bool Client_SetEnvProperty(Client *client, cs_byte property, cs_int32 value) {
	if(Client_GetExtVer(client, EXT_MAPASPECT)) {
		CPE_WriteMapProperty(client, property, value);
		return true;
	}
	return false;
}

cs_bool Client_SetEnvColor(Client *client, cs_byte type, Color3* color) {
	if(Client_GetExtVer(client, EXT_ENVCOLOR)) {
		CPE_WriteEnvColor(client, type, color);
		return true;
	}
	return false;
}

cs_bool Client_SetTexturePack(Client *client, cs_str url) {
	if(Client_GetExtVer(client, EXT_MAPASPECT)) {
		CPE_WriteTexturePack(client, url);
		return true;
	}
	return false;
}

cs_bool Client_SetWeather(Client *client, cs_int8 type) {
	if(Client_GetExtVer(client, EXT_WEATHER)) {
		CPE_WriteWeatherType(client, type);
		return true;
	}
	return false;
}

cs_bool Client_SetInvOrder(Client *client, cs_byte order, BlockID block) {
	if(Block_IsValid(block) && Client_GetExtVer(client, EXT_INVORDER)) {
		CPE_WriteInventoryOrder(client, order, block);
		return true;
	}
	return false;
}

cs_bool Client_SetServerIdent(Client *client, cs_str name, cs_str motd) {
	if(Client_CheckState(client, PLAYER_STATE_INGAME) && !Client_GetExtVer(client, EXT_INSTANTMOTD))
		return false;
	Vanilla_WriteServerIdent(client, name, motd);
	return true;
}

cs_bool Client_SetHeldBlock(Client *client, BlockID block, cs_bool canChange) {
	if(Block_IsValid(block) && Client_GetExtVer(client, EXT_HELDBLOCK)) {
		CPE_WriteHoldThis(client, block, canChange);
		return true;
	}
	return false;
}

cs_bool Client_SetClickDistance(Client *client, cs_uint16 dist) {
	if(Client_GetExtVer(client, EXT_CLICKDIST)) {
		CPE_WriteClickDistance(client, dist);
		client->cpeData->clickDist = dist;
		return true;
	}
	return false;
}

cs_bool Client_SetHotkey(Client *client, cs_str action, cs_int32 keycode, cs_int8 keymod) {
	if(Client_GetExtVer(client, EXT_TEXTHOTKEY)) {
		CPE_WriteSetHotKey(client, action, keycode, keymod);
		return true;
	}
	return false;
}

cs_bool Client_SetHotbar(Client *client, cs_byte pos, BlockID block) {
	if(Block_IsValid(block) && pos < 9 && Client_GetExtVer(client, EXT_SETHOTBAR)) {
		CPE_WriteSetHotBar(client, pos, block);
		return true;
	}
	return false;
}

cs_bool Client_SetBlockPerm(Client *client, BlockID block, cs_bool allowPlace, cs_bool allowDestroy) {
	if(Block_IsValid(block) && Client_GetExtVer(client, EXT_BLOCKPERM)) {
		CPE_WriteBlockPerm(client, block, allowPlace, allowDestroy);
		return true;
	}
	return false;
}

cs_bool Client_SetModel(Client *client, cs_int16 model) {
	if(!client->cpeData || !CPE_CheckModel(model)) return false;
	client->cpeData->model = model;
	client->cpeData->updates |= PCU_MODEL;
	return true;
}

cs_bool Client_SetSkin(Client *client, cs_str skin) {
	if(!client->cpeData) return false;
	String_Copy(client->cpeData->skin, 65, skin);
	client->cpeData->updates |= PCU_SKIN;
	return true;
}

cs_uint32 Client_GetAddr(Client *client) {
	return client->addr;
}

cs_int32 Client_GetPing(Client *client) {
	if(Client_GetExtVer(client, EXT_TWOWAYPING)) {
		return client->cpeData->pingTime;
	}
	return -1;
}

cs_float Client_GetAvgPing(Client *client) {
	if(Client_GetExtVer(client, EXT_TWOWAYPING)) {
		return client->cpeData->pingAvgTime;
	}
	return -1.0f;
}

cs_bool Client_GetPosition(Client *client, Vec *pos, Ang *ang) {
	if(client->playerData) {
		if(pos) *pos = client->playerData->position;
		if(ang) *ang = client->playerData->angle;
		return true;
	}
	return false;
}

cs_bool Client_SetOP(Client *client, cs_bool state) {
	if(!client->playerData) return false;
	client->playerData->isOP = state;
	return true;
}

cs_bool Client_SetSpawn(Client *client, Vec *pos, Ang *ang) {
	if(Client_GetExtVer(client, EXT_SETSPAWN)) {
		CPE_WriteSetSpawnPoint(client, pos, ang);
		return true;
	}
	return false;
}

cs_bool Client_SetVelocity(Client *client, Vec *velocity, cs_bool mode) {
	if(Client_GetExtVer(client, EXT_VELCTRL)) {
		CPE_WriteVelocityControl(client, velocity, mode);
		return true;
	}
	return false;
}

cs_bool Client_RegisterParticle(Client *client, CustomParticle *e) {
	if(Client_GetExtVer(client, EXT_PARTICLE)) {
		CPE_WriteDefineEffect(client, e);
		return true;
	}
	return false;
}

cs_bool Client_SpawnParticle(Client *client, cs_byte id, Vec *pos, Vec *origin) {
	if(Client_GetExtVer(client, EXT_PARTICLE)) {
		CPE_WriteSpawnEffect(client, id, pos, origin);
		return true;
	}
	return false;
}

cs_bool Client_SendPluginMessage(Client *client, cs_byte channel, cs_str message) {
	if(Client_GetExtVer(client, EXT_PLUGINMESSAGE)) {
		CPE_WritePluginMessage(client, channel, message);
		return true;
	}
	return false;
}

cs_bool Client_SetRotation(Client *client, cs_byte axis, cs_int32 value) {
	if(axis < 2 || !client->cpeData) return false;
	client->cpeData->rotation[axis] = value;
	client->cpeData->updates |= PCU_ENTPROP;
	return true;
}

cs_bool Client_SetModelStr(Client *client, cs_str model) {
	return Client_SetModel(client, CPE_GetModelNum(model));
}

cs_bool Client_SetGroup(Client *client, cs_int16 gid) {
	if(!client->cpeData) return false;
	client->cpeData->group = gid;
	client->cpeData->updates |= PCU_GROUP;
	return true;
}

cs_bool Client_SendHacks(Client *client, CPEHacks *hacks) {
	if(Client_GetExtVer(client, EXT_HACKCTRL)) {
		CPE_WriteHackControl(client, hacks);
		return true;
	}
	return false;
}

cs_bool Client_BulkBlockUpdate(Client *client, BulkBlockUpdate *bbu) {
	if(Client_GetExtVer(client, EXT_BULKUPDATE)) {
		CPE_WriteBulkBlockUpdate(client, bbu);
		return true;
	}
	return false;
}

cs_bool Client_DefineBlock(Client *client, BlockDef *block) {
	if(block->flags & BDF_UNDEFINED) return false;
	if(block->flags & BDF_EXTENDED) {
		if(Client_GetExtVer(client, EXT_BLOCKDEF2)) {
			CPE_WriteDefineExBlock(client, block);
			return true;
		}
	} else {
		if(Client_GetExtVer(client, EXT_BLOCKDEF)) {
			CPE_WriteDefineBlock(client, block);
			return true;
		}
	}

	return false;
}

cs_bool Client_UndefineBlock(Client *client, BlockID id) {
	if(Client_GetExtVer(client, EXT_BLOCKDEF)) {
		CPE_WriteUndefineBlock(client, id);
		return true;
	}
	return false;
}

cs_bool Client_AddTextColor(Client *client, Color4* color, cs_char code) {
	if(Client_GetExtVer(client, EXT_TEXTCOLORS)) {
		CPE_WriteAddTextColor(client, color, code);
		return true;
	}
	return false;
}

cs_bool Client_Update(Client *client) {
	if(!client->cpeData || client->cpeData->updates == PCU_NONE) return false;
	cs_bool hasplsupport = Client_GetExtVer(client, EXT_PLAYERLIST) == 2,
	hassmsupport = Client_GetExtVer(client, EXT_CHANGEMODEL) == 1,
	hasentprop = Client_GetExtVer(client, EXT_ENTPROP) == 1;

	for(ClientID id = 0; id < MAX_CLIENTS; id++) {
		Client *other = Clients_List[id];
		if(other) {
			if(client->cpeData->updates & PCU_GROUP && hasplsupport)
				CPE_WriteAddName(other, client);
			if(client->cpeData->updates & PCU_MODEL && hassmsupport)
				CPE_WriteSetModel(other, client);
			if(client->cpeData->updates & PCU_SKIN && hasplsupport)
				CPE_WriteAddEntity2(other, client);
			if(client->cpeData->updates & PCU_ENTPROP && hasentprop)
				for(cs_int8 i = 0; i < 3; i++) {
					CPE_WriteSetEntityProperty(other, client, i, client->cpeData->rotation[i]);
				}
		}
	}

	client->cpeData->updates = PCU_NONE;
	return true;
}

void Client_Free(Client *client) {
	if(client->waitend) {
		Waitable_Free(client->waitend);
		client->waitend = NULL;
	}
	if(client->mutex) {
		Mutex_Free(client->mutex);
		client->mutex = NULL;
	}
	if(client->websock) {
		Memory_Free(client->websock);
		client->websock = NULL;
	}
	if(client->playerData) {
		Memory_Free(client->playerData);
		client->playerData = NULL;
	}

	if(client->cpeData) {
		CPEExt *prev, *ptr = client->cpeData->headExtension;

		while(ptr) {
			prev = ptr;
			ptr = ptr->next;
			Memory_Free((void *)prev->name);
			Memory_Free(prev);
		}

		if(client->cpeData->message) {
			Memory_Free(client->cpeData->message);
			client->cpeData->message = NULL;
		}

		Memory_Free(client->cpeData);
		client->cpeData = NULL;
	}

	while(client->headNode) {
		Memory_Free(client->headNode->value.ptr);
		KList_Remove(&client->headNode, client->headNode);
	}

	GrowingBuffer_Cleanup(&client->gb);
	Compr_Cleanup(&client->compr);
	Memory_Free(client);
}

cs_bool Client_RawSend(Client *client, cs_char *buf, cs_int32 len) {
	if(client->closed || len < 1) return false;

	if(client == Broadcast) {
		for(ClientID i = 0; i < MAX_CLIENTS; i++) {
			Client *bClient = Clients_List[i];

			if(bClient && !bClient->closed) {
				Mutex_Lock(bClient->mutex);
				Client_RawSend(bClient, buf, len);
				Mutex_Unlock(bClient->mutex);
			}
		}

		return true;
	}

	if(client->websock) {
		if(!WebSock_SendFrame(client->websock, 0x02, buf, (cs_uint16)len)) {
			client->closed = true;
			return false;
		}
	} else {
		if(Socket_Send(client->sock, buf, len) != len) {
			client->closed = true;
			return false;
		}
	}

	return true;
}

cs_bool Client_SendAnytimeData(Client *client, cs_int32 size) {
	cs_char *data = GrowingBuffer_GetCurrentPoint(&client->gb);
	return Client_RawSend(client, data, size);
}

cs_bool Client_FlushBuffer(Client *client) {
	cs_uint32 size = 0;
	cs_char *data = GrowingBuffer_PopFullData(&client->gb, &size);
	return Client_RawSend(client, data, size);
}

INL static void PacketReceiverWs(Client *client) {
	cs_byte packetId;
	Packet *packet;
	cs_bool extended = false;
	cs_uint16 packetSize, recvSize;
	cs_char *data = client->rdbuf;

	if(WebSock_ReceiveFrame(client->websock)) {
		if(client->websock->opcode == 0x08) {
			client->closed = true;
			return;
		}

		recvSize = client->websock->plen - 1;
		packet_handle:
		packetId = *data++;
		packet = Packet_Get(packetId);
		if(!packet) {
			Client_KickFormat(client, Sstor_Get("KICK_PERR_NOHANDLER"), packetId);
			return;
		}

		packetSize = GetPacketSizeFor(packet, client, &extended);

		if(packetSize <= recvSize) {
			HandlePacket(client, data, packet, extended);
			if(recvSize > packetSize && !client->closed) {
				data += packetSize;
				recvSize -= packetSize + 1;
				goto packet_handle;
			}

			return;
		} else Client_Kick(client, Sstor_Get("KICK_PERR_WS"));
	} else
		client->closed = true;
}

INL static void PacketReceiverRaw(Client *client) {
	Packet *packet;
	cs_uint16 packetSize;
	cs_byte packetId;
	cs_bool extended = false;

	if(Socket_Receive(client->sock, (cs_char *)&packetId, 1, 0) == 1) {
		packet = Packet_Get(packetId);
		if(!packet) {
			Client_KickFormat(client, Sstor_Get("KICK_PERR_NOHANDLER"), packetId);
			return;
		}

		packetSize = GetPacketSizeFor(packet, client, &extended);

		if(packetSize > 0) {
			cs_int32 len = Socket_Receive(client->sock, client->rdbuf, packetSize, MSG_WAITALL);

			if(packetSize == len)
				HandlePacket(client, client->rdbuf, packet, extended);
			else client->closed = true;
		}
	} else client->closed = true;
}

NOINL static void SendWorld(Client *client, World *world) {
	if(!world->loaded) {
		if(!World_Load(world)) {
			Client_Kick(client, Sstor_Get("KICK_INT"));
			return;
		}

		World_Lock(world, 0);
		World_Unlock(world);
	}

	World_StartTask(world);

	cs_bool hasfastmap = Client_GetExtVer(client, EXT_FASTMAP) == 1;
	cs_uint32 wsize = 0;
	cs_bool compr_ok;

	if(hasfastmap) {
		compr_ok = Compr_Init(&client->compr, COMPR_TYPE_DEFLATE);
		if(compr_ok) {
			cs_byte *map = World_GetBlockArray(world, &wsize);
			Compr_SetInBuffer(&client->compr, map, wsize);
			CPE_WriteFastMapInit(client, wsize);
		}
	} else {
		compr_ok = Compr_Init(&client->compr, COMPR_TYPE_GZIP);
		if(compr_ok) {
			cs_byte *map = World_GetData(world, &wsize);
			Compr_SetInBuffer(&client->compr, map, wsize);
			Vanilla_WriteLvlInit(client);
		}
	}

	if(compr_ok) {
		cs_byte data[1029] = {0x03};
		cs_uint16 *len = (cs_uint16 *)(data + 1);
		cs_byte *cmpdata = data + 3;
		cs_byte *progr = data + 1028;

		client->playerData->world = world;
		client->playerData->position = world->info.spawnVec;
		client->playerData->angle = world->info.spawnAng;

		if(Client_GetExtVer(client, EXT_BLOCKDEF)) {
			for(BlockID id = 0; id < 254; id++) {
				BlockDef *bdef = Block_GetDefinition(id);
				if(bdef) Client_DefineBlock(client, bdef);
			}
		}

		do {
			if(!compr_ok || client->closed) break;
			Mutex_Lock(client->mutex);
			Compr_SetOutBuffer(&client->compr, cmpdata, 1024);
			if((compr_ok = Compr_Update(&client->compr)) == true) {
				if(!client->closed && client->compr.written) {
					*len = htons((cs_uint16)client->compr.written);
					*progr = 100 - (cs_byte)((cs_float)client->compr.queued / wsize * 100);
					compr_ok = Client_RawSend(client, (cs_char *)data, 1028);
				}
			}
			Mutex_Unlock(client->mutex);
		} while(client->compr.state != COMPR_STATE_DONE);

		if(compr_ok) {
			Vanilla_WriteLvlFin(client, &world->info.dimensions);
			client->playerData->state = PLAYER_STATE_INGAME;
			Client_FlushBuffer(client);
			Client_Spawn(client);
		} else
			Client_KickFormat(client, Sstor_Get("KICK_ZERR"), Compr_GetLastError(&client->compr));

		Compr_Reset(&client->compr);
	} else
		Client_KickFormat(client, Sstor_Get("KICK_ZERR"), Compr_GetLastError(&client->compr));

	World_EndTask(world);
}

void Client_Tick(Client *client) {
	if(client->websock)
		PacketReceiverWs(client);
	else
		PacketReceiverRaw(client);

	if(client->playerData && client->playerData->reqWorldChange) {
		SendWorld(client, client->playerData->reqWorldChange);
		client->playerData->reqWorldChange = NULL;
	}
}

INL static void SendSpawnPacket(Client *client, Client *other) {
	if(Client_GetExtVer(client, EXT_PLAYERLIST))
		CPE_WriteAddEntity2(client, other);
	else
		Vanilla_WriteSpawn(client, other);
}

cs_bool Client_Spawn(Client *client) {
	if(client->closed || !client->playerData || client->playerData->spawned)
		return false;

	Client_UpdateWorldInfo(client, client->playerData->world, true);

	for(ClientID i = 0; i < MAX_CLIENTS; i++) {
		Client *other = Clients_List[i];
		if(!other) continue;

		if(client->playerData->firstSpawn) {
			if(Client_GetExtVer(other, EXT_PLAYERLIST))
				CPE_WriteAddName(other, client);
			if(Client_GetExtVer(client, EXT_PLAYERLIST) && client != other)
				CPE_WriteAddName(client, other);
		}

		if(Client_IsInSameWorld(client, other)) {
			SendSpawnPacket(other, client);

			if(Client_GetExtVer(other, EXT_CHANGEMODEL))
				CPE_WriteSetModel(other, client);

			if(client != other) {
				SendSpawnPacket(client, other);

				if(Client_GetExtVer(client, EXT_CHANGEMODEL))
					CPE_WriteSetModel(client, other);
			}
		}
	}

	client->playerData->spawned = true;
	Event_Call(EVT_ONSPAWN, client);
	client->playerData->firstSpawn = false;
	return true;
}

void Client_Kick(Client *client, cs_str reason) {
	if(client->closed) return;
	if(!reason) reason = Sstor_Get("KICK_NOREASON");
	Vanilla_WriteKick(client, reason);
	client->closed = true;
	Mutex_Lock(client->mutex);
	Socket_Shutdown(client->sock, SD_SEND);
	while(Socket_Receive(client->sock, client->rdbuf, 134, 0) > 0);
	Socket_Close(client->sock);
	Mutex_Unlock(client->mutex);
}

void Client_KickFormat(Client *client, cs_str fmtreason, ...) {
	char kickreason[65];
	va_list args;
	va_start(args, fmtreason);
	if(String_FormatBufVararg(kickreason, 65, fmtreason, &args))
		Client_Kick(client, kickreason);
	else
		Client_Kick(client, fmtreason);
	va_end(args);
}
