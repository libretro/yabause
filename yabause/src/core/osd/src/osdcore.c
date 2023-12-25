/*  Copyright 2012 Guillaume Duhamel

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file osdcore.c
    \brief OSD dummy and software interfaces.
*/

#include "osdcore.h"
#include "vdp1.h"
#include "font.h"

#include <stdlib.h>
#include <stdarg.h>

#ifndef YAB_PORT_OSD
/*
Heya fellow port developper :)
If you're reading this, that may be because you want to
use your own OSD core in your port and you're about to
do it here...
Don't.
Please define the CPP constant YAB_PORT_OSD and define
your own list of OSD cores in your port.
This definition was added here to avoid breaking "everything"
when we the new OSD system was added.
*/
OSD_struct *OSDCoreList[] = {
&OSDDummy,
NULL
};
#else
extern OSD_struct * OSDCoreList[];
#endif

static OSD_struct * OSD = NULL;
static OSDMessage_struct osdmessages[OSDMSG_COUNT] = {0};

int OSDInit(int coreid)
{
   int i;

   for (i = 0; OSDCoreList[i] != NULL; i++)
   {
      if (OSDCoreList[i]->id == coreid)
      {
         OSD = OSDCoreList[i];
         break;
      }
   }

   if (OSD == NULL)
      return -1;

   if (OSD->Init() != 0)
      return -1;

   memset(osdmessages, 0, sizeof(osdmessages));
   osdmessages[OSDMSG_FPS].hidden = 1;
   osdmessages[OSDMSG_DEBUG].hidden = 1;

   return 0;
}

void OSDDeInit() {
   if (OSD)
      OSD->DeInit();
   OSD = NULL;

   for (int i =0 ; i<OSDMSG_COUNT; i++) {
     if (osdmessages[i].message != NULL) {
       free(osdmessages[i].message);
       osdmessages[i].message = NULL;
     }
   }
}

int OSDChangeCore(int coreid)
{
   int preservefps, fpshidden, dbghidden;

   preservefps = (OSD != NULL);
   fpshidden = osdmessages[OSDMSG_FPS].hidden;
   dbghidden = osdmessages[OSDMSG_DEBUG].hidden;

   OSDDeInit();
   OSDInit(coreid);

   if (preservefps)
   {
      osdmessages[OSDMSG_FPS].hidden = fpshidden;
      osdmessages[OSDMSG_DEBUG].hidden = dbghidden;
   }

   return 0;
}

void OSDPushMessage(int msgtype, int ttl, const char * format, ...)
{
   va_list arglist;
   char message[1024];

   if (ttl == 0) return;

   va_start(arglist, format);
   vsprintf(message, format, arglist);
   va_end(arglist);

   osdmessages[msgtype].type = msgtype;
   osdmessages[msgtype].message = strdup(message);
   osdmessages[msgtype].timetolive = ttl;
   osdmessages[msgtype].timeleft = ttl;
}

int OSDDisplayMessages(pixel_t * buffer, int w, int h)
{
   int i = 0;
   int somethingnew = 0;

   if (OSD == NULL) return somethingnew;

   for(i = 0;i < OSDMSG_COUNT;i++)
      if (osdmessages[i].timeleft > 0)
      {
         if (osdmessages[i].hidden == 0)
         {
            somethingnew = 1;
            OSD->DisplayMessage(osdmessages + i, buffer, w, h);
         }
         osdmessages[i].timeleft--;
         if (osdmessages[i].timeleft == 0) {
           free(osdmessages[i].message);
           osdmessages[i].message = NULL;
         }
      }

   return somethingnew;
}

void  OSDAddLogString( char * log ){
   if (OSD && OSD->AddLogString){
      OSD->AddLogString(log);
   }
}

void OSDToggle(int what)
{
   if ((what < 0) || (what >= OSDMSG_COUNT)) return;

   osdmessages[what].hidden = 1 - osdmessages[what].hidden;
}

int OSDIsVisible(int what)
{
   if ((what < 0) || (what >= OSDMSG_COUNT)) return -1;

   return 1 - osdmessages[what].hidden;
}

void OSDSetVisible(int what, int visible)
{
   if ((what < 0) || (what >= OSDMSG_COUNT)) return;

   visible = visible == 0 ? 0 : 1;
   osdmessages[what].hidden = 1 - visible;
}

int OSDUseBuffer(void)
{
   if (OSD == NULL) return 0;

   return OSD->UseBuffer();
}

void ToggleFPS()
{
   OSDToggle(OSDMSG_FPS);
   OSDToggle(OSDMSG_VDP1_FPS);
}

int GetOSDToggle(void)
{
   return OSDIsVisible(OSDMSG_FPS);
}

void SetOSDToggle(int toggle)
{
   OSDSetVisible(OSDMSG_FPS, toggle);
   OSDSetVisible(OSDMSG_VDP1_FPS, toggle);
}

void DisplayMessage(const char* str)
{
   OSDPushMessage(OSDMSG_STATUS, 120, str);
}

static int OSDDummyInit(void);
static void OSDDummyDeInit(void);
static void OSDDummyReset(void);
static void OSDDummyDisplayMessage(OSDMessage_struct * message, pixel_t * buffer, int w, int h);
static int OSDDummyUseBuffer(void);

OSD_struct OSDDummy = {
    OSDCORE_DUMMY,
    "Dummy OSD Interface",
    OSDDummyInit,
    OSDDummyDeInit,
    OSDDummyReset,
    OSDDummyDisplayMessage,
    OSDDummyUseBuffer,
	NULL,
};

int OSDDummyInit(void)
{
   return 0;
}

void OSDDummyDeInit(void)
{
}

void OSDDummyReset(void)
{
}

void OSDDummyDisplayMessage(OSDMessage_struct * message, pixel_t * buffer, int w, int h)
{
}

int OSDDummyUseBuffer(void)
{
   return 0;
}
