//-----------------------------------------------------------------------------
// Loek Sangers and Romke van Dijk, Jan 2016
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Definitions used within the proxmark client interface.
//-----------------------------------------------------------------------------



#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>


#include "proxmark3.h"
#include "ui.h"
#include "cmdparser.h"
#include "util.h"
#include "iso14443crc.h"
#include "data.h"
#include "cmdhf14a.h"
#include "common.h"
#include "cmdmain.h"
#include "mifare.h"
#include "cmdhfmfu.h"
#include "bumpattacks.h"

static int CmdHelp(const char *Cmd);

void stringToUID(uint8_t *uid, int len, const char * pos){
	// convert uid back to byte_t array

	for(int i = 0; i < len; i++) {
		sscanf(pos, "%2hhx", &uid[i]);
		pos += 2;
	}
}

void attackUID(uint64_t uid){
	PrintAndLog(" AttackingUID");
	PrintAndLog(" Attacking : %llx", (long long)uid);

	uint8_t t[10];

	num_to_bytes(uid, 4, t);
	PrintAndLog(" UID : %s", sprint_hex(t, 4));



	int sak;

	sqlite3 *db;
	sqlite3_stmt *sqlRes;
	int rc = sqlite3_open("cards.db", &db);
	char q[200];

	snprintf(q, sizeof(q), "SELECT * FROM cards WHERE UID='%08"llx"';", uid);
	PrintAndLog(q);

	rc = sqlite3_prepare_v2(db, q, -1, &sqlRes, 0);
	if (rc != SQLITE_OK) {
		PrintAndLog("Failed to fetch data: %s\n", sqlite3_errmsg(db));
	}
	rc = sqlite3_step(sqlRes);
	if (rc == SQLITE_ROW) {
		if((int) sqlite3_column_int(sqlRes, 2) == 1){
			PrintAndLog("Already have complete card!");

			sqlite3_finalize(sqlRes);
			sqlite3_close(db);

			return;
		}
		sak = sqlite3_column_int(sqlRes, 1);

	}
	sqlite3_finalize(sqlRes);
	sqlite3_close(db);

	PrintAndLog("SAK: %d", sak);
		switch (sak) {
			case 0:
				PrintAndLog("ATTACKING : NXP MIFARE Ultralight"); 
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 1:
				PrintAndLog("ATTACKING : NXP TNP3xxx Activision Game Appliance");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 4:
				PrintAndLog("ATTACKING : NXP MIFARE (various !DESFire !DESFire EV1)"); 
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 8:
				PrintAndLog("ATTACKING : NXP MIFARE CLASSIC 1k | Plus 2k SL1"); 
				attackMifareClassic(uid, 1);
				break;
			case 9:
				PrintAndLog("ATTACKING : NXP MIFARE Mini 0.3k"); 
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 10:
				PrintAndLog("ATTACKING : NXP MIFARE Plus 2k SL2"); 
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 11:
				PrintAndLog("ATTACKING : NXP MIFARE Plus 4k SL2");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 18:
				PrintAndLog("ATTACKING : NXP MIFARE Classic 4k | Plus 4k SL1");
				attackMifareClassic(uid, 4);
				break;
			case 20:
				PrintAndLog("ATTACKING : NXP MIFARE DESFire 4k | DESFire EV1 2k/4k/8k | Plus 2k/4k SL3 | JCOP 31/41");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 24:
				PrintAndLog("ATTACKING : NXP MIFARE DESFire | DESFire EV1");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 28:
				PrintAndLog("ATTACKING : JCOP31 or JCOP41 v2.3.1");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 38:
				PrintAndLog("ATTACKING : Nokia 6212 or 6131 MIFARE CLASSIC 4K");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 88:
				PrintAndLog("ATTACKING : Infineon MIFARE CLASSIC 1K");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			case 98:
				PrintAndLog("ATTACKING : Gemplus MPCOS");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
			default:
				PrintAndLog("ATTACKING : UNKNOWN");
				PrintAndLog("NOT IMPLEMENTED!");
				break;
		}

		PrintAndLog("Leaving AttackingUID");

}

void selectAllUIDs(sqlite3 *db, sqlite3_stmt *res){
	PrintAndLog("selectAllUIDs");
		int rc = sqlite3_prepare_v2(db, "SELECT * FROM cards", -1, &res, 0);
    
    		if (rc != SQLITE_OK) {        
        		PrintAndLog("Failed to fetch data: %s\n", sqlite3_errmsg(db));
        		sqlite3_close(db);
        
        		return;
    		}
		rc = sqlite3_step(res);  
		PrintAndLog("UIDs in DB:");
		while (rc == SQLITE_ROW) {

			// convert uid back to byte_t array
			int len = sqlite3_column_bytes(res, 0);

			const char * pos = sqlite3_column_text(res, 0);
			byte_t buid[len];
			stringToUID(buid, len * 2, pos);
			PrintAndLog(" UID : %s", sprint_hex(buid, len / 2));
			
			rc = sqlite3_step(res);
		}
		sqlite3_finalize(res);
}

int AllUIDs(const char *Cmd, bool attack)
{
	PrintAndLog("CmdGetAllUIDs");

	UsbCommand c = {CMD_BUMP_HF_UIDS};
	SendCommand(&c);

	UsbCommand resp;
	WaitForResponse(CMD_ACK,&resp);

	uint64_t number_cards = resp.arg[0];

	iso14a_card_select_t cards[number_cards];

	sqlite3 *db;
	sqlite3_stmt *res;
	int rc = sqlite3_open("cards.db", &db);

	if(rc != SQLITE_OK){
		PrintAndLog("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);

		return 1;
	}

	//create tables if not exist
	rc = sqlite3_prepare_v2(db, "CREATE TABLE IF NOT EXISTS cards \
( UID VARCHAR(20) UNIQUE,\
 SAK VARCHAR(2),\
 done BOOL);", -1, &res, 0);
	rc = sqlite3_step(res);

	rc = sqlite3_prepare_v2(db, "CREATE TABLE IF NOT EXISTS keys \
	( UID INT,\
	 block INT,\
	 keyA VARCHAR(12),\
	 keyB VARCHAR(12),\
	 data VARCHAR(513));", -1, &res, 0);
		rc = sqlite3_step(res);

	for(int i = 0; i < number_cards;i++){

		UsbCommand resp;
		WaitForResponse(CMD_ACK,&resp);

		iso14a_card_select_t card;
		memcpy(&card, (iso14a_card_select_t *)resp.d.asBytes, sizeof(iso14a_card_select_t));

		cards[i] = card;
		PrintAndLog(" UID : %s", sprint_hex(card.uid, card.uidlen));
		PrintAndLog(" SAK : %02x", card.sak);
		
		switch (card.sak) {
			case 0x00: PrintAndLog("TYPE : NXP MIFARE Ultralight"); break;
			case 0x01: PrintAndLog("TYPE : NXP TNP3xxx Activision Game Appliance"); break;
			case 0x04: PrintAndLog("TYPE : NXP MIFARE (various !DESFire !DESFire EV1)"); break;
			case 0x08: PrintAndLog("TYPE : NXP MIFARE CLASSIC 1k | Plus 2k SL1"); break;
			case 0x09: PrintAndLog("TYPE : NXP MIFARE Mini 0.3k"); break;
			case 0x10: PrintAndLog("TYPE : NXP MIFARE Plus 2k SL2"); break;
			case 0x11: PrintAndLog("TYPE : NXP MIFARE Plus 4k SL2"); break;
			case 0x18: PrintAndLog("TYPE : NXP MIFARE Classic 4k | Plus 4k SL1"); break;
			case 0x20: PrintAndLog("TYPE : NXP MIFARE DESFire 4k | DESFire EV1 2k/4k/8k | Plus 2k/4k SL3 | JCOP 31/41"); break;
			case 0x24: PrintAndLog("TYPE : NXP MIFARE DESFire | DESFire EV1"); break;
			case 0x28: PrintAndLog("TYPE : JCOP31 or JCOP41 v2.3.1"); break;
			case 0x38: PrintAndLog("TYPE : Nokia 6212 or 6131 MIFARE CLASSIC 4K"); break;
			case 0x88: PrintAndLog("TYPE : Infineon MIFARE CLASSIC 1K"); break;
			case 0x98: PrintAndLog("TYPE : Gemplus MPCOS"); break;
			default: PrintAndLog("TYPE : UNKNOWN"); break;
		}

		char q[1024];
		snprintf(q, sizeof(q), "INSERT INTO cards VALUES ('%08"llx"', '%02x', 0)", bytes_to_num(card.uid, card.uidlen), card.sak);
		PrintAndLog(q);

		rc = sqlite3_prepare_v2(db, q, -1, &res, 0);
    
    		if (rc != SQLITE_OK) {        
        		PrintAndLog("Failed to fetch data: %s\n", sqlite3_errmsg(db));
        		sqlite3_close(db);
        
        		return 1;
    		}
		rc = sqlite3_step(res);  

		//selectAllUIDs(db, res);


		sqlite3_finalize(res);	
	}
	sqlite3_close(db);

	if(attack){
		uint64_t tmp;

		PrintAndLog("Starting Attacks %d", number_cards);
		for(int i = 0; i < number_cards;i++){
			PrintAndLog("Attack %d of %d", i + 1, number_cards);

			uint8_t *t = cards[i].uid;

			tmp = bytes_to_num(t, 4);

			attackUID(tmp);
		}
	}

	return 0;
}

int CmdGetAllUIDs(const char *Cmd){

	return AllUIDs(Cmd, false);
}

int CmdAttackAllUIDs(const char *Cmd){

	return AllUIDs(Cmd, true);
}

static command_t CommandTable[] = 
{
	{"help",	CmdHelp,		1, "This help"},
	{"uids",	CmdGetAllUIDs,		1, "Get a list of all UIDs"},
	{"attack",	CmdAttackAllUIDs,		1, "Get a list of all UIDs"},
	{NULL,		NULL,			0, NULL}
};

int CmdBump(const char *Cmd)
{
  CmdsParse(CommandTable, Cmd);
  return 0; 
}

int CmdHelp(const char *Cmd)
{
  CmdsHelp(CommandTable);
  return 0;
}
