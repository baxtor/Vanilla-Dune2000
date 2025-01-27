//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/* $Header: /CounterStrike/NETDLG.CPP 13    10/13/97 2:20p Steve_t $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                         						  *
 *                 Project Name : Command & Conquer                        						  *
 *                                                                         						  *
 *                    File Name : NETDLG.CPP                               						  *
 *                                                                         						  *
 *                   Programmer : Bill Randolph                            						  *
 *                                                                         						  *
 *                   Start Date : January 23, 1995                         						  *
 *                                                                         						  *
 *                  Last Update : December 12, 1995 [BRR]                  						  *
 *                                                                         						  *
 *---------------------------------------------------------------------------------------------*
 *                                                                         						  *
 * These routines establish & maintain peer-to-peer connections between this system				  *
 * and all others in the game.  Each system finds out the IPX address of the others,			  *
 * and forms a direct connection (IPXConnectionClass) to that system.  Systems are				  *
 * found out via broadcast queries.  Every system broadcasts its queries, and every				  *
 * system replies to queries it receives.  At the point when the game owner signals				  *
 * 'OK', every system must know about all the other systems in the game.							  *
 *                                                                         						  *
 * How Bridges are handled:																						  *
 * Currently, bridges are handled by specifying the destination IPX address of the				  *
 * "server" (game owner's system) on the command-line.  This address is used to					  *
 * derive a broadcast address to that destination network, and this system's queries			  *
 * are broadcast over its network & the server's network; replies to the queries come			  *
 * with each system's IPX address attached, so once we have the address, we can form			  *
 * a connection with any system on the bridged net.														  *
 *                                                                         						  *
 * The flaw in this plan is that we can only cross one bridge.  If there are 3 nets				  *
 * bridged (A, B, & C), and the server is on net B, and we're on net A, our broadcasts			  *
 * will reach nets A & B, but not C.  The way to circumvent this (if it becomes a problem)	  *
 * would be to have the server tell us what other systems are in its game, not each				  *
 * individual player's system.  Thus, each system would find out about all the other systems	  *
 * by interacting with the game's owner system (this would be more involved than what			  *
 * I'm doing here).																									  *
 *                                                                         						  *
 * Here's a list of all the different packets sent over the Global Channel:						  *
 *																															  *
 *	NET_QUERY_GAME * (no other data)																			  *
 *	NET_ANSWER_GAME																									  *
 *							Name:					game owner's name * GameInfo:			game's version & open state
 ** NET_QUERY_PLAYER * Name:					name of game we want players to respond for				  *
 *	NET_ANSWER_PLAYER * Name:					player's name														  *
 *							PlayerInfo:			info about player													  *
 * NET_CHAT_ANNOUNCE * Chat:					unique id of the local node, so I can tell				  * if this chat
 *announcement is from myself					  * NET_QUERY_JOIN
 ** Name:					name of player wanting to join								  * PlayerInfo: player's
 *requested house, color, & version range		  * NET_CONFIRM_JOIN
 ** PlayerInfo:			approves player's house & color								  * NET_REJECT_JOIN
 ** Reject.Why:			tells why we got rejected										  * NET_GAME_OPTIONS
 ** ScenarioInfo:		info about scenario												  * NET_SIGN_OFF
 ** Name:					name of player signing off										  * NET_PING
 ** (no other data)																			  * NET_GO
 ** Delay:            value of one-way response time, in frames               *
 * 																														  *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                              						  *
 *   Init_Network -- initializes network stuff                             						  *
 *   Shutdown_Network -- shuts down network stuff                          						  *
 *   Process_Global_Packet -- responds to remote queries                   						  *
 *   Destroy_Connection -- destroys the given connection													  *
 *   Remote_Connect -- handles connecting this user to others              						  *
 *   Net_Join_Dialog -- lets user join an existing game, or start a new one						  *
 *   Request_To_Join -- Sends a JOIN request packet to game owner           	 					  *
 *   Unjoin_Game -- Cancels joining a game                                                     *
 *   Send_Join_Queries -- sends queries for the Join Dialog												  *
 *   Get_Join_Responses -- sends queries for the Join Dialog											  *
 *   Net_New_Dialog -- lets user start a new game															  *
 *   Get_NewGame_Responses -- processes packets for New Game dialog                            *
 *   Compute_Name_CRC -- computes CRC from char string                                         *
 *   Net_Reconnect_Dialog -- Draws/updates the network reconnect dialog                        *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
//	Warning - Most disgusting cpp file of all time. ajw

#include "function.h"
#include "common/framelimit.h"
#include "colrlist.h"
#include "cheklist.h"
#include "drop.h"
#include "edit.h"
#include "msgbox.h"
#include "statbtn.h"
#include "textbtn.h"

#ifdef REMASTER_BUILD // PG Stubs
void Destroy_Connection(int, int)
{
}
bool Process_Global_Packet(GlobalPacketType*, IPXAddressClass*)
{
    return false;
}
bool Client_Remote_Connect(void)
{
    return false;
}
bool Server_Remote_Connect(void)
{
    return false;
}
bool Init_Network(void)
{
    return false;
}
bool Remote_Connect(void)
{
    return false;
}
void Net_Reconnect_Dialog(int reconn, int fresh, int oldest_index, unsigned int timeval)
{
}
#endif

#ifdef NETWORK
#ifdef WINSOCK_IPX
#include "wsproto.h"
#else // WINSOCK_IPX
#include "common/tcpip.h"
#endif // WINSOCK_IPX
#else  // _WIN32
#include "fakesock.h"
#endif // _WIN32

#include <time.h>

#define SHOW_MONO 0
//#define OLDWAY			1

#ifdef FRENCH
#define TXT_HACKHACK "Connexion En Cours..."
#endif
#if defined(ENGLISH) || defined(GERMAN)
#define TXT_HACKHACK Text_String(TXT_CONNECTING)
#endif

#ifdef FIXIT_CSII //	checked - ajw 9/28/98
bool Is_Mission_126x126(char* file_name);
bool Is_Mission_Aftermath(char* file_name);
bool Is_Mission_Counterstrike(char* file_name);
bool bSpecialAftermathScenario(const char* szScenarioDescription)
{
    //	Returns true if szScenarioDescription matches one of the descriptions for Aftermath multiplayer
    //	scenarios that have special Aftermath-only units *embedded* within them.
    if (strcmp(szScenarioDescription, "Booby Traps (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "Central Conflict Extreme (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "Circles of Death (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "Holy Grounds (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "Island Wars Extreme (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "King of the Hills Extreme (Mega 8 players)") == 0
        || strcmp(szScenarioDescription, "The Hills Have Eyes (Mega 8 players)") == 0)
        return true;
    return false;
}
#endif

#ifdef FIXIT_VERSION_3
bool Force_Scenario_Available(const char* szName);
#endif

//---------------------------------------------------------------------------
//	The possible states of the join-game dialog
//---------------------------------------------------------------------------
typedef enum
{
    JOIN_REJECTED = -1,   // we've been rejected
    JOIN_NOTHING,         // we're not trying to join a game
    JOIN_WAIT_CONFIRM,    // we're asking to join, & waiting for confirmation
    JOIN_CONFIRMED,       // we've been confirmed
    JOIN_GAME_START,      // the game we've joined is starting
    JOIN_GAME_START_LOAD, // the game we've joined is starting; load saved game
} JoinStateType;

//---------------------------------------------------------------------------
//	The possible return codes from Get_Join_Responses()
//---------------------------------------------------------------------------
typedef enum
{
    EV_NONE,           // nothing happened
    EV_STATE_CHANGE,   // Join dialog is in a new state
    EV_NEW_GAME,       // a new game formed, or is now open
    EV_NEW_PLAYER,     // a new player was detected
    EV_PLAYER_SIGNOFF, // a player has signed off
    EV_GAME_SIGNOFF,   // a gamed owner has signed off
    EV_GAME_OPTIONS,   // a game options packet was received
    EV_MESSAGE,        // a message was received
} JoinEventType;

//---------------------------------------------------------------------------
//	The possible reasons we're rejected from joining a game
//---------------------------------------------------------------------------
typedef enum
{
    REJECT_DUPLICATE_NAME,  // player's name is a duplicate
    REJECT_GAME_FULL,       // game is full
    REJECT_VERSION_TOO_OLD, // joiner's version is too old
    REJECT_VERSION_TOO_NEW, // joiner's version is too new
    REJECT_BY_OWNER,        // game owner clicked "reject"
    REJECT_DISBANDED,       // game was disbanded
    REJECT_MISMATCH,        // "rules.ini" file mismatch.
} RejectType;

extern char const* EngMisStr[];

#ifndef REMASTER_BUILD // PG
/*
******************************** Prototypes *********************************
*/
static int Net_Join_Dialog(void);
static int Request_To_Join(char* playername, int join_index, HousesType house, PlayerColorType color);
static void Unjoin_Game(char* namebuf,
                        JoinStateType joinstate,
                        ListClass* gamelist,
                        ColorListClass* playerlist,
                        int game_index,
                        int goto_lobby,
                        int msg_x,
                        int msg_y,
                        int msg_h,
                        int send_x,
                        int send_y,
                        int msg_len);
static void Send_Join_Queries(int curgame,
                              JoinStateType joinstate,
                              int gamenow,
                              int playernow,
                              int chatnow,
                              char* myname,
                              int init = 0);
static JoinEventType Get_Join_Responses(JoinStateType* joinstate,
                                        ListClass* gamelist,
                                        ColorListClass* playerlist,
                                        int join_index,
                                        char* my_name,
                                        RejectType* why);
static int Net_New_Dialog(void);
static JoinEventType Get_NewGame_Responses(ColorListClass* playerlist, int* color_used);
void Start_WWChat(ColorListClass* playerlist);
int Update_WWChat(void);

#define PCOLOR_BROWN PCOLOR_GREY

/***********************************************************************************************
 * Init_Network -- initializes network stuff                               						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		none. *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		true = Initialization OK, false = error																  *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/14/1995 BR : Created.                                              						  *
 *=============================================================================================*/
bool Init_Network(void)
{
    NetNumType net;
    NetNodeType node;
#ifdef WINSOCK_IPX
    assert(PacketTransport != NULL);
#endif // WINSOCK_IPX

    //------------------------------------------------------------------------
    //	This call allocates all necessary queue buffers, allocates Real-mode
    //	memory, and commands IPX to start listening on the Global Channel.
    //------------------------------------------------------------------------
    if (!Ipx.Init()) {
        return (false);
    }

    return (true);

} /* end of Init_Network */

/***********************************************************************************************
 * Shutdown_Network -- shuts down network stuff                            						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		none. *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		none. *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/14/1995 BR : Created.                                              						  *
 *=============================================================================================*/
void Shutdown_Network(void)
{
//
// Note: The thought behind this section of code was that if the program
// terminates early, without an EventClass::EXIT event, it still needs to
// tell the other systems that it's gone, so it would send a SIGN_OFF packet.
// BUT, this causes a sync bug if the systems are running slow and this system
// is running ahead of the others; it will send the NET_SIGN_OFF >>before<<
// the other system execute their EventClass::EXIT event, and the other systems
// will kill the connection at some random Frame # & turn my stuff over to
// the computer possibly at different times.
// BRR, 10/29/96
//
#if 0
	//------------------------------------------------------------------------
	// If the Players vector contains at least one name, send a sign-off
	// packet.  If 'Players' is empty, I have no name, so there's no point
	// in sending a sign-off.
	//------------------------------------------------------------------------
	if (Session.Players.Count()) {
		//.....................................................................
		// Build a sign-off packet & send it
		//.....................................................................
		memset (&Session.GPacket, 0, sizeof(GlobalPacketType));
		Session.GPacket.Command = NET_SIGN_OFF;
		strcpy (Session.GPacket.Name, Session.Players[0]->Name);

		Ipx.Send_Global_Message (&Session.GPacket, sizeof (GlobalPacketType),
			0, NULL);
		Ipx.Send_Global_Message (&Session.GPacket, sizeof (GlobalPacketType),
			0, NULL);

		if (Session.IsBridge	&& !Winsock.Get_Connected()) {
			Ipx.Send_Global_Message (&Session.GPacket, sizeof(GlobalPacketType), 0,
				&Session.BridgeNet);
			Ipx.Send_Global_Message (&Session.GPacket, sizeof(GlobalPacketType), 0,
				&Session.BridgeNet);
		}

		//.....................................................................
		//	Wait for the packets to finish going out (or the Global Channel
		// times out)
		//.....................................................................
		for (;;) {
			if (Ipx.Global_Num_Send()==0) {
				break;
			}
			Ipx.Service();
		}
	}
#endif

    //------------------------------------------------------------------------
    //	If I was in a game, I'm not now, so clear the game name
    //------------------------------------------------------------------------
    Session.GameName[0] = 0;

} /* end of Shutdown_Network */

/***********************************************************************************************
 * Process_Global_Packet -- responds to remote queries                     						  *
 *                                                                         						  *
 * The only commands from other systems this routine responds to are NET_QUERY_GAME				  *
 * and NET_QUERY_PLAYER.  The other commands are too context-specific to be able					  *
 * to handle here, such as joining the game or signing off; but this routine handles			  *
 * the majority of the program's needs.																		  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		packet		ptr to packet to process																	  *
 *		address		source address of sender																	  *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		true = packet was processed, false = wasn't															  *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		Session.GameName must have been filled in before this function can be called.				  *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/15/1995 BR : Created.                                              						  *
 *=============================================================================================*/
bool Process_Global_Packet(GlobalPacketType* packet, IPXAddressClass* address)
{
    GlobalPacketType mypacket;

    //------------------------------------------------------------------------
    // If our Players vector is empty, just return.
    //------------------------------------------------------------------------
    if (Session.Players.Count() == 0) {
        return (true);
    }

    //------------------------------------------------------------------------
    //	Another system asking what game this is
    //------------------------------------------------------------------------
    if (packet->Command == NET_QUERY_GAME && Session.NetStealth == 0) {

        //.....................................................................
        //	If the game is closed, let every player respond, and let the sender of
        //	the query sort it all out.  This way, if the game's host exits the game,
        //	the game still shows up on other players' dialogs.
        //	If the game is open, only the game owner may respond.
        //.....................................................................
        if (strlen(Session.GameName) > 0
            && ((!Session.NetOpen) || (Session.NetOpen && !strcmp(Session.Players[0]->Name, Session.GameName)))) {

            memset(&mypacket, 0, sizeof(GlobalPacketType));

            mypacket.Command = NET_ANSWER_GAME;
            strcpy(mypacket.Name, Session.GameName);
            mypacket.GameInfo.IsOpen = Session.NetOpen;

            Ipx.Send_Global_Message(&mypacket, sizeof(GlobalPacketType), 1, address);
        }
        return (true);
    }

    //------------------------------------------------------------------------
    //	Another system asking what player I am
    //------------------------------------------------------------------------
    else if (packet->Command == NET_QUERY_PLAYER && !strcmp(packet->Name, Session.GameName)
             && (strlen(Session.GameName) > 0) && Session.NetStealth == 0) {

        memset(&mypacket, 0, sizeof(GlobalPacketType)); // changed DRD 9/26

        mypacket.Command = NET_ANSWER_PLAYER;
        strcpy(mypacket.Name, Session.Players[0]->Name);
        mypacket.PlayerInfo.House = Session.House;
        mypacket.PlayerInfo.Color = Session.ColorIdx;
        mypacket.PlayerInfo.NameCRC = Compute_Name_CRC(Session.GameName);

        Ipx.Send_Global_Message(&mypacket, sizeof(GlobalPacketType), 1, address);
        return (true);
    }

    return (false);

} /* end of Process_Global_Packet */

/***********************************************************************************************
 * Destroy_Connection -- destroys the given connection													  *
 *                                                                         						  *
 * Call this routine when a connection goes bad, or another player signs off.						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		id			connection ID to destroy; this should be the HousesType of the player			  *
 *             on this connection																				  *
 *		error		0 = user signed off; 1 = connection error; otherwise, no error is shown.		  *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		none. *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   04/22/1995 BR : Created.                                              						  *
 *=============================================================================================*/
void Destroy_Connection(int id, int error)
{
    int i;
    HouseClass* housep;
    char txt[80];

    if (Debug_Print_Events) {
        printf("Destroying connection for house %d (%s)\n", id, HouseClass::As_Pointer((HousesType)id)->IniName);
    }

    //------------------------------------------------------------------------
    //	Do nothing if the house isn't human.
    //------------------------------------------------------------------------
    housep = HouseClass::As_Pointer((HousesType)id);
    if (!housep || !housep->IsHuman)
        return;

    //------------------------------------------------------------------------
    //	Create a message to display to the user
    //------------------------------------------------------------------------
    txt[0] = '\0';
    if (error == 1) {
        sprintf(txt, Text_String(TXT_CONNECTION_LOST), housep->IniName);
    } else if (error == 0) {
        sprintf(txt, Text_String(TXT_LEFT_GAME), housep->IniName);
    }

    if (strlen(txt)) {
        Session.Messages.Add_Message(NULL, 0, txt, housep->RemapColor, TPF_TEXT, Rule.MessageDelay * TICKS_PER_MINUTE);
        Map.Flag_To_Redraw(false);
    }

    //------------------------------------------------------------------------
    // Remove this player from the Players vector
    //------------------------------------------------------------------------
    for (i = 0; i < Session.Players.Count(); i++) {
        if (!stricmp(Session.Players[i]->Name, housep->IniName)) {
            delete Session.Players[i];
            Session.Players.Delete(Session.Players[i]);
            break;
        }
    }

    //------------------------------------------------------------------------
    //	Delete the IPX connection
    //------------------------------------------------------------------------
    Ipx.Delete_Connection(id);

    //------------------------------------------------------------------------
    //	Turn the player's house over to the computer's AI
    //------------------------------------------------------------------------
    housep->IsHuman = false;
    housep->IQ = Rule.MaxIQ;
    strcpy(housep->IniName, Text_String(TXT_COMPUTER));

    Session.NumPlayers--;

    //------------------------------------------------------------------------
    //	If we're the last player left, tell the user.
    //------------------------------------------------------------------------
    if (Session.NumPlayers == 1) {
        sprintf(txt, "%s", Text_String(TXT_JUST_YOU_AND_ME));
        Session.Messages.Add_Message(NULL, 0, txt, housep->RemapColor, TPF_TEXT, Rule.MessageDelay * TICKS_PER_MINUTE);
        Map.Flag_To_Redraw(false);
    }

} /* end of Destroy_Connection */

/***********************************************************************************************
 * Remote_Connect -- handles connecting this user to others                						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		none. *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		true = connections established; false = not															  *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/14/1995 BR : Created.                                              						  *
 *=============================================================================================*/
bool Remote_Connect(void)
{
    int rc;
    int stealth; // original state of Session.NetStealth flag

    //------------------------------------------------------------------------
    //	Init network timing parameters; these values should work for both a
    // "real" network, and a simulated modem network (ie Kali)
    //------------------------------------------------------------------------
    Ipx.Set_Timing(30,   // retry 2 times per second
                   -1,   // ignore max retries
                   600); // give up after 10 seconds

    //------------------------------------------------------------------------
    //	Save the original value of the NetStealth flag, so we can turn stealth
    //	off for now (during this portion of the dialogs, we must show ourselves)
    //------------------------------------------------------------------------
    stealth = Session.NetStealth;
    Session.NetStealth = 0;

    //------------------------------------------------------------------------
    //	Init my game name to 0-length, since I haven't joined any game yet.
    //------------------------------------------------------------------------
    Session.GameName[0] = 0;

    //------------------------------------------------------------------------
    //	The game is now "open" for joining.  Close it as soon as we exit this
    //	routine.
    //------------------------------------------------------------------------
    Session.NetOpen = 1;

    //------------------------------------------------------------------------
    //	Keep looping until something useful happens.
    //------------------------------------------------------------------------
    while (1) {
        //.....................................................................
        //	Pop up the network Join/New dialog
        //.....................................................................
        rc = Net_Join_Dialog();

        //.....................................................................
        //	-1 = user selected Cancel
        //.....................................................................
        if (rc == -1) {
            Session.NetStealth = stealth;
            Session.NetOpen = 0;
            return (false);
        }

        //.....................................................................
        //	0 = user has joined an existing game; save values & return
        //.....................................................................
        else if (rc == 0) {
            Session.Write_MultiPlayer_Settings();
            Session.NetStealth = stealth;
            Session.NetOpen = 0;

            return (true);
        }

        //.....................................................................
        //	1 = user requests New Network Game
        //.....................................................................
        else if (rc == 1) {
            //..................................................................
            //	Pop up the New Network Game dialog; if user selects OK, return
            //	'true'; otherwise, return to the Join Dialog.
            //..................................................................
            if (Net_New_Dialog()) {
                Session.Write_MultiPlayer_Settings();
                Session.NetStealth = stealth;
                Session.NetOpen = 0;

                return (true);
            } else {
                continue;
            }
        }
    }

} /* end of Remote_Connect */

/***********************************************************************************************
 * Net_Join_Dialog -- lets user join an existing game or start a new one                       *
 *                                                                                             *
 * This dialog displays an edit field for the player's name, and a list of all non-stealth-    *
 * mode games.  Clicking once on a game name displays a list of who's in that game.  Clicking  *
 * "New" takes the user to the Net_New dialog, where he waits for other users to join his      *
 * game.  All other input is done through this dialog.                                         *
 *                                                                                             *
 * The dialog has several "states":                                                            *
 *                                                                                             *
 *   1) Initially, it waits for the user to fill in his/her name and then to select Join or    *
 *      New; if New is selected, this dialog is exited.                                        *
 *                                                                                             *
 *   2) If Join is selected, the Join & New buttons are removed, but the Cancel button remains.*
 *      The join request is transmitted to the game's owner, and the message "Waiting for      *
 *      Confirmation" is displayed, until a confirmation or denial is received from the game's *
 *      owner.  The user may click Cancel at this point to cancel the join request.            *
 *      (Once Join is selected, the name editing field is disabled, and becomes a display-only *
 *      field.  If cancel is selected, it reappears as an edit field.) The user can still click*
 *      around & see who's in which games.                                                     *
 *                                                                                             *
 *   3) If the join request is denied, the dialog re-initializes to its pre-join state; the    *
 *      Join & New buttons reappear, & the Name field is available again.                      *
 *                                                                                             *
 *   4) If join confirmation is obtained, the message changes to show all the current game	  *
 *      settings.  The user cannot click around & look at other games any more.					  *
 *                                                                                             *
 * Any game running in Stealth mode won't show up on this dialog.                              *
 *                                                                                             *
 * The 'Players' vector matches one-for-one the contents of the list box.  The local system	  *
 * is always listed first; the IPX Address of the local system will NOT be valid in the		  *
 * Players vector.  The Games & Players vectors should be left filled in even after this		  *
 * routine is exited; their contents are used to form connections to the other players,		  *
 * after the scenario has been loaded.																			  *
 *                                                                                             *
 * The 'Chat' vector contains the address of everyone who sends me a chat announcement.		  *
 * The address field is used to send everyone my outgoing messages.  The LastTime				  *
 * field is used as a timeout; if enough time goes by & we don't hear from this node,			  *
 * we ping him, requesting a CHAT_ANNOUNCE if he's still in chat.  If we don't hear				  *
 * from him after that, we remove him from our list.														  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      -1 = cancel, 0 = OK, 1 = New net game requested                                        *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
static int Net_Join_Dialog(void)
{
    //------------------------------------------------------------------------
    //	Dialog & button dimensions
    //------------------------------------------------------------------------
    int d_dialog_w = 320 * RESFACTOR;                      // dialog width
    int d_dialog_h = 200 * RESFACTOR;                      // dialog height
    int d_dialog_x = ((320 * RESFACTOR - d_dialog_w) / 2); // dialog x-coord
    int d_dialog_y = ((200 * RESFACTOR - d_dialog_h) / 2); // centered y-coord
    int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);       // center x-coord

    int d_txt6_h = 6 * RESFACTOR + 1; // ht of 6-pt text
    int d_margin1 = 17 * RESFACTOR;   // large margin
    int d_margin2 = 7 * RESFACTOR;    // small margin

    int d_name_w = 70 * RESFACTOR;
    int d_name_h = 9 * RESFACTOR;
    int d_name_x = d_dialog_x + (d_dialog_w / 4) - (d_name_w / 2);
    int d_name_y = d_dialog_y + d_margin2 + d_txt6_h + (2 * RESFACTOR);

#ifdef OLDWAY
    int d_gdi_w = 40 * RESFACTOR;
    int d_gdi_h = 9 * RESFACTOR;
    int d_gdi_x = d_dialog_cx - d_gdi_w;
    int d_gdi_y = d_name_y;

    int d_nod_w = 40 * RESFACTOR;
    int d_nod_h = 9 * RESFACTOR;
    int d_nod_x = d_dialog_cx;
    int d_nod_y = d_name_y;
#else
    int d_house_w = 60 * RESFACTOR;
    int d_house_h = (8 * 5 * RESFACTOR);
    int d_house_x = d_dialog_cx - (d_house_w / 2);
    int d_house_y = d_name_y;
#endif

    int d_color_w = 10 * RESFACTOR;
    int d_color_h = 9 * RESFACTOR;
    int d_color_y = d_name_y;
    int d_color_x = d_dialog_x + ((d_dialog_w / 4) * 3) - (d_color_w * 3);

    int d_gamelist_w = 155 * RESFACTOR;
    int d_gamelist_h = ((6 * 6) + 3) * RESFACTOR; // 6 rows high
    int d_gamelist_x = d_dialog_x + d_margin1 - 2 * RESFACTOR;
    int d_gamelist_y = d_color_y + d_color_h + d_margin2 + 2 * RESFACTOR /*KO + d_txt6_h + d_margin2*/;

    // BG		int d_playerlist_w = 113 *RESFACTOR;
    int d_playerlist_w = 118 * RESFACTOR;
    int d_playerlist_h = ((6 * 6) + 3) * RESFACTOR; // 6 rows high
    int d_playerlist_x = d_dialog_x + d_dialog_w - (d_margin1 + d_playerlist_w - 2 * RESFACTOR);
    int d_playerlist_y = d_color_y + d_color_h + d_margin2 + 2 * RESFACTOR /*KO + d_txt6_h + d_margin2*/;

    int d_count_w = 25 * RESFACTOR;
    int d_count_h = d_txt6_h;
    int d_count_x = d_gamelist_x + (d_gamelist_w / 2);
    int d_count_y = d_gamelist_y + d_gamelist_h + d_margin2;

    int d_level_w = 25 * RESFACTOR;
    int d_level_h = d_txt6_h;
    int d_level_x = d_gamelist_x + (d_gamelist_w / 2);
    int d_level_y = d_count_y + d_count_h;

    int d_credits_w = 25 * RESFACTOR;
    int d_credits_h = d_txt6_h;
    int d_credits_x = d_gamelist_x + (d_gamelist_w / 2);
    int d_credits_y = d_level_y + d_level_h;

    int d_aiplayers_w = 25 * RESFACTOR;
    int d_aiplayers_h = d_txt6_h;
    int d_aiplayers_x = d_gamelist_x + (d_gamelist_w / 2);
    int d_aiplayers_y = d_credits_y + d_level_h;

    int d_options_w = d_playerlist_w;
    int d_options_h = ((5 * 6) + 4) * RESFACTOR;
    int d_options_x = d_playerlist_x;
    int d_options_y = d_playerlist_y + d_playerlist_h + d_margin2 - (2 * RESFACTOR);

    int d_message1_w = d_dialog_w - (d_margin1 * 2) + 4 * RESFACTOR;
    int d_message1_h = (14 * d_txt6_h) + 3 * RESFACTOR;
    int d_message1_x = d_dialog_x + (d_dialog_w - d_message1_w) / 2;
    int d_message1_y = d_dialog_y + d_dialog_h - (27 * RESFACTOR + d_message1_h);

    int d_message2_w = d_message1_w;
    int d_message2_h = (8 * d_txt6_h) + 3 * RESFACTOR;
    int d_message2_x = d_message1_x;
    int d_message2_y = d_dialog_y + d_dialog_h - (27 * RESFACTOR + d_message2_h);

#ifdef FRENCH // VG2
    int d_join_w = 60 * RESFACTOR;
#else
    int d_join_w = 40 * RESFACTOR;
#endif
    int d_join_h = 9 * RESFACTOR;
    int d_join_x = d_dialog_x + (d_dialog_w / 6) - (d_join_w / 2);
    int d_join_y = d_dialog_y + d_dialog_h - d_join_h - 8 * RESFACTOR;

    int d_cancel_w = 50 * RESFACTOR;
    int d_cancel_h = 9 * RESFACTOR;
    int d_cancel_x = d_dialog_cx - d_cancel_w / 2;
    int d_cancel_y = d_join_y;

#ifdef FRENCH
    int d_new_w = 60 * RESFACTOR;
#else
    int d_new_w = 40 * RESFACTOR;
#endif
    int d_new_h = 9 * RESFACTOR;
    int d_new_x = d_dialog_x + ((d_dialog_w * 5) / 6) - (d_new_w / 2);
    int d_new_y = d_join_y;

    int d_send_w = d_message1_w;
    int d_send_h = 9 * RESFACTOR;
    int d_send_x = d_message1_x;
    int d_send_y = d_message1_y + d_message1_h;

    //------------------------------------------------------------------------
    //	Button Enumerations
    //------------------------------------------------------------------------
    enum
    {
        BUTTON_NAME = 100,
#ifdef OLDWAY
        BUTTON_GDI,
        BUTTON_NOD,
#else
        BUTTON_HOUSE,
#endif
        BUTTON_GAMELIST,
        BUTTON_PLAYERLIST,
        BUTTON_JOIN,
        BUTTON_CANCEL,
        BUTTON_NEW,
        BUTTON_COUNT,
        BUTTON_LEVEL,
        BUTTON_CREDITS,
        BUTTON_AI_PLAYERS,
        BUTTON_OPTIONS,
    };

    //------------------------------------------------------------------------
    //	Redraw values: in order from "top" to "bottom" layer of the dialog
    //------------------------------------------------------------------------
    typedef enum
    {
        REDRAW_NONE = 0,
        REDRAW_PARMS,
        REDRAW_MESSAGE,
        REDRAW_COLORS,
        REDRAW_BUTTONS,
        REDRAW_BACKGROUND,
        REDRAW_ALL = REDRAW_BACKGROUND
    } RedrawType;

    //------------------------------------------------------------------------
    //	Dialog variables
    //------------------------------------------------------------------------
    RedrawType display = REDRAW_ALL; // redraw level
    bool process = true;             // process while true
    KeyNumType input;
    int cbox_x[] = {
        d_color_x,
        d_color_x + d_color_w,
        d_color_x + (d_color_w * 2),
        d_color_x + (d_color_w * 3),
        d_color_x + (d_color_w * 4),
        d_color_x + (d_color_w * 5),
        d_color_x + (d_color_w * 6),
        d_color_x + (d_color_w * 7),
    };
    char housetext[25] = ""; // buffer for house droplist
    int isdropped = 0;

    JoinStateType joinstate = JOIN_NOTHING; // current "state" of this dialog
    char namebuf[MPLAYER_NAME_MAX] = {0};   // buffer for player's name
    int playertabs[] = {71 * RESFACTOR};    // tabs for player list box
    int optiontabs[] = {8};                 // tabs for player list box
    int game_index = -1;                    // index of currently-selected game
    int join_index = -1;                    // index of game we're joining
    int rc = 0;                             // -1 = user cancelled, 1 = New
    JoinEventType event;                    // event from incoming packet
    int i;                                  // loop counter
    char txt[128];
    char const* p;
    int parms_received = 0; // 1 = game options received
    int found;
    NodeNameType* who;                             // node to add to Players
    RejectType why;                                // reason for rejection
    TTimerClass<SystemTimerClass> lastclick_timer; // time b/w send periods
    int lastclick_idx = 0;                         // index of item last clicked on
    RemapControlType* scheme = GadgetClass::Get_Color_Scheme();
    Session.Options.ScenarioDescription[0] = 0; // Flag that we dont know the scenario name yet

    char* item;
    unsigned int starttime;
    int load_game = 0; // 1 = load saved game
    int goto_lobby;
    bool messages_have_focus = true; // Gadget focus starts on the message system

    //------------------------------------------------------------------------
    //	Buttons
    //------------------------------------------------------------------------
    GadgetClass* commands; // button list

    EditClass name_edt(BUTTON_NAME,
                       namebuf,
                       MPLAYER_NAME_MAX,
                       TPF_TEXT,
                       d_name_x,
                       d_name_y,
                       d_name_w,
                       d_name_h,
                       EditClass::ALPHANUMERIC);

#ifdef OLDWAY
    TextButtonClass gdibtn(BUTTON_GDI, TXT_ALLIES, TPF_BUTTON, d_gdi_x, d_gdi_y, d_gdi_w);
    TextButtonClass nodbtn(BUTTON_NOD, TXT_SOVIET, TPF_BUTTON, d_nod_x, d_nod_y, d_nod_w);
#else
    Fancy_Text_Print("", 0, 0, 0, 0, TPF_TEXT);
    DropListClass housebtn(BUTTON_HOUSE,
                           housetext,
                           sizeof(housetext),
                           TPF_TEXT,
                           d_house_x,
                           d_house_y,
                           d_house_w,
                           d_house_h,
                           MFCD::Retrieve("BTN-UP.SHP"),
                           MFCD::Retrieve("BTN-DN.SHP"));
#endif

    ListClass gamelist(BUTTON_GAMELIST,
                       d_gamelist_x,
                       d_gamelist_y,
                       d_gamelist_w,
                       d_gamelist_h,
                       TPF_TEXT,
                       MFCD::Retrieve("BTN-UP.SHP"),
                       MFCD::Retrieve("BTN-DN.SHP"));
    ColorListClass playerlist(BUTTON_PLAYERLIST,
                              d_playerlist_x,
                              d_playerlist_y,
                              d_playerlist_w,
                              d_playerlist_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));
    TextButtonClass joinbtn(BUTTON_JOIN, TXT_JOIN, TPF_BUTTON, d_join_x, d_join_y, d_join_w);
    TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON, d_cancel_x, d_cancel_y, d_cancel_w);
    TextButtonClass newbtn(BUTTON_NEW, TXT_NEW, TPF_BUTTON, d_new_x, d_new_y, d_new_w);
    GaugeClass countgauge(BUTTON_COUNT, d_count_x, d_count_y, d_count_w, d_count_h);
    GaugeClass levelgauge(BUTTON_LEVEL, d_level_x, d_level_y, d_level_w, d_level_h);
    GaugeClass creditsgauge(BUTTON_CREDITS, d_credits_x, d_credits_y, d_credits_w, d_credits_h);
    GaugeClass aiplayersgauge(BUTTON_AI_PLAYERS, d_aiplayers_x, d_aiplayers_y, d_aiplayers_w, d_aiplayers_h);
    CheckListClass optionlist(BUTTON_OPTIONS,
                              d_options_x,
                              d_options_y,
                              d_options_w,
                              d_options_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));
    StaticButtonClass descrip(
        0, "", TPF_CENTER | TPF_TEXT, d_dialog_x + 16 * RESFACTOR, d_name_y, d_dialog_w - 32 * RESFACTOR, d_txt6_h + 1);
    StaticButtonClass staticcount(0, "     ", TPF_TEXT, d_count_x + d_count_w + 2 * RESFACTOR, d_count_y);
    StaticButtonClass staticlevel(0, "     ", TPF_TEXT, d_level_x + d_level_w + 2 * RESFACTOR, d_level_y);
    StaticButtonClass staticcredits(0, "          ", TPF_TEXT, d_credits_x + d_credits_w + 2 * RESFACTOR, d_credits_y);
    StaticButtonClass staticaiplayers(
        0, "     ", TPF_TEXT, d_aiplayers_x + d_aiplayers_w + 2 * RESFACTOR, d_aiplayers_y);

    //------------------------------------------------------------------------
    //	Init the button states
    //------------------------------------------------------------------------
    //........................................................................
    // Name & Color
    //........................................................................
    Session.ColorIdx = Session.PrefColor; // init my preferred color
    strcpy(namebuf, Session.Handle);      // set my name
    name_edt.Set_Text(namebuf, MPLAYER_NAME_MAX);
    if (Session.ColorIdx == PCOLOR_DIALOG_BLUE) {
        name_edt.Set_Color(&ColorRemaps[PCOLOR_REALLY_BLUE]);
    } else {
        name_edt.Set_Color(&ColorRemaps[Session.ColorIdx]);
    }

    //........................................................................
    // List boxes
    //........................................................................
    playerlist.Set_Tabs(playertabs);
    playerlist.Set_Selected_Style(ColorListClass::SELECT_NORMAL);

    optionlist.Set_Tabs(optiontabs);
    optionlist.Set_Read_Only(1);

    optionlist.Add_Item(Text_String(TXT_BASES));
    optionlist.Add_Item(Text_String(TXT_ORE_SPREADS));
    optionlist.Add_Item(Text_String(TXT_CRATES));
    optionlist.Add_Item(Text_String(TXT_CAPTURE_THE_FLAG));
    optionlist.Add_Item(Text_String(TXT_SHADOW_REGROWS));

    optionlist.Check_Item(0, Rule.IsMPBasesOn);
    optionlist.Check_Item(1, Rule.IsMPTiberiumGrow);
    optionlist.Check_Item(2, Rule.IsMPCrates);
    optionlist.Check_Item(3, Rule.IsMPCaptureTheFlag);
    optionlist.Check_Item(4, Rule.IsMPShadowGrow);

    //........................................................................
    // House buttons
    //........................................................................
#ifdef OLDWAY
    if (Session.House == HOUSE_GOOD) {
        gdibtn.Turn_On();
    } else {
        nodbtn.Turn_On();
    }
#else
    for (HousesType house = HOUSE_USSR; house <= HOUSE_FRANCE; house++) {
        //	for (HousesType house = HOUSE_FIRST; house <= HOUSE_TURKEY; house++) {
        housebtn.Add_Item(Text_String(HouseTypeClass::As_Reference(house).Full_Name()));
    }
    housebtn.Set_Selected_Index(Session.House - HOUSE_USSR);
    housebtn.Set_Read_Only(true);
#endif

    //........................................................................
    // Option gauges
    //........................................................................
    countgauge.Use_Thumb(0);
    countgauge.Set_Maximum(SessionClass::CountMax[Session.Options.Bases]
                           - SessionClass::CountMin[Session.Options.Bases]);
    countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);

    levelgauge.Use_Thumb(0);
    levelgauge.Set_Maximum(MPLAYER_BUILD_LEVEL_MAX - 1);
    levelgauge.Set_Value(BuildLevel - 1);

    creditsgauge.Use_Thumb(0);
    creditsgauge.Set_Maximum(Rule.MPMaxMoney);
    creditsgauge.Set_Value(Session.Options.Credits);

    aiplayersgauge.Use_Thumb(0);
    aiplayersgauge.Set_Maximum(Session.Options.AIPlayers);
    aiplayersgauge.Set_Value(Session.Options.AIPlayers);

    Fancy_Text_Print("", 0, 0, scheme, TBLACK, TPF_TEXT);

    Session.Messages.Init(d_message1_x + 1 * RESFACTOR,
                          d_message1_y + 1 * RESFACTOR,
                          14,
                          MAX_MESSAGE_LENGTH,
                          d_txt6_h,
                          d_send_x + 1 * RESFACTOR,
                          d_send_y + 1 * RESFACTOR,
                          1,
                          20,
                          MAX_MESSAGE_LENGTH - 5,
                          d_message2_w);
    Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx,
                              TPF_TEXT,
                              NULL,
                              '_',
                              d_message2_w);
    Session.WWChat = 0;

    lastclick_timer = 0;

    //------------------------------------------------------------------------
    //	Clear the list of games, players, and the chat list
    //------------------------------------------------------------------------
    Clear_Vector(&Session.Games);
    Clear_Vector(&Session.Players);
    Clear_Vector(&Session.Chat);

    //------------------------------------------------------------------------
    // Add myself to the Chat vector
    //------------------------------------------------------------------------
    who = new NodeNameType;
    strcpy(who->Name, namebuf);
    who->Chat.LastTime = 0;
    who->Chat.LastChance = 0;
    who->Chat.Color = Session.GPacket.Chat.Color;
    Session.Chat.Add(who);

    //------------------------------------------------------------------------
    // Create the "Lobby" game name on the games list, and create a bogus
    // node for the gamelist, so Games[i] will always match gamelist[i]
    //------------------------------------------------------------------------
    who = new NodeNameType;
    strcpy(who->Name, "");
    who->Game.IsOpen = 0;
    who->Game.LastTime = 0;
    Session.Games.Add(who);
    item = new char[MPLAYER_NAME_MAX];
    strcpy(item, Text_String(TXT_LOBBY));
    gamelist.Add_Item(item);
    gamelist.Set_Selected_Index(0);
    game_index = 0;

    //------------------------------------------------------------------------
    //	Send game-name query & chat announcement; also, initialize timers.
    //------------------------------------------------------------------------
    Send_Join_Queries(game_index, joinstate, 1, 0, 1, namebuf, 1);
    Load_Title_Page(true);
    CCPalette.Set(); // GamePalette.Set();

//------------------------------------------------------------------------
//	Init Mono Output
//------------------------------------------------------------------------
#if (SHOW_MONO)
    Ipx.Configure_Debug(-1, sizeof(GlobalHeaderType), sizeof(NetCommandType), GlobalPacketNames, 0, 13);
    Ipx.Mono_Debug_Print(-1, 1);
#endif
    //------------------------------------------------------------------------
    //	Processing loop
    //------------------------------------------------------------------------
    while (process) {
#if (SHOW_MONO)
        Ipx.Mono_Debug_Print(-1, 0);
#endif

        /*
        ** Kludge to make sure we redraw the message input line when it loses focus.
        ** If we dont do this then the cursor doesnt disappear.
        */
        if (messages_have_focus) {
            if (name_edt.Has_Focus()) {
                if (display < REDRAW_MESSAGE)
                    display = REDRAW_MESSAGE;
                messages_have_focus = false;
            }
        } else {
            if (!name_edt.Has_Focus()) {
                messages_have_focus = true;
            }
        }

        /*
        ** Collapse the country list if we are going to redraw the game list
        */
        if (gamelist.Is_To_Redraw() && housebtn.IsDropped) {
            housebtn.Collapse();
            if (display < REDRAW_BACKGROUND)
                display = REDRAW_BACKGROUND;
        }

        /*
        ** If we have just received input focus again after running in the background then
        ** we need to redraw.
        */
        if (AllSurfaces.SurfacesRestored) {
            AllSurfaces.SurfacesRestored = false;
            display = REDRAW_ALL;
        }

        //.....................................................................
        //	Refresh display if needed
        //.....................................................................
        if (display) {
            Hide_Mouse();

            //..................................................................
            //	Redraw backgound & dialog box
            //..................................................................
            if (display >= REDRAW_BACKGROUND) {
                housebtn.Collapse();
                Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);

                //...............................................................
                //	Dialog & Field labels
                //...............................................................
                Fancy_Text_Print(TXT_CHANNEL_GAMES,
                                 d_gamelist_x + (d_gamelist_w / 2),
                                 d_gamelist_y - d_txt6_h,
                                 scheme,
                                 TBLACK,
                                 TPF_CENTER | TPF_TEXT);
                Fancy_Text_Print(TXT_PLAYERS,
                                 d_playerlist_x + (d_playerlist_w / 2),
                                 d_playerlist_y - d_txt6_h,
                                 scheme,
                                 TBLACK,
                                 TPF_CENTER | TPF_TEXT);

                //...............................................................
                // For game-browsing, label the name, side, & color buttons:
                //...............................................................
                if (joinstate < JOIN_CONFIRMED) {
                    Fancy_Text_Print(TXT_YOUR_NAME,
                                     d_name_x + (d_name_w / 2),
                                     d_name_y - d_txt6_h,
                                     scheme,
                                     TBLACK,
                                     TPF_CENTER | TPF_TEXT);

#ifdef OLDWAY
                    Fancy_Text_Print(
                        TXT_SIDE_COLON, d_gdi_x + d_gdi_w, d_gdi_y - d_txt6_h, scheme, TBLACK, TPF_CENTER | TPF_TEXT);
#else
                    Fancy_Text_Print(TXT_SIDE_COLON,
                                     d_house_x + (d_house_w / 2),
                                     d_house_y - d_txt6_h,
                                     scheme,
                                     TBLACK,
                                     TPF_CENTER | TPF_TEXT);
#endif

                    Fancy_Text_Print(TXT_COLOR_COLON,
                                     d_dialog_x + ((d_dialog_w / 4) * 3),
                                     d_color_y - d_txt6_h,
                                     scheme,
                                     TBLACK,
                                     TPF_CENTER | TPF_TEXT);
                } else {
                    //...............................................................
                    // If we're joined to a game, just print the player's name & side.
                    //...............................................................
#ifdef OLDWAY
                    if (Session.House == HOUSE_GOOD) {
                        sprintf(txt, Text_String(TXT_S_PLAYING_S), namebuf, Text_String(TXT_ALLIES));
                    } else {
                        sprintf(txt, Text_String(TXT_S_PLAYING_S), namebuf, Text_String(TXT_SOVIET));
                    }
#else  // OLDWAY
                    sprintf(txt,
                            Text_String(TXT_S_PLAYING_S),
                            namebuf,
                            Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
                    Fancy_Text_Print(txt,
                                     d_dialog_cx,
                                     d_dialog_y + d_margin2 + (1 * RESFACTOR),
                                     (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                              : &ColorRemaps[Session.ColorIdx],
                                     TBLACK,
                                     TPF_CENTER | TPF_TEXT);
                }

                //...............................................................
                //	Rebuild the button list
                //...............................................................
                cancelbtn.Zap();
                gamelist.Zap();
                playerlist.Zap();
#ifdef OLDWAY
                gdibtn.Zap();
                nodbtn.Zap();
#else
                housebtn.Zap();
#endif
                name_edt.Zap();
                joinbtn.Zap();
                newbtn.Zap();
                countgauge.Zap();
                levelgauge.Zap();
                creditsgauge.Zap();
                aiplayersgauge.Zap();
                staticcount.Zap();
                staticlevel.Zap();
                staticcredits.Zap();
                staticaiplayers.Zap();

                commands = &cancelbtn;
                gamelist.Add_Tail(*commands);
                playerlist.Add_Tail(*commands);

                //...............................................................
                //	Only add the name edit field, the House, Join & New buttons if
                //	we're doing nothing, or we've just been rejected.
                //...............................................................
                if (joinstate < JOIN_CONFIRMED) {
#ifdef OLDWAY
                    gdibtn.Add_Tail(*commands);
                    nodbtn.Add_Tail(*commands);
#else
                    housebtn.Add_Tail(*commands);
#endif
                    name_edt.Add_Tail(*commands);
                    joinbtn.Add_Tail(*commands);
                    newbtn.Add_Tail(*commands);
                } else {
                    countgauge.Add_Tail(*commands);
                    levelgauge.Add_Tail(*commands);
                    creditsgauge.Add_Tail(*commands);
                    aiplayersgauge.Add_Tail(*commands);
                    staticcount.Add_Tail(*commands);
                    staticlevel.Add_Tail(*commands);
                    staticcredits.Add_Tail(*commands);
                    staticaiplayers.Add_Tail(*commands);
                    optionlist.Add_Tail(*commands);
                    descrip.Add_Tail(*commands);
                }
            }

            //..................................................................
            //	Redraw buttons
            //..................................................................
            if (display >= REDRAW_BUTTONS) {
                aiplayersgauge.Set_Maximum(Rule.MaxPlayers - Session.Players.Count());
                if (Session.Options.AIPlayers > Rule.MaxPlayers - Session.Players.Count()) {
                    aiplayersgauge.Set_Value(Rule.MaxPlayers - Session.Players.Count());
                }
                commands->Draw_All();
            }

            //..................................................................
            //	Draw the color boxes
            //..................................................................
            if (display >= REDRAW_COLORS && joinstate < JOIN_CONFIRMED) {
                for (i = 0; i < MAX_MPLAYER_COLORS; i++) {
                    LogicPage->Fill_Rect(cbox_x[i] + 1,
                                         d_color_y + 1,
                                         cbox_x[i] + 1 + d_color_w - 2 * RESFACTOR,
                                         d_color_y + 1 + d_color_h - 2,
                                         ColorRemaps[i].Box);
                    //						(i == PCOLOR_DIALOG_BLUE) ? ColorRemaps[PCOLOR_REALLY_BLUE].Box :
                    //ColorRemaps[i].Box);

                    if (i == Session.ColorIdx) {
                        Draw_Box(cbox_x[i], d_color_y, d_color_w, d_color_h, BOXSTYLE_DOWN, false);
                    } else {
                        Draw_Box(cbox_x[i], d_color_y, d_color_w, d_color_h, BOXSTYLE_RAISED, false);
                    }
                }
            }

            //..................................................................
            // Draw the message system; erase old messages first
            //..................................................................
            if (display >= REDRAW_MESSAGE) {
                if (joinstate == JOIN_CONFIRMED) {
                    Draw_Box(d_message2_x, d_message2_y, d_message2_w, d_message2_h, BOXSTYLE_BOX, true);
                } else {
                    Draw_Box(d_message1_x, d_message1_y, d_message1_w, d_message1_h, BOXSTYLE_BOX, true);
                }
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true);
                Session.Messages.Draw();
            }

            //..................................................................
            // Redraw the game options
            //..................................................................
            if (display >= REDRAW_PARMS && parms_received && joinstate >= JOIN_CONFIRMED) {

                //...............................................................
                // Scenario title
                //...............................................................
                //				LogicPage->Fill_Rect(d_dialog_x + 16 *RESFACTOR, d_name_y, d_dialog_x + d_dialog_w - 16
                //*RESFACTOR, d_name_y + d_txt6_h, BLACK);

                p = Text_String(TXT_SCENARIO_COLON);
                if (Session.Options.ScenarioDescription[0]) {

                    // EW - Scenario language translation goes here!!!!!!!! VG
                    int ii;
                    for (ii = 0; EngMisStr[ii] != NULL; ii++) {
                        if (!strcmp(Session.Options.ScenarioDescription, EngMisStr[ii])) {
#if defined(GERMAN) || defined(FRENCH)
                            sprintf(txt, "%s %s", p, EngMisStr[ii + 1]);
#else
                            sprintf(txt, "%s %s", p, Session.Options.ScenarioDescription);
#endif
                            break;
                        }
                    }
                    if (EngMisStr[ii] == NULL) {
                        sprintf(txt, "%s %s", p, Session.Options.ScenarioDescription);
                    }
                    descrip.Set_Text(txt);

                    //					sprintf(txt, "%s %s", p, Session.Options.ScenarioDescription);
                    //					descrip.Set_Text(txt);
                    //					Fancy_Text_Print("%s %s", d_dialog_cx, d_name_y, scheme, BLACK, TPF_TEXT | TPF_CENTER,
                    //p, Session.Options.ScenarioDescription);
                } else {
                    sprintf(txt, "%s %s", p, Text_String(TXT_NOT_FOUND));
                    descrip.Set_Text(txt);
                    //					Fancy_Text_Print("%s %s", d_dialog_cx, d_name_y, &ColorRemaps[PCOLOR_RED], TBLACK,
                    //TPF_TEXT | TPF_CENTER, p, Text_String(TXT_NOT_FOUND));
                }
                //...............................................................
                // Unit count, tech level, credits, ai players
                //...............................................................
                //				LogicPage->Fill_Rect(d_count_x + d_count_w + 2 *RESFACTOR, d_count_y, d_count_x + d_count_w +
                //35 *RESFACTOR, d_aiplayers_y + d_aiplayers_h+RESFACTOR, BLACK);

                Fancy_Text_Print(TXT_COUNT, d_count_x - 2 * RESFACTOR, d_count_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);

                sprintf(txt, "%d", Session.Options.UnitCount);
                staticcount.Set_Text(txt);
                staticcount.Draw_Me();
                //				Fancy_Text_Print(txt, d_count_x + d_count_w + 2 *RESFACTOR, d_count_y, scheme, BLACK,
                //TPF_TEXT);

                Fancy_Text_Print(TXT_LEVEL, d_level_x - 2 * RESFACTOR, d_level_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
                if (BuildLevel <= MPLAYER_BUILD_LEVEL_MAX) {
                    sprintf(txt, "%d", BuildLevel);
                } else {
                    sprintf(txt, "**");
                }
                staticlevel.Set_Text(txt);
                staticlevel.Draw_Me();
                //				Fancy_Text_Print(txt, d_level_x + d_level_w + 2 *RESFACTOR, d_level_y, scheme, BLACK,
                //TPF_TEXT);

                Fancy_Text_Print(
                    TXT_CREDITS_COLON, d_credits_x - 2 * RESFACTOR, d_credits_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
                sprintf(txt, "%d", Session.Options.Credits);
                staticcredits.Set_Text(txt);
                staticcredits.Draw_Me();
                //				Fancy_Text_Print(txt, d_credits_x + d_credits_w + 2 *RESFACTOR, d_credits_y, scheme, BLACK,
                //TPF_TEXT);

                Fancy_Text_Print(TXT_AI_PLAYERS_COLON,
                                 d_aiplayers_x - 2 * RESFACTOR,
                                 d_aiplayers_y,
                                 scheme,
                                 TBLACK,
                                 TPF_TEXT | TPF_RIGHT);
                sprintf(txt, "%d", Session.Options.AIPlayers);
                staticaiplayers.Set_Text(txt);
                staticaiplayers.Draw_Me();
                //				Fancy_Text_Print(txt, d_aiplayers_x + d_aiplayers_w + 2 *RESFACTOR, d_aiplayers_y, scheme,
                //BLACK, TPF_TEXT);
            }

            Show_Mouse();
            display = REDRAW_NONE;
        }

        //.....................................................................
        //	Get user input
        //.....................................................................
        input = commands->Input();

        if (input & KN_BUTTON) {
            housebtn.Collapse();
            display = REDRAW_BACKGROUND;
        }

        //.....................................................................
        //	Process input
        //.....................................................................
        switch (input) {

        //..................................................................
        // Mouse Click:
        // If we're joined to a game, display an error if the user tries to
        // modify a read-only control.
        // If user clicks on a color button:
        //	- If we've joined a game, don't allow a new color selection
        //	- otherwise, select that color
        // - Change the color of the user's name & message field to match
        //   the newly-selected color.
        //..................................................................
        case KN_LMOUSE:
            if (joinstate > JOIN_NOTHING) {
                if ((Get_Mouse_X() >= d_count_x && Get_Mouse_X() <= d_count_x + d_count_w && Get_Mouse_Y() >= d_count_y
                     && Get_Mouse_Y() <= d_aiplayers_y + d_aiplayers_h)
                    || (Get_Mouse_X() >= d_options_x && Get_Mouse_X() <= d_options_x + d_options_w
                        && Get_Mouse_Y() >= d_options_y && Get_Mouse_Y() <= d_options_y + d_options_h)) {
                    Session.Messages.Add_Message(
                        NULL, 0, (char*)Text_String(TXT_ONLY_HOST_CAN_MODIFY), PCOLOR_BROWN, TPF_TEXT, 1200);
                    Sound_Effect(VOC_SYS_ERROR);
                    display = REDRAW_MESSAGE;
                }
                break;
            }

            if (Keyboard->MouseQX > cbox_x[0] && Keyboard->MouseQX < (cbox_x[MAX_MPLAYER_COLORS - 1] + d_color_w)
                && Keyboard->MouseQY > d_color_y && Keyboard->MouseQY < (d_color_y + d_color_h)) {
                Session.PrefColor = (PlayerColorType)((Keyboard->MouseQX - cbox_x[0]) / d_color_w);
                Session.ColorIdx = Session.PrefColor;

                if (Session.ColorIdx == PCOLOR_DIALOG_BLUE) {
                    name_edt.Set_Color(&ColorRemaps[PCOLOR_REALLY_BLUE]);
                } else {
                    name_edt.Set_Color(
                        &ColorRemaps[(Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx]);
                }
                name_edt.Flag_To_Redraw();

                Session.Messages.Set_Edit_Color((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                         : Session.ColorIdx);

                display = REDRAW_COLORS;
            }
            break;

        //..................................................................
        //	User clicks on the game list:
        //..................................................................
        case (BUTTON_GAMELIST | KN_BUTTON):
            //...............................................................
            // Handle a double-click
            //...............................................................
            if (lastclick_timer < 30 && gamelist.Current_Index() == lastclick_idx) {

                //............................................................
                // If we're in a game, & the item clicked on is a different
                // game, un-join the game we're in.
                //............................................................
                if ((joinstate == JOIN_CONFIRMED || joinstate == JOIN_WAIT_CONFIRM) && lastclick_idx != game_index) {
                    if (gamelist.Current_Index() == 0) {
                        goto_lobby = 1;
                    } else {
                        goto_lobby = 0;
                    }
                    Unjoin_Game(namebuf,
                                joinstate,
                                &gamelist,
                                &playerlist,
                                game_index,
                                goto_lobby,
                                d_message1_x,
                                d_message1_y,
                                d_txt6_h,
                                d_send_x,
                                d_send_y,
                                MAX_MESSAGE_LENGTH);
                    joinstate = JOIN_NOTHING;
                    display = REDRAW_ALL;

                    //.........................................................
                    // Clear the Player vector & the player list box, since
                    // our game_index has changed.
                    //.........................................................
                    Clear_Listbox(&playerlist);
                    Clear_Vector(&Session.Players);
                    game_index = gamelist.Current_Index();
                }
                //............................................................
                // If we clicked on another game, join that game.
                //............................................................
                if (joinstate != JOIN_CONFIRMED && joinstate != JOIN_WAIT_CONFIRM && lastclick_idx > 0) {
                    gamelist.Set_Selected_Index(lastclick_idx);
                    game_index = lastclick_idx;
                    name_edt.Clear_Focus();
                    name_edt.Flag_To_Redraw();
                    strcpy(Session.Handle, namebuf);
#ifndef OLDWAY
                    Session.House = (HousesType)(housebtn.Current_Index() + HOUSE_USSR);
#endif
                    join_index = gamelist.Current_Index();
                    parms_received = 0;
                    if (Request_To_Join(namebuf, join_index, Session.House, Session.ColorIdx)) {
                        joinstate = JOIN_WAIT_CONFIRM;
                    } else {
                        display = REDRAW_ALL;
                    }
                }
                //............................................................
                // Otherwise, we must have joined the lobby
                //............................................................
                if (game_index == 0) {
                    Clear_Listbox(&playerlist);
                    Session.Messages.Init(d_message1_x + 1 * RESFACTOR,
                                          d_message1_y + 1 * RESFACTOR,
                                          14,
                                          MAX_MESSAGE_LENGTH,
                                          d_txt6_h,
                                          d_send_x + 1 * RESFACTOR,
                                          d_send_y + 1 * RESFACTOR,
                                          1,
                                          20,
                                          MAX_MESSAGE_LENGTH - 5,
                                          d_message2_w);
                    Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                       : Session.ColorIdx,
                                              TPF_TEXT,
                                              NULL,
                                              '_',
                                              d_message2_w);
                    Session.WWChat = 0;
                    display = REDRAW_ALL;
                }
            } else {
                //...............................................................
                // Handle a single-click
                //...............................................................
                //............................................................
                // If no double-click occurred, set the last-clicked index
                // & double-click timer.
                //............................................................
                lastclick_timer = 0;
                lastclick_idx = gamelist.Current_Index();

                //............................................................
                //	If we've joined a game, don't allow the selected item to
                // change
                //............................................................
                if (joinstate == JOIN_CONFIRMED || joinstate == JOIN_WAIT_CONFIRM) {
                    gamelist.Set_Selected_Index(game_index);
                }

                //............................................................
                // If we're not in a game, and the user clicks on a different
                // entry, clear the player list & send a player query;
                // init the click timer, to detect a double-click of this item.
                //............................................................
                else if (gamelist.Current_Index() != game_index) {

                    Clear_Listbox(&playerlist);
                    Clear_Vector(&Session.Players);
                    game_index = gamelist.Current_Index();
                    Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
                }
            }
            break;

#ifdef OLDWAY
        //..................................................................
        //	House Buttons: set the player's desired House
        //..................................................................
        case (BUTTON_GDI | KN_BUTTON):
            Session.House = HOUSE_GOOD;
            gdibtn.Turn_On();
            nodbtn.Turn_Off();
            break;

        case (BUTTON_NOD | KN_BUTTON):
            Session.House = HOUSE_BAD;
            gdibtn.Turn_Off();
            nodbtn.Turn_On();
            break;
#else
#endif

        //..................................................................
        //	JOIN: send a join request packet & switch to waiting-for-
        // confirmation mode.
        //..................................................................
        case (BUTTON_JOIN | KN_BUTTON):
            name_edt.Clear_Focus();
            name_edt.Flag_To_Redraw();
            strcpy(Session.Handle, namebuf);
#ifndef OLDWAY
            Session.House = (HousesType)(housebtn.Current_Index() + HOUSE_USSR);
#endif
            join_index = gamelist.Current_Index();
            parms_received = 0;
            if (Request_To_Join(namebuf, join_index, Session.House, Session.ColorIdx)) {
                joinstate = JOIN_WAIT_CONFIRM;
            } else {
                display = REDRAW_MESSAGE;
            }
            break;

        //..................................................................
        // ESC / CANCEL: send a SIGN_OFF
        // - If we're part of a game, stay in this dialog; otherwise, exit
        //..................................................................
        case (KN_ESC):
        case (BUTTON_CANCEL | KN_BUTTON):
            if (housebtn.IsDropped) {
                housebtn.Collapse();
            }
            //...............................................................
            //	If we're joined to a game, make extra sure the other players in
            //	that game know I'm exiting; send my SIGN_OFF as an ack-required
            //	packet.  Don't send this to myself (index 0).
            //...............................................................
            if (joinstate == JOIN_CONFIRMED) {
                Unjoin_Game(namebuf,
                            joinstate,
                            &gamelist,
                            &playerlist,
                            game_index,
                            1,
                            d_message1_x,
                            d_message1_y,
                            d_txt6_h,
                            d_send_x,
                            d_send_y,
                            MAX_MESSAGE_LENGTH);
                joinstate = JOIN_NOTHING;
                display = REDRAW_ALL;
            } else {
                //...............................................................
                // If I'm not joined to a game, send a SIGN_OFF to all players
                // in my Chat vector (but not to myself, index 0)
                //...............................................................
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_SIGN_OFF;
                strcpy(Session.GPacket.Name, namebuf);
                for (i = 1; i < Session.Chat.Count(); i++) {
                    Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Chat[i]->Address));
                    Ipx.Service();
                }

                //............................................................
                //	Now broadcast a SIGN_OFF just to be thorough
                //............................................................
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

                while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                    ;

                //............................................................
                // exit the dialog
                //............................................................
                process = false;
                rc = -1;
            }
            break;

        //..................................................................
        //	NEW: bail out with return code 1
        //..................................................................
        case (BUTTON_NEW | KN_BUTTON):
            //...............................................................
            //	Force user to enter a name
            //...............................................................
            if (strlen(namebuf) == 0) {
                Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_NAME_ERROR), PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_SYS_ERROR);
                display = REDRAW_MESSAGE;
                break;
            }

            //...............................................................
            //	Ensure name is unique
            //...............................................................
            found = 0;
            for (i = 1; i < Session.Games.Count(); i++) {
                if (!stricmp(Session.Games[i]->Name, namebuf)) {
                    found = 1;
                    Session.Messages.Add_Message(
                        NULL, 0, (char*)Text_String(TXT_GAMENAME_MUSTBE_UNIQUE), PCOLOR_BROWN, TPF_TEXT, 1200);
                    Sound_Effect(VOC_SYS_ERROR);
                    display = REDRAW_MESSAGE;
                    break;
                }
            }

            if (found) {
                break;
            }

            //...............................................................
            //	Save player & game name
            //...............................................................
            strcpy(Session.Handle, namebuf);
            strcpy(Session.GameName, namebuf);
#ifndef OLDWAY
            Session.House = (HousesType)(housebtn.Current_Index() + HOUSE_USSR);
#endif

            name_edt.Clear_Focus();
            name_edt.Flag_To_Redraw();

            rc = 1;
            process = false;
            break;

        //..................................................................
        //	Default: manage the inter-player messages
        //..................................................................
        default:
            if (Session.Messages.Manage()) {
                display = REDRAW_MESSAGE;
            }

            //...............................................................
            //	Service keyboard input for any message being edited.
            //...............................................................
            i = Session.Messages.Input(input);

            //...............................................................
            //	If 'Input' returned 1, it means refresh the message display.
            // (We have to redraw the edit line, to erase the cursor.)
            //...............................................................
            if (i == 1) {
                Hide_Mouse();
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true);
                Session.Messages.Draw();
                Show_Mouse();
            } else if (i == 2) {
                //...............................................................
                //	If 'Input' returned 2, it means redraw the message display.
                // Rather than setting 'display', which would redraw all msgs,
                // we only need to erase & redraw the edit box here.
                //...............................................................
                Hide_Mouse();
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true);
                Session.Messages.Draw();
                Show_Mouse();
            } else if (i == 3 || i == 4) {
                //...............................................................
                //	If 'Input' returned 3, it means send the current message.
                //...............................................................
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_MESSAGE;
                strcpy(Session.GPacket.Name, namebuf);
                if (i == 3) {
                    strcpy(Session.GPacket.Message.Buf, Session.Messages.Get_Edit_Buf());
                } else {
                    strcpy(Session.GPacket.Message.Buf, Session.Messages.Get_Overflow_Buf());
                    Session.Messages.Clear_Overflow_Buf();
                }
                Session.GPacket.Message.Color = Session.ColorIdx;
                Session.GPacket.Message.NameCRC = Compute_Name_CRC(Session.GameName);

                //............................................................
                //	If we're joined in a game, send the message to every player
                // in our player list.  Skip the local system (index 0).
                //............................................................
                if (joinstate == JOIN_CONFIRMED) {
                    for (i = 1; i < Session.Players.Count(); i++) {
                        Ipx.Send_Global_Message(
                            &Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                        Ipx.Service();
                    }
                } else {
                    //............................................................
                    // Otherwise, send the message to all players in our chat list.
                    //............................................................
                    for (i = 1; i < Session.Chat.Count(); i++) {
                        Ipx.Send_Global_Message(
                            &Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Chat[i]->Address));
                        Ipx.Service();
                    }
                    if (Obfuscate(Session.GPacket.Message.Buf) == 0x72A47EF6) {
                        Session.WWChat = 1;
                        Clear_Listbox(&playerlist);
                        Start_WWChat(&playerlist);
                    }
                }

                //............................................................
                //	Add the message to our own list, since we're not in the
                // player list on this dialog.
                //............................................................
                Session.Messages.Add_Message(
                    Session.GPacket.Name,
                    (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                          : Session.GPacket.Message.Color,
                    Session.GPacket.Message.Buf,
                    (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx,
                    TPF_TEXT,
                    -1);
                Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                   : Session.ColorIdx,
                                          TPF_TEXT,
                                          NULL,
                                          '_',
                                          d_message2_w);
                display = REDRAW_MESSAGE;
            }

#ifdef OBSOLETE
            //
            // This is for the old drop-down list of houses, not used any more.
            //
            if (housebtn.IsDropped) {
                isdropped = 1;
            } else if (isdropped) {
                display = REDRAW_ALL;
            }
#endif

            break;
        }

        //.....................................................................
        //	Resend our query packets
        //.....................................................................
        Send_Join_Queries(game_index, joinstate, 0, 0, 0, namebuf);

        //.....................................................................
        //	Process incoming packets
        //.....................................................................
        event = Get_Join_Responses(&joinstate, &gamelist, &playerlist, join_index, namebuf, &why);

        //.....................................................................
        //	If we've changed state, redraw everything; if we're starting the game,
        //	break out of the loop.  If we've just joined, send out a player query
        //	so I'll get added to the list instantly.
        //.....................................................................
        if (event == EV_STATE_CHANGE) {
            display = REDRAW_ALL;

            if (joinstate == JOIN_GAME_START || joinstate == JOIN_GAME_START_LOAD) {
                if (joinstate == JOIN_GAME_START_LOAD) {
                    load_game = 1;
                }

                /*
                ** Special new kludge for counterstrike.
                **
                ** Find local scenario will fail to match a counterstrike mission
                ** unless the CS CD is in the drive. So....
                **
                ** If Counterstrike is installed and this is an official map and
                ** the file name matches a counterstrike map then tell the host
                ** that I have the scenario so he can continue while we make
                ** sure the local user has the Counterstrike CD in the drive.
                **
                */
                //	This is duplicated for Aftermath scenarios. ajw

                bool ready_packet_was_sent = false;
#ifdef FIXIT_CSII //	checked - ajw 9/28/98
                if (Session.ScenarioIsOfficial
                    && ((Expansion_CS_Present() && Is_Mission_Counterstrike(Session.ScenarioFileName))
                        || (Expansion_AM_Present() && Is_Mission_Aftermath(Session.ScenarioFileName)))) {
#else
                if (Expansion_CS_Present() && Session.ScenarioIsOfficial) {
#endif

                    CCFileClass check_file(Session.ScenarioFileName);
                    if (!check_file.Is_Available()) {

                        int current_drive = CCFileClass::Get_CD_Drive();
#ifdef FIXIT_CSII                       //	checked - ajw 9/28/98
                        int index = -1; /* Get_CD_Index(current_drive, 1 * 60); */
                        bool needcd = false;
                        if (Is_Mission_Counterstrike(Session.ScenarioFileName)) {
                            if (index != 2 && index != 3) {
                                RequiredCD = 2;
                                needcd = true;
                            }
                        }
                        if (Is_Mission_Aftermath(Session.ScenarioFileName)) {
                            if (index != 3) {
                                RequiredCD = 3;
                                needcd = true;
                            }
                        }
                        if (needcd) {
#else
                        if (Get_CD_Index(current_drive, 1 * 60) != 2) {
                            RequiredCD = 2;
#endif

                            /*
                            ** We should have the scenario but the wrong disk is in.
                            ** Tell the host that I am ready to go anyway.
                            */
                            memset((void*)&(Session.GPacket), 0, sizeof(Session.GPacket));
                            Session.GPacket.Command = NET_READY_TO_GO;
                            Ipx.Send_Global_Message(
                                &Session.GPacket, sizeof(GlobalPacketType), 1, &Session.HostAddress);
                            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                                ;
                            ready_packet_was_sent = true;

                            if (!Force_CD_Available(RequiredCD)) {
                                Emergency_Exit(EXIT_FAILURE);
                            }

                            /*
                            ** Update the internal list of scenarios to include the counterstrike
                            ** list.
                            */
                            Session.Read_Scenario_Descriptions();
                        }
                    }
                }

                /*
                ** If the scenario that the host wants to play doesn't exist locally then we
                **	need to request that it is sent. If we can identify the scenario locally then
                **	we need to fix up the file name so we load the right one.
                */
                Ipx.Set_Timing(25, (unsigned int)-1, 1000);
                if (Find_Local_Scenario(Session.Options.ScenarioDescription,
                                        Session.ScenarioFileName,
                                        Session.ScenarioFileLength,
                                        Session.ScenarioDigest,
                                        Session.ScenarioIsOfficial)) {

                    Session.Options.ScenarioIndex = 1; // We dont care what it
                                                       // is as long as it isnt -1

                    /*
                    ** We have the scenario. Tell the host that I am ready to go.
                    */
                    if (!ready_packet_was_sent) {
                        memset((void*)&(Session.GPacket), 0, sizeof(Session.GPacket));
                        Session.GPacket.Command = NET_READY_TO_GO;
                        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.HostAddress);

                        while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                            ;
                    }
                } else {
#ifndef FIXIT_VERSION_3 //	Removed restriction on downloading official maps.
                    /*
                    ** Oh dear. Thats a scenario I don't have. Request that the host sends the
                    **	scenario to me provided it's not an official scenario.
                    **
                    ** If the file is received OK then we will get a true return value and the
                    ** actual file name to load will be in Session.ScenarioFileName
                    */
                    if (Session.ScenarioIsOfficial) {
                        Session.Options.ScenarioIndex = -1;
                    } else {
#endif
                        Session.Options.ScenarioIndex = 1; // We dont care what it
                                                           // is as long as it isnt -1
#ifdef FIXIT_VERSION_3
                        if (bSpecialAftermathScenario(Session.Options.ScenarioDescription))
                            break;
#endif

                        if (!Get_Scenario_File_From_Host(Session.ScenarioFileName, 1)) {
                            break;
                        } else {
                            /*
                            ** Make sure we dont time-out because of the download
                            */
                        }
#ifndef FIXIT_VERSION_3
                    }
#endif
                }

                Ipx.Set_Timing(30, (unsigned int)-1, 600);
                strcpy(Scen.ScenarioName, Session.ScenarioFileName);
                rc = 0;
                process = false;

            } else if (joinstate == JOIN_CONFIRMED) {

                //..................................................................
                //	If we're newly-confirmed, add myself to the Players list, and
                // immediately send out a player query
                //..................................................................
                //...............................................................
                //	Clear the player list, then add myself to the list.
                //...............................................................

                Clear_Listbox(&playerlist);
                Clear_Vector(&Session.Players);

                item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
                if (Session.House == HOUSE_GOOD) {
                    sprintf(item, "%s\t%s", namebuf, Text_String(TXT_ALLIES));
                } else {
                    sprintf(item, "%s\t%s", namebuf, Text_String(TXT_SOVIET));
                }
#else  // OLDWAY
                sprintf(item, "%s\t%s", namebuf, Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
                playerlist.Add_Item(item,
                                    (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                             : &ColorRemaps[Session.ColorIdx]);

                who = new NodeNameType;
                strcpy(who->Name, namebuf);
                who->Player.House = Session.House;
                who->Player.Color = Session.ColorIdx;
                Session.Players.Add(who);

                Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);

                //...............................................................
                // Re-init the message system to its new smaller size
                //...............................................................
                Session.Messages.Init(d_message2_x + 1 * RESFACTOR,
                                      d_message2_y + 1 * RESFACTOR,
                                      8,
                                      MAX_MESSAGE_LENGTH,
                                      d_txt6_h,
                                      d_send_x + 1 * RESFACTOR,
                                      d_send_y + 1 * RESFACTOR,
                                      1,
                                      20,
                                      MAX_MESSAGE_LENGTH - 5,
                                      d_message2_w);
                Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                   : Session.ColorIdx,
                                          TPF_TEXT,
                                          NULL,
                                          '_',
                                          d_message2_w);
            } else if (joinstate == JOIN_REJECTED) {
                //..................................................................
                //	If we've been rejected, clear any messages we may have been
                // typing, add a message stating why we were rejected, and send a
                // chat announcement.
                //..................................................................
                Session.Messages.Init(d_message1_x + 1 * RESFACTOR,
                                      d_message1_y + 1 * RESFACTOR,
                                      14,
                                      MAX_MESSAGE_LENGTH,
                                      d_txt6_h,
                                      d_send_x + 1 * RESFACTOR,
                                      d_send_y + 1 * RESFACTOR,
                                      1,
                                      20,
                                      MAX_MESSAGE_LENGTH - 5,
                                      d_message2_w);
                Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                   : Session.ColorIdx,
                                          TPF_TEXT,
                                          NULL,
                                          '_',
                                          d_message2_w);

                Session.Messages.Add_Message(
                    NULL, 0, (char*)Text_String(TXT_REQUEST_DENIED), PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_SYS_ERROR);

                item = NULL;
                if (why == REJECT_DUPLICATE_NAME) {
                    item = (char*)Text_String(TXT_NAME_MUSTBE_UNIQUE);
                } else if (why == REJECT_GAME_FULL) {
                    item = (char*)Text_String(TXT_GAME_FULL);
                } else if (why == REJECT_VERSION_TOO_OLD) {
                    item = (char*)Text_String(TXT_YOURGAME_OUTDATED);
                } else if (why == REJECT_VERSION_TOO_NEW) {
                    item = (char*)Text_String(TXT_DESTGAME_OUTDATED);
                } else if (why == REJECT_MISMATCH) {
                    item = (char*)Text_String(TXT_MISMATCH);
                } else if (why == REJECT_DISBANDED) {
                    item = (char*)Text_String(TXT_GAME_CANCELLED);
                }
                if (item) {
                    Session.Messages.Add_Message(NULL, 0, item, PCOLOR_BROWN, TPF_TEXT, 1200);
                }

                Send_Join_Queries(game_index, joinstate, 0, 0, 1, namebuf);
                display = REDRAW_ALL;
            }
        } else if (event == EV_GAME_OPTIONS) {
            //.....................................................................
            //	If the game options have changed, print them.
            //.....................................................................
            countgauge.Set_Maximum(SessionClass::CountMax[Session.Options.Bases]
                                   - SessionClass::CountMin[Session.Options.Bases]);
            countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);
            levelgauge.Set_Value(BuildLevel - 1);
            creditsgauge.Set_Value(Session.Options.Credits);
            if (Session.Options.AIPlayers > Rule.MaxPlayers - Session.Players.Count()) {
                aiplayersgauge.Set_Value(Rule.MaxPlayers - Session.Players.Count());
            } else {
                aiplayersgauge.Set_Value(Session.Options.AIPlayers);
            }
            optionlist.Check_Item(0, Session.Options.Bases);
            optionlist.Check_Item(1, Session.Options.Tiberium);
            optionlist.Check_Item(2, Session.Options.Goodies);
            optionlist.Check_Item(3, Special.IsCaptureTheFlag);
            optionlist.Check_Item(4, Special.IsShadowGrow);
            optionlist.Flag_To_Redraw();

            Sound_Effect(VOC_OPTIONS_CHANGED);

            parms_received = 1;
            display = REDRAW_PARMS;
        } else if (event == EV_MESSAGE) {
            //.....................................................................
            //	Draw an incoming message
            //.....................................................................
            display = REDRAW_MESSAGE;
            Sound_Effect(VOC_INCOMING_MESSAGE);
        } else if (event == EV_NEW_GAME) {
            //.....................................................................
            // If a new game has formed, or an existing game has changed state
            // (from open to closed or closed to open), redraw the message system.
            //.....................................................................
            display = REDRAW_MESSAGE;
        } else if (event == EV_NEW_PLAYER || event == EV_PLAYER_SIGNOFF) {
            aiplayersgauge.Set_Maximum(Rule.MaxPlayers - Session.Players.Count());
            if (Session.Options.AIPlayers > Rule.MaxPlayers - Session.Players.Count()) {
                aiplayersgauge.Set_Value(Rule.MaxPlayers - Session.Players.Count());
            }
        } else if (event == EV_GAME_SIGNOFF) {

            //.....................................................................
            // EV_GAME_SIGNOFF:
            //	A game before the one I've selected is gone, so we have a new index
            // now. 'game_index' must be kept set to the currently-selected list
            // item, so we send out queries for the currently-selected game.  It's
            // therefore imperative that we detect any changes to the game list.
            // If we're joined in a game, we must decrement our game_index to keep
            // it aligned with the game we're joined to.
            //.....................................................................
            if (joinstate == JOIN_CONFIRMED) {
                game_index--;
                join_index--;
                gamelist.Set_Selected_Index(join_index);
            } else {
                gamelist.Flag_To_Redraw();
                Clear_Listbox(&playerlist);
                Clear_Vector(&Session.Players);
                game_index = gamelist.Current_Index();
                Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
            }
        }

        //.....................................................................
        //	Service the Ipx connections
        //.....................................................................
        Ipx.Service();

        if (process) {
            //.....................................................................
            //	Clean out the Game List; if an old entry is found:
            //	- Remove it
            //	- Clear the player list
            //	- Send queries for the new selected game, if there is one
            //.....................................................................
            for (i = 1; i < Session.Games.Count(); i++) {
                if (TickCount - Session.Games[i]->Game.LastTime > 400) {

                    delete Session.Games[i];
                    Session.Games.Delete(Session.Games[i]);

                    item = (char*)(gamelist.Get_Item(i));
                    gamelist.Remove_Item(item);
                    delete[] item;

                    gamelist.Flag_To_Redraw();

                    if (i <= game_index) {
                        if (joinstate == JOIN_CONFIRMED) {
                            game_index--;
                            join_index--;
                            gamelist.Set_Selected_Index(join_index);
                        } else {
                            gamelist.Flag_To_Redraw();
                            Clear_Listbox(&playerlist);
                            Clear_Vector(&Session.Players);
                            game_index = gamelist.Current_Index();
                            Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
                        }
                    }
                }
            }

            //.....................................................................
            // If I've changed my name or color, make sure those changes go into
            // the Chat vector.
            //.....................................................................
            strcpy(Session.Chat[0]->Name, namebuf);
            Session.Chat[0]->Chat.Color = Session.ColorIdx;
            if (Session.Chat[0]->Chat.Color == PCOLOR_DIALOG_BLUE) {
                Session.Chat[0]->Chat.Color = PCOLOR_REALLY_BLUE;
            }

            //.....................................................................
            // Clean out the chat vector.  If we find a node that we haven't heard
            // from in 6 seconds, delete that node.
            // If we haven't heard from a node in 5 seconds, send him a request
            // for a chat announcement; he then has 1 second to reply.
            //.....................................................................
            for (i = 1; i < Session.Chat.Count(); i++) {
                if (TickCount - Session.Chat[i]->Chat.LastTime > 360) {
                    delete Session.Chat[i];
                    Session.Chat.Delete(Session.Chat[i]);
                } else if (TickCount - Session.Chat[i]->Chat.LastTime > 300 && Session.Chat[i]->Chat.LastChance == 0) {
                    memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                    Session.GPacket.Name[0] = 0;
                    Session.GPacket.Command = NET_CHAT_REQUEST;
                    Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, &(Session.Chat[i]->Address));
                    Ipx.Service();
                    Session.Chat[i]->Chat.LastChance = 1;
                }
            }

            //.....................................................................
            // Manage the Lobby list:
            // If the user has selected the 1st Game ("Lobby"), the names of all
            // users in the Chat area show up in the Players list box.
            // Users can be changing their names and their colors at any time, so
            // we scan the Chat list each time to see if anything's changed; if
            // so, we redraw the player list.
            // (If WWChat is on, the Chat list is ignored, and the playerlist
            // contains custom names.)
            //.....................................................................
            if (game_index == 0) {
                if (!Session.WWChat) {
                    while (Session.Chat.Count() > playerlist.Count()) {
                        item = new char[MPLAYER_NAME_MAX];
                        item[0] = 0;
                        playerlist.Add_Item(item);
                        playerlist.Flag_To_Redraw();
                    }
                    while (playerlist.Count() > Session.Chat.Count()) {
                        item = (char*)playerlist.Get_Item(0);
                        playerlist.Remove_Item(item);
                        delete[] item;
                        playerlist.Flag_To_Redraw();
                    }
                    for (i = 0; i < Session.Chat.Count(); i++) {
                        if (stricmp(Session.Chat[i]->Name, playerlist.Get_Item(i))
                            || &ColorRemaps[(Session.Chat[i]->Chat.Color == PCOLOR_DIALOG_BLUE)
                                                ? PCOLOR_REALLY_BLUE
                                                : Session.Chat[i]->Chat.Color]
                                   != playerlist.Colors[i]) {

                            playerlist.Colors[i] = &ColorRemaps[Session.Chat[i]->Chat.Color];
                            if (playerlist.Colors[i] == &ColorRemaps[PCOLOR_DIALOG_BLUE]) {
                                playerlist.Colors[i] = &ColorRemaps[PCOLOR_REALLY_BLUE];
                            }
                            strcpy((char*)playerlist.Get_Item(i), Session.Chat[i]->Name);
                            playerlist.Flag_To_Redraw();
                        }
                    }
                } else {
                    if (stricmp(Session.Chat[0]->Name, playerlist.Get_Item(0))
                        || &ColorRemaps[Session.Chat[0]->Chat.Color] != playerlist.Colors[0]) {
                        playerlist.Colors[0] = &ColorRemaps[Session.Chat[0]->Chat.Color];
                        strcpy((char*)playerlist.Get_Item(0), Session.Chat[0]->Name);
                        playerlist.Flag_To_Redraw();
                    }
                    if (Update_WWChat()) {
                        display = REDRAW_MESSAGE;
                    }
                }
            }
        }
        //.....................................................................
        //	Service the sounds & score; GameActive must be false at this point,
        //	so Call_Back() doesn't intercept global messages from me!
        //.....................................................................
        Call_Back();
        Frame_Limiter();
    } // end of while

    //------------------------------------------------------------------------
    //	Establish connections with all other players.
    //------------------------------------------------------------------------
    if (rc == 0) {
        //.....................................................................
        //	If the other guys are playing a scenario I don't have (sniff), I can't
        //	play.  Try to bail gracefully.
        //.....................................................................
        if (Session.Options.ScenarioIndex == -1) {
            WWMessageBox().Process(TXT_UNABLE_PLAY_WAAUGH);

            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_SIGN_OFF;
            strcpy(Session.GPacket.Name, namebuf);

            //..................................................................
            // Don't send myself the message.
            //..................................................................
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                Ipx.Service();
            }

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;

            rc = -1;

        } else {

            //---------------------------------------------------------------------
            // Prepare to load the scenario.
            //---------------------------------------------------------------------
            //..................................................................
            //	Set the number of players in this game, and the scenario number.
            //..................................................................
            Session.NumPlayers = Session.Players.Count();
        }

        //.....................................................................
        //	Wait a while, polling the IPX service routines, to give our ACK
        //	a chance to get to the other system.  If he doesn't get our ACK,
        // he'll be waiting the whole time we load MIX files.
        //.....................................................................
        i = max(Ipx.Global_Response_Time() * 2, 60);
        starttime = TickCount;
        while (TickCount - starttime < (unsigned)i) {
            Ipx.Service();
        }
    }

    //------------------------------------------------------------------------
    //	Init network timing values, using previous response times as a measure
    //	of what our retry delta & timeout should be.
    //------------------------------------------------------------------------
    //	Ipx.Set_Timing (Ipx.Global_Response_Time() + 2, -1,
    //		Ipx.Global_Response_Time() * 4);
    Ipx.Set_Timing(Ipx.Global_Response_Time() + 2, (unsigned int)-1, max(120, Ipx.Global_Response_Time() * 8));

    //------------------------------------------------------------------------
    //	Clear all lists, but NOT the Games & Players vectors.
    //------------------------------------------------------------------------
    Clear_Listbox(&gamelist);
    Clear_Listbox(&playerlist);

    //------------------------------------------------------------------------
    // Remove the chat edit box
    //------------------------------------------------------------------------
    Session.Messages.Remove_Edit();

    //------------------------------------------------------------------------
    //	Restore screen
    //------------------------------------------------------------------------
    Hide_Mouse();
    Load_Title_Page(true);
    Show_Mouse();

    //------------------------------------------------------------------------
    // Load a game if the game owner told us to
    //------------------------------------------------------------------------
    if (load_game) {
        if (!Load_Game(-1)) {
            WWMessageBox().Process(TXT_ERROR_LOADING_GAME);
            rc = -1;
        }
        Frame++;
    }

    //------------------------------------------------------------------------
    // Clear the Players & Games vectors if we're not joined to a game.
    // Clear the Chat vector regardless.
    //------------------------------------------------------------------------
    if (rc != 0) {
        Clear_Vector(&Session.Games);
        Clear_Vector(&Session.Players);
    }
    Clear_Vector(&Session.Chat);

    return (rc);

} /* end of Net_Join_Dialog */

/***************************************************************************
 * Request_To_Join -- Sends a JOIN request packet to game owner            *
 *                                                                         *
 * Regardless of the return code, the Join Dialog will need to be redrawn	*
 * after calling this routine.															*
 *                                                                         *
 * INPUT:                                                                  *
 *		playername		player's name														*
 *		join_index		index of game we're joining									*
 *		house				requested house													*
 *		color				requested color													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = Packet sent, 0 = wasn't														*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *=========================================================================*/
static int Request_To_Join(char* playername, int join_index, HousesType house, PlayerColorType color)
{
    //------------------------------------------------------------------------
    //	Validate join_index
    //------------------------------------------------------------------------
    if (join_index < 1) {
        Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_MUST_SELECT_GAME), PCOLOR_BROWN, TPF_TEXT, 1200);
        Sound_Effect(VOC_SYS_ERROR);
        return (false);
    }
    if ((Session.Games.Count() <= 1) || join_index > Session.Games.Count()) {
        Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_NOTHING_TO_JOIN), PCOLOR_BROWN, TPF_TEXT, 1200);
        Sound_Effect(VOC_SYS_ERROR);
        return (false);
    }

    //------------------------------------------------------------------------
    //	Force user to enter a name
    //------------------------------------------------------------------------
    if (strlen(playername) == 0) {
        Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_NAME_ERROR), PCOLOR_BROWN, TPF_TEXT, 1200);
        Sound_Effect(VOC_SYS_ERROR);
        return (false);
    }

    //------------------------------------------------------------------------
    //	The game must be open
    //------------------------------------------------------------------------
    if (!Session.Games[join_index]->Game.IsOpen) {
        Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_GAME_IS_CLOSED), PCOLOR_BROWN, TPF_TEXT, 1200);
        Sound_Effect(VOC_SYS_ERROR);
        return (false);
    }

    //------------------------------------------------------------------------
    //	Send packet to game's owner
    //------------------------------------------------------------------------
    memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

    Session.GPacket.Command = NET_QUERY_JOIN;
    strcpy(Session.GPacket.Name, playername);
    Session.GPacket.PlayerInfo.House = house;
    Session.GPacket.PlayerInfo.Color = color;
#ifdef FIXIT_VERSION_3
    //	Guest sends host his version.
    //	Added to the transmitted _min_ version number is a bit indicating presence of Aftermath expansion.
    if (Is_Aftermath_Installed()) {
        //		debugprint( "Guest tells host 'I have Aftermath'\n" );
        Session.GPacket.PlayerInfo.MinVersion = VerNum.Min_Version() | 0x80000000;
    } else {
        //		debugprint( "Guest tells host 'I don't have Aftermath'\n" );
        Session.GPacket.PlayerInfo.MinVersion = VerNum.Min_Version();
    }
#else
    Session.GPacket.PlayerInfo.MinVersion = VerNum.Min_Version();
#endif
    Session.GPacket.PlayerInfo.MaxVersion = VerNum.Max_Version();
    Session.GPacket.PlayerInfo.CheatCheck = RuleINI.Get_Unique_ID();

    Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Games[join_index]->Address));

    return (true);

} /* end of Request_To_Join */

/***************************************************************************
 * Unjoin_Game -- Cancels joining a game                                   *
 *                                                                         *
 * INPUT:                                                                  *
 *		namebuf			current player name												*
 *		joinstate		current join state												*
 *		gamelist			ListBox of game names											*
 *		playerlist		ListBox of player names											*
 *		game_index		index in 'gamelist' of game we're leaving					*
 *		goto_lobby		true = we're going to the lobby								*
 *		msg_x				message system x-coord											*
 *		msg_y				message system y-coord											*
 *		msg_h				message system char height										*
 *		send_x			message system send x-coord									*
 *		send_y			message system send y-coord									*
 *		msg_len			message system max msg length									*
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/12/1995 BRR : Created.                                             *
 *=========================================================================*/
static void Unjoin_Game(char* namebuf,
                        JoinStateType joinstate,
                        ListClass* gamelist,
                        ColorListClass* playerlist,
                        int game_index,
                        int goto_lobby,
                        int msg_x,
                        int msg_y,
                        int msg_h,
                        int send_x,
                        int send_y,
                        int msg_len)
{
    int i;
    char* item;

    //------------------------------------------------------------------------
    // Fill in a SIGN_OFF packet
    //------------------------------------------------------------------------
    memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
    Session.GPacket.Command = NET_SIGN_OFF;
    strcpy(Session.GPacket.Name, namebuf);

    //------------------------------------------------------------------------
    //	If we're joined to a game, make extra sure the other players in
    //	that game know I'm exiting; send my SIGN_OFF as an ack-required
    //	packet.  Don't send this to myself (index 0).
    //------------------------------------------------------------------------
    for (i = 1; i < Session.Players.Count(); i++) {
        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
        Ipx.Service();
    }

    if (joinstate == JOIN_WAIT_CONFIRM || joinstate == JOIN_CONFIRMED) {
        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Games[game_index]->Address));
    }

    //------------------------------------------------------------------------
    // Re-init the message system to its new larger size
    //------------------------------------------------------------------------
    Session.Messages.Init(msg_x + 1, msg_y + 1, 14, msg_len, msg_h, send_x + 1, send_y + 1, 1, 20, msg_len - 5);
    Session.Messages.Add_Edit(
        (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx, TPF_TEXT, NULL, '_');

    //------------------------------------------------------------------------
    // Remove myself from the player list, and reset my game name
    //------------------------------------------------------------------------
    if (playerlist->Count()) {
        item = (char*)(playerlist->Get_Item(0));
        playerlist->Remove_Item(item);
        delete[] item;
        playerlist->Flag_To_Redraw();
    }

    if (Session.Players.Count() > 0) {
        delete Session.Players[0];
        Session.Players.Delete(Session.Players[0]);
    }

    Session.GameName[0] = 0;

    //------------------------------------------------------------------------
    // Highlight "Lobby" on the Game list, Announce I'm ready to chat
    //------------------------------------------------------------------------
    if (goto_lobby) {
        gamelist->Set_Selected_Index(0);
        Send_Join_Queries(game_index, joinstate, 0, 0, 1, namebuf);
    }

} // end of Unjoin_Game

/***********************************************************************************************
 * Send_Join_Queries -- sends queries for the Join Dialog												  *
 *                                                                         						  *
 * This routine [re]sends the queries related to the Join Dialog:										  *
 * - NET_QUERY_GAME																									  *
 * - NET_QUERY_PLAYER for the game currently selected (if there is one)								  *
 *																															  *
 * The queries are "staggered" in time so they aren't all sent at once; otherwise, we'd		  *
 * be inundated with reply packets & we'd miss some (even though the replies will require		  *
 * ACK's). *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		curgame		index of currently-selected game; -1 = none											  *
 *		joinstate	our current joinstate																		  *
 *		gamenow		if 1, will immediately send the game query											  *
 *		playernow	if 1, will immediately send the player query for currently-selected game	  *
 *		chatnow		if 1, will immediately send the chat announcement									  *
 *		myname		user's name																						  *
 *		init			initialize the timers																		  *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		none. *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/14/1995 BR : Created.                                              						  *
 *   04/15/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
static void
Send_Join_Queries(int curgame, JoinStateType joinstate, int gamenow, int playernow, int chatnow, char* myname, int init)
{
    //........................................................................
    // These values control the timeouts for sending various types of packets;
    // they're designed such that they'll rarely occur simultaneously.
    //........................................................................
    enum
    {
        GAME_QUERY_TIME = 120,
        PLAYER_QUERY_TIME = 35,
        CHAT_ANNOUNCE_TIME = 83,
    };
    static CDTimerClass<SystemTimerClass> game_timer;   // time between NET_QUERY_GAME's
    static CDTimerClass<SystemTimerClass> player_timer; // time between NET_QUERY_PLAYERS's
    static CDTimerClass<SystemTimerClass> chat_timer;   // time between NET_CHAT_ANNOUNCE's

    //------------------------------------------------------------------------
    // Initialize timers
    //------------------------------------------------------------------------
    if (init) {
        game_timer = GAME_QUERY_TIME;
        player_timer = PLAYER_QUERY_TIME;
        chat_timer = CHAT_ANNOUNCE_TIME;
    }

    //------------------------------------------------------------------------
    //	Send the game-name query if the time has expired, or we're told to do
    //	it right now
    //------------------------------------------------------------------------
    if (!game_timer || gamenow) {

        game_timer = GAME_QUERY_TIME;

        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

        Session.GPacket.Command = NET_QUERY_GAME;

        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
    }

    //------------------------------------------------------------------------
    //	Send the player query for the game currently clicked on, if the time has
    //	expired and there is a currently-selected game, or we're told to do it
    //	right now
    //------------------------------------------------------------------------
    if (((curgame > 0) && (curgame < Session.Games.Count()) && !player_timer) || playernow) {

        player_timer = PLAYER_QUERY_TIME;

        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

        Session.GPacket.Command = NET_QUERY_PLAYER;
        strcpy(Session.GPacket.Name, Session.Games[curgame]->Name);

        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
    }

    //------------------------------------------------------------------------
    // Send the chat announcement
    //------------------------------------------------------------------------
    if ((!chat_timer && joinstate != JOIN_CONFIRMED) || chatnow) {

        chat_timer = CHAT_ANNOUNCE_TIME;

        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

        Session.GPacket.Command = NET_CHAT_ANNOUNCE;
        strcpy(Session.GPacket.Name, myname);
        Session.GPacket.Chat.ID = Session.UniqueID;
        Session.GPacket.Chat.Color = Session.ColorIdx;

        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
    }

} /* end of Send_Join_Queries */

/***********************************************************************************************
 * Get_Join_Responses -- sends queries for the Join Dialog												  *
 *                                                                         						  *
 * This routine polls the Global Channel to see if there are any incoming packets;				  *
 * if so, it processes them.  This routine can change the state of the Join Dialog, or			  *
 * the contents of the list boxes, based on what the packet is.										  *
 *                                                                         						  *
 * The list boxes are passed in as pointers; they can't be made globals, because they			  *
 * can't be constructed, because they require shape pointers to the arrow buttons, and			  *
 * the mix files won't have been initialized when the global variables' constructors are		  *
 * called. *
 *                                                                         						  *
 * This routine sets the globals 																				  *
 *		Session.House					(from NET_CONFIRM_JOIN)													  *
 *		Session.ColorIdx				(from NET_CONFIRM_JOIN)													  *
 *		Session.Options.Bases		(from NET_GAME_OPTIONS)													  *
 *		Session.Options.Tiberium	(from NET_GAME_OPTIONS)													  *
 *		Session.Options.Goodies		(from NET_GAME_OPTIONS)													  *
 *		Session.Options.Ghosts		(from NET_GAME_OPTIONS)													  *
 *		ScenarioIdx				(from NET_GAME_OPTIONS; -1 = scenario not found)						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		joinstate		current state of Join Dialog															  *
 *		gamelist			list box containing game names														  *
 *		playerlist		list box containing player names for the currently-selected game			  *
 *		join_index		index of the game we've joined or are asking to join							  *
 *		my_name			name of local system																		  *
 *		why				ptr: filled in with reason for rejection from a game							  *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		Event that occurred																							  *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none. *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   02/14/1995 BR : Created.                                              						  *
 *   04/15/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
static JoinEventType Get_Join_Responses(JoinStateType* joinstate,
                                        ListClass* gamelist,
                                        ColorListClass* playerlist,
                                        int join_index,
                                        char* my_name,
                                        RejectType* why)
{
    int rc;
    char* item;        // general-purpose string
    NodeNameType* who; // node to add to Games or Players
    int i;
    int found;
    JoinEventType retcode = EV_NONE;
    char txt[80];

    //------------------------------------------------------------------------
    //	If there is no incoming packet, just return
    //------------------------------------------------------------------------
    rc = Ipx.Get_Global_Message(&Session.GPacket, &Session.GPacketlen, &Session.GAddress, &Session.GProductID);
    if (!rc || Session.GProductID != IPXGlobalConnClass::COMMAND_AND_CONQUER0)
        return (EV_NONE);

    //------------------------------------------------------------------------
    //	If we're joined in a game, handle the packet in a standard way; otherwise,
    //	don't answer standard queries.
    //------------------------------------------------------------------------
    if ((*joinstate) == JOIN_CONFIRMED && Process_Global_Packet(&Session.GPacket, &Session.GAddress) != 0) {
        return (EV_NONE);
    }

    //------------------------------------------------------------------------
    //	NET_ANSWER_GAME:  Another system is answering our GAME query, so add that
    //	system to our list box if it's new.
    //------------------------------------------------------------------------
    if (Session.GPacket.Command == NET_ANSWER_GAME) {

        //.....................................................................
        //	See if this name is unique
        //.....................................................................
        retcode = EV_NONE;
        found = 0;
        for (i = 1; i < Session.Games.Count(); i++) {
            if (!strcmp(Session.Games[i]->Name, Session.GPacket.Name)) {
                found = 1;

                //...............................................................
                //	If name was found, update the node's time stamp & IsOpen flag.
                //...............................................................
                Session.Games[i]->Game.LastTime = TickCount;
                if (Session.Games[i]->Game.IsOpen != Session.GPacket.GameInfo.IsOpen) {
                    item = (char*)gamelist->Get_Item(i);
                    if (Session.GPacket.GameInfo.IsOpen) {
                        sprintf(item, Text_String(TXT_THATGUYS_GAME), Session.GPacket.Name);
                    } else {
                        sprintf(item, Text_String(TXT_THATGUYS_GAME_BRACKET), Session.GPacket.Name);
                    }
                    Session.Games[i]->Game.IsOpen = Session.GPacket.GameInfo.IsOpen;
                    gamelist->Flag_To_Redraw();

                    //............................................................
                    //	If this game has gone from closed to open, copy the
                    // responder's address into our Game slot, since the guy
                    // responding to this must be game owner.
                    //............................................................
                    if (Session.Games[i]->Game.IsOpen) {
                        Session.Games[i]->Address = Session.GAddress;
                    }

                    //............................................................
                    // If we're in chat mode, print a message that the state of
                    // this game has changed.
                    //............................................................
                    if (*joinstate < JOIN_CONFIRMED) {
                        if (Session.Games[i]->Game.IsOpen) {
                            sprintf(txt,
                                    Text_String(TXT_S_FORMED_NEW_GAME),
                                    Session.Games[Session.Games.Count() - 1]->Name);
                            Sound_Effect(VOC_GAME_FORMING);
                        } else {
                            sprintf(txt,
                                    Text_String(TXT_GAME_NOW_IN_PROGRESS),
                                    Session.Games[Session.Games.Count() - 1]->Name);
                            Sound_Effect(VOC_GAME_CLOSED);
                        }
                        Session.Messages.Add_Message(NULL, 0, txt, PCOLOR_BROWN, TPF_TEXT, 1200);
                        retcode = EV_NEW_GAME;
                    }
                }
                break;
            }
        }

        //.....................................................................
        //	name not found (or addresses are different); add it to 'Games'
        //.....................................................................
        if (found == 0) {

            //..................................................................
            //	Create a new node structure, fill it in, add it to 'Games'
            //..................................................................
            who = new NodeNameType;
            strcpy(who->Name, Session.GPacket.Name);
            who->Address = Session.GAddress;
            who->Game.IsOpen = Session.GPacket.GameInfo.IsOpen;
            who->Game.LastTime = TickCount;
            Session.Games.Add(who);

            //..................................................................
            //	Create a string for "xxx's Game", leaving room for brackets around
            //	the string if it's a closed game
            //..................................................................
            item = new char[MPLAYER_NAME_MAX + 64];
            if (Session.GPacket.GameInfo.IsOpen) {
                sprintf(item, Text_String(TXT_THATGUYS_GAME), Session.GPacket.Name);
            } else {
                sprintf(item, Text_String(TXT_THATGUYS_GAME_BRACKET), Session.GPacket.Name);
            }
            gamelist->Add_Item(item);

            //..................................................................
            // If this player's in the Chat vector, remove him from there
            //..................................................................
            for (i = 1; i < Session.Chat.Count(); i++) {
                if (Session.Chat[i]->Address == Session.GAddress) {
                    delete Session.Chat[i];
                    Session.Chat.Delete(Session.Chat[i]);
                    break;
                }
            }

            //..................................................................
            // If this game is open, display a message stating that it's
            // now available.
            //..................................................................
            if (Session.GPacket.GameInfo.IsOpen && (*joinstate) < JOIN_CONFIRMED) {
                sprintf(txt, Text_String(TXT_S_FORMED_NEW_GAME), Session.GPacket.Name);
                Session.Messages.Add_Message(NULL, 0, txt, PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_GAME_FORMING);
            }

            retcode = EV_NEW_GAME;
        }
    }

    //------------------------------------------------------------------------
    //	NET_ANSWER_PLAYER: Another system is answering our PLAYER query, so add
    // it to our player list box & the Player Vector if it's new
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_ANSWER_PLAYER) {
        //.....................................................................
        //	See if this name is unique
        //.....................................................................
        retcode = EV_NONE;
        found = 0;
        for (i = 0; i < Session.Players.Count(); i++) {

            //..................................................................
            //	If the address is already present, re-copy their name, color &
            //	house into the existing entry, in case they've changed it without
            //	our knowledge; set the 'found' flag so we won't create a new entry.
            //..................................................................
            if (Session.Players[i]->Address == Session.GAddress) {
                strcpy(Session.Players[i]->Name, Session.GPacket.Name);
                Session.Players[i]->Player.House = Session.GPacket.PlayerInfo.House;
                Session.Players[i]->Player.Color = Session.GPacket.PlayerInfo.Color;

                playerlist->Colors[i] = &ColorRemaps[Session.GPacket.PlayerInfo.Color];

                if (playerlist->Colors[i] == &ColorRemaps[PCOLOR_DIALOG_BLUE]) {
                    playerlist->Colors[i] = &ColorRemaps[PCOLOR_REALLY_BLUE];
                }

                found = 1;
                break;
            }
        }

        //.....................................................................
        //	Don't add this player if he's not part of the game that's selected.
        //.....................................................................
        i = gamelist->Current_Index();
        if (Session.Games.Count() && Session.GPacket.PlayerInfo.NameCRC != Compute_Name_CRC(Session.Games[i]->Name)) {
            found = 1;
        }

        //.....................................................................
        //	Don't add this player if it's myself.  (We must check the name
        // since the address of myself in 'Players' won't be valid.)
        //.....................................................................
        if (!strcmp(my_name, Session.GPacket.Name)) {
            found = 1;
        }

        //.....................................................................
        //	name not found, or address didn't match; add to player list box
        // & Players Vector
        //.....................................................................
        if (found == 0) {
            //..................................................................
            //	Create & add a node to the Vector
            //..................................................................
            who = new NodeNameType;
            strcpy(who->Name, Session.GPacket.Name);
            who->Address = Session.GAddress;
            who->Player.House = Session.GPacket.PlayerInfo.House;
            who->Player.Color = Session.GPacket.PlayerInfo.Color;
            Session.Players.Add(who);

            //..................................................................
            //	Create & add a string to the list box
            //..................................................................
            item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
            if (Session.GPacket.PlayerInfo.House == HOUSE_GOOD) {
                sprintf(item, "%s\t%s", Session.GPacket.Name, Text_String(TXT_ALLIES));
            } else {
                sprintf(item, "%s\t%s", Session.GPacket.Name, Text_String(TXT_SOVIET));
            }
#else  // OLDWAY
            sprintf(item,
                    "%s\t%s",
                    Session.GPacket.Name,
                    Text_String(HouseTypeClass::As_Reference(Session.GPacket.PlayerInfo.House).Full_Name()));
#endif // OLDWAY
            playerlist->Add_Item(item,
                                 (who->Player.Color == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                           : &ColorRemaps[who->Player.Color]);

            //..................................................................
            // If this player's in the Chat vector, remove him from there
            //..................................................................
            for (i = 1; i < Session.Chat.Count(); i++) {
                if (Session.Chat[i]->Address == Session.GAddress) {
                    delete Session.Chat[i];
                    Session.Chat.Delete(Session.Chat[i]);
                    break;
                }
            }

            //..................................................................
            // If this player has joined our game, play a special sound.
            //..................................................................
            if ((*joinstate) >= JOIN_CONFIRMED) {
                Sound_Effect(VOC_PLAYER_JOINED);
            }

            retcode = EV_NEW_PLAYER;
        }
    }

    //------------------------------------------------------------------------
    //	NET_CONFIRM_JOIN: The game owner has confirmed our JOIN query; mark us
    // as being confirmed, and start answering queries from other systems
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_CONFIRM_JOIN) {
        if ((*joinstate) != JOIN_CONFIRMED) {
            strcpy(Session.GameName, Session.GPacket.Name);
            Session.House = Session.GPacket.PlayerInfo.House;
            Session.ColorIdx = Session.GPacket.PlayerInfo.Color;

            (*joinstate) = JOIN_CONFIRMED;
            retcode = EV_STATE_CHANGE;
        }
    }

    //------------------------------------------------------------------------
    //	NET_REJECT_JOIN: The game owner has turned down our JOIN query; restore
    //	the dialog state to its first pop-up state.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_REJECT_JOIN) {
        //.....................................................................
        // If we're confirmed in a game, broadcast a sign-off to tell all other
        // systems that I'm no longer a part of any game; this way, I'll be
        // properly removed from their dialogs.
        //.....................................................................
        if ((*joinstate) == JOIN_CONFIRMED) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_SIGN_OFF;
            strcpy(Session.GPacket.Name, my_name);

            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                Ipx.Service();
            }

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;

            Session.GameName[0] = 0;

            //..................................................................
            // remove myself from the player list
            //..................................................................
            item = (char*)(playerlist->Get_Item(0));
            playerlist->Remove_Item(item);
            delete[] item;
            playerlist->Flag_To_Redraw();

            delete Session.Players[0];
            Session.Players.Delete(Session.Players[0]);

            (*joinstate) = JOIN_REJECTED;
            (*why) = REJECT_BY_OWNER;
            retcode = EV_STATE_CHANGE;
        }
        //.....................................................................
        // If we're waiting for confirmation & got rejected, tell the user why
        //.....................................................................
        else if ((*joinstate) == JOIN_WAIT_CONFIRM) {
            (*why) = (RejectType)Session.GPacket.Reject.Why;
            (*joinstate) = JOIN_REJECTED;
            retcode = EV_STATE_CHANGE;
        }
    }

    //------------------------------------------------------------------------
    //	NET_GAME_OPTIONS: The game owner has changed the game options & is
    // sending us the new values.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_GAME_OPTIONS) {
        if ((*joinstate) == JOIN_CONFIRMED || (*joinstate) == JOIN_WAIT_CONFIRM) {
            Session.Options.Credits = Session.GPacket.ScenarioInfo.Credits;
            Session.Options.Bases = Session.GPacket.ScenarioInfo.IsBases;
            Session.Options.Tiberium = Session.GPacket.ScenarioInfo.IsTiberium;
            Session.Options.Goodies = Session.GPacket.ScenarioInfo.IsGoodies;
            //			Session.Options.Ghosts = Session.GPacket.ScenarioInfo.IsGhosties;
            Session.Options.AIPlayers = Session.GPacket.ScenarioInfo.AIPlayers;
            BuildLevel = Session.GPacket.ScenarioInfo.BuildLevel;
            Session.Options.UnitCount = Session.GPacket.ScenarioInfo.UnitCount;
            Seed = Session.GPacket.ScenarioInfo.Seed;
            Special = Session.GPacket.ScenarioInfo.Special;
            Options.GameSpeed = Session.GPacket.ScenarioInfo.GameSpeed;

#ifdef FIXIT_VERSION_3
            //	Guest receives game version number from host.
            //	Added to the transmitted version number is a bit indicating presence of Aftermath expansion.
            unsigned int lVersion = Session.GPacket.ScenarioInfo.Version & ~0x80000000; //	Actual version number.
            Session.CommProtocol = VerNum.Version_Protocol(lVersion);
            bAftermathMultiplayer = Session.GPacket.ScenarioInfo.Version & 0x80000000;
//			if( bAftermathMultiplayer )
//				debugprint( "Guest hears host say 'This is an Aftermath game'\n" );
//			else
//				debugprint( "Guest hears host say 'This is NOT an Aftermath game'\n" );
#else
            Session.CommProtocol = VerNum.Version_Protocol(Session.GPacket.ScenarioInfo.Version);
            PlayingAgainstVersion = Session.GPacket.ScenarioInfo.Version;
#endif

            if (Session.Options.Tiberium) {
                Special.IsTGrowth = 1;
                Rule.IsTGrowth = 1;
                Special.IsTSpread = 1;
                Rule.IsTSpread = 1;
            } else {
                Special.IsTGrowth = 0;
                Rule.IsTGrowth = 0;
                Special.IsTSpread = 0;
                Rule.IsTSpread = 0;
            }

            /*...............................................................
            Copy the information about the scenario that the host wants to
            play so ee can request this scenario from the host if we don't
            have it locally.
            ...............................................................*/
            strcpy(Session.Options.ScenarioDescription, Session.GPacket.ScenarioInfo.Scenario);
            strcpy(Session.ScenarioFileName, Session.GPacket.ScenarioInfo.ShortFileName);
#ifdef WOLAPI_INTEGRATION
            strncpy(Session.ScenarioDigest,
                    (char*)Session.GPacket.ScenarioInfo.FileDigest,
                    sizeof(Session.GPacket.ScenarioInfo.FileDigest));
#else
            strcpy(Session.ScenarioDigest, (char*)Session.GPacket.ScenarioInfo.FileDigest);
#endif
            Session.ScenarioIsOfficial = Session.GPacket.ScenarioInfo.OfficialScenario;
            Session.ScenarioFileLength = Session.GPacket.ScenarioInfo.FileLength;

            retcode = EV_GAME_OPTIONS;
        }
    }

    //------------------------------------------------------------------------
    //	NET_SIGN_OFF: Another system is signing off: search for that system in
    //	both the game list & player list, & remove it if found
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_SIGN_OFF) {
        //.....................................................................
        //	Remove this name from the list of games
        //.....................................................................
        for (i = 1; i < Session.Games.Count(); i++) {
            if (!strcmp(Session.Games[i]->Name, Session.GPacket.Name)
                && Session.Games[i]->Address == Session.GAddress) {

                //...............................................................
                //	If the system signing off is the currently-selected list
                //	item, clear the player list since that game is no longer
                //	forming.
                //...............................................................
                if (i == gamelist->Current_Index()) {
                    Clear_Listbox(playerlist);
                    Clear_Vector(&Session.Players);
                }

                //...............................................................
                //	If the system signing off was the owner of our game, mark
                //	ourselves as rejected
                //...............................................................
                if ((*joinstate) > JOIN_NOTHING && i == join_index) {
                    (*joinstate) = JOIN_REJECTED;
                    retcode = EV_STATE_CHANGE;
                    (*why) = REJECT_DISBANDED;
                }

                //...............................................................
                //	Set my return code
                //...............................................................
                if (retcode == EV_NONE) {
                    if (i <= gamelist->Current_Index()) {
                        retcode = EV_GAME_SIGNOFF;
                    } else {
                        retcode = EV_PLAYER_SIGNOFF;
                    }
                }

                //...............................................................
                //	Remove game name from game list
                //...............................................................
                delete Session.Games[i];
                Session.Games.Delete(Session.Games[i]);
                item = (char*)(gamelist->Get_Item(i));
                gamelist->Remove_Item(item);
                delete[] item;
                gamelist->Flag_To_Redraw();
            }
        }

        //.....................................................................
        //	Remove this name from the list of players
        //.....................................................................
        for (i = 0; i < Session.Players.Count(); i++) {

            //..................................................................
            //	Name found; remove it
            //..................................................................
            if (Session.Players[i]->Address == Session.GAddress) {
                item = (char*)(playerlist->Get_Item(i));
                playerlist->Remove_Item(item);
                delete[] item;

                delete Session.Players[i];
                Session.Players.Delete(Session.Players[i]);

                playerlist->Flag_To_Redraw();

                //...............................................................
                // If this player has left our game, play a special sound.
                //...............................................................
                if ((*joinstate) >= JOIN_CONFIRMED) {
                    Sound_Effect(VOC_PLAYER_LEFT);
                }

                if (retcode == EV_NONE) {
                    retcode = EV_PLAYER_SIGNOFF;
                }
            }
        }

        //.....................................................................
        //	Remove this name from the chat list
        //.....................................................................
        for (i = 1; i < Session.Chat.Count(); i++) {

            //..................................................................
            //	Name found; remove it
            //..................................................................
            if (Session.Chat[i]->Address == Session.GAddress) {

                delete Session.Chat[i];
                Session.Chat.Delete(Session.Chat[i]);

                if (retcode == EV_NONE) {
                    retcode = EV_PLAYER_SIGNOFF;
                }
            }
        }
    }

    //------------------------------------------------------------------------
    //	NET_GO: The game's owner is signalling us to start playing.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_GO) {
        if ((*joinstate) == JOIN_CONFIRMED) {
            Session.MaxAhead = Session.GPacket.ResponseTime.OneWay;
            (*joinstate) = JOIN_GAME_START;
            retcode = EV_STATE_CHANGE;
            Session.HostAddress = Session.GAddress;
        }
    }

    //------------------------------------------------------------------------
    //	NET_LOADGAME: The game's owner is signalling us to start playing.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_LOADGAME) {
        if ((*joinstate) == JOIN_CONFIRMED) {
            Session.MaxAhead = Session.GPacket.ResponseTime.OneWay;
            (*joinstate) = JOIN_GAME_START_LOAD;
            retcode = EV_STATE_CHANGE;
        }
    }

    //------------------------------------------------------------------------
    // NET_CHAT_ANNOUNCE: Someone is ready to chat; add them to our list, if
    // they aren't already on it, and it's not myself.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_CHAT_ANNOUNCE) {
        found = 0;
        //.....................................................................
        // If this packet is from myself, don't add it to the list
        //.....................................................................
        if (Session.GPacket.Chat.ID == Session.UniqueID) {
            found = 1;
        }
        //.....................................................................
        // Otherwise, see if we already have this address stored in our list
        // If so, update that node's time values & name (in case the user
        // changed it), and return.
        //.....................................................................
        else {
            for (i = 0; i < Session.Chat.Count(); i++) {
                if (Session.Chat[i]->Address == Session.GAddress) {
                    strcpy(Session.Chat[i]->Name, Session.GPacket.Name);
                    Session.Chat[i]->Chat.LastTime = TickCount;
                    Session.Chat[i]->Chat.LastChance = 0;
                    Session.Chat[i]->Chat.Color = Session.GPacket.Chat.Color;
                    found = 1;
                    break;
                }
            }
        }
        //.....................................................................
        // Add a new node to the list
        //.....................................................................
        if (!found) {
            who = new NodeNameType;
            strcpy(who->Name, Session.GPacket.Name);
            who->Address = Session.GAddress;
            who->Chat.LastTime = TickCount;
            who->Chat.LastChance = 0;
            who->Chat.Color = Session.GPacket.Chat.Color;
            Session.Chat.Add(who);
        }
    }

    //------------------------------------------------------------------------
    // NET_CHAT_REQUEST: Someone is requesting a CHAT_ANNOUNCE from us; send
    // one to him directly.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_CHAT_REQUEST) {
        if ((*joinstate) != JOIN_WAIT_CONFIRM && (*joinstate) != JOIN_CONFIRMED) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_CHAT_ANNOUNCE;
            strcpy(Session.GPacket.Name, my_name);
            Session.GPacket.Chat.ID = Session.UniqueID;
            Session.GPacket.Chat.Color = Session.ColorIdx;

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);

            Ipx.Service();
        }
    }

    //------------------------------------------------------------------------
    // NET_MESSAGE: Someone is sending us a message
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_MESSAGE) {
        //.....................................................................
        // If we're in a game, the sender must be in our game.
        //.....................................................................
        if ((*joinstate) == JOIN_CONFIRMED) {
            if (Session.GPacket.Message.NameCRC == Compute_Name_CRC(Session.GameName)) {
                Session.Messages.Add_Message(
                    Session.GPacket.Name,
                    (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                          : Session.GPacket.Message.Color,
                    Session.GPacket.Message.Buf,
                    (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                          : Session.GPacket.Message.Color,
                    TPF_TEXT,
                    -1);
            }
        }
        //.....................................................................
        // Otherwise, we're in the chat room; display any old message.
        //.....................................................................
        else {
            Session.Messages.Add_Message(Session.GPacket.Name,
                                         Session.GPacket.Message.Color,
                                         Session.GPacket.Message.Buf,
                                         (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE)
                                             ? PCOLOR_REALLY_BLUE
                                             : Session.GPacket.Message.Color,
                                         TPF_TEXT,
                                         -1);
        }

        retcode = EV_MESSAGE;
    }

    //------------------------------------------------------------------------
    //	NET_PING: Someone is pinging me to get a response time measure (will only
    //	happen after I've joined a game).  Do nothing; the IPX Manager will handle
    //	sending an ACK, and updating the response time measurements.
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_PING) {
        retcode = EV_NONE;
    }

    //------------------------------------------------------------------------
    //	Default case: nothing happened.  (This case will be hit every time I
    //	receive my own NET_QUERY_GAME or NET_QUERY_PLAYER packets.)
    //------------------------------------------------------------------------
    else {
        retcode = EV_NONE;
    }

    return (retcode);

} /* end of Get_Join_Responses */

/***********************************************************************************************
 * Net_New_Dialog -- lets user start a new game                                                *
 *                                                                                             *
 * This dialog shows a list of who's requesting to join this game, and lets                    *
 * the game initiator selectively approve each user.                                           *
 *                                                                                             *
 * The 'Players' vector matches one-for-one the contents of the list box.  The local system	  *
 * is always listed first; the IPX Address of the local system will NOT be valid in the		  *
 * Players vector.  The Games & Players vectors should be left filled in even after this		  *
 * routine is exited; their contents are used to form connections to the other players,		  *
 * after the scenario has been loaded.																			  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      true = success, false = cancel                                                         *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      Session.GameName must contain this player's name.                                       *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
static int Net_New_Dialog(void)
{
    typedef enum
    {
        NUM_MESSAGES = 10
    } NumMessagesType;

    //------------------------------------------------------------------------
    //	Dialog & button dimensions
    //------------------------------------------------------------------------
    int d_dialog_w = 320 * RESFACTOR;                      // dialog width
    int d_dialog_h = 200 * RESFACTOR;                      // dialog height
    int d_dialog_x = ((320 * RESFACTOR - d_dialog_w) / 2); // dialog x-coord
    int d_dialog_y = ((200 * RESFACTOR - d_dialog_h) / 2); // centered y-coord
    int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);       // center x-coord

    int d_txt6_h = 6 * RESFACTOR + 1; // ht of 6-pt text
    int d_margin1 = 5 * RESFACTOR;    // margin width/height
    int d_margin2 = 2 * RESFACTOR;    // margin width/height

    // BG	int d_playerlist_w = 118*RESFACTOR;
    int d_playerlist_w = 124 * RESFACTOR;
    int d_playerlist_h = (6 * d_txt6_h) + 3 * RESFACTOR; // 6 rows high
    int d_playerlist_x = d_dialog_x + d_margin1 + d_margin1 + 5 * RESFACTOR;
    int d_playerlist_y = d_dialog_y + d_margin1 + d_txt6_h + 3 * RESFACTOR;

    int d_scenariolist_w = 162 * RESFACTOR;
    int d_scenariolist_h = (6 * d_txt6_h) + 3 * RESFACTOR; // 6 rows high
    int d_scenariolist_x = d_dialog_x + d_dialog_w - d_margin1 - d_margin1 - d_scenariolist_w - (5 * RESFACTOR);
    int d_scenariolist_y = d_playerlist_y;

    int d_reject_w = 55 * RESFACTOR;
    int d_reject_h = 9 * RESFACTOR;
    int d_reject_x = d_playerlist_x + (d_playerlist_w / 2) - (d_reject_w / 2);
    int d_reject_y = d_playerlist_y + d_playerlist_h + d_margin2;

    int d_count_w = 25 * RESFACTOR;
    int d_count_h = d_txt6_h;
    int d_count_x = d_playerlist_x + (d_playerlist_w / 2) + 20 * RESFACTOR; // (fudged)
    int d_count_y = d_reject_y + d_reject_h /*KO+ d_margin2*/;

    int d_level_w = 25 * RESFACTOR;
    int d_level_h = d_txt6_h;
    int d_level_x = d_playerlist_x + (d_playerlist_w / 2) + 20 * RESFACTOR; // (fudged)
    int d_level_y = d_count_y + d_count_h;

    int d_credits_w = 25 * RESFACTOR;
    int d_credits_h = d_txt6_h;
    int d_credits_x = d_playerlist_x + (d_playerlist_w / 2) + 20 * RESFACTOR; // (fudged)
    int d_credits_y = d_level_y + d_level_h;

    int d_aiplayers_w = 25 * RESFACTOR;
    int d_aiplayers_h = d_txt6_h;
    int d_aiplayers_x = d_playerlist_x + (d_playerlist_w / 2) + 20 * RESFACTOR; // (fudged)
    int d_aiplayers_y = d_credits_y + d_credits_h;

    int d_options_w = 106 * RESFACTOR;
    int d_options_h = ((5 * 6) + 4) * RESFACTOR;
    int d_options_x = d_scenariolist_x + ((d_scenariolist_w - d_options_w) / 2);
    int d_options_y = d_scenariolist_y + d_scenariolist_h + d_margin1;

    int d_message_w = d_dialog_w - (d_margin1 * 2) - 20 * RESFACTOR;
    int d_message_h = (NUM_MESSAGES * d_txt6_h) + 3 * RESFACTOR; // 10 rows high
    int d_message_x = d_dialog_x + d_margin1 + 10 * RESFACTOR;
    int d_message_y = d_dialog_y + d_dialog_h - (27 * RESFACTOR + d_message_h);
    //	int d_message_y = d_options_y + d_options_h + d_margin1;

    int d_send_w = d_dialog_w - (d_margin1 * 2) - 20 * RESFACTOR;
    int d_send_h = 9 * RESFACTOR;
    int d_send_x = d_dialog_x + d_margin1 + 10 * RESFACTOR;
    int d_send_y = d_message_y + d_message_h;

    int d_ok_w = 50 * RESFACTOR;
    int d_ok_h = 9 * RESFACTOR;
    int d_ok_x = d_dialog_x + (d_dialog_w / 6) - (d_ok_w / 2);
    int d_ok_y = d_dialog_y + d_dialog_h - d_ok_h - d_margin1 - 3 * RESFACTOR;

    int d_cancel_w = 50 * RESFACTOR;
    int d_cancel_h = 9 * RESFACTOR;
    int d_cancel_x = d_dialog_cx - (d_cancel_w / 2);
    int d_cancel_y = d_dialog_y + d_dialog_h - d_cancel_h - d_margin1 - 3 * RESFACTOR;

    int d_load_w = 50 * RESFACTOR;
    int d_load_h = 9 * RESFACTOR;
    int d_load_x = d_dialog_x + ((d_dialog_w * 5) / 6) - (d_load_w / 2);
    int d_load_y = d_dialog_y + d_dialog_h - d_load_h - d_margin1 - 3 * RESFACTOR;

    //------------------------------------------------------------------------
    //	Button Enumerations
    //------------------------------------------------------------------------
    enum
    {
        BUTTON_PLAYERLIST = 100,
        BUTTON_SCENARIOLIST,
        BUTTON_REJECT,
        BUTTON_COUNT,
        BUTTON_LEVEL,
        BUTTON_CREDITS,
        BUTTON_AIPLAYERS,
        BUTTON_OPTIONS,
        BUTTON_OK,
        BUTTON_LOAD,
        BUTTON_CANCEL,
    };

    //------------------------------------------------------------------------
    //	Redraw values: in order from "top" to "bottom" layer of the dialog
    //------------------------------------------------------------------------
    typedef enum
    {
        REDRAW_NONE = 0,
        REDRAW_PARMS,
        REDRAW_MESSAGE,
        REDRAW_BUTTONS,
        REDRAW_BACKGROUND,
        REDRAW_ALL = REDRAW_BACKGROUND
    } RedrawType;

    //------------------------------------------------------------------------
    //	Dialog variables
    //------------------------------------------------------------------------
    RedrawType display = REDRAW_ALL; // redraw level
    bool process = true;             // process while true
    KeyNumType input;

    int transmit; // 1 = re-transmit new game options

    int ok_timer = 0; // for timing OK button
    int index;        // index for rejecting a player
    int rc;
    int i, j;
    char* item;
    int tabs[] = {77 * RESFACTOR};      // tabs for player list box
    int optiontabs[] = {8 * RESFACTOR}; // tabs for option list box

    NodeNameType* who;  // node to add to Players
    int ping_timer = 0; // for sending Ping packets

    int color_used[MAX_MPLAYER_COLORS]; // 1 = color has been used
    char txt[80];
    JoinEventType whahoppa;    // event generated by received packets
    static int first_time = 1; // 1 = 1st time this dialog is run
    CCFileClass loadfile("SAVEGAME.NET");
    int load_game = 0; // 1 = load a saved game
    RemapControlType* scheme = GadgetClass::Get_Color_Scheme();

    //------------------------------------------------------------------------
    // Buttons
    //------------------------------------------------------------------------
    GadgetClass* commands; // button list

    ColorListClass playerlist(BUTTON_PLAYERLIST,
                              d_playerlist_x,
                              d_playerlist_y,
                              d_playerlist_w,
                              d_playerlist_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));
    ListClass scenariolist(BUTTON_SCENARIOLIST,
                           d_scenariolist_x,
                           d_scenariolist_y,
                           d_scenariolist_w,
                           d_scenariolist_h,
                           TPF_TEXT,
                           MFCD::Retrieve("BTN-UP.SHP"),
                           MFCD::Retrieve("BTN-DN.SHP"));
    TextButtonClass rejectbtn(BUTTON_REJECT, TXT_REJECT, TPF_BUTTON, d_reject_x, d_reject_y);
    GaugeClass countgauge(BUTTON_COUNT, d_count_x, d_count_y, d_count_w, d_count_h);
    GaugeClass levelgauge(BUTTON_LEVEL, d_level_x, d_level_y, d_level_w, d_level_h);
    GaugeClass creditsgauge(BUTTON_CREDITS, d_credits_x, d_credits_y, d_credits_w, d_credits_h);
    GaugeClass aiplayersgauge(BUTTON_AIPLAYERS, d_aiplayers_x, d_aiplayers_y, d_aiplayers_w, d_aiplayers_h);
    CheckListClass optionlist(BUTTON_OPTIONS,
                              d_options_x,
                              d_options_y,
                              d_options_w,
                              d_options_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));
    TextButtonClass okbtn(BUTTON_OK, TXT_OK, TPF_BUTTON, d_ok_x, d_ok_y, 60 * RESFACTOR);
    TextButtonClass loadbtn(BUTTON_LOAD, TXT_LOAD_BUTTON, TPF_BUTTON, d_load_x, d_load_y, 60 * RESFACTOR);
    TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON, d_cancel_x, d_cancel_y, 60 * RESFACTOR);

    StaticButtonClass staticunit(0, "    ", TPF_TEXT, d_count_x + d_count_w + 2 * RESFACTOR, d_count_y);
    StaticButtonClass staticlevel(0, "    ", TPF_TEXT, d_level_x + d_level_w + 2 * RESFACTOR, d_level_y);
    StaticButtonClass staticcredits(0, "         ", TPF_TEXT, d_credits_x + d_credits_w + 2 * RESFACTOR, d_credits_y);
    StaticButtonClass staticaiplayers(0, "   ", TPF_TEXT, d_aiplayers_x + d_aiplayers_w + 2 * RESFACTOR, d_aiplayers_y);

    //------------------------------------------------------------------------
    //	Build the button list
    //------------------------------------------------------------------------
    commands = &playerlist;
    scenariolist.Add_Tail(*commands);
    rejectbtn.Add_Tail(*commands);
    staticunit.Add_Tail(*commands);
    staticlevel.Add_Tail(*commands);
    staticcredits.Add_Tail(*commands);
    staticaiplayers.Add_Tail(*commands);
    countgauge.Add_Tail(*commands);
    creditsgauge.Add_Tail(*commands);
    aiplayersgauge.Add_Tail(*commands);
    levelgauge.Add_Tail(*commands);
    optionlist.Add_Tail(*commands);
    okbtn.Add_Tail(*commands);
    cancelbtn.Add_Tail(*commands);
    if (loadfile.Is_Available()) {
#ifdef FIXIT_MULTI_SAVE
//		loadbtn.Add_Tail(*commands);
#endif
    } else {
        cancelbtn.X = loadbtn.X;
    }
    playerlist.Set_Tabs(tabs);

    //------------------------------------------------------------------------
    //	Init dialog values, only the first time through
    //------------------------------------------------------------------------
    Special.IsCaptureTheFlag = Rule.IsMPCaptureTheFlag;
    if (first_time) {
        Session.Options.Credits = Rule.MPDefaultMoney; // init credits & credit buffer
        Session.Options.Bases = Rule.IsMPBasesOn;      // init scenario parameters
        Session.Options.Tiberium = Rule.IsMPTiberiumGrow;
        Session.Options.Goodies = Rule.IsMPCrates;
        Session.Options.AIPlayers = 0;
        Session.Options.UnitCount =
            (SessionClass::CountMax[Session.Options.Bases] + SessionClass::CountMin[Session.Options.Bases]) / 2;
        first_time = 0;
    }

    //------------------------------------------------------------------------
    //	Init button states
    //------------------------------------------------------------------------
    optionlist.Set_Tabs(optiontabs);
    optionlist.Set_Read_Only(0);

    optionlist.Add_Item(Text_String(TXT_BASES));
    optionlist.Add_Item(Text_String(TXT_ORE_SPREADS));
    optionlist.Add_Item(Text_String(TXT_CRATES));
    optionlist.Add_Item(Text_String(TXT_CAPTURE_THE_FLAG));
    optionlist.Add_Item(Text_String(TXT_SHADOW_REGROWS));

    optionlist.Check_Item(0, Session.Options.Bases);
    optionlist.Check_Item(1, Session.Options.Tiberium);
    optionlist.Check_Item(2, Session.Options.Goodies);
    optionlist.Check_Item(3, Special.IsCaptureTheFlag);
    optionlist.Check_Item(4, Special.IsShadowGrow);

    countgauge.Set_Maximum(SessionClass::CountMax[Session.Options.Bases]
                           - SessionClass::CountMin[Session.Options.Bases]);
    countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);

    levelgauge.Set_Maximum(MPLAYER_BUILD_LEVEL_MAX - 1);
    levelgauge.Set_Value(BuildLevel - 1);

    creditsgauge.Set_Maximum(Rule.MPMaxMoney);
    creditsgauge.Set_Value(Session.Options.Credits);

    //------------------------------------------------------------------------
    //	Init other scenario parameters
    //------------------------------------------------------------------------
    Special.IsTGrowth = Session.Options.Tiberium;
    Rule.IsTGrowth = Session.Options.Tiberium;
    Special.IsTSpread = Session.Options.Tiberium;
    Rule.IsTSpread = Session.Options.Tiberium;
    transmit = 0;

    //------------------------------------------------------------------------
    //	Init scenario description list box
    //------------------------------------------------------------------------
    for (i = 0; i < Session.Scenarios.Count(); i++) {
        for (j = 0; EngMisStr[j] != NULL; j++) {
            if (!strcmp(Session.Scenarios[i]->Description(), EngMisStr[j])) {
#ifdef FIXIT_CSII //	ajw Added Aftermath installed checks (before, it was assumed).
                //	Add mission if it's available to us.
                if (!((Is_Mission_Counterstrike((char*)(Session.Scenarios[i]->Get_Filename()))
                       && !Is_Counterstrike_Installed())
                      || (Is_Mission_Aftermath((char*)(Session.Scenarios[i]->Get_Filename()))
                          && !Is_Aftermath_Installed())))
#endif
#if defined(GERMAN) || defined(FRENCH)
                    scenariolist.Add_Item(EngMisStr[j + 1]);
#else
                scenariolist.Add_Item(EngMisStr[j]);
#endif

                break;
            }
        }
        if (EngMisStr[j] == NULL) {
#ifdef FIXIT_CSII //	ajw Added Aftermath installed checks (before, it was assumed). Added officialness check.
            //	Add mission if it's available to us.
            if (!Session.Scenarios[i]->Get_Official()
                || !((Is_Mission_Counterstrike((char*)(Session.Scenarios[i]->Get_Filename()))
                      && !Is_Counterstrike_Installed())
                     || (Is_Mission_Aftermath((char*)(Session.Scenarios[i]->Get_Filename()))
                         && !Is_Aftermath_Installed())))
#endif
            {
                scenariolist.Add_Item(Session.Scenarios[i]->Description());
            }
        }
    }

    Session.Options.ScenarioIndex = 0; // 1st scenario is selected
#ifdef FIXIT_VERSION_3
    bAftermathMultiplayer = Is_Aftermath_Installed();
//	debugprint( "Host decides that, initially, bAftermathMultiplayer is %i\n", bAftermathMultiplayer );
#else
    PlayingAgainstVersion = VerNum.Version_Number();
#endif

    //------------------------------------------------------------------------
    //	Init player color-used flags
    //------------------------------------------------------------------------
    for (i = 0; i < MAX_MPLAYER_COLORS; i++) {
        color_used[i] = 0; // init all colors to available
    }
    color_used[Session.ColorIdx] = 1; // set my color to used
    playerlist.Set_Selected_Style(ColorListClass::SELECT_BAR, scheme);

    //------------------------------------------------------------------------
    //	Init random-number generator, & create a seed to be used for all random
    //	numbers from here on out
    //------------------------------------------------------------------------
    srand(time(NULL));
    Seed = rand();

    //------------------------------------------------------------------------
    //	Init the message display system
    //------------------------------------------------------------------------
    Session.Messages.Init(d_message_x + 1 * RESFACTOR,
                          d_message_y + 1 * RESFACTOR,
                          NUM_MESSAGES,
                          MAX_MESSAGE_LENGTH,
                          d_txt6_h,
                          d_send_x + 1 * RESFACTOR,
                          d_send_y + 1 * RESFACTOR,
                          1,
                          20,
                          MAX_MESSAGE_LENGTH - 5,
                          d_message_w);
    Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx,
                              TPF_TEXT,
                              NULL,
                              '_',
                              d_message_w);

    //------------------------------------------------------------------------
    //	Init the version-clipping system
    //------------------------------------------------------------------------
    VerNum.Init_Clipping();

    //------------------------------------------------------------------------
    //	Clear the list of players
    //------------------------------------------------------------------------
    Clear_Vector(&Session.Players);

    //------------------------------------------------------------------------
    //	Add myself to the list, and to the Players vector.
    //------------------------------------------------------------------------
    item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
    if (Session.House == HOUSE_GOOD) {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_ALLIES));
    } else {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_SOVIET));
    }
#else  // OLDWAY
    sprintf(item, "%s\t%s", Session.Handle, Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
    playerlist.Add_Item(item,
                        (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                 : &ColorRemaps[Session.ColorIdx]);

    who = new NodeNameType;
    strcpy(who->Name, Session.Handle);
    who->Player.House = Session.House;
    who->Player.Color = Session.ColorIdx;
    Session.Players.Add(who);
    Load_Title_Page(true);
    CCPalette.Set(); // GamePalette.Set();

    //
    // Now init the max range of the AI players slider.
    //
    aiplayersgauge.Set_Maximum(Rule.MaxPlayers - Session.Players.Count());
    aiplayersgauge.Set_Value(Session.Options.AIPlayers);

    //------------------------------------------------------------------------
    //	Processing loop
    //------------------------------------------------------------------------
    while (process) {
#if (SHOW_MONO)
        Ipx.Mono_Debug_Print(-1, 0);
#endif

        /*
        ** If we have just received input focus again after running in the background then
        ** we need to redraw.
        */
        if (AllSurfaces.SurfacesRestored) {
            AllSurfaces.SurfacesRestored = false;
            display = REDRAW_ALL;
        }

        //.....................................................................
        //	Refresh display if needed
        //.....................................................................
        if (display) {
            Hide_Mouse();

            //..................................................................
            //	Redraw backgound & dialog box
            //..................................................................
            if (display >= REDRAW_BACKGROUND) {
                Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);

                //...............................................................
                //	Dialog & Field labels
                //...............................................................
                Fancy_Text_Print(TXT_PLAYERS,
                                 d_playerlist_x + (d_playerlist_w / 2),
                                 d_playerlist_y - d_txt6_h,
                                 scheme,
                                 TBLACK,
                                 TPF_TEXT | TPF_CENTER);
                Fancy_Text_Print(TXT_SCENARIOS,
                                 d_scenariolist_x + (d_scenariolist_w / 2),
                                 d_scenariolist_y - d_txt6_h,
                                 scheme,
                                 TBLACK,
                                 TPF_TEXT | TPF_CENTER);
                Fancy_Text_Print(TXT_COUNT, d_count_x - 2 * RESFACTOR, d_count_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
                Fancy_Text_Print(TXT_LEVEL, d_level_x - 2 * RESFACTOR, d_level_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
                Fancy_Text_Print(
                    TXT_CREDITS_COLON, d_credits_x - 2 * RESFACTOR, d_credits_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
                Fancy_Text_Print(TXT_AI_PLAYERS_COLON,
                                 d_aiplayers_x - 2 * RESFACTOR,
                                 d_aiplayers_y,
                                 scheme,
                                 TBLACK,
                                 TPF_TEXT | TPF_RIGHT);
            }

            //..................................................................
            //	Redraw buttons
            //..................................................................
            if (display >= REDRAW_BUTTONS) {

                /*
                ** Zap, Zap, Zap
                */
                playerlist.Zap();
                scenariolist.Zap();
                rejectbtn.Zap();
                staticunit.Zap();
                staticlevel.Zap();
                staticcredits.Zap();
                staticaiplayers.Zap();
                countgauge.Zap();
                creditsgauge.Zap();
                aiplayersgauge.Zap();
                levelgauge.Zap();
                optionlist.Zap();
                okbtn.Zap();
                cancelbtn.Zap();
                loadbtn.Zap();

                /*
                ** Hack hack, hack
                */
                commands = &playerlist;
                scenariolist.Add_Tail(*commands);
                rejectbtn.Add_Tail(*commands);
                staticunit.Add_Tail(*commands);
                staticlevel.Add_Tail(*commands);
                staticcredits.Add_Tail(*commands);
                staticaiplayers.Add_Tail(*commands);
                countgauge.Add_Tail(*commands);
                creditsgauge.Add_Tail(*commands);
                aiplayersgauge.Add_Tail(*commands);
                levelgauge.Add_Tail(*commands);
                optionlist.Add_Tail(*commands);
                okbtn.Add_Tail(*commands);
                cancelbtn.Add_Tail(*commands);
#ifdef FIXIT_VERSION_3
                if (loadfile.Is_Available()) {
#else
                if (loadfile.Is_Available() && PlayingAgainstVersion > VERSION_RED_ALERT_104) {
#endif
#ifdef FIXIT_MULTI_SAVE
                    loadbtn.Add_Tail(*commands);
#endif
                }
                commands->Draw_All();
            }

            //..................................................................
            //	Draw the messages:
            //	- Erase an old message first
            //	- If we're in a game, print the game options (if they've been
            //	  received)
            //	- If we've been rejected from a game, print that message
            //..................................................................
            if (display >= REDRAW_MESSAGE) {
                Draw_Box(d_message_x, d_message_y, d_message_w, d_message_h, BOXSTYLE_BOX, true);
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true);
                Session.Messages.Draw();
            }

            //..................................................................
            // Update game parameter labels
            //..................................................................
            if (display >= REDRAW_PARMS) {
                //				LogicPage->Fill_Rect(d_count_x + d_count_w + 2*RESFACTOR, d_count_y, d_count_x + d_count_w +
                //35*RESFACTOR, d_aiplayers_y + d_aiplayers_h+RESFACTOR, BLACK);

                sprintf(txt, "%d", Session.Options.UnitCount);
                staticunit.Set_Text(txt);
                staticunit.Draw_Me();
                //				Fancy_Text_Print(txt, d_count_x + d_count_w + 2*RESFACTOR, d_count_y, scheme, BLACK,
                //TPF_TEXT);

                if (BuildLevel <= MPLAYER_BUILD_LEVEL_MAX) {
                    sprintf(txt, "%d", BuildLevel);
                } else {
                    sprintf(txt, "**");
                }
                staticlevel.Set_Text(txt);
                staticlevel.Draw_Me();
                //				Fancy_Text_Print(txt, d_level_x + d_level_w + 2*RESFACTOR, d_level_y, scheme, BLACK,
                //TPF_TEXT);

                sprintf(txt, "%d", Session.Options.Credits);
                staticcredits.Set_Text(txt);
                staticcredits.Draw_Me();
                //				Fancy_Text_Print(txt, d_credits_x + d_credits_w + 2*RESFACTOR, d_credits_y, scheme, BLACK,
                //TPF_TEXT);

                sprintf(txt, "%d", Session.Options.AIPlayers);
                staticaiplayers.Set_Text(txt);
                staticaiplayers.Draw_Me();
                //				Fancy_Text_Print(txt, d_aiplayers_x + d_aiplayers_w + 2*RESFACTOR, d_aiplayers_y, scheme,
                //BLACK, TPF_TEXT);
            }

            Show_Mouse();
            display = REDRAW_NONE;
        }

        //.....................................................................
        //	Get user input
        //.....................................................................
        input = commands->Input();

        //.....................................................................
        //	Process input
        //.....................................................................
        switch (input) {

            //..................................................................
            //	New Scenario selected.
            //..................................................................
#ifdef FIXIT_VERSION_3 //	All scenarios now allowable as downloads. ajw
        case (BUTTON_SCENARIOLIST | KN_BUTTON):
            if (scenariolist.Current_Index() != Session.Options.ScenarioIndex) {
                Session.Options.ScenarioIndex = scenariolist.Current_Index();
                transmit = 1;
            }
            break;

#else             //	FIXIT_VERSION_3
        case (BUTTON_SCENARIOLIST | KN_BUTTON):
            if (scenariolist.Current_Index() != Session.Options.ScenarioIndex) {
#ifdef FIXIT_CSII //	checked - ajw
                if ((PlayingAgainstVersion != VERSION_RED_ALERT_107 && PlayingAgainstVersion != VERSION_RED_ALERT_108
                     && PlayingAgainstVersion < VERSION_AFTERMATH_CS)
                    &&
#else
                if (PlayingAgainstVersion < VERSION_RED_ALERT_107 &&
#endif
                    Session.Scenarios[scenariolist.Current_Index()]->Get_Expansion()) {
                    scenariolist.Set_Selected_Index(Session.Options.ScenarioIndex);
                    Session.Messages.Add_Message(
                        NULL, 0, (char*)Text_String(TXT_NO_CS_SCENARIOS), PCOLOR_BROWN, TPF_TEXT, 1200);
                    Sound_Effect(VOC_SYS_ERROR);
                    if (display < REDRAW_MESSAGE)
                        display = REDRAW_MESSAGE;
#ifdef FIXIT_CSII //	checked - ajw
                } else if (PlayingAgainstVersion < VERSION_AFTERMATH_CS
                           && Is_Mission_126x126(
                               (char*)Session.Scenarios[scenariolist.Current_Index()]->Get_Filename())) {
                    scenariolist.Set_Selected_Index(Session.Options.ScenarioIndex);
                    Session.Messages.Add_Message(
                        NULL, 0, (char*)Text_String(TXT_NO_CS_SCENARIOS), PCOLOR_BROWN, TPF_TEXT, 1200);
                    Sound_Effect(VOC_SYS_ERROR);
                    if (display < REDRAW_MESSAGE)
                        display = REDRAW_MESSAGE;
#endif
                } else {
                    Session.Options.ScenarioIndex = scenariolist.Current_Index();
                    transmit = 1;
                }
            }
            break;
#endif //	FIXIT_VERSION_3

        //..................................................................
        //	Reject the currently-selected player (don't allow rejecting myself,
        //	who will be the first entry in the list)
        //..................................................................
        case (BUTTON_REJECT | KN_BUTTON):
            index = playerlist.Current_Index();

            if (index == 0) {
                Session.Messages.Add_Message(
                    NULL, 0, (char*)Text_String(TXT_CANT_REJECT_SELF), PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_SYS_ERROR);
                display = REDRAW_MESSAGE;
                break;

            } else if (index < 0 || index >= playerlist.Count()) {
                Session.Messages.Add_Message(
                    NULL, 0, (char*)Text_String(TXT_SELECT_PLAYER_REJECT), PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_SYS_ERROR);
                display = REDRAW_MESSAGE;
                break;
            }
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_REJECT_JOIN;

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[index]->Address));
            break;

        //..................................................................
        //	User adjusts max # units
        //..................................................................
        case (BUTTON_COUNT | KN_BUTTON):
            Session.Options.UnitCount = countgauge.Get_Value() + SessionClass::CountMin[Session.Options.Bases];
            transmit = 1;
            display = REDRAW_PARMS;
            break;

        //..................................................................
        //	User adjusts build level
        //..................................................................
        case (BUTTON_LEVEL | KN_BUTTON):
            BuildLevel = levelgauge.Get_Value() + 1;
            if (BuildLevel > MPLAYER_BUILD_LEVEL_MAX) // if it's pegged, max it out
                BuildLevel = MPLAYER_BUILD_LEVEL_MAX;
            transmit = 1;
            display = REDRAW_PARMS;
            break;

        //..................................................................
        //	User edits the credits value; retransmit new game options
        // Round the credits to the nearest 500.
        //..................................................................
        case (BUTTON_CREDITS | KN_BUTTON):
            Session.Options.Credits = creditsgauge.Get_Value();
            Session.Options.Credits = ((Session.Options.Credits + 250) / 500) * 500;
            transmit = 1;
            display = REDRAW_PARMS;
            break;

        //..................................................................
        //	User adjusts # of AI players
        //..................................................................
        case (BUTTON_AIPLAYERS | KN_BUTTON):
            Session.Options.AIPlayers = aiplayersgauge.Get_Value();
            if (Session.Options.AIPlayers + Session.Players.Count() > Rule.MaxPlayers) { // if it's pegged, max it out
                Session.Options.AIPlayers = Rule.MaxPlayers - Session.Players.Count();
                aiplayersgauge.Set_Value(Session.Options.AIPlayers);
            }
            transmit = 1;
            display = REDRAW_PARMS;
            break;

        //..................................................................
        // Toggle-able options:
        // If 'Bases' gets toggled, we have to change the range of the
        // UnitCount slider.
        // Also, if Tiberium gets toggled, we have to set the flags
        // in SpecialClass.
        //..................................................................
        case (BUTTON_OPTIONS | KN_BUTTON):
            if (Special.IsCaptureTheFlag != optionlist.Is_Checked(3) && !Special.IsCaptureTheFlag) {
                optionlist.Check_Item(0, true);
            }
            if (Session.Options.Bases != (optionlist.Is_Checked(0) ? 1 : 0)) {
                Session.Options.Bases = (optionlist.Is_Checked(0) ? 1 : 0);
                if (Session.Options.Bases) {
                    Session.Options.UnitCount =
                        Fixed_To_Cardinal(SessionClass::CountMax[1] - SessionClass::CountMin[1],
                                          Cardinal_To_Fixed(SessionClass::CountMax[0] - SessionClass::CountMin[0],
                                                            Session.Options.UnitCount - SessionClass::CountMin[0]))
                        + SessionClass::CountMin[1];
                } else {
                    Session.Options.UnitCount =
                        Fixed_To_Cardinal(SessionClass::CountMax[0] - SessionClass::CountMin[0],
                                          Cardinal_To_Fixed(SessionClass::CountMax[1] - SessionClass::CountMin[1],
                                                            Session.Options.UnitCount - SessionClass::CountMin[1]))
                        + SessionClass::CountMin[0];
                }
                countgauge.Set_Maximum(SessionClass::CountMax[Session.Options.Bases]
                                       - SessionClass::CountMin[Session.Options.Bases]);
                countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);
            }
            Session.Options.Tiberium = optionlist.Is_Checked(1);
            Special.IsTGrowth = Session.Options.Tiberium;
            Rule.IsTGrowth = Session.Options.Tiberium;
            Special.IsTSpread = Session.Options.Tiberium;
            Rule.IsTSpread = Session.Options.Tiberium;

            Session.Options.Goodies = optionlist.Is_Checked(2);
            Special.IsCaptureTheFlag = optionlist.Is_Checked(3);
            Special.IsShadowGrow = optionlist.Is_Checked(4);

            transmit = 1;
            display = REDRAW_PARMS;
            break;

        //..................................................................
        //	OK: exit loop with true status
        //..................................................................
        case (BUTTON_LOAD | KN_BUTTON):
        case (BUTTON_OK | KN_BUTTON):
            //...............................................................
            //	If a new player has joined in the last second, don't allow
            //	an OK; force a wait longer than 1 second (to give all players
            //	a chance to know about this new guy)
            //...............................................................
            i = max(Ipx.Global_Response_Time() * 2, 60);
            while ((int)TickCount - ok_timer < i) {
                Ipx.Service();
            }

            //...............................................................
            //	If there are at least 2 players, go ahead & play; error otherwise
            //...............................................................
            if (Session.Players.Count() > 1) {
                //				if (Session.Players.Count() + Session.Options.AIPlayers > 1 ) {
                rc = true;
                process = false;
            } else {
                Session.Messages.Add_Message(NULL, 0, (char*)Text_String(TXT_ONLY_ONE), PCOLOR_BROWN, TPF_TEXT, 1200);
                Sound_Effect(VOC_SYS_ERROR);
                display = REDRAW_MESSAGE;
            }
            if (input == (BUTTON_LOAD | KN_BUTTON)) {
                load_game = 1;
            } else {
                load_game = 0;
            }
            break;

        //..................................................................
        //	CANCEL: send a SIGN_OFF, bail out with error code
        //..................................................................
        case (KN_ESC):
        case (BUTTON_CANCEL | KN_BUTTON):
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_SIGN_OFF;
            strcpy(Session.GPacket.Name, Session.Handle);

            //...............................................................
            //	Broadcast my sign-off over my network
            //...............................................................
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;

            //...............................................................
            //	And now, just be absolutely sure, send my sign-off to each
            //	player in my game.  (If there's a bridge between us, the other
            //	player will have specified my address, so he can cross the
            //	bridge; but I may not have specified a bridge address, so the
            //	only way I have of crossing the bridge is to send a packet
            //	directly to him.)
            // Don't send this message to myself.
            //...............................................................
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                Ipx.Service();
            }
            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;
            Session.GameName[0] = 0;
            process = false;
            rc = false;
            break;

        //..................................................................
        //	Default: manage the inter-player messages
        //..................................................................
        default:
            if (Session.Messages.Manage()) {
                display = REDRAW_MESSAGE;
            }

            //...............................................................
            //	Re-draw the messages & service keyboard input for any message
            //	being edited.
            //...............................................................
            i = Session.Messages.Input(input);

            //...............................................................
            //	If 'Input' returned 1, it means refresh the message display.
            //...............................................................
            if (i == 1) {
                Hide_Mouse();
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true); // (erase the cursor)
                Session.Messages.Draw();
                Show_Mouse();
            } else if (i == 2) {
                //...............................................................
                //	If 'Input' returned 2, it means redraw the message display.
                //...............................................................
                Hide_Mouse();
                Draw_Box(d_send_x, d_send_y, d_send_w, d_send_h, BOXSTYLE_BOX, true); // (erase the cursor)
                Session.Messages.Draw();
                Show_Mouse();
            } else if (i == 3 || i == 4) {
                //...............................................................
                //	If 'input' returned 3, it means send the current message.
                //...............................................................
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_MESSAGE;
                strcpy(Session.GPacket.Name, Session.Handle);
                if (i == 3) {
                    strcpy(Session.GPacket.Message.Buf, Session.Messages.Get_Edit_Buf());
                } else {
                    strcpy(Session.GPacket.Message.Buf, Session.Messages.Get_Overflow_Buf());
                    Session.Messages.Clear_Overflow_Buf();
                }
                Session.GPacket.Message.Color = Session.ColorIdx;
                Session.GPacket.Message.NameCRC = Compute_Name_CRC(Session.GameName);

                //............................................................
                //	Send the message to every player in our player list, except
                // myself.
                //............................................................
                for (i = 1; i < Session.Players.Count(); i++) {
                    Ipx.Send_Global_Message(
                        &Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                    Ipx.Service();
                }

                //............................................................
                //	Add the message to our own list, since we're not in the
                // player list on this dialog.
                // If there's no message with this ID already displayed, just
                // add a new message; if there is one, concatenate it.
                //............................................................
                Session.Messages.Add_Message(
                    Session.GPacket.Name,
                    (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                          : Session.GPacket.Message.Color,
                    Session.GPacket.Message.Buf,
                    (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.ColorIdx,
                    TPF_TEXT,
                    -1);

                Session.Messages.Add_Edit((Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE
                                                                                   : Session.ColorIdx,
                                          TPF_TEXT,
                                          NULL,
                                          '_',
                                          d_message_w);

                display = REDRAW_MESSAGE;
            }
            break;
        }

        //.....................................................................
        //	Process incoming packets
        //.....................................................................
#ifndef FIXIT_VERSION_3
        int oldversion = PlayingAgainstVersion;
#endif
        whahoppa = Get_NewGame_Responses(&playerlist, color_used);
        if (whahoppa == EV_NEW_PLAYER) {
            ok_timer = TickCount;
            aiplayersgauge.Set_Maximum(Rule.MaxPlayers - Session.Players.Count());
            Session.Options.AIPlayers = aiplayersgauge.Get_Value();
            if (Session.Options.AIPlayers + Session.Players.Count() > Rule.MaxPlayers) { // if it's pegged, max it out
                Session.Options.AIPlayers = Rule.MaxPlayers - Session.Players.Count();
                aiplayersgauge.Set_Value(Session.Options.AIPlayers);
            }
#ifdef FIXIT_VERSION_3 //	All scenarios now allowable for download, regardless of if CS scen. or 126x126 scen.
            if (display < REDRAW_PARMS)
                display = REDRAW_PARMS;
#else             //	FIXIT_VERSION_3
            if (oldversion == PlayingAgainstVersion) {
                if (display < REDRAW_PARMS)
                    display = REDRAW_PARMS;
            } else {

                /*
                ** If a CS scenario was selected and we now have a red alert only player
                ** in the mix then deselect the cs scenario.
                */
#ifdef FIXIT_CSII //	checked - ajw
                if ((PlayingAgainstVersion != VERSION_RED_ALERT_107 && PlayingAgainstVersion != VERSION_RED_ALERT_108
                     && PlayingAgainstVersion < VERSION_AFTERMATH_CS)
                    &&
#else
                if (PlayingAgainstVersion < VERSION_RED_ALERT_107 &&
#endif
                    Session.Scenarios[scenariolist.Current_Index()]->Get_Expansion()) {
                    scenariolist.Set_Selected_Index(0);
                    Session.Options.ScenarioIndex = scenariolist.Current_Index();
                }
#ifdef FIXIT_CSII //	checked - ajw
                /*
                ** If an AM mega scenario was selected and we now have a non-AM
                ** player in the mix then deselect the mega scenario.
                */
                if (PlayingAgainstVersion < VERSION_AFTERMATH_CS
                    && Is_Mission_126x126((char*)Session.Scenarios[scenariolist.Current_Index()]->Get_Filename())) {
                    scenariolist.Set_Selected_Index(0);
                    Session.Options.ScenarioIndex = scenariolist.Current_Index();
                }
#endif
                display = REDRAW_BACKGROUND;
            }
#endif //	FIXIT_VERSION_3

            transmit = 1;
        } else if (whahoppa == EV_MESSAGE) {
            display = REDRAW_MESSAGE;
        } else if (whahoppa == EV_PLAYER_SIGNOFF) {
            aiplayersgauge.Set_Maximum(Rule.MaxPlayers - Session.Players.Count());
            Session.Options.AIPlayers = aiplayersgauge.Get_Value();
            if (Session.Options.AIPlayers + Session.Players.Count() > Rule.MaxPlayers) { // if it's pegged, max it out
                Session.Options.AIPlayers = Rule.MaxPlayers - Session.Players.Count();
                aiplayersgauge.Set_Value(Session.Options.AIPlayers);
                display = REDRAW_PARMS;
            }
            transmit = 1;
        }

        //.....................................................................
        //	If our Transmit flag is set, we need to send out a game option packet
        // Don't send it to myself.
        //.....................................................................
        if (transmit) {
            for (i = 1; i < Session.Players.Count(); i++) {
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

                Session.GPacket.Command = NET_GAME_OPTIONS;

                /*
                ** Set up the scenario info so the remote player can match the scenario on his machine
                ** or request a download if it doesnt exist
                */
                strcpy(Session.GPacket.ScenarioInfo.Scenario,
                       Session.Scenarios[Session.Options.ScenarioIndex]->Description());
                CCFileClass file(Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());
                Session.GPacket.ScenarioInfo.FileLength = file.Size();
#ifdef WOLAPI_INTEGRATION
                strcpy(Session.GPacket.ScenarioInfo.ShortFileName,
                       Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());
#else
                strncpy(Session.GPacket.ScenarioInfo.ShortFileName,
                        Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename(),
                        sizeof(Session.GPacket.ScenarioInfo.ShortFileName));
#endif
                strncpy((char*)Session.GPacket.ScenarioInfo.FileDigest,
                        Session.Scenarios[Session.Options.ScenarioIndex]->Get_Digest(),
                        sizeof(Session.GPacket.ScenarioInfo.FileDigest));
                Session.GPacket.ScenarioInfo.OfficialScenario =
                    Session.Scenarios[Session.Options.ScenarioIndex]->Get_Official();

                Session.GPacket.ScenarioInfo.Credits = Session.Options.Credits;
                Session.GPacket.ScenarioInfo.IsBases = Session.Options.Bases;
                Session.GPacket.ScenarioInfo.IsTiberium = Session.Options.Tiberium;
                Session.GPacket.ScenarioInfo.IsGoodies = Session.Options.Goodies;
                Session.GPacket.ScenarioInfo.BuildLevel = BuildLevel;
                Session.GPacket.ScenarioInfo.UnitCount = Session.Options.UnitCount;
                Session.GPacket.ScenarioInfo.AIPlayers = Session.Options.AIPlayers;
                Session.GPacket.ScenarioInfo.Seed = Seed;
                Session.GPacket.ScenarioInfo.Special = Special;
                Session.GPacket.ScenarioInfo.GameSpeed = Options.GameSpeed;
#ifdef FIXIT_VERSION_3
                Session.GPacket.ScenarioInfo.Version = VerNum.Get_Clipped_Version();
                //	Host encodes whether or not this is an Aftermath game in the highest bit.
                if (bAftermathMultiplayer) {
                    //					debugprint( "Host tells guests 'This is an Aftermath game'\n" );
                    Session.GPacket.ScenarioInfo.Version = VerNum.Get_Clipped_Version() | 0x80000000;
                } else {
                    //					debugprint( "Host tells guests 'This is NOT an Aftermath game'\n" );
                    Session.GPacket.ScenarioInfo.Version = VerNum.Get_Clipped_Version();
                }
#else
                Session.GPacket.ScenarioInfo.Version = VerNum.Get_Clipped_Version();
#endif

                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
            }
            Sound_Effect(VOC_OPTIONS_CHANGED);
            transmit = 0;
        }

        //.....................................................................
        //	Ping every player in my game, to force the Global Channel to measure
        //	the connection response time.  Don't ping myself (index 0).
        //.....................................................................
        if (TickCount - ping_timer > 15) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_PING;
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
            }
            ping_timer = TickCount;
        }

        //.....................................................................
        //	Service the Ipx connections
        //.....................................................................
        Ipx.Service();

        //.....................................................................
        //	Service the sounds & score; GameActive must be false at this point,
        //	so Call_Back() doesn't intercept global messages from me!
        //.....................................................................
        Call_Back();
        Frame_Limiter();
    }

    //------------------------------------------------------------------------
    //	Prepare to load the scenario
    //------------------------------------------------------------------------
    if (rc) {
        //.....................................................................
        //	Set the player count & scenario number
        //.....................................................................
        Session.NumPlayers = Session.Players.Count();

        Scen.Scenario = Session.Options.ScenarioIndex;
        strcpy(Scen.ScenarioName, Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());

        //.....................................................................
        //	Compute frame delay value for packet transmissions:
        //	- Divide global channel's response time by 8 (2 to convert to 1-way
        //	  value, 4 more to convert from ticks to frames)
        //.....................................................................
        if (Session.CommProtocol == COMM_PROTOCOL_MULTI_E_COMP) {
            Session.MaxAhead =
                max(((((Ipx.Global_Response_Time() / 8) + (Session.FrameSendRate - 1)) / Session.FrameSendRate)
                     * Session.FrameSendRate),
                    (Session.FrameSendRate * 2));
        } else {
            Session.MaxAhead = max((Ipx.Global_Response_Time() / 8), NETWORK_MIN_MAX_AHEAD);
        }

        Ipx.Set_Timing(25, (unsigned int)-1, 1000);

        //.....................................................................
        //	Send all players the NET_GO packet.  Wait until all ACK's have been
        //	received.
        //.....................................................................
        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
        if (load_game)
            Session.GPacket.Command = NET_LOADGAME;
        else
            Session.GPacket.Command = NET_GO;
        Session.GPacket.ResponseTime.OneWay = Session.MaxAhead;
        for (i = 1; i < Session.Players.Count(); i++) {
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
        }
        //.....................................................................
        //	Wait for all the ACK's to come in.
        //.....................................................................
        while (Ipx.Global_Num_Send() > 0) {
            Ipx.Service();
        }

        /*
        ** Wait for the go responses from each player in case someone needs the scenario
        ** file to be sent.
        */
        int responses[20]; // In big trub if more than 20 players
        memset(responses, 0, sizeof(responses));
        int num_responses = 0;
        bool send_scenario = false;
        WWDebugString("RA95 - About to wait for 'GO' response.");
        CDTimerClass<SystemTimerClass> response_timer; // timeout timer for waiting for responses
        response_timer = 60 * 10; // Wait for 10 seconds. If we dont hear by then assume someone crashed

        do {
            Ipx.Service();
            int retcode =
                Ipx.Get_Global_Message(&Session.GPacket, &Session.GPacketlen, &Session.GAddress, &Session.GProductID);
            if (retcode && Session.GProductID == IPXGlobalConnClass::COMMAND_AND_CONQUER0) {

                for (i = 1; i < Session.Players.Count(); i++) {
                    if (Session.Players[i]->Address == Session.GAddress) {
                        if (!responses[i]) {
                            if (Session.GPacket.Command == NET_REQ_SCENARIO) {
                                responses[i] = Session.GPacket.Command;
                                send_scenario = true;
                                num_responses++;
                            }
                            if (Session.GPacket.Command == NET_READY_TO_GO) {
                                responses[i] = Session.GPacket.Command;
                                num_responses++;
                            }
                        }
                    }
                }
            }
        } while (num_responses < Session.Players.Count() - 1 && response_timer);

#ifdef FIXIT_VERSION_3
        if (Session.Scenarios[Session.Options.ScenarioIndex]->Get_Official()) {
            if (!Force_Scenario_Available(Scen.ScenarioName))
                Emergency_Exit(EXIT_FAILURE);
        }
#endif

        /*
        ** If one of the machines requested that the scenario be sent then send it.
        */
        if (send_scenario) {
            memset(Session.ScenarioRequests, 0, sizeof(Session.ScenarioRequests));
            Session.RequestCount = 0;
            for (i = 1; i < Session.Players.Count(); i++) {
                if (responses[i] == NET_REQ_SCENARIO) {
                    Session.ScenarioRequests[Session.RequestCount++] = i;
                }
            }
            Send_Remote_File(Scen.ScenarioName, 1);
        }
    }

    //------------------------------------------------------------------------
    //	Init network timing values, using previous response times as a measure
    //	of what our retry delta & timeout should be.
    //------------------------------------------------------------------------
    // Ipx.Set_Timing (Ipx.Global_Response_Time() + 2, -1, Ipx.Global_Response_Time() * 4);
    Ipx.Set_Timing(Ipx.Global_Response_Time() + 2, (unsigned int)-1, max(120, Ipx.Global_Response_Time() * 8));

    //------------------------------------------------------------------------
    //	Clear all lists, but NOT the Games or Players vectors.
    //------------------------------------------------------------------------
    while (scenariolist.Count()) {
        scenariolist.Remove_Item(scenariolist.Get_Item(0));
    }
    Clear_Listbox(&playerlist);

    //------------------------------------------------------------------------
    // Remove the chat edit box
    //------------------------------------------------------------------------
    Session.Messages.Remove_Edit();

    //------------------------------------------------------------------------
    //	Restore screen
    //------------------------------------------------------------------------
    Hide_Mouse();
    Load_Title_Page(true);
    Show_Mouse();

    if (load_game) {
        if (!Load_Game(-1)) {
            WWMessageBox().Process(TXT_ERROR_LOADING_GAME);
            rc = false;
        }
        Frame++;
    }

    //------------------------------------------------------------------------
    // Clear the Players vector if we're not starting a game.
    //------------------------------------------------------------------------
    if (!rc) {
        Clear_Vector(&Session.Players);
    }

    //	while (optionlist.Count()>0) {
    //		item = (char *)optionlist.Get_Item(0);
    //		delete [] item;
    //		optionlist.Remove_Item(item);
    //	}

    return (rc);
} /* end of Net_New_Dialog */

/***************************************************************************
 * Get_NewGame_Responses -- processes packets for New Game dialog          *
 *                                                                         *
 * This routine can modify the contents of the given list box, as well		*
 * as the contents of the Players Vector global.									*
 *                                                                         *
 * INPUT:                                                                  *
 *		playerlist		list of players in this game									*
 *		color_used		array of color-used flags										*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		EV_NONE = nothing happened															*
 *		EV_NEW_PLAYER = a new player has joined; false otherwise					*
 *		EV_PLAYER_SIGNOFF = a player has left											*
 *		EV_MESSAGE = a message was received												*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   04/18/1995 BRR : Created.                                             *
 *=========================================================================*/
static JoinEventType Get_NewGame_Responses(ColorListClass* playerlist, int* color_used)
{
    int rc;
    char* item;        // general-purpose string
    NodeNameType* who; // node to add to Players Vector
    int i;
    int found;
    JoinEventType retval = EV_NONE;
    int resend;
    unsigned int version; // version # to use

    //------------------------------------------------------------------------
    //	If there is no incoming packet, just return
    //------------------------------------------------------------------------
    rc = Ipx.Get_Global_Message(&Session.GPacket, &Session.GPacketlen, &Session.GAddress, &Session.GProductID);
    if (!rc || Session.GProductID != IPXGlobalConnClass::COMMAND_AND_CONQUER0) {
        return (EV_NONE);
    }

    //------------------------------------------------------------------------
    //	Try to handle the packet in a standard way
    //------------------------------------------------------------------------
    if (Process_Global_Packet(&Session.GPacket, &Session.GAddress) != 0) {
        return (EV_NONE);
    } else if (Session.GPacket.Command == NET_QUERY_JOIN) {
        //------------------------------------------------------------------------
        //	NET_QUERY_JOIN:
        //------------------------------------------------------------------------
        //.....................................................................
        //	See if this name is unique:
        //	- If the name matches, but the address is different, reject this player
        //	- If the name & address match, this packet must be a re-send of a
        //	  previous request; in this case, do nothing.  The other player must have
        //	  received my CONFIRM_JOIN packet (since it was sent with an ACK
        //	  required), so we can ignore this resend.
        //.....................................................................
        found = 0;
        resend = 0;
        for (i = 1; i < Session.Players.Count(); i++) {
            if (!strcmp(Session.Players[i]->Name, Session.GPacket.Name)) {
                if (Session.Players[i]->Address != Session.GAddress) {
                    found = 1;
                } else {
                    resend = 1;
                }
                break;
            }
        }
        //.....................................................................
        // If his name is the same as mine, treat it like a duplicate name
        //.....................................................................
        if (!strcmp(Session.Players[0]->Name, Session.GPacket.Name)) {
            found = 1;
        }

        //.....................................................................
        //	Reject if name is a duplicate
        //.....................................................................
        if (found) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_REJECT_JOIN;
            Session.GPacket.Reject.Why = (int)REJECT_DUPLICATE_NAME;
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);
            return (EV_NONE);
        }

        //.....................................................................
        //	Reject if there are too many players
        //.....................................................................
        else if ((Session.Players.Count() >= Session.MaxPlayers) && !resend) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_REJECT_JOIN;
            Session.GPacket.Reject.Why = (int)REJECT_GAME_FULL;
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);
            return (EV_NONE);
        }

        /*
        **	Don't allow joining if the rules.ini file doesn't appear to match.
        */
        else if (Session.GPacket.PlayerInfo.CheatCheck != RuleINI.Get_Unique_ID() && !resend) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_REJECT_JOIN;
            Session.GPacket.Reject.Why = (int)REJECT_MISMATCH;
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);
            return (EV_NONE);
        }

        //.....................................................................
        //	If this packet is NOT a resend, accept the player.  Grant him the
        //	requested color if possible.
        //.....................................................................
        else if (!resend) {
            //..................................................................
            //	Check the player's version range against our own, to see if
            // there's an overlap region
            //..................................................................
#ifdef FIXIT_VERSION_3
            //	Added to the transmitted _min_ version number is a bit indicating presence of Aftermath expansion.
            bool bGuestHasAftermath = Session.GPacket.PlayerInfo.MinVersion & 0x80000000;
            if (bGuestHasAftermath)
                //				debugprint( "Host hears guest say 'I have Aftermath'\n" );
                ;
            else {
                //				debugprint( "Host hears guest say 'I don't have Aftermath'\n" );
                if (bAftermathMultiplayer) {
                    //					debugprint( "Host decides this is no longer an Aftermath game!\n" );
                    bAftermathMultiplayer = false;
                }
            }

            Session.GPacket.PlayerInfo.MinVersion &= ~0x80000000; //	Strip special bit.
#endif
            version = VerNum.Clip_Version(Session.GPacket.PlayerInfo.MinVersion, Session.GPacket.PlayerInfo.MaxVersion);

#ifndef FIXIT_VERSION_3
            PlayingAgainstVersion = version;
#endif

            // TCTCTC save off version number

            //..................................................................
            // Reject player if his version is too old
            //..................................................................
            if (version == 0) {
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_REJECT_JOIN;
                Session.GPacket.Reject.Why = (int)REJECT_VERSION_TOO_OLD;
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);
                return (EV_NONE);
            }

            //..................................................................
            // Reject player if his version is too new
            //..................................................................
            else if (version == 0xffffffff) {
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_REJECT_JOIN;
                Session.GPacket.Reject.Why = (int)REJECT_VERSION_TOO_NEW;
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);
                return (EV_NONE);
            }
            //..................................................................
            // If the player is accepted, our mutually-accepted version may be
            // different; set the CommProtocol accordingly.
            //..................................................................
            else {
                Session.CommProtocol = VerNum.Version_Protocol(version);
            }

            //..................................................................
            //	Add node to the Vector list
            //..................................................................
            who = new NodeNameType;
            strcpy(who->Name, Session.GPacket.Name);
            who->Address = Session.GAddress;
            who->Player.House = Session.GPacket.PlayerInfo.House;
            Session.Players.Add(who);

            //..................................................................
            //	Set player's color; if requested color isn't used, give it to him;
            //	otherwise, give him the 1st available color.  Mark the color we
            //	give him as used.
            //..................................................................
            if (color_used[Session.GPacket.PlayerInfo.Color] == 0) {
                who->Player.Color = Session.GPacket.PlayerInfo.Color;
            } else {
                for (i = 0; i < MAX_MPLAYER_COLORS; i++) {
                    if (color_used[i] == 0) {
                        who->Player.Color = (PlayerColorType)i;
                        break;
                    }
                }
            }
            color_used[who->Player.Color] = 1;

            //..................................................................
            //	Add player name to the list box
            //..................................................................
            item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
            if (Session.GPacket.PlayerInfo.House == HOUSE_GOOD) {
                sprintf(item, "%s\t%s", Session.GPacket.Name, Text_String(TXT_ALLIES));
            } else {
                sprintf(item, "%s\t%s", Session.GPacket.Name, Text_String(TXT_SOVIET));
            }
#else  // OLDWAY
            sprintf(item,
                    "%s\t%s",
                    Session.GPacket.Name,
                    Text_String(HouseTypeClass::As_Reference(Session.GPacket.PlayerInfo.House).Full_Name()));
#endif // OLDWAY
            playerlist->Add_Item(item,
                                 (who->Player.Color == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                           : &ColorRemaps[who->Player.Color]);

            //..................................................................
            //	Send a confirmation packet
            //..................................................................
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_CONFIRM_JOIN;
            strcpy(Session.GPacket.Name, Session.Handle);
            Session.GPacket.PlayerInfo.House = who->Player.House;
            Session.GPacket.PlayerInfo.Color = who->Player.Color;

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.GAddress);

            //..................................................................
            // Play a special sound.
            //..................................................................
            Sound_Effect(VOC_PLAYER_JOINED);

            retval = EV_NEW_PLAYER;
        }
    }

    //------------------------------------------------------------------------
    //	NET_SIGN_OFF: Another system is signing off: search for that system in
    //	the player list, & remove it if found
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_SIGN_OFF) {
        for (i = 0; i < Session.Players.Count(); i++) {

            //..................................................................
            //	Name found; remove it
            //..................................................................
            if (!strcmp(Session.Players[i]->Name, Session.GPacket.Name)
                && Session.Players[i]->Address == Session.GAddress) {

                //...............................................................
                //	Remove from the list box
                //...............................................................
                item = (char*)(playerlist->Get_Item(i));
                playerlist->Remove_Item(item);
                playerlist->Flag_To_Redraw();
                delete[] item;

                //...............................................................
                //	Mark his color as available
                //...............................................................
                color_used[Session.Players[i]->Player.Color] = 0;

                //...............................................................
                //	Delete from the Vector list
                //...............................................................
                delete Session.Players[i];
                Session.Players.Delete(Session.Players[i]);

                //...............................................................
                // Play a special sound.
                //...............................................................
                Sound_Effect(VOC_PLAYER_LEFT);

                retval = EV_PLAYER_SIGNOFF;

                break;
            }
        }
    }

    //------------------------------------------------------------------------
    //	NET_MESSAGE: Someone is sending us a message
    //------------------------------------------------------------------------
    else if (Session.GPacket.Command == NET_MESSAGE) {
        Session.Messages.Add_Message(
            Session.GPacket.Name,
            (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.GPacket.Message.Color,
            Session.GPacket.Message.Buf,
            (Session.GPacket.Message.Color == PCOLOR_DIALOG_BLUE) ? PCOLOR_REALLY_BLUE : Session.GPacket.Message.Color,
            TPF_TEXT,
            -1);

        Sound_Effect(VOC_INCOMING_MESSAGE);

        retval = EV_MESSAGE;
    }

    return (retval);

} /* end of Get_NewGame_Responses */

/***************************************************************************
 * Compute_Name_CRC -- computes CRC from char string                       *
 *                                                                         *
 * INPUT:                                                                  *
 *		name		string to create CRC for												*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		CRC																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   06/29/1995 BRR : Created.                                             *
 *=========================================================================*/
unsigned int Compute_Name_CRC(char* name)
{
    char buf[80];
    unsigned int crc = 0;
    int i;

    strcpy(buf, name);
    strupr(buf);

    for (i = 0; i < (int)strlen(buf); i++) {
        Add_CRC(&crc, (unsigned int)buf[i]);
    }

    return (crc);

} /* end of Compute_Name_CRC */

/***************************************************************************
 * Net_Reconnect_Dialog -- Draws/updates the network reconnect dialog      *
 *                                                                         *
 * INPUT:                                                                  *
 *		reconn			1 = reconnect, 0 = waiting for first-time connection	*
 *		fresh				1 = draw from scratch, 0 = only update time counter	*
 *		oldest_index	IPX connection index of oldest connection 				*
 *							(only used for reconnection)									*
 *		timeval			value to print in the countdown field						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   07/08/1995 BRR : Created.                                             *
 *=========================================================================*/
void Net_Reconnect_Dialog(int reconn, int fresh, int oldest_index, unsigned int timeval)
{
    reconn;
    fresh;
    oldest_index;
    timeval;
#if (0) // PG
    static int x, y, w, h;
    int id;
    char buf1[40] = {0};
    char buf2[40] = {0};
    char const* buf3 = "";

    int d_txt6_h = 6 * RESFACTOR + 1;
    int d_margin = 5 * RESFACTOR;

    RemapControlType* scheme = GadgetClass::Get_Color_Scheme();

    /*
    ** If we have just received input focus again after running in the background then
    ** we need to redraw.
    */
    if (AllSurfaces.SurfacesRestored) {
        AllSurfaces.SurfacesRestored = false;
        fresh = true;
    }

    //------------------------------------------------------------------------
    //	Draw the dialog from scratch
    //------------------------------------------------------------------------
    if (fresh) {
        Fancy_Text_Print("", 0, 0, scheme, TBLACK, TPF_CENTER | TPF_TEXT);

        switch (Session.Type) {
        case GAME_IPX:
        case GAME_INTERNET:
        case GAME_MODEM:
        case GAME_NULL_MODEM:

            if (reconn) {
                id = Ipx.Connection_ID(oldest_index);
                sprintf(buf1, Text_String(TXT_RECONNECTING_TO), Ipx.Connection_Name(id));
            } else {
                sprintf(buf1, Text_String(TXT_WAITING_FOR_CONNECTIONS));
            }
            break;
        }

        sprintf(buf2, Text_String(TXT_TIME_ALLOWED), timeval + 1);
        buf3 = Text_String(TXT_PRESS_ESC);

        w = max(String_Pixel_Width(buf1), String_Pixel_Width(buf2));

#ifdef FIXIT_VERSION_3
        char szNewCancelMessage[300];
        sprintf(szNewCancelMessage, "%s%s", buf3, TXT_WOL_CANCELMEANSFORFEIT);

        if (Session.Type == GAME_INTERNET && pWolapi && pWolapi->GameInfoCurrent.bTournament) {
            w = max(String_Pixel_Width(szNewCancelMessage),
                    w); // * RESFACTOR;		why was it ever multiplied by this!!!?
            w += (d_margin * 12);
        } else {
            w = max(String_Pixel_Width(buf3), w) * RESFACTOR;
            w += (d_margin * 5);
        }
#else
        w = max(String_Pixel_Width(buf3), w) * RESFACTOR;
        w += (d_margin * 5);
#endif

        h = (d_txt6_h * 3) + (d_margin * 6);
        x = 160 * RESFACTOR - (w / 2);
        y = 100 * RESFACTOR - (h / 2);

        Hide_Mouse();
        Set_Logic_Page(SeenBuff);
        Dialog_Box(x, y, w, h);

        Fancy_Text_Print(buf1, 160 * RESFACTOR, y + (d_margin * 2), scheme, TBLACK, TPF_CENTER | TPF_TEXT);

        Fancy_Text_Print(
            buf2, 160 * RESFACTOR, y + (d_margin * 2) + d_txt6_h + d_margin, scheme, TBLACK, TPF_CENTER | TPF_TEXT);

#ifdef FIXIT_VERSION_3
        if (Session.Type == GAME_INTERNET && pWolapi && pWolapi->GameInfoCurrent.bTournament)
            Fancy_Text_Print(szNewCancelMessage,
                             160 * RESFACTOR,
                             y + (d_margin * 2) + (d_txt6_h + d_margin) * 2,
                             scheme,
                             TBLACK,
                             TPF_CENTER | TPF_TEXT);
        else
            Fancy_Text_Print(buf3,
                             160 * RESFACTOR,
                             y + (d_margin * 2) + (d_txt6_h + d_margin) * 2,
                             scheme,
                             TBLACK,
                             TPF_CENTER | TPF_TEXT);
#else
        Fancy_Text_Print(buf3,
                         160 * RESFACTOR,
                         y + (d_margin * 2) + (d_txt6_h + d_margin) * 2,
                         scheme,
                         TBLACK,
                         TPF_CENTER | TPF_TEXT);
#endif

        Show_Mouse();
    }
    //------------------------------------------------------------------------
    //	Just update the timeout value on the dialog
    //------------------------------------------------------------------------
    else {
        Hide_Mouse();
        Set_Logic_Page(SeenBuff);

        sprintf(buf2, Text_String(TXT_TIME_ALLOWED), timeval + 1);

        int fillx = 160 * RESFACTOR - (String_Pixel_Width(buf2) / 2) - 6;
        LogicPage->Fill_Rect(fillx,
                             y + (d_margin * 2) + d_txt6_h + d_margin,
                             fillx + String_Pixel_Width(buf2) + 12,
                             y + (d_margin * 2) + d_txt6_h + d_margin + d_txt6_h + 1 * RESFACTOR,
                             BLACK);

        Fancy_Text_Print(
            buf2, 160 * RESFACTOR, y + (d_margin * 2) + d_txt6_h + d_margin, scheme, BLACK, TPF_CENTER | TPF_TEXT);

        Show_Mouse();
    }
#endif
} /* end of Net_Reconnect_Dialog */

#define MAX_CHAT_PHRASE 45

struct WWPerson
{
    char Name[MPLAYER_NAME_MAX];
    char Phrase[MAX_CHAT_PHRASE];
    PlayerColorType Color;
    unsigned int LastTime;
};

struct WWPerson WWPersons[] = {
    {"Bill R", "Hey, isn't this cool?", PCOLOR_GREEN, 0},
    {"Bill R", "Hey, want some seafood?", PCOLOR_GREEN, 0},
    {"Barry G", "Great. Just great.", PCOLOR_GREEN, 0},
    {"Barry G", "Wonderful. Perfect, in fact.", PCOLOR_GREEN, 0},
    {"Bill P", "You call this AI?  Sheesh!", PCOLOR_GREEN, 0},
    {"Bill P", "Bills rule!", PCOLOR_GREEN, 0},
    {"Brett S", "Take this out, now!", PCOLOR_GREEN, 0},
    {"Brett S", "Why can't you fit any more letters on the l", PCOLOR_GREEN, 0},
    {"Brett S", "Ed, I want to talk to you about this.", PCOLOR_GREEN, 0},
    {"Brett S", "Ed, come to my office, now!  [crack!]", PCOLOR_GREEN, 0},
    {"Penina F", "Has anyone seen my stapler?", PCOLOR_GREEN, 0},
    {"Penina F", "Has anyone seen my thesaurus?", PCOLOR_GREEN, 0},
    {"Penina F", "Has anyone seen my pencil?", PCOLOR_GREEN, 0},
    {"Milo B", "Hey Steve, can we meet in your office?", PCOLOR_GREEN, 0},
    {"Milo B", "Steve, it'll only take a minute.", PCOLOR_GREEN, 0},
    {"Rob S", "Milo, dial 249.", PCOLOR_GREEN, 0},
    {"Elsbeth W", "That's 'Elsbeth', with an E-L-S.", PCOLOR_GREEN, 0},
    {"Elsbeth W", "That's 'Elsbeth', not 'Elsbarf'.", PCOLOR_GREEN, 0},
    {"Elsbeth W", "That's 'Elsbeth', not 'Elfbutt'.", PCOLOR_GREEN, 0},
    {"Karen G", "This is sooo weird!", PCOLOR_GREEN, 0},
    {"Karen G", "Duh!", PCOLOR_GREEN, 0},
    {"Phil G", "Hey, it worked on my computer!", PCOLOR_GREEN, 0},
    {"Phil G", "This thing is clearly a waste of memory.", PCOLOR_GREEN, 0},
    {"Joe B", "Hmm, I see!", PCOLOR_GREEN, 0},
    {"Joe B", "Ahh, yes.", PCOLOR_GREEN, 0},
    {"Maria DMML", "Not another installer!", PCOLOR_GREEN, 0},
    {"Maria DMML", "HA HA HA HA HA!", PCOLOR_GREEN, 0},
    {"Mike L", "This is really cool!  Gosh guys!  Wow!", PCOLOR_GREEN, 0},
    {"Mike L", "Wow!  You guys are the greatest!", PCOLOR_GREEN, 0},
    {"Mike G", "Grayfish for lunch again?", PCOLOR_GREEN, 0},
    {"Mike G", "This is lame.", PCOLOR_GREEN, 0},
    {"Glenn S", "This thing's bugged.", PCOLOR_GREEN, 0},
    {"Glenn S", "Ship it!!", PCOLOR_GREEN, 0},
    {"Steve W", "OK everyone, out of my office.", PCOLOR_GREEN, 0},
    {"Ed D", "A good chat program is like a ninja...", PCOLOR_GREEN, 0},
    {"Ed D", "A good ninja is like a ninja...", PCOLOR_GREEN, 0},
    {"Erik Y", "I wear designer jeans.", PCOLOR_GREEN, 0},
    {"Erik Y", "Hey Bill, this thing keeps cra\031\032\030hing", PCOLOR_GREEN, 0},
    {"Jeff F", "This is absolutely the best ever!", PCOLOR_GREEN, 0},
    {"Jeff F", "This is absolutely the worst ever!", PCOLOR_GREEN, 0},
    {"Rick N", "Sounds like a driver problem.", PCOLOR_GREEN, 0},
    {"Scott B", "I need this for LOL2!", PCOLOR_GREEN, 0},
    {"Steve T", "This seems rather silly, actually.", PCOLOR_GREEN, 0},
    {"Paul M", "No ....  W A Y!!", PCOLOR_GREEN, 0},
    {"Rick P", "AH HA HA HA!!  HAHA!!  AHH HA!", PCOLOR_GREEN, 0},
    {"Joe K", "Stalin killed my father.", PCOLOR_GREEN, 0},
    {"Joe K", "I hope you're proud of yourself, Bill.", PCOLOR_GREEN, 0},
    {"Big Willy", "Anybody fer a spittin' contest?", PCOLOR_GREEN, 0},
    {"Eric W", "Karate is good but you use it for evil!", PCOLOR_GREEN, 0},
    {"Ted M", "Ha.  Funny.  This is funny.", PCOLOR_GREEN, 0},
    {"Damon R", "I blame the French.", PCOLOR_GREEN, 0},
    {"Dwight O", "Oh, man!", PCOLOR_GREEN, 0},
    {"Kia H", "Ion cannon ready.", PCOLOR_GREEN, 0},
    {"Kia H", "Select target.", PCOLOR_GREEN, 0},
    {"Bruce J", "I am the Genie of the lamp!", PCOLOR_GREEN, 0},
    {"Laura W", "Go away, Bill", PCOLOR_GREEN, 0},
    {"David D", "Hmmm...", PCOLOR_GREEN, 0},
    {"Mike Li", "Call me Nate.", PCOLOR_GREEN, 0},
    {"Denzil L", "This would be better on the Mac.", PCOLOR_GREEN, 0},
    {"Greg H", "But does it fit into 2 MB?", PCOLOR_GREEN, 0},
    {"Jonathan L", "I think I need a haircut.", PCOLOR_GREEN, 0},
    {"Adam I", "Whatever.  Beef!", PCOLOR_GREEN, 0},
    {"Kurt O", "Fix it yourself.  I'm busy.", PCOLOR_GREEN, 0},
    {"Joseph H", "<Icy glare>", PCOLOR_GREEN, 0},
    {"Matt H", "I'm never good with clever on the spot...", PCOLOR_GREEN, 0},
    {"Chris R", "I love my job.  Yeah.", PCOLOR_GREEN, 0},
    {"Pat C", "I'm a professional yodeller.", PCOLOR_GREEN, 0},
    {"Ian L", "", PCOLOR_GREEN, 0},
    {"Jack M", "I live about a block from the proposed site.", PCOLOR_GREEN, 0},
    {"Jeff B", "They all look like ants from up here.", PCOLOR_GREEN, 0},
};

CDTimerClass<SystemTimerClass> wwperson_timer;

void Start_WWChat(ColorListClass* playerlist)
{
    char* item;
    int i;
    HousesType house;

    //------------------------------------------------------------------------
    // Ensure a different sequence each time
    //------------------------------------------------------------------------
    Scen.RandomNumber = rand();

    //------------------------------------------------------------------------
    // Add myself to the player list
    //------------------------------------------------------------------------
    item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
    if (Session.House == HOUSE_GOOD) {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_ALLIES));
    } else {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_SOVIET));
    }
#else  // OLDWAY
    sprintf(item, "%s\t%s", Session.Handle, Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
    playerlist->Add_Item(item,
                         (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                  : &ColorRemaps[Session.ColorIdx]);

    //------------------------------------------------------------------------
    // Add everyone else to the list
    //------------------------------------------------------------------------
    for (i = 0; i < sizeof(WWPersons) / sizeof(struct WWPerson); i++) {
        //.....................................................................
        // Add the 1st entry to the list no matter what; for entries after the
        // 1st, only add the name if it's different from the previous name.
        //.....................................................................
        if (i == 0 || strcmp(WWPersons[i].Name, WWPersons[i - 1].Name)) {
            WWPersons[i].Color = (PlayerColorType)(Random_Pick(0, (int)(PCOLOR_LAST - 1)));
            if (Percent_Chance(50)) {
                house = HOUSE_GREECE;
            } else {
                house = HOUSE_USSR;
            }
            //			house = (HousesType)Random_Pick((int)HOUSE_GOOD,(int)HOUSE_BAD);
            item = new char[MPLAYER_NAME_MAX + 64];
#if defined(__GNUC__) && (__GNUC__ >= 10)
#pragma GCC diagnostic push
/* avoid a false positive */
#pragma GCC diagnostic ignored "-Wformat-overflow"
#endif
            if (house != HOUSE_USSR && house != HOUSE_UKRAINE) {
                sprintf(item, "%s\t%s", WWPersons[i].Name, Text_String(TXT_ALLIES));
            } else {
                sprintf(item, "%s\t%s", WWPersons[i].Name, Text_String(TXT_SOVIET));
            }
#if defined(__GNUC__) && (__GNUC__ >= 10)
#pragma GCC diagnostic pop
#endif
            playerlist->Add_Item(item,
                                 (WWPersons[i].Color == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                            : &ColorRemaps[WWPersons[i].Color]);
        }
        //.....................................................................
        // If this entry's name is the same as the previous, copy the color
        // value from the previous entry.
        //.....................................................................
        else if (i > 0) {
            WWPersons[i].Color = WWPersons[i - 1].Color;
        }
    }

    // wwperson_timer = 240;
    wwperson_timer = Random_Pick(90, 360);

} // end of Start_WWChat

int Update_WWChat(void)
{
    int i;
    int j;

    //------------------------------------------------------------------------
    // Do nothing if it's too soon.
    //------------------------------------------------------------------------
    if (wwperson_timer > 0) {
        return (0);
    }

    // wwperson_timer = 240;
    wwperson_timer = Random_Pick(90, 360);

    //------------------------------------------------------------------------
    // Even after the timer's gone off, there's only a 1/8 chance we'll
    // print a message.
    //------------------------------------------------------------------------
    if (Percent_Chance(12)) {
        return (0);
    }

    //------------------------------------------------------------------------
    // Randomly select a message to "send"; ensure we pick a message that
    // hasn't been recently displayed.
    //------------------------------------------------------------------------
    j = sizeof(WWPersons) / sizeof(struct WWPerson);
    i = Random_Pick(0, j - 1);
    if ((TickCount - WWPersons[i].LastTime) < 1800 && WWPersons[i].LastTime != 0) {
        return (0);
    }

    Session.Messages.Add_Message(WWPersons[i].Name, 0, WWPersons[i].Phrase, WWPersons[i].Color, TPF_TEXT, -1);
    WWPersons[i].LastTime = TickCount;

    return (1);

} // end of Update_WWChat

#if (0)
/*****************************************************************************/
void Start_Logging(void)
{
    FILE* fp;
    static char* ColorNames[6] = {
        "Yellow",
        "Red",
        "BlueGreen",
        "Orange",
        "Green",
        "Blue",
    };
    int i;
    int id;
    HousesType house;
    char* housenames[] = {
        "ALLIES",
        "SOVIET",
        "Neutral",
        "Special",
        "Multi1",
        "Multi2",
        "Multi3",
        "Multi4",
    };
    time_t t;

    fp = fopen("NETPLAY.LOG", "at");
    if (!fp)
        return;

    /*
    **	Print game header
    */
    t = time(NULL);
    fprintf(fp, "==============================================================\n");
    fprintf(fp, "Date:            %s", ctime(&t));
    fprintf(fp, "Scenario:        %s\n", Session.Scenarios[Session.Options.ScenarioIndex].Description());
    fprintf(fp, "Total # players: %d\n", Session.NumPlayers);
    fprintf(fp, "==============================================================\n");

    /*
    **	Print connection info
    */

    /*
    **	Print player data
    */
    fprintf(fp, "Local Player:\n");
    fprintf(fp, "             Name:%s\n", Session.Handle);
    fprintf(fp, "            Color:%s\n", ColorNames[Session.ColorIdx]);
    fprintf(fp, "            House:%s\n", housenames[Session.House]);
    fprintf(fp, "\n");

    for (i = 0; i < Ipx.Num_Connections(); i++) {
        id = Ipx.Connection_ID(i);
        house = (HouseClass)id;
        fprintf(fp, "Connection %d:\n", i);
        fprintf(fp, "             Name:%s\n", Ipx.Connection_Name(id));
        fprintf (fp,"            Color:%s\n",
			ColorNames[HouseClass::As_Pointer(house)->RemapColor);
        fprintf(fp, "            House:%s\n", housenames[house]);
        fprintf(fp, "\n");
    }

    /*
    **	Print game options
    */
    fprintf(fp, "Game Options:\n");
    fprintf(fp, "             Bases:%d\n", Session.Options.Bases);
    fprintf(fp, "           Credits:%d\n", Session.Options.Credits);
    fprintf(fp, "          Tiberium:%d\n", Session.Options.Tiberium);
    fprintf(fp, "            Crates:%d\n", Session.Options.Goodies);
    fprintf(fp, "        AI Players:%d\n", Session.Options.AIPlayers);
    fprintf(fp, "       Build Level:%d\n", BuildLevel);
    fprintf(fp, "        Unit Count:%d\n", Session.Options.UnitCount);
    fprintf(fp, "              Seed:%d\n", Seed);
    fprintf(fp, "\n");

    fclose(fp);

} /* end of Start_Logging */

void Log_Message(char* msg)
{
    FILE* fp;

    fp = fopen("NETPLAY.LOG", "at");
    if (!fp)
        return;

    fprintf(fp, "%s (Frame:%d)\n", msg, Frame);
    fclose(fp);
    flushall();

} /* end of Log_Message */
#endif

#ifndef WOLAPI_INTEGRATION //	Rest of file ifdeffed out.

// extern bool Spawn_WChat(bool can_launch);

/***********************************************************************************************
 * Net_Fake_New_Dialog -- lets user start a new game                                           *
 *                                                                                             *
 * This dialog shows a list of who's requesting to join this game, and lets                    *
 * the game initiator selectively approve each user.                                           *
 *                                                                                             *
 * The 'Players' vector matches one-for-one the contents of the list box.  The local system	  *
 * is always listed first; the IPX Address of the local system will NOT be valid in the		  *
 * Players vector.  The Games & Players vectors should be left filled in even after this		  *
 * routine is exited; their contents are used to form connections to the other players,		  *
 * after the scenario has been loaded.																			  *
 *                                                                                             *
 *    旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴커                                           *
 *    �              New Network Game              �                                           *
 *    �                                            �                                           *
 *    �     Players               Scenario         �                                           *
 *    � 旼컴컴컴컴컴컴쩡�   旼컴컴컴컴컴컴컴컴컫커 �                                           *
 *    � � Boffo       �.�   � Green Acres      �.� �                                           *
 *    � � Bozo        쳐�   � Brown Sewers     쳐� �                                           *
 *    � � Bonzo       � �   �      ...         � � �                                           *
 *    � �             쳐�   �                  쳐� �                                           *
 *    � �             �.�   �                  �.� �                                           *
 *    � 읕컴컴컴컴컴컴좔�   읕컴컴컴컴컴컴컴컴컨켸 �                                           *
 *    �     [Reject]             Count:--- ##      �                                           *
 *    �                          Level:--- ##      �                                           *
 *    �                                            �                                           *
 *    �               Credits: _____               �                                           *
 *    �       [  Bases   ]   [   Crates   ]        �                                           *
 *    �       [ Tiberium ]   [ AI Players ]        �                                           *
 *    �  旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�   �                                           *
 *    �  �                                     �   �                                           *
 *    �  �                                     �   �                                           *
 *    �  읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�   �                                           *
 *    �  旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�   �                                           *
 *    �  읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�   �                                           *
 *    �              [OK]    [Cancel]              �                                           *
 *    읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸                                           *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      true = success, false = cancel                                                         *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      Session.GameName must contain this player's name.                                      *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
static int Net_Fake_New_Dialog(void)
{
#ifdef WIN32
    //------------------------------------------------------------------------
    //	Dialog & button dimensions
    //------------------------------------------------------------------------
    int d_dialog_w = 120 * RESFACTOR;                      // dialog width
    int d_dialog_h = 80 * RESFACTOR;                       // dialog height
    int d_dialog_x = ((320 * RESFACTOR - d_dialog_w) / 2); // dialog x-coord
    int d_dialog_y = ((200 * RESFACTOR - d_dialog_h) / 2); // centered y-coord
    int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);       // center x-coord

    int d_txt6_h = 6 * RESFACTOR + 1; // ht of 6-pt text
    int d_margin1 = 5 * RESFACTOR;    // margin width/height
    int d_margin2 = 2 * RESFACTOR;    // margin width/height

    int d_playerlist_w = 118 * RESFACTOR;
    int d_playerlist_h = ((6 * 6) + 3) * RESFACTOR; // 6 rows high
    // int d_playerlist_x = d_dialog_x + d_margin1 + d_margin1;
    int d_playerlist_x = 500 * RESFACTOR;
    int d_playerlist_y = d_dialog_y + d_margin1 + d_txt6_h;

#if (GERMAN | FRENCH)
    int d_cancel_w = 50 * RESFACTOR;
#else
    int d_cancel_w = 40 * RESFACTOR;
#endif
    int d_cancel_h = 9 * RESFACTOR;
    int d_cancel_x = d_dialog_cx - d_cancel_w / 2;
    int d_cancel_y = d_dialog_y + d_dialog_h - 20 * RESFACTOR;

    //------------------------------------------------------------------------
    //	Button Enumerations
    //------------------------------------------------------------------------
    enum
    {
        BUTTON_PLAYERLIST = 100,
        BUTTON_CANCEL,
    };

    //------------------------------------------------------------------------
    //	Redraw values: in order from "top" to "bottom" layer of the dialog
    //------------------------------------------------------------------------
    typedef enum
    {
        REDRAW_NONE = 0,
        REDRAW_PARMS,
        REDRAW_MESSAGE,
        REDRAW_BUTTONS,
        REDRAW_BACKGROUND,
        REDRAW_ALL = REDRAW_BACKGROUND
    } RedrawType;

    //------------------------------------------------------------------------
    //	Dialog variables
    //------------------------------------------------------------------------
    RedrawType display = REDRAW_ALL; // redraw level
    bool process = true;             // process while true
    KeyNumType input;

    int transmit; // 1 = re-transmit new game options

    int ok_timer = 0; // for timing OK button
    int rc;
    int i;
    char* item;
    int tabs[] = {77};      // tabs for player list box
    int optiontabs[] = {8}; // tabs for option list box

    NodeNameType* who;  // node to add to Players
    int ping_timer = 0; // for sending Ping packets

    int color_used[MAX_MPLAYER_COLORS]; // 1 = color has been used
    JoinEventType whahoppa;             // event generated by received packets
    CCFileClass loadfile("SAVEGAME.NET");
    int load_game = 0; // 1 = load a saved game
    RemapControlType* scheme = GadgetClass::Get_Color_Scheme();

    int width;
    int height;

    bool player_joined = false;
    CountDownTimerClass join_timer;

    // PG_TO_FIX
    // Fancy_Text_Print(TXT_NONE,0,0,TBLACK,TBLACK,TPF_6PT_GRAD | TPF_NOSHADOW);
    Format_Window_String((char*)TXT_HACKHACK, SeenBuff.Get_Height(), width, height);

    //------------------------------------------------------------------------
    // Buttons
    //------------------------------------------------------------------------
    GadgetClass* commands; // button list

    ColorListClass playerlist(BUTTON_PLAYERLIST,
                              d_playerlist_x,
                              d_playerlist_y,
                              d_playerlist_w,
                              d_playerlist_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));

    TextButtonClass cancelbtn(BUTTON_CANCEL,
                              TXT_CANCEL,
                              TPF_BUTTON,
                              //#if (GERMAN | FRENCH)
                              //		d_cancel_x, d_cancel_y);
                              //#else
                              d_cancel_x,
                              d_cancel_y,
                              d_cancel_w,
                              d_cancel_h);
    //#endif

    //------------------------------------------------------------------------
    //	Build the button list
    //------------------------------------------------------------------------
    commands = &playerlist;
    cancelbtn.Add_Tail(*commands);
    playerlist.Set_Tabs(tabs);

    //------------------------------------------------------------------------
    //	Init other scenario parameters
    //------------------------------------------------------------------------
    Rule.IsTGrowth = Special.IsTGrowth;
    Special.IsTSpread = Session.Options.Tiberium;
    Rule.IsTSpread = Special.IsTSpread;
    transmit = 0;

    //------------------------------------------------------------------------
    //	Init player color-used flags
    //------------------------------------------------------------------------
    for (i = 0; i < MAX_MPLAYER_COLORS; i++) {
        color_used[i] = 0; // init all colors to available
    }
    color_used[Session.ColorIdx] = 1; // set my color to used
    playerlist.Set_Selected_Style(ColorListClass::SELECT_BAR, scheme);

    //------------------------------------------------------------------------
    //	Init random-number generator, & create a seed to be used for all random
    //	numbers from here on out
    //------------------------------------------------------------------------
    //	randomize();
    Seed = rand();

    //------------------------------------------------------------------------
    //	Init the version-clipping system
    //------------------------------------------------------------------------
    VerNum.Init_Clipping();

    //------------------------------------------------------------------------
    //	Clear the list of players
    //------------------------------------------------------------------------
    Clear_Vector(&Session.Players);
    //------------------------------------------------------------------------
    //	Add myself to the list, and to the Players vector.
    //------------------------------------------------------------------------
    item = new char[MPLAYER_NAME_MAX + 64];
#ifdef OLDWAY
    if (Session.House == HOUSE_GOOD) {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_ALLIES));
    } else {
        sprintf(item, "%s\t%s", Session.Handle, Text_String(TXT_SOVIET));
    }
#else  // OLDWAY
    sprintf(item, "%s\t%s", Session.Handle, Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
    playerlist.Add_Item(item,
                        (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                 : &ColorRemaps[Session.ColorIdx]);

    /*
    ** Process the message loop until we are in focus.
    */
    CDTimerClass<SystemTimerClass> focus_timer; // Timer to allow a wait after client joins
    focus_timer = 5 * 60;

    WWDebugString("RA95 - About to enter wait for focus loop.\n");
    SetForegroundWindow(MainWindow);
    ShowWindow(MainWindow, SW_SHOWMAXIMIZED);

    if (!GameInFocus) {
        do {
            Keyboard->Check();
            if (!focus_timer) {
                WWDebugString("RA95 - Calling SetForgroundWindow.\n");
                SetForegroundWindow(MainWindow);
                WWDebugString("RA95 - Calling ShowWindow.\n");
                ShowWindow(MainWindow, SW_SHOWMAXIMIZED);
                focus_timer = 5 * 60;
            }
        } while (!GameInFocus);
        AllSurfaces.SurfacesRestored = false;
    }

    who = new NodeNameType;
    strcpy(who->Name, Session.Handle);
    who->Player.House = Session.House;
    who->Player.Color = Session.ColorIdx;
    Session.Players.Add(who);

    if (LogicPage != &SeenBuff && LogicPage != &HidPage) {
        Set_Logic_Page(SeenBuff);
    }

    Load_Title_Page(true);
    CCPalette.Set(); // GamePalette.Set();

    //------------------------------------------------------------------------
    //	Processing loop
    //------------------------------------------------------------------------
    while (process) {
#if (SHOW_MONO)
        Ipx.Mono_Debug_Print(-1, 0);
#endif

#ifdef WIN32
        /*
        ** If we have just received input focus again after running in the background then
        ** we need to redraw.
        */
        if (AllSurfaces.SurfacesRestored) {
            AllSurfaces.SurfacesRestored = false;
            display = REDRAW_ALL;
        }
#endif
        //.....................................................................
        //	Refresh display if needed
        //.....................................................................
        if (display) {
            Hide_Mouse();

            //..................................................................
            //	Redraw backgound & dialog box
            //..................................................................
            if (display >= REDRAW_BACKGROUND) {
                Load_Title_Page(true);
                CCPalette.Set(); // GamePalette.Set();
                Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);

                //...............................................................
                //	Dialog & Field labels
                //...............................................................
                Fancy_Text_Print(
                    TXT_HACKHACK, d_dialog_cx - width / 2, d_dialog_y + 25 * RESFACTOR, scheme, TBLACK, TPF_TEXT);
            }

            //..................................................................
            //	Redraw buttons
            //..................................................................
            if (display >= REDRAW_BUTTONS) {
                commands->Draw_All();
            }

            Show_Mouse();
            display = REDRAW_NONE;
        }

        //.....................................................................
        //	Get user input
        //.....................................................................
        input = commands->Input();

        //.....................................................................
        //	Process input
        //.....................................................................
        switch (input) {

        case (KN_ESC):
        case (BUTTON_CANCEL | KN_BUTTON):
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_SIGN_OFF;
            strcpy(Session.GPacket.Name, Session.Handle);

            //...............................................................
            //	Broadcast my sign-off over my network
            //...............................................................
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;

            //...............................................................
            //	And now, just be absolutely sure, send my sign-off to each
            //	player in my game.  (If there's a bridge between us, the other
            //	player will have specified my address, so he can cross the
            //	bridge; but I may not have specified a bridge address, so the
            //	only way I have of crossing the bridge is to send a packet
            //	directly to him.)
            // Don't send this message to myself.
            //...............................................................
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                Ipx.Service();
            }
            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;
            Session.GameName[0] = 0;
            process = false;
            rc = false;
            // Send_Data_To_DDE_Server("Hello", strlen("Hello"), DDEServerClass::DDE_CONNECTION_FAILED);
            GameStatisticsPacketSent = false;
            // Spawn_WChat(false);
            break;

        //..................................................................
        //	Default: manage the inter-player messages
        //..................................................................
        default:
            if (Session.Players.Count() > 1) {

                /*
                ** Wait for several secs after receiving request to join before sending
                ** start game packet
                */
                if (!player_joined) {
                    player_joined = true;
                    join_timer.Set(3 * 60, true);
                    break;
                } else {
                    if (join_timer.Time())
                        break;
                }

                //...............................................................
                //	If a new player has joined in the last second, don't allow
                //	an OK; force a wait longer than 1 second (to give all players
                //	a chance to know about this new guy)
                //...............................................................
                i = MAX((int)Ipx.Global_Response_Time() * 2, 60 * 2);
                while ((int)TickCount - ok_timer < i) {
                    Ipx.Service();
                }

                //...............................................................
                //	If there are at least 2 players, go ahead & play; error otherwise
                //...............................................................
                if (Session.Solo || Session.Players.Count() > 1 || Session.Options.Ghosts) {
                    rc = true;
                    process = false;
                } else {
                    WWMessageBox().Process(TXT_ONLY_ONE, TXT_OOPS, NULL);
                    display = REDRAW_ALL;
                    Sound_Effect(VOC_SYS_ERROR);
                }
                // if (input==(BUTTON_LOAD | KN_BUTTON))
                //	load_game = 1;
                break;
            }
            break;
        }

        //.....................................................................
        //	Process incoming packets
        //.....................................................................
        whahoppa = Get_NewGame_Responses(&playerlist, color_used);
        if (whahoppa == EV_NEW_PLAYER) {
            ok_timer = TickCount;
            transmit = 1;
        } else if (whahoppa == EV_MESSAGE) {
            display = REDRAW_MESSAGE;
        }

        //.....................................................................
        //	If our Transmit flag is set, we need to send out a game option packet
        // Don't send it to myself.
        //.....................................................................
        if (transmit) {
            for (i = 1; i < Session.Players.Count(); i++) {
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

                Session.GPacket.Command = NET_GAME_OPTIONS;
                /*
                ** Set up the scenario info so the remote player can match the scenario on his machine
                ** or request a download if it doesnt exist
                */
                strcpy(Session.GPacket.ScenarioInfo.Scenario,
                       Session.Scenarios[Session.Options.ScenarioIndex]->Description());
                CCFileClass file(Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());

                //	ajw - I don't understand why this check is done here and not later.
                if (!file.Is_Available()) {

                    /*
                    ** Special new kludge for counterstrike.
                    **
                    ** The only time the file can be unavailable is if its a counterstrike
                    ** mission and the CS CD is not in the drive so
                    ** make sure the counterstrike CD is in the drive.
                    **
                    ** If Counterstrike is installed and
                    ** the file name matches a counterstrike map then force the CD
                    ** to be there.
                    **
                    */
#ifdef FIXIT_CSII //	checked - ajw 9/28/98
                    if (Expansion_CS_Present() || Expansion_AM_Present()) {
                        if (toupper(Session.ScenarioFileName[2]) == 'M') {

                            int current_drive = CCFileClass::Get_CD_Drive();
                            int index = -1; /* Get_CD_Index(current_drive, 1 * 60); */
                            bool needcd = false;
                            if (Is_Mission_Counterstrike(Session.ScenarioFileName) && index != 2 && index != 3) {
                                needcd = true;
                                RequiredCD = 2;
                            }
                            if (Is_Mission_Aftermath(Session.ScenarioFileName) && index != 3) {
                                needcd = true;
                                RequiredCD = 3;
                            }
                            if (needcd) {
#else
                    if (Expansion_CS_Present()) {
                        if (toupper(Session.ScenarioFileName[2]) == 'M') {
                            int scen_num;
                            sscanf(Session.ScenarioFileName, "SCM%02d", &scen_num);

                            int current_drive = CCFileClass::Get_CD_Drive();
                            if (scen_num > 24 && Get_CD_Index(current_drive, 1 * 60) != 2) {
                                RequiredCD = 2;
#endif
                                if (!Force_CD_Available(RequiredCD)) {
                                    Emergency_Exit(EXIT_FAILURE);
                                }

                                /*
                                ** Update the internal list of scenarios to include the counterstrike
                                ** list.
                                */
                                Session.Read_Scenario_Descriptions();
                            }
                        }
                    }

                    /*
                    ** See if that file is available now. Its fatal if it isnt.
                    */
                    if (!file.Is_Available()) {

                        WWMessageBox().Process(TXT_UNABLE_PLAY_WAAUGH);

                        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

                        Session.GPacket.Command = NET_SIGN_OFF;
                        strcpy(Session.GPacket.Name, Session.Handle);

                        //..................................................................
                        // Don't send myself the message.
                        //..................................................................
                        for (i = 1; i < Session.Players.Count(); i++) {
                            Ipx.Send_Global_Message(
                                &Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                            Ipx.Service();
                        }

                        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
                        Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

                        while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                            ;

                        Clear_Listbox(&playerlist);

                        //------------------------------------------------------------------------
                        //	Restore screen
                        //------------------------------------------------------------------------
                        Hide_Mouse();
                        Load_Title_Page(true);
                        Show_Mouse();

                        //------------------------------------------------------------------------
                        // Clear the Players vector if we're not starting a game.
                        //------------------------------------------------------------------------
                        Clear_Vector(&Session.Players);
                        return (false);
                    }
                }

                Session.GPacket.ScenarioInfo.FileLength = file.Size();
#ifdef WOLAPI_INTEGRATION
                strcpy(Session.GPacket.ScenarioInfo.ShortFileName,
                       Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());
#else
                strncpy(Session.GPacket.ScenarioInfo.ShortFileName,
                        Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename(),
                        sizeof(Session.GPacket.ScenarioInfo.ShortFileName));
#endif
                strncpy((char*)Session.GPacket.ScenarioInfo.FileDigest,
                        Session.Scenarios[Session.Options.ScenarioIndex]->Get_Digest(),
                        sizeof(Session.GPacket.ScenarioInfo.FileDigest));
                Session.GPacket.ScenarioInfo.OfficialScenario =
                    Session.Scenarios[Session.Options.ScenarioIndex]->Get_Official();

                Session.GPacket.ScenarioInfo.Credits = Session.Options.Credits;
                Session.GPacket.ScenarioInfo.IsBases = Session.Options.Bases;
                Session.GPacket.ScenarioInfo.IsTiberium = Session.Options.Tiberium;
                Session.GPacket.ScenarioInfo.IsGoodies = Session.Options.Goodies;
                // Session.GPacket.ScenarioInfo.IsGhosties = Session.Options.Ghosts;
                Session.GPacket.ScenarioInfo.BuildLevel = BuildLevel;
                Session.GPacket.ScenarioInfo.UnitCount = Session.Options.UnitCount;
                Session.GPacket.ScenarioInfo.AIPlayers = Session.Options.AIPlayers;
                Session.GPacket.ScenarioInfo.Seed = Seed;
                Session.GPacket.ScenarioInfo.Special = Special;
                Session.GPacket.ScenarioInfo.GameSpeed = Options.GameSpeed;
                Session.GPacket.ScenarioInfo.Version = VerNum.Get_Clipped_Version();

                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
            }
            Sound_Effect(VOC_OPTIONS_CHANGED);
            transmit = 0;
        }

        //.....................................................................
        //	Ping every player in my game, to force the Global Channel to measure
        //	the connection response time.  Don't ping myself (index 0).
        //.....................................................................
        if (TickCount - ping_timer > 15) {
            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
            Session.GPacket.Command = NET_PING;
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
            }
            ping_timer = TickCount;
        }

        //.....................................................................
        //	Service the Ipx connections
        //.....................................................................
        Ipx.Service();

        //.....................................................................
        //	Service the sounds & score; GameActive must be false at this point,
        //	so Call_Back() doesn't intercept global messages from me!
        //.....................................................................
        Call_Back();
        Frame_Limiter();
    }

    //------------------------------------------------------------------------
    //	Prepare to load the scenario
    //------------------------------------------------------------------------
    if (rc) {
        //.....................................................................
        //	Set the player count & scenario number
        //.....................................................................
        Session.NumPlayers = Session.Players.Count();

        Scen.Scenario = Session.Options.ScenarioIndex;
        strcpy(Scen.ScenarioName, Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());

        //.....................................................................
        //	Compute frame delay value for packet transmissions:
        //	- Divide global channel's response time by 8 (2 to convert to 1-way
        //	  value, 4 more to convert from ticks to frames)
        //.....................................................................
        if (Session.CommProtocol == COMM_PROTOCOL_MULTI_E_COMP) {
            Session.MaxAhead =
                MAX(((((Ipx.Global_Response_Time() / 8) + (Session.FrameSendRate - 1)) / Session.FrameSendRate)
                     * Session.FrameSendRate),
                    (Session.FrameSendRate * 2));
        } else {
            Session.MaxAhead = MAX(((int)Ipx.Global_Response_Time() / 8), NETWORK_MIN_MAX_AHEAD);
        }

        //.....................................................................
        //	Send all players the NET_GO packet.  Wait until all ACK's have been
        //	received.
        //.....................................................................
        memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
        if (load_game)
            Session.GPacket.Command = NET_LOADGAME;
        else
            Session.GPacket.Command = NET_GO;
        Session.GPacket.ResponseTime.OneWay = Session.MaxAhead;
        for (i = 1; i < Session.Players.Count(); i++) {
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
        }
        //.....................................................................
        //	Wait for all the ACK's to come in.
        //.....................................................................
        while (Ipx.Global_Num_Send() > 0) {
            Ipx.Service();
        }

        /*
        ** Wait for the go responses from each player in case someone needs the scenario
        ** file to be sent.
        */
        int responses[20]; // In big trub if more than 20 players
        memset(responses, 0, sizeof(responses));
        int num_responses = 0;
        bool send_scenario = false;
        WWDebugString("RA95 - About to wait for 'GO' response.\n");

        CDTimerClass<SystemTimerClass> response_timer; // timeout timer for waiting for responses
        response_timer = 60 * 30; // Wait for 30 seconds. If we dont hear by then assume someone crashed

        do {
            Ipx.Service();
            int retcode =
                Ipx.Get_Global_Message(&Session.GPacket, &Session.GPacketlen, &Session.GAddress, &Session.GProductID);
            if (retcode && Session.GProductID == IPXGlobalConnClass::COMMAND_AND_CONQUER0) {

                for (i = 1; i < Session.Players.Count(); i++) {
                    if (Session.Players[i]->Address == Session.GAddress) {
                        if (!responses[i]) {
                            if (Session.GPacket.Command == NET_REQ_SCENARIO) {
                                responses[i] = Session.GPacket.Command;
                                send_scenario = true;
                                num_responses++;
                            }
                            if (Session.GPacket.Command == NET_READY_TO_GO) {
                                responses[i] = Session.GPacket.Command;
                                num_responses++;
                            }
                        }
                    }
                }
            }
        } while (num_responses < Session.Players.Count() - 1 && response_timer);

        WWDebugString("RA95 - Exited response wait loop.\n");

        /*
        ** If one of the machines requested that the scenario be sent then send it.
        */
        if (send_scenario) {
            memset(Session.ScenarioRequests, 0, sizeof(Session.ScenarioRequests));
            Session.RequestCount = 0;
            for (i = 1; i < Session.Players.Count(); i++) {
                if (responses[i] == NET_REQ_SCENARIO) {
                    Session.ScenarioRequests[Session.RequestCount++] = i;
                }
            }
            Send_Remote_File(Scen.ScenarioName, 1);
        }
    }

    //.....................................................................
    //	Wait a while, polling the IPX service routines, to give our ACK
    //	a chance to get to the other system.  If he doesn't get our ACK,
    // he'll be waiting the whole time we load MIX files.
    //.....................................................................
    i = MAX((int)Ipx.Global_Response_Time() * 2, 60 * 2);
    int starttime = TickCount;
    while ((int)TickCount - starttime < i) {
        Ipx.Service();
    }

    //------------------------------------------------------------------------
    //	Init network timing values, using previous response times as a measure
    //	of what our retry delta & timeout should be.
    //------------------------------------------------------------------------
    // Ipx.Set_Timing (Ipx.Global_Response_Time() + 2, -1,
    //	Ipx.Global_Response_Time() * 4);
    Ipx.Set_Timing(Ipx.Global_Response_Time() + 2, (unsigned int)-1, max(120, Ipx.Global_Response_Time() * 8));

    Clear_Listbox(&playerlist);

    //------------------------------------------------------------------------
    //	Restore screen
    //------------------------------------------------------------------------
    Hide_Mouse();
    Load_Title_Page(true);
    Show_Mouse();

    if (load_game) {
        if (!Load_Game(-1)) {
            WWMessageBox().Process(TXT_ERROR_LOADING_GAME);
            rc = false;
        }
        Frame++;
    }

    //------------------------------------------------------------------------
    // Clear the Players vector if we're not starting a game.
    //------------------------------------------------------------------------
    if (!rc) {
        Clear_Vector(&Session.Players);
    }

    //	while (optionlist.Count()>0) {
    //		item = (char *)optionlist.Get_Item(0);
    //		delete [] item;
    //		optionlist.Remove_Item(item);
    //	}

    return (rc);

#else // WIN32

    return (0);

#endif // WIN32
} /* end of Net_Fake_New_Dialog */

/***********************************************************************************************
 * Net_Join_Dialog -- lets user join an existing game or start a new one                       *
 *                                                                                             *
 * This dialog displays an edit field for the player's name, and a list of all non-stealth-    *
 * mode games.  Clicking once on a game name displays a list of who's in that game.  Clicking  *
 * "New" takes the user to the Net_New dialog, where he waits for other users to join his      *
 * game.  All other input is done through this dialog.                                         *
 *                                                                                             *
 * The dialog has several "states":                                                            *
 *                                                                                             *
 *   1) Initially, it waits for the user to fill in his/her name and then to select Join or    *
 *      New; if New is selected, this dialog is exited.                                        *
 *                                                                                             *
 *   2) If Join is selected, the Join & New buttons are removed, but the Cancel button remains.*
 *      The join request is transmitted to the game's owner, and the message "Waiting for      *
 *      Confirmation" is displayed, until a confirmation or denial is received from the game's *
 *      owner.  The user may click Cancel at this point to cancel the join request.            *
 *      (Once Join is selected, the name editing field is disabled, and becomes a display-only *
 *      field.  If cancel is selected, it reappears as an edit field.) The user can still click*
 *      around & see who's in which games.                                                     *
 *                                                                                             *
 *   3) If the join request is denied, the dialog re-initializes to its pre-join state; the    *
 *      Join & New buttons reappear, & the Name field is available again.                      *
 *                                                                                             *
 *   4) If join confirmation is obtained, the message changes to show all the current game	  *
 *      settings.  The user cannot click around & look at other games any more.					  *
 *                                                                                             *
 * Any game running in Stealth mode won't show up on this dialog.                              *
 *                                                                                             *
 * The 'Players' vector matches one-for-one the contents of the list box.  The local system	  *
 * is always listed first; the IPX Address of the local system will NOT be valid in the		  *
 * Players vector.  The Games & Players vectors should be left filled in even after this		  *
 * routine is exited; their contents are used to form connections to the other players,		  *
 * after the scenario has been loaded.																			  *
 *                                                                                             *
 * The 'Chat' vector contains the address of everyone who sends me a chat announcement.		  *
 * The address field is used to send everyone my outgoing messages.  The LastTime				  *
 * field is used as a timeout; if enough time goes by & we don't hear from this node,			  *
 * we ping him, requesting a CHAT_ANNOUNCE if he's still in chat.  If we don't hear				  *
 * from him after that, we remove him from our list.														  *
 *                                                                                             *
 *    旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�                                    *
 *    �                 Network Games                     �                                    *
 *    �                                                   �                                    *
 *    �              Your Name: ____________              �                                    *
 *    �                  House: [GDI] [NOD]               �                                    *
 *    �          Desired Color: [ ][ ][ ][ ]              �                                    *
 *    �                                                   �                                    *
 *    �            Games                 Players          �                                    *
 *    � 旼컴컴컴컴컴컴컴컴컴컴컫커 旼컴컴컴컴컴컴컴컴컫커 �                                    *
 *    � �(Bill's Game         )�.� � Peter Parker GDI �.� �                                    *
 *    � � Peter Parker's Game  쳐� � Mary Jane    GDI 쳐� �                                    *
 *    � �(Magnum PI's Game    )� � � JJ Jameson   NOD � � �                                    *
 *    � �                      쳐� �                  쳐� �                                    *
 *    � �                      �.� �                  �.� �                                    *
 *    � 읕컴컴컴컴컴컴컴컴컴컴컨켸 읕컴컴컴컴컴컴컴컴컨켸 �                                    *
 *    �           Scenario: Big Long Description          �                                    *
 *    �                Starting Credits: xxxx             �                                    *
 *    �          Count: ---          Level: ---           �                                    *
 *    �          Bases: ON          Crates: ON            �                                    *
 *    �       Tiberium: ON      AI Players: ON            �                                    *
 *    �                                                   �                                    *
 *    �  旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�  �                                    *
 *    �  �                                             �  �                                    *
 *    �  �                                             �  �                                    *
 *    �  읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�  �                                    *
 *    �  旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�  �                                    *
 *    �  읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�  �                                    *
 *    �            [Join]  [Cancel]    [New]              �                                    *
 *    읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�                                    *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      -1 = cancel, 0 = OK, 1 = New net game requested                                        *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
static int Net_Fake_Join_Dialog(void)
{

#ifdef WIN32
    //------------------------------------------------------------------------
    //	Dialog & button dimensions
    //------------------------------------------------------------------------
    int d_dialog_w = 120 * RESFACTOR;                      // dialog width
    int d_dialog_h = 80 * RESFACTOR;                       // dialog height
    int d_dialog_x = ((320 * RESFACTOR - d_dialog_w) / 2); // dialog x-coord
    int d_dialog_y = ((200 * RESFACTOR - d_dialog_h) / 2); // centered y-coord
    int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);       // center x-coord

    int d_txt6_h = 6 * RESFACTOR + 1; // ht of 6-pt text
    int d_margin1 = 5 * RESFACTOR;    // large margin
    int d_margin2 = 7 * RESFACTOR;    // small margin

    int d_gamelist_w = 160 * RESFACTOR;
    int d_gamelist_h = ((6 * 6) + 3) * RESFACTOR; // 6 rows high
                                                  //		int d_gamelist_x = d_dialog_x + d_margin1 + d_margin1;
    int d_gamelist_x = 500 * RESFACTOR;           // d_dialog_x + d_margin1 + d_margin1;
    int d_gamelist_y = 50 + d_margin2 + 2 * RESFACTOR /*KO + d_txt6_h + d_margin2*/;

    int d_playerlist_w = 118 * RESFACTOR;
    int d_playerlist_h = ((6 * 6) + 3) * RESFACTOR; // 6 rows high
    // int d_playerlist_x = d_dialog_x + d_dialog_w - d_margin1 - d_margin1 - d_playerlist_w;
    int d_playerlist_x = 500 * RESFACTOR;
    int d_playerlist_y = 50 + d_margin2 + 2 * RESFACTOR /*KO + d_txt6_h + d_margin2*/;

#if (GERMAN | FRENCH)
    int d_cancel_w = 50 * RESFACTOR;
#else
    int d_cancel_w = 40 * RESFACTOR;
#endif
    int d_cancel_h = 9 * RESFACTOR;
    int d_cancel_x = d_dialog_cx - d_cancel_w / 2;
    int d_cancel_y = d_dialog_y + d_dialog_h - 20 * RESFACTOR;

    //------------------------------------------------------------------------
    //	Button Enumerations
    //------------------------------------------------------------------------
    enum
    {
        BUTTON_GAMELIST = 100,
        BUTTON_PLAYERLIST,
        BUTTON_CANCEL,
    };

    //------------------------------------------------------------------------
    //	Redraw values: in order from "top" to "bottom" layer of the dialog
    //------------------------------------------------------------------------
    typedef enum
    {
        REDRAW_NONE = 0,
        REDRAW_PARMS,
        REDRAW_MESSAGE,
        REDRAW_COLORS,
        REDRAW_BUTTONS,
        REDRAW_BACKGROUND,
        REDRAW_ALL = REDRAW_BACKGROUND
    } RedrawType;

    //------------------------------------------------------------------------
    //	Dialog variables
    //------------------------------------------------------------------------
    RedrawType display = REDRAW_ALL; // redraw level
    bool process = true;             // process while true
    KeyNumType input;
    char housetext[25] = ""; // buffer for house droplist
    int isdropped = 0;

    JoinStateType joinstate = JOIN_NOTHING;        // current "state" of this dialog
    char namebuf[MPLAYER_NAME_MAX] = {0};          // buffer for player's name
    int playertabs[] = {77 * RESFACTOR};           // tabs for player list box
    int optiontabs[] = {8};                        // tabs for player list box
    int game_index = -1;                           // index of currently-selected game
    int join_index = -1;                           // index of game we're joining
    int rc = 0;                                    // -1 = user cancelled, 1 = New
    JoinEventType event;                           // event from incoming packet
    int i;                                         // loop counter
    int parms_received = 0;                        // 1 = game options received
    NodeNameType* who;                             // node to add to Players
    RejectType why;                                // reason for rejection
    TTimerClass<SystemTimerClass> lastclick_timer; // time b/w send periods
    int lastclick_idx = 0;                         // index of item last clicked on
    RemapControlType* scheme = GadgetClass::Get_Color_Scheme();
    int ready_to_go = 0;
    Session.Options.ScenarioDescription[0] = 0; // Flag that we dont know the scenario name yet
    int width;
    int height;

    // PG_TO_FIX
    // Fancy_Text_Print(TXT_NONE,0,0,TBLACK,TBLACK,TPF_6PT_GRAD | TPF_NOSHADOW);
    Format_Window_String((char*)TXT_HACKHACK, SeenBuff.Get_Height(), width, height);
    char* item;
    unsigned int starttime;
    int load_game = 0; // 1 = load saved game

    //------------------------------------------------------------------------
    //	Buttons
    //------------------------------------------------------------------------
    GadgetClass* commands; // button list

    ListClass gamelist(BUTTON_GAMELIST,
                       d_gamelist_x,
                       d_gamelist_y,
                       d_gamelist_w,
                       d_gamelist_h,
                       TPF_TEXT,
                       MFCD::Retrieve("BTN-UP.SHP"),
                       MFCD::Retrieve("BTN-DN.SHP"));

    ColorListClass playerlist(BUTTON_PLAYERLIST,
                              d_playerlist_x,
                              d_playerlist_y,
                              d_playerlist_w,
                              d_playerlist_h,
                              TPF_TEXT,
                              MFCD::Retrieve("BTN-UP.SHP"),
                              MFCD::Retrieve("BTN-DN.SHP"));

    TextButtonClass cancelbtn(BUTTON_CANCEL,
                              TXT_CANCEL,
                              TPF_BUTTON,
                              //#if (GERMAN | FRENCH)
                              //		d_cancel_x, d_cancel_y);
                              //#else
                              d_cancel_x,
                              d_cancel_y,
                              d_cancel_w,
                              d_cancel_h);
    //#endif

    //------------------------------------------------------------------------
    //	Init the button states
    //------------------------------------------------------------------------
    //........................................................................
    // Name & Color
    //........................................................................
    Session.ColorIdx = Session.PrefColor; // init my preferred color
    strcpy(namebuf, Session.Handle);      // set my name

    //........................................................................
    // List boxes
    //........................................................................
    playerlist.Set_Tabs(playertabs);
    playerlist.Set_Selected_Style(ColorListClass::SELECT_NORMAL);

    Fancy_Text_Print("", 0, 0, scheme, TBLACK, TPF_CENTER | TPF_TEXT);

    lastclick_timer = 0;

    //------------------------------------------------------------------------
    //	Clear the list of games, players, and the chat list
    //------------------------------------------------------------------------
    Clear_Vector(&Session.Games);
    Clear_Vector(&Session.Players);
    Clear_Vector(&Session.Chat);

    //------------------------------------------------------------------------
    // Add myself to the Chat vector
    //------------------------------------------------------------------------
    who = new NodeNameType;
    strcpy(who->Name, namebuf);
    who->Chat.LastTime = 0;
    who->Chat.LastChance = 0;
    who->Chat.Color = Session.GPacket.Chat.Color;
    Session.Chat.Add(who);

    //------------------------------------------------------------------------
    // Create the "Lobby" game name on the games list, and create a bogus
    // node for the gamelist, so Games[i] will always match gamelist[i]
    //------------------------------------------------------------------------
    who = new NodeNameType;
    strcpy(who->Name, "");
    who->Game.IsOpen = 0;
    who->Game.LastTime = 0;
    Session.Games.Add(who);
    item = new char[MPLAYER_NAME_MAX];
    strcpy(item, Text_String(TXT_LOBBY));
    gamelist.Add_Item(item);
    gamelist.Set_Selected_Index(0);
    game_index = 0;

    //------------------------------------------------------------------------
    //	Send game-name query & chat announcement; also, initialize timers.
    //------------------------------------------------------------------------
    Send_Join_Queries(game_index, joinstate, 1, 0, 1, namebuf, 1);

    /*
    ** Process the message loop until we are in focus.
    */
    CDTimerClass<SystemTimerClass> focus_timer; // Timer to allow a wait after client joins
    focus_timer = 5 * 60;

    WWDebugString("RA95 - About to enter wait for focus loop.\n");
    SetForegroundWindow(MainWindow);
    ShowWindow(MainWindow, SW_SHOWMAXIMIZED);

    if (!GameInFocus) {
        do {
            Keyboard->Check();
            if (!focus_timer) {
                WWDebugString("RA95 - Calling SetForgroundWindow.\n");
                SetForegroundWindow(MainWindow);
                WWDebugString("RA95 - Calling ShowWindow.\n");
                ShowWindow(MainWindow, SW_SHOWMAXIMIZED);
                focus_timer = 5 * 60;
            }
        } while (!GameInFocus);
        AllSurfaces.SurfacesRestored = false;
    }

    if (LogicPage != &SeenBuff && LogicPage != &HidPage) {
        Set_Logic_Page(SeenBuff);
    }

    Load_Title_Page(true);
    CCPalette.Set(); // GamePalette.Set();

    //------------------------------------------------------------------------
    //	Processing loop
    //------------------------------------------------------------------------
    while (process) {

#ifdef WIN32
        /*
        ** If we have just received input focus again after running in the background then
        ** we need to redraw.
        */
        if (AllSurfaces.SurfacesRestored) {
            AllSurfaces.SurfacesRestored = false;
            display = REDRAW_ALL;
        }
#endif
        //.....................................................................
        //	Refresh display if needed
        //.....................................................................
        if (display) {
            Hide_Mouse();

            //..................................................................
            //	Redraw backgound & dialog box
            //..................................................................
            if (display >= REDRAW_BACKGROUND) {
                Load_Title_Page(true);
                CCPalette.Set(); // GamePalette.Set();
                Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);

                //...............................................................
                //	Dialog & Field labels
                //...............................................................
                Fancy_Text_Print(
                    TXT_HACKHACK, d_dialog_cx - width / 2, d_dialog_y + 25 * RESFACTOR, scheme, TBLACK, TPF_TEXT);

                //...............................................................
                //	Rebuild the button list
                //...............................................................
                cancelbtn.Zap();
                gamelist.Zap();
                playerlist.Zap();

                commands = &cancelbtn;
                gamelist.Add_Tail(*commands);
                playerlist.Add_Tail(*commands);
            }

            //..................................................................
            //	Redraw buttons
            //..................................................................
            if (display >= REDRAW_BUTTONS) {
                commands->Draw_All();
            }

            Show_Mouse();
            display = REDRAW_NONE;
        }

        //.....................................................................
        //	Get user input
        //.....................................................................
        input = commands->Input();

        //.....................................................................
        //	Process input
        //.....................................................................
        switch (input) {

        //..................................................................
        // ESC / CANCEL: send a SIGN_OFF
        // - If we're part of a game, stay in this dialog; otherwise, exit
        //..................................................................
        case (KN_ESC):
        case (BUTTON_CANCEL | KN_BUTTON):
            //...............................................................
            //	If we're joined to a game, make extra sure the other players in
            //	that game know I'm exiting; send my SIGN_OFF as an ack-required
            //	packet.  Don't send this to myself (index 0).
            //...............................................................
            if (joinstate == JOIN_CONFIRMED) {
                Unjoin_Game(namebuf,
                            joinstate,
                            &gamelist,
                            &playerlist,
                            game_index,
                            1,
                            0,
                            0,
                            d_txt6_h,
                            0,
                            0,
                            MAX_MESSAGE_LENGTH);
                joinstate = JOIN_NOTHING;
                display = REDRAW_ALL;
            }
            //...............................................................
            // If I'm not joined to a game, send a SIGN_OFF to all players
            // in my Chat vector (but not to myself, index 0)
            //...............................................................
            else {
                memset(&Session.GPacket, 0, sizeof(GlobalPacketType));
                Session.GPacket.Command = NET_SIGN_OFF;
                strcpy(Session.GPacket.Name, namebuf);
                for (i = 1; i < Session.Chat.Count(); i++) {
                    Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Chat[i]->Address));
                    Ipx.Service();
                }

                //............................................................
                //	Now broadcast a SIGN_OFF just to be thorough
                //............................................................
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

                while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                    ;

                //............................................................
                // exit the dialog
                //............................................................
                // Send_Data_To_DDE_Server("Hello", strlen("Hello"), DDEServerClass::DDE_CONNECTION_FAILED);
                GameStatisticsPacketSent = false;
                // Spawn_WChat(false);
                process = false;
                rc = -1;
            }
            break;

        //..................................................................
        //	JOIN: send a join request packet & switch to waiting-for-
        // confirmation mode.
        //..................................................................
        default:
            if (joinstate == JOIN_NOTHING && gamelist.Count() > 1) {
                gamelist.Set_Selected_Index(1); // lastclick_idx);
                game_index = 1;
                strcpy(Session.Handle, namebuf);
                // Session.House = (HousesType)housebtn.Current_Index();
                join_index = gamelist.Current_Index();
                parms_received = 0;
                if (Request_To_Join(namebuf, join_index, Session.House, Session.ColorIdx)) {
                    joinstate = JOIN_WAIT_CONFIRM;
                } else {
                    display = REDRAW_ALL;
                }
            }
            break;
        }

        //.....................................................................
        //	Resend our query packets
        //.....................................................................
        Send_Join_Queries(game_index, joinstate, 0, 0, 0, namebuf);

        //.....................................................................
        //	Process incoming packets
        //.....................................................................
        event = Get_Join_Responses(&joinstate, &gamelist, &playerlist, join_index, namebuf, &why);

        //.....................................................................
        //	If we've changed state, redraw everything; if we're starting the game,
        //	break out of the loop.  If we've just joined, send out a player query
        //	so I'll get added to the list instantly.
        //.....................................................................
        if (event == EV_STATE_CHANGE) {
            display = REDRAW_ALL;
            if (joinstate == JOIN_GAME_START || joinstate == JOIN_GAME_START_LOAD) {
                if (joinstate == JOIN_GAME_START_LOAD)
                    load_game = 1;

                bool ready_packet_was_sent = false;

                if (!load_game) {
                    /*
                    ** Special new kludge for counterstrike.
                    **
                    ** Find local scenario will fail to match a counterstrike mission
                    ** unless the CS CD is in the drive. So....
                    **
                    ** If Counterstrike is installed and this is an official map and
                    ** the file name matches a counterstrike map then tell the host
                    ** that I have the scenario so he can continue while we make
                    ** sure the local user has the Counterstrike CD in the drive.
                    **
                    */
                    //	This is duplicated for Aftermath scenarios. ajw

#ifdef FIXIT_CSII //	checked - ajw 9/28/98
                    if (Session.ScenarioIsOfficial
                        && ((Expansion_CS_Present() && Is_Mission_Counterstrike(Session.ScenarioFileName))
                            || (Expansion_AM_Present() && Is_Mission_Aftermath(Session.ScenarioFileName)))) {
#else
                    if (Expansion_CS_Present() && Session.ScenarioIsOfficial) {
#endif

                        CCFileClass check_file(Session.ScenarioFileName);
                        if (!check_file.Is_Available()) {

                            int current_drive = CCFileClass::Get_CD_Drive();
#ifdef FIXIT_CSII                           //	checked - ajw 9/28/98
                            int index = -1; /* Get_CD_Index(current_drive, 1 * 60); */
                            bool needcd = false;
                            if (Is_Mission_Counterstrike(Session.ScenarioFileName)) {
                                if (index != 2 && index != 3) {
                                    RequiredCD = 2;
                                    needcd = true;
                                }
                            }
                            if (Is_Mission_Aftermath(Session.ScenarioFileName)) {
                                if (index != 3) {
                                    RequiredCD = 3;
                                    needcd = true;
                                }
                            }
                            if (needcd) {
#else
                            if (Get_CD_Index(current_drive, 1 * 60) != 2) {
                                RequiredCD = 2;
#endif

                                /*
                                ** We should have the scenario but the wrong disk is in.
                                ** Tell the host that I am ready to go anyway.
                                */
                                memset((void*)&(Session.GPacket), 0, sizeof(Session.GPacket));
                                Session.GPacket.Command = NET_READY_TO_GO;
                                Ipx.Send_Global_Message(
                                    &Session.GPacket, sizeof(GlobalPacketType), 1, &Session.HostAddress);
                                while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                                    ;
                                ready_packet_was_sent = true;

                                if (!Force_CD_Available(RequiredCD)) {
                                    Emergency_Exit(EXIT_FAILURE);
                                }

                                CCFileClass testfile(Session.ScenarioFileName);

                                /*
                                ** Update the internal list of scenarios to include the counterstrike
                                ** list.
                                */
                                Session.Read_Scenario_Descriptions();

                                /*
                                ** See if that scenario is available now. Its pretty fatal if it isnt.
                                */
                                Session.Options.ScenarioIndex = -1;
                                for (i = 0; i < Session.Scenarios.Count(); i++) {
                                    if (!strcmp(Session.Scenarios[i]->Description(),
                                                Session.Options.ScenarioDescription)) {
                                        Session.Options.ScenarioIndex = i;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    /*
                    ** If the scenario that the host wants to play doesnt exist locally then we
                    **	need to request that it is sent. If we can identify the scenario locally then
                    **	we need to fix up the file name so we load the right one.
                    */
                    if (Find_Local_Scenario(Session.Options.ScenarioDescription,
                                            Session.ScenarioFileName,
                                            Session.ScenarioFileLength,
                                            Session.ScenarioDigest,
                                            Session.ScenarioIsOfficial)) {

                        Session.Options.ScenarioIndex = 1; // We dont care what it
                                                           // is as long as it isnt -1

                        /*
                        ** We have the scenario. Tell the host that I am ready to go.
                        */
                        if (!ready_packet_was_sent) {
                            memset((void*)&(Session.GPacket), 0, sizeof(Session.GPacket));
                            Session.GPacket.Command = NET_READY_TO_GO;
                            Ipx.Send_Global_Message(
                                &Session.GPacket, sizeof(GlobalPacketType), 1, &Session.HostAddress);
                            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                                ;
                        }
                    } else {
#ifndef FIXIT_VERSION_3 //	Removed restriction on downloading official maps.
                        /*
                        ** Oh dear. Thats a scenario I dont have. Request that the host sends the
                        **	scenario to me provided its not an official Westwood scenario.
                        **
                        ** If the file is received OK then we will get a true return value and the
                        ** actual file name to load will be in Session.ScenarioFileName
                        */
                        if (Session.ScenarioIsOfficial) {
                            Session.Options.ScenarioIndex = -1;
                        } else {
#endif
                            Session.Options.ScenarioIndex = 1; // We dont care what it
                                                               // is as long as it isnt -1
#ifdef FIXIT_VERSION_3
                            if (bSpecialAftermathScenario(Session.Options.ScenarioDescription))
                                break;
#endif

                            if (!Get_Scenario_File_From_Host(Session.ScenarioFileName, 1)) {
                                break;
                            } else {
                                /*
                                ** Make sure we dont time-out because of the download
                                */
                            }
#ifndef FIXIT_VERSION_3
                        }
#endif
                    }
                } else {
                    /*
                    ** Make sure we respond to the host in a load game
                    */
                    memset((void*)&(Session.GPacket), 0, sizeof(Session.GPacket));
                    Session.GPacket.Command = NET_READY_TO_GO;
                    Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &Session.HostAddress);
                    while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                        ;
                }

                strcpy(Scen.ScenarioName, Session.ScenarioFileName);

                rc = 0;
                process = false;
            }

            //..................................................................
            //	If we're newly-confirmed, add myself to the Players list, and
            // immediately send out a player query
            //..................................................................
            else if (joinstate == JOIN_CONFIRMED) {
                //...............................................................
                //	Clear the player list, then add myself to the list.
                //...............................................................
                Clear_Listbox(&playerlist);
                Clear_Vector(&Session.Players);

                item = new char[MPLAYER_NAME_MAX + 12];
#ifdef OLDWAY
                if (Session.House == HOUSE_GOOD) {
                    sprintf(item, "%s\t%s", namebuf, Text_String(TXT_ALLIES));
                } else {
                    sprintf(item, "%s\t%s", namebuf, Text_String(TXT_SOVIET));
                }
#else  // OLDWAY
                sprintf(item, "%s\t%s", namebuf, Text_String(HouseTypeClass::As_Reference(Session.House).Full_Name()));
#endif // OLDWAY
                playerlist.Add_Item(item,
                                    (Session.ColorIdx == PCOLOR_DIALOG_BLUE) ? &ColorRemaps[PCOLOR_REALLY_BLUE]
                                                                             : &ColorRemaps[Session.ColorIdx]);

                who = new NodeNameType;
                strcpy(who->Name, namebuf);
                who->Player.House = Session.House;
                who->Player.Color = Session.ColorIdx;
                Session.Players.Add(who);

                Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
#if (0)
                //...............................................................
                // Re-init the message system to its new smaller size
                //...............................................................
                Session.Messages.Init(0, 0, 8, MAX_MESSAGE_LENGTH, d_txt6_h, 0, 0, 1, 20, MAX_MESSAGE_LENGTH - 5);
                Session.Messages.Add_Edit(Session.ColorIdx, TPF_TEXT | TPF_BRIGHT_COLOR, NULL, '_');
#endif //(0)
            }

            //..................................................................
            //	If we've been rejected, clear any messages we may have been
            // typing, add a message stating why we were rejected, and send a
            // chat announcement.
            //..................................................................
            else if (joinstate == JOIN_REJECTED) {
                Sound_Effect(VOC_SYS_ERROR);
                Send_Join_Queries(game_index, joinstate, 0, 0, 1, namebuf);
            }
        } // end of state change

        //.....................................................................
        //	If the game options have changed, print them.
        //.....................................................................
        else if (event == EV_GAME_OPTIONS) {

            Sound_Effect(VOC_OPTIONS_CHANGED);

            parms_received = 1;
            display = REDRAW_PARMS;
        }

        //.....................................................................
        // EV_GAME_SIGNOFF:
        //	A game before the one I've selected is gone, so we have a new index
        // now. 'game_index' must be kept set to the currently-selected list
        // item, so we send out queries for the currently-selected game.  It's
        // therefore imperative that we detect any changes to the game list.
        // If we're joined in a game, we must decrement our game_index to keep
        // it aligned with the game we're joined to.
        //.....................................................................
        else if (event == EV_GAME_SIGNOFF) {
            if (joinstate == JOIN_CONFIRMED) {
                game_index--;
                join_index--;
                gamelist.Set_Selected_Index(join_index);
            } else {
                gamelist.Flag_To_Redraw();
                Clear_Listbox(&playerlist);
                Clear_Vector(&Session.Players);
                game_index = gamelist.Current_Index();
                Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
            }
        }

        //.....................................................................
        //	Service the Ipx connections
        //.....................................................................
        Ipx.Service();

        if (process) {
            //.....................................................................
            //	Clean out the Game List; if an old entry is found:
            //	- Remove it
            //	- Clear the player list
            //	- Send queries for the new selected game, if there is one
            //.....................................................................
            for (i = 1; i < Session.Games.Count(); i++) {
                if (TickCount - Session.Games[i]->Game.LastTime > 400) {

                    delete Session.Games[i];
                    Session.Games.Delete(Session.Games[i]);

                    item = (char*)(gamelist.Get_Item(i));
                    gamelist.Remove_Item(item);
                    delete[] item;

                    if (i <= game_index) {
                        if (joinstate == JOIN_CONFIRMED) {
                            game_index--;
                            join_index--;
                            gamelist.Set_Selected_Index(join_index);
                        } else {
                            gamelist.Flag_To_Redraw();
                            Clear_Listbox(&playerlist);
                            Clear_Vector(&Session.Players);
                            game_index = gamelist.Current_Index();
                            Send_Join_Queries(game_index, joinstate, 0, 1, 0, namebuf);
                        }
                    }
                }
            }
        }

#if (0)
        /*
        ** If we were flagged to start the game and we recognise both players then quit the loop
        */
        if (ready_to_go && Session.Players.Count() == 2) {
            rc = 0;
            process = false;
        }
#endif //(0)

        //.....................................................................
        //	Service the sounds & score; GameActive must be false at this point,
        //	so Call_Back() doesn't intercept global messages from me!
        //.....................................................................
        Call_Back();
        Frame_Limiter();
    } // end of while

    //------------------------------------------------------------------------
    //	Establish connections with all other players.
    //------------------------------------------------------------------------
    if (rc == 0) {
        //.....................................................................
        //	If the other guys are playing a scenario I don't have (sniff), I can't
        //	play.  Try to bail gracefully.
        //.....................................................................
        if (Session.Options.ScenarioIndex == -1) {
            WWMessageBox().Process(TXT_UNABLE_PLAY_WAAUGH);

            memset(&Session.GPacket, 0, sizeof(GlobalPacketType));

            Session.GPacket.Command = NET_SIGN_OFF;
            strcpy(Session.GPacket.Name, namebuf);

            //..................................................................
            // Don't send myself the message.
            //..................................................................
            for (i = 1; i < Session.Players.Count(); i++) {
                Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 1, &(Session.Players[i]->Address));
                Ipx.Service();
            }

            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);
            Ipx.Send_Global_Message(&Session.GPacket, sizeof(GlobalPacketType), 0, NULL);

            while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)
                ;

            rc = -1;

        }
        //---------------------------------------------------------------------
        // Prepare to load the scenario.
        //---------------------------------------------------------------------
        else {
            //..................................................................
            //	Set the number of players in this game, and the scenario number.
            //..................................................................
            Session.NumPlayers = Session.Players.Count();
            ///*
            //**	Hack to fake a scenario name as if it had been sent over the connection.
            //*/
            // sprintf(Scen.ScenarioName, "SCM%02dEA.INI", Session.Options.ScenarioIndex+1);

            //			Scen.Scenario = Session.Options.ScenarioIndex;
        }

        //.....................................................................
        //	Wait a while, polling the IPX service routines, to give our ACK
        //	a chance to get to the other system.  If he doesn't get our ACK,
        // he'll be waiting the whole time we load MIX files.
        //.....................................................................
        i = MAX((int)Ipx.Global_Response_Time() * 2, 60 * 2);
        starttime = TickCount;
        while (TickCount - starttime < (unsigned)i) {
            Ipx.Service();
        }
    }

    //------------------------------------------------------------------------
    //	Init network timing values, using previous response times as a measure
    //	of what our retry delta & timeout should be.
    //------------------------------------------------------------------------
    // Ipx.Set_Timing (Ipx.Global_Response_Time() + 2, -1,
    //	Ipx.Global_Response_Time() * 4);
    Ipx.Set_Timing(Ipx.Global_Response_Time() + 2, (unsigned int)-1, max(120, Ipx.Global_Response_Time() * 8));

    //------------------------------------------------------------------------
    //	Clear all lists, but NOT the Games & Players vectors.
    //------------------------------------------------------------------------
    Clear_Listbox(&gamelist);
    Clear_Listbox(&playerlist);

    //------------------------------------------------------------------------
    //	Restore screen
    //------------------------------------------------------------------------
    Hide_Mouse();
    Load_Title_Page(true);
    Show_Mouse();
    //------------------------------------------------------------------------
    // Load a game if the game owner told us to
    //------------------------------------------------------------------------
    if (load_game) {
        if (!Load_Game(-1)) {
            WWMessageBox().Process(TXT_ERROR_LOADING_GAME);
            rc = -1;
        }
        Frame++;
    }

    //------------------------------------------------------------------------
    // Clear the Players & Games vectors if we're not joined to a game.
    // Clear the Chat vector regardless.
    //------------------------------------------------------------------------
    if (rc != 0) {
        Clear_Vector(&Session.Games);
        Clear_Vector(&Session.Players);
    }
    Clear_Vector(&Session.Chat);

    return (rc);

#else // WIN32

    return (0);

#endif // WIN32

} /* end of Net_Join_Dialog */

bool Server_Remote_Connect(void)
{
    int stealth; // original state of Session.NetStealth flag

    //------------------------------------------------------------------------
    //	Init network timing parameters; these values should work for both a
    // "real" network, and a simulated modem network (ie Kali)
    //------------------------------------------------------------------------
    Ipx.Set_Timing(30,   // retry 2 times per second
                   -1,   // ignore max retries
                   600); // give up after 10 seconds

    //------------------------------------------------------------------------
    //	Save the original value of the NetStealth flag, so we can turn stealth
    //	off for now (during this portion of the dialogs, we must show ourselves)
    //------------------------------------------------------------------------
    stealth = Session.NetStealth;
    Session.NetStealth = 0;

    //------------------------------------------------------------------------
    //	The game is now "open" for joining.  Close it as soon as we exit this
    //	routine.
    //------------------------------------------------------------------------
    Session.NetOpen = 1;

    //------------------------------------------------------------------------
    //	Read in last multiplayer game settings from the .INI file
    //------------------------------------------------------------------------
    if (!Net_Fake_New_Dialog()) {
        Session.Write_MultiPlayer_Settings();
        return (false);
    }

    Session.NetOpen = 0;
    Session.NetStealth = stealth;
    Session.Write_MultiPlayer_Settings();
    return (true);

} /* end of Server_Remote_Connect */

bool Client_Remote_Connect(void)
{
    int rc;
    int stealth; // original state of Session.NetStealth flag

    //------------------------------------------------------------------------
    //	Init network timing parameters; these values should work for both a
    // "real" network, and a simulated modem network (ie Kali)
    //------------------------------------------------------------------------
    Ipx.Set_Timing(30,   // retry 2 times per second
                   -1,   // ignore max retries
                   600); // give up after 10 seconds

    //------------------------------------------------------------------------
    //	Save the original value of the NetStealth flag, so we can turn stealth
    //	off for now (during this portion of the dialogs, we must show ourselves)
    //------------------------------------------------------------------------
    stealth = Session.NetStealth;
    Session.NetStealth = 0;

    //------------------------------------------------------------------------
    //	The game is now "open" for joining.  Close it as soon as we exit this
    //	routine.
    //------------------------------------------------------------------------
    Session.NetOpen = 1;

    //------------------------------------------------------------------------
    //	Read in last multiplayer game settings from the .INI file
    //------------------------------------------------------------------------
    rc = Net_Fake_Join_Dialog();
    Session.Write_MultiPlayer_Settings();

    Session.NetStealth = stealth;
    Session.NetOpen = 0;

    if (rc == -1) {
        return (false);
    } else {
        return (true);
    }

} /* end of Client_Remote_Connect */

/*************************** end of netdlg.cpp *****************************/

#endif //#ifndef WOLAPI_INTEGRATION
#endif
