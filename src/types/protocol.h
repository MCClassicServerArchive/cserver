#ifndef PROTOCOLTYPES_H
#define PROTOCOLTYPES_H
#include "core.h"

#define PROTOCOL_VERSION 0x07

#define EXT_CLICKDIST 0x6DD2B567ul
#define EXT_CUSTOMBLOCKS 0x98455F43ul
#define EXT_HELDBLOCK 0x40C33F88ul
#define EXT_TEXTHOTKEY 0x73BB9FBFul
#define EXT_PLAYERLIST 0xBB0CD618ul
#define EXT_ENVCOLOR 0x4C056274ul
#define EXT_CUBOID 0xE45DA299ul
#define EXT_BLOCKPERM 0xB2E8C3D6ul
#define EXT_CHANGEMODEL 0xAE3AEBAAul
#define EXT_MAPPROPS 0xB46CAFABul
#define EXT_WEATHER 0x40501770ul
#define EXT_MESSAGETYPE 0x7470960Eul
#define EXT_HACKCTRL 0x6E4CED2Dul
#define EXT_PLAYERCLICK 0x29442DBul
#define EXT_CP437 0x27FBB82Ful
#define EXT_LONGMSG 0x8535AB13ul
#define EXT_BLOCKDEF 0xC6BAA7Bul
#define EXT_BLOCKDEF2 0xEFB2BBECul
#define EXT_BULKUPDATE 0x29509B8Ful
#define EXT_TEXTCOLORS 0x56C393B8ul
#define EXT_MAPASPECT 0xB3F9BDF0ul
#define EXT_ENTPROP 0x5865D50Eul
#define EXT_ENTPOS 0x37D3033Ful
#define EXT_TWOWAYPING 0xBBC796E8ul
#define EXT_INVORDER 0xEE0F7B71ul
#define EXT_INSTANTMOTD 0x462BFA8Ful
#define EXT_FASTMAP 0x7791DB5Ful
#define EXT_SETHOTBAR 0xB8703914ul
#define EXT_SETSPAWN 0x9149FD59ul
#define EXT_VELCTRL 0xF8DF4FF7ul
#define EXT_CUSTOMPARTS 0x0D732743
#define EXT_PARTICLE 0x0D732743ul
#define EXT_CUSTOMMODELS 0xB27EFA32ul
#define EXT_PLUGINMESSAGE 0x59FA7285ul

typedef enum _EPacketID {
	// Vanilla packets
	PACKET_IDENTIFICATION = 0x00,
	PACKET_PING = 0x01,
	PACKET_LEVELINIT = 0x02,
	PACKET_LEVELCHUNK = 0x03,
	PACKET_LEVELFINAL = 0x04,
	PACKET_SETBLOCK_CLIENT = 0x05,
	PACKET_SETBLOCK_SERVER = 0x06,
	PACKET_ENTITYSPAWN = 0x07,
	PACKET_ENTITYTELEPORT = 0x08,
	PACKET_ENTITYUPDATEFULL = 0x09,
	PACKET_ENTITYUPDATEPOS = 0x0A,
	PACKET_ENTITYUPDATEORIENT = 0x0B,
	PACKET_ENTITYDESPAWN = 0x0C,
	PACKET_SENDMESSAGE = 0x0D,
	PACKET_ENTITYREMOVE = 0x0E,
	PACKET_USERUPDATE = 0x0F,

	// CPE packets
	PACKET_EXTINFO = 0x10,
	PACKET_EXTENTRY = 0x11,
	PACKET_CLICKDIST = 0x12,
	PACKET_CUSTOMBLOCKSLVL = 0x13,
	PACKET_HOLDTHIS = 0x14,
	PACKET_SETHOTKEY = 0x15,
	PACKET_NAMEADD = 0x16,
	PACKET_EXTENTITYADDv1,
	PACKET_NAMEREMOVE = 0x18,
	PACKET_SETENVCOLOR = 0x19,
	PACKET_SELECTIONADD = 0x1A,
	PACKET_SELECTIONREMOVE = 0x1B,
	PACKET_SETBLOCKPERMISSION = 0x1C,
	PACKET_ENTITYMODEL = 0x1D,
	PACKET_SETMAPAPPEARANCE = 0x1E,
	PACKET_SETENVWEATHER = 0x1F,
	PACKET_UPDATEHACKS = 0x20,
	PACKET_EXTENTITYADDv2 = 0x21,
	PACKET_PLAYERCLICKED = 0x22,
	PACKET_DEFINEBLOCK = 0x23,
	PACKET_UNDEFINEBLOCK = 0x24,
	PACKET_DEFINEBLOCKEXT = 0x25,
	PACKET_BULKBLOCKUPDATE = 0x26,
	PACKET_ADDTEXTCOLOR = 0x27,
	PACKET_ENVSETPACKURL = 0x28,
	PACKET_SETENVPROP = 0x29,
	PACKET_ENTITYPROPERTY = 0x2A,
	PACKET_TWOWAYPING = 0x2B,
	PACKET_SETINVORDER = 0x2C,
	PACKET_SETHOTBAR = 0x2D,
	PACKET_SETSPAWNPOINT = 0x2E,
	PACKET_SETVELOCITY = 0x2F,
	PACKET_DEFINEEFFECT = 0x30,
	PACKET_SPAWNEFFECT = 0x31,
	PACKET_DEFINEMODEL = 0x32,
	PACKET_DEFINEMODELPART = 0x33,
	PACKET_UNDEFINEMODEL = 0x34,
	PACKET_PLUGINMESSAGE = 0x35
} EPacketID;

typedef struct _Packet {
	EPacketID id;
	cs_bool haveCPEImp;
	cs_uint16 size, extSize;
	cs_ulong exthash;
	cs_int32 extVersion;
	void *handler;
	void *extHandler;
} Packet;
#endif
