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

/* $Header: /CounterStrike/KEY.CPP 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Westwood Keyboard Library                                    *
 *                                                                                             *
 *                    File Name : KEYBOARD.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Philip W. Gorrow                                             *
 *                                                                                             *
 *                   Start Date : 10/16/95                                                     *
 *                                                                                             *
 *                  Last Update : November 2, 1996 [JLB]                                       *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   WWKeyboardClass::Buff_Get -- Lowlevel function to get a key from key buffer               *
 *   WWKeyboardClass::Check -- Checks to see if a key is in the buffer                         *
 *   WWKeyboardClass::Clear -- Clears the keyboard buffer.                                     *
 *   WWKeyboardClass::Down -- Checks to see if the specified key is being held down.           *
 *   WWKeyboardClass::Fetch_Element -- Extract the next element in the keyboard buffer.        *
 *   WWKeyboardClass::Fill_Buffer_From_Syste -- Extract and process any queued windows messages*
 *   WWKeyboardClass::Get -- Logic to get a metakey from the buffer                            *
 *   WWKeyboardClass::Get_Mouse_X -- Returns the mouses current x position in pixels           *
 *   WWKeyboardClass::Get_Mouse_XY -- Returns the mouses x,y position via reference vars       *
 *   WWKeyboardClass::Get_Mouse_Y -- returns the mouses current y position in pixels           *
 *   WWKeyboardClass::Is_Buffer_Empty -- Checks to see if the keyboard buffer is empty.        *
 *   WWKeyboardClass::Is_Buffer_Full -- Determines if the keyboard buffer is full.             *
 *   WWKeyboardClass::Is_Mouse_Key -- Checks to see if specified key refers to the mouse.      *
 *   WWKeyboardClass::Message_Handler -- Process a windows message as it relates to the keyboar*
 *   WWKeyboardClass::Peek_Element -- Fetches the next element in the keyboard buffer.         *
 *   WWKeyboardClass::Put -- Logic to insert a key into the keybuffer]                         *
 *   WWKeyboardClass::Put_Element -- Put a keyboard data element into the buffer.              *
 *   WWKeyboardClass::Put_Key_Message -- Translates and inserts wParam into Keyboard Buffer    *
 *   WWKeyboardClass::To_ASCII -- Convert the key value into an ASCII representation.          *
 *   WWKeyboardClass::Available_Buffer_Room -- Fetch the quantity of free elements in the keybo*
 *   WWKeyboardClass::Put_Mouse_Message -- Stores a mouse type message into the keyboard buffer*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "wwkeyboard.h"
#include "video.h"
#include "miscasm.h"
#include <string.h>
#include <cstdlib>
#include "settings.h"

#define ARRAY_SIZE(x) int(sizeof(x) / sizeof(x[0]))

/*
**  Focus handlers for both games.
*/
extern void Focus_Loss();
extern void Focus_Restore();

/***********************************************************************************************
 * WWKeyboardClass::WWKeyBoardClass -- Construction for Westwood Keyboard Class                *
 *                                                                                             *
 * INPUT:		none							                                                        *
 *                                                                                             *
 * OUTPUT:     none							                                                        *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/16/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
WWKeyboardClass::WWKeyboardClass(void)
    : MouseQX(0)
    , MouseQY(0)
    , Head(0)
    , Tail(0)
    , DownSkip(0)
{
    memset(DownState, '\0', sizeof(DownState));
}

WWKeyboardClass::~WWKeyboardClass()
{
}

/***********************************************************************************************
 * WWKeyboardClass::Buff_Get -- Lowlevel function to get a key from key buffer                 *
 *                                                                                             *
 * INPUT:		none                                                        						  *
 *                                                                                             *
 * OUTPUT:     int		- the key value that was pulled from buffer (includes bits)				  * *
 *                                                                                             *
 * WARNINGS:   If the key was a mouse event MouseQX and MouseQY will be updated                *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/17/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
unsigned short WWKeyboardClass::Buff_Get(void)
{
    while (!Check()) {
    } // wait for key in buffer

    unsigned short temp = Fetch_Element();
    if (Is_Mouse_Key(temp)) {
        MouseQX = Fetch_Element();
        MouseQY = Fetch_Element();
    }
    return (temp);
}

/***********************************************************************************************
 * WWKeyboardClass::Is_Mouse_Key -- Checks to see if specified key refers to the mouse.        *
 *                                                                                             *
 *    This checks the specified key code to see if it refers to the mouse buttons.             *
 *                                                                                             *
 * INPUT:   key   -- The key to check.                                                         *
 *                                                                                             *
 * OUTPUT:  bool; Is the key a mouse button key?                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Is_Mouse_Key(unsigned short key)
{
    key &= 0xFF;
    return (key == KN_LMOUSE || key == KN_MMOUSE || key == KN_RMOUSE);
}

/***********************************************************************************************
 * WWKeyboardClass::Check -- Checks to see if a key is in the buffer                           *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/16/1995 PWG : Created.                                                                 *
 *   09/24/1996 JLB : Converted to new style keyboard system.                                  *
 *=============================================================================================*/
KeyNumType WWKeyboardClass::Check(void) const
{
    ((WWKeyboardClass*)this)->Fill_Buffer_From_System();
    if (Is_Buffer_Empty())
        return KN_NONE;
    return (KeyNumType)(Peek_Element());
}

/***********************************************************************************************
 * WWKeyboardClass::Get -- Logic to get a metakey from the buffer                              *
 *                                                                                             *
 * INPUT:		none                                                        						  *
 *                                                                                             *
 * OUTPUT:     int		- the meta key taken from the buffer.											  *
 *                                                                                             *
 * WARNINGS:	This routine will not return until a keypress is received							  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/16/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
KeyNumType WWKeyboardClass::Get(void)
{
    while (!Check()) {
    } // wait for key in buffer
    return (KeyNumType)(Buff_Get());
}

/***********************************************************************************************
 * WWKeyboardClass::Put -- Logic to insert a key into the keybuffer]                           *
 *                                                                                             *
 * INPUT:		int	 	- the key to insert into the buffer          								  *
 *                                                                                             *
 * OUTPUT:     bool		- true if key is sucessfuly inserted.							              *
 *                                                                                             *
 * WARNINGS:   none							                                                        *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/16/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Put(unsigned short key)
{
    if (!Is_Buffer_Full()) {
        Put_Element(key);
        return (true);
    }
    return (false);
}

/***********************************************************************************************
 * WWKeyboardClass::Put_Key_Message -- Translates and inserts wParam into Keyboard Buffer      *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/16/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Put_Key_Message(unsigned short vk_key, bool release)
{
    /*
    ** Get the status of all of the different keyboard modifiers.  Note, only pay attention
    ** to numlock and caps lock if we are dealing with a key that is affected by them.  Note
    ** that we do not want to set the shift, ctrl and alt bits for Mouse keypresses as this
    ** would be incompatible with the dos version.
    */
    if (!Is_Mouse_Key(vk_key)) {
        if (Down(KN_LSHIFT) || Down(KN_RSHIFT) || Down(KN_CAPSLOCK) || Down(KN_NUMLOCK)) {
            vk_key |= WWKEY_SHIFT_BIT;
        }
        if (Down(KN_LCTRL) || Down(KN_RCTRL)) {
            vk_key |= WWKEY_CTRL_BIT;
        }
        if (Down(KN_LALT) || Down(KN_RALT)) {
            vk_key |= WWKEY_ALT_BIT;
        }
    }

    if (release) {
        vk_key |= WWKEY_RLS_BIT;
    }

    /*
    ** Finally use the put command to enter the key into the keyboard
    ** system.
    */
    return (Put(vk_key));
}

/***********************************************************************************************
 * WWKeyboardClass::Put_Mouse_Message -- Stores a mouse type message into the keyboard buffer. *
 *                                                                                             *
 *    This routine will store the mouse type event into the keyboard buffer. It also checks    *
 *    to ensure that there is enough room in the buffer so that partial mouse events won't     *
 *    be recorded.                                                                             *
 *                                                                                             *
 * INPUT:   vk_key   -- The mouse key message itself.                                          *
 *                                                                                             *
 *          x,y      -- The mouse coordinates at the time of the event.                        *
 *                                                                                             *
 *          release  -- Is this a mouse button release?                                        *
 *                                                                                             *
 * OUTPUT:  bool; Was the event stored sucessfully into the keyboard buffer?                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/02/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Put_Mouse_Message(unsigned short vk_key, int x, int y, bool release)
{
    if (Available_Buffer_Room() >= 3 && Is_Mouse_Key(vk_key)) {
        Put_Key_Message(vk_key, release);
        Put((unsigned short)x);
        Put((unsigned short)y);
        return (true);
    }
    return (false);
}

/***********************************************************************************************
 * WWKeyboardClass::Down -- Checks to see if the specified key is being held down.             *
 *                                                                                             *
 *    This routine will examine the key specified to see if it is currently being held down.   *
 *                                                                                             *
 * INPUT:   key   -- The key to check.                                                         *
 *                                                                                             *
 * OUTPUT:  bool; Is the specified key currently being held down?                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Down(unsigned short key)
{
    return Get_Bit(DownState, key);
}

/***********************************************************************************************
 * WWKeyboardClass::Fetch_Element -- Extract the next element in the keyboard buffer.          *
 *                                                                                             *
 *    This routine will extract the next pending element in the keyboard queue. If there is    *
 *    no element available, then NULL is returned.                                             *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with the element extracted from the queue. An empty queue is signified     *
 *          by a 0 return value.                                                               *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
unsigned short WWKeyboardClass::Fetch_Element(void)
{
    unsigned short val = 0;
    if (Head != Tail) {
        val = Buffer[Head];

        Head = (Head + 1) % ARRAY_SIZE(Buffer);
    }
    return (val);
}

/***********************************************************************************************
 * WWKeyboardClass::Peek_Element -- Fetches the next element in the keyboard buffer.           *
 *                                                                                             *
 *    This routine will examine and return with the next element in the keyboard buffer but    *
 *    it will not alter or remove that element. Use this routine to see what is pending in     *
 *    the keyboard queue.                                                                      *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with the next element in the keyboard queue. If the keyboard buffer is     *
 *          empty, then 0 is returned.                                                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
unsigned short WWKeyboardClass::Peek_Element(void) const
{
    if (!Is_Buffer_Empty()) {
        return (Buffer[Head]);
    }
    return (0);
}

/***********************************************************************************************
 * WWKeyboardClass::Put_Element -- Put a keyboard data element into the buffer.                *
 *                                                                                             *
 *    This will put one keyboard data element into the keyboard buffer. Typically, this data   *
 *    is a key code, but it might be mouse coordinates.                                        *
 *                                                                                             *
 * INPUT:   val   -- The data element to add to the keyboard buffer.                           *
 *                                                                                             *
 * OUTPUT:  bool; Was the keyboard element added successfully? A failure would indicate that   *
 *                the keyboard buffer is full.                                                 *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Put_Element(unsigned short val)
{
    if (!Is_Buffer_Full()) {
        int temp = (Tail + 1) % ARRAY_SIZE(Buffer);
        Buffer[Tail] = val;
        Tail = temp;

        /* set cached down state for given key, remove all bits */
        if (DownSkip == 0) {
            bool isDown = !(val & KN_RLSE_BIT);
            val &= ~(KN_SHIFT_BIT | KN_CTRL_BIT | KN_ALT_BIT | KN_RLSE_BIT | KN_BUTTON);
            Set_Bit(DownState, val, isDown);

            if (Is_Mouse_Key(val)) {
                DownSkip = 2;
            }
        } else {
            DownSkip--;
        }

        return (true);
    }
    return (false);
}

/***********************************************************************************************
 * WWKeyboardClass::Is_Buffer_Full -- Determines if the keyboard buffer is full.               *
 *                                                                                             *
 *    This routine will examine the keyboard buffer to determine if it is completely           *
 *    full of queued keyboard events.                                                          *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Is the keyboard buffer completely full?                                      *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Is_Buffer_Full(void) const
{
    if ((Tail + 1) % ARRAY_SIZE(Buffer) == Head) {
        return (true);
    }
    return (false);
}

/***********************************************************************************************
 * WWKeyboardClass::Is_Buffer_Empty -- Checks to see if the keyboard buffer is empty.          *
 *                                                                                             *
 *    This routine will examine the keyboard buffer to see if it contains no events at all.    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Is the keyboard buffer currently without any pending events queued?          *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Is_Buffer_Empty(void) const
{
    if (Head == Tail) {
        return (true);
    }
    return (false);
}

/***********************************************************************************************
 * WWKeyboardClass::Fill_Buffer_From_Syste -- Extract and process any queued windows messages. *
 *                                                                                             *
 *    This routine will extract and process any windows messages in the windows message        *
 *    queue. It is presumed that the normal message handler will call the keyboard             *
 *    message processing function.                                                             *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
bool WWKeyboardClass::Is_Gamepad_Active()
{
    return false;
}

void WWKeyboardClass::Open_Controller()
{
}

void WWKeyboardClass::Close_Controller()
{
}

bool WWKeyboardClass::Is_Analog_Scroll_Active()
{
    return false;
}

unsigned char WWKeyboardClass::Get_Scroll_Direction()
{
    return SDIR_NONE;
}

/***********************************************************************************************
 * WWKeyboardClass::Clear -- Clears the keyboard buffer.                                       *
 *                                                                                             *
 *    This routine will clear the keyboard buffer of all pending keyboard events.              *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
void WWKeyboardClass::Clear(void)
{
    /*
    **	Extract any windows pending keyboard message events and then clear out the keyboard
    **	buffer.
    */
    Fill_Buffer_From_System();
    Head = Tail;

    /*
    **	Perform a second clear to handle the rare case of the keyboard buffer being full and there
    **	still remains keyboard related events in the windows message queue.
    */
    Fill_Buffer_From_System();
    Head = Tail;

    /*
    **  Reset all keys to not being held down to prevent stuck modifiers.
    */
    memset(DownState, '\0', sizeof(DownState));
}

/***********************************************************************************************
 * WWKeyboardClass::Message_Handler -- Process a windows message as it relates to the keyboard *
 *                                                                                             *
 *    This routine will examine the Windows message specified. If the message relates to an    *
 *    event that the keyboard input system needs to process, then it will be processed         *
 *    accordingly.                                                                             *
 *                                                                                             *
 * INPUT:   window   -- Handle to the window receiving the message.                            *
 *                                                                                             *
 *          message  -- The message number of this event.                                      *
 *                                                                                             *
 *          wParam   -- The windows specific word parameter (meaning depends on message).      *
 *                                                                                             *
 *          lParam   -- The windows specific long word parameter (meaning is message dependant)*
 *                                                                                             *
 * OUTPUT:  bool; Was this keyboard message recognized and processed? A 'false' return value   *
 *                means that the message should be processed normally.                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
#if defined(_WIN32) && !defined(SDL_BUILD)
bool WWKeyboardClass::Message_Handler(HWND window, UINT message, UINT wParam, LONG lParam)
{
// ST - 5/13/2019
#ifndef REMASTER_BUILD // #if (0)
    bool processed = false;

    /*
    **	Examine the message to see if it is one that should be processed. Only keyboard and
    **	pertinant mouse messages are processed.
    */
    switch (message) {

    /*
    **	System key has been pressed. This is the normal keyboard event message.
    */
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        Put_Key_Message((unsigned short)wParam);
        processed = true;
        break;

    /*
    **	The key has been released. This is the normal key release message.
    */
    case WM_SYSKEYUP:
    case WM_KEYUP:
        Put_Key_Message((unsigned short)wParam, true);
        processed = true;
        break;

    /*
    **	Press of the left mouse button.
    */
    case WM_LBUTTONDOWN:
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam));
        processed = true;
        break;

    /*
    **	Release of the left mouse button.
    */
    case WM_LBUTTONUP:
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	Double click of the left mouse button. Fake this into being
    **	just a rapid click of the left button twice.
    */
    case WM_LBUTTONDBLCLK:
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	Press of the middle mouse button.
    */
    case WM_MBUTTONDOWN:
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam));
        processed = true;
        break;

    /*
    **	Release of the middle mouse button.
    */
    case WM_MBUTTONUP:
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	Middle button double click gets translated into two
    **	regular middle button clicks.
    */
    case WM_MBUTTONDBLCLK:
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_MBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	Right mouse button press.
    */
    case WM_RBUTTONDOWN:
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam));
        processed = true;
        break;

    /*
    **	Right mouse button release.
    */
    case WM_RBUTTONUP:
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	Translate a double click of the right button
    **	into being just two regular right button clicks.
    */
    case WM_RBUTTONDBLCLK:
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam));
        Put_Mouse_Message(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam), true);
        processed = true;
        break;

    /*
    **	If the message is not pertinant to the keyboard system,
    **	then do nothing.
    */
    default:
        break;
    }

    /*
    **	If this message has been processed, then pass it on to the system
    **	directly.
    */
    if (processed) {
        DefWindowProcA(window, message, wParam, lParam);
        return (true);
    }
#endif
    return (false);
}
#endif

/***********************************************************************************************
 * WWKeyboardClass::Available_Buffer_Room -- Fetch the quantity of free elements in the keyboa *
 *                                                                                             *
 *    This examines the keyboard buffer queue and determine how many elements are available    *
 *    for use before the buffer becomes full. Typical use of this would be when inserting      *
 *    mouse events that require more than one element. Such an event must detect when there    *
 *    would be insufficient room in the buffer and bail accordingly.                           *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with the number of elements that may be stored in to the keyboard buffer   *
 *          before it becomes full and cannot accept any more elements.                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/02/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
int WWKeyboardClass::Available_Buffer_Room(void) const
{
    int avail;
    if (Head == Tail) {
        avail = ARRAY_SIZE(Buffer);
    }
    if (Head < Tail) {
        avail = Tail - Head;
    }
    if (Head > Tail) {
        avail = (Tail + ARRAY_SIZE(Buffer)) - Head;
    }
    return (avail);
}
