/*
** X1kTemp.docky
** (c) 2013 Alexandre Balaban <alexandre -(@)- balaban -(.)- fr>
**
** Based upon Datetime.docky (c) by Fredrik Wikstrom
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
*/

#ifndef DOCKYBASE_H
#define DOCKYBASE_H

#include <exec/exec.h>
#include <dos/dos.h>
#include <utility/utility.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <libraries/docky.h>
#include <expansion/pci.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <proto/expansion.h>
#include <proto/application.h>
#include <proto/timer.h>
#include <interfaces/docky.h>

struct DockyBase;
struct DockyData;

#include "readtemp.h"

struct Pos {
	uint16 x, y;
};

#define MAX_RECORD_LENGTH 72

typedef uint16 recTmp_t[MAX_RECORD_LENGTH];

struct DockyData {
    struct DockyBase *Base;
	struct DockyObjectNr self;
	struct DockySize size;
	struct RastPort *rp;
	struct TextFont *font;
    struct DrawInfo *dri;

	struct Screen *scr;
	uint16 shadowpen, textpen, graphpen;
	struct Pos MBPos, CPUPos, Core1Pos, Core2Pos;

	BOOL freefont;
	BOOL blink;

	uint32 shadowcolor;
	uint32 textcolor;
    uint32 graphcolor, graphcolor2;
    struct GradientSpec GradSpecGraph;

    // record temperatures as circular buffers
    recTmp_t MBTemp, CPUTemp, Core1Temp, Core2Temp;
    // current index in the circular index, and max
    // index. Current Max index is the bound where
    // the buffer returns to zero in case of the
    // display does not allow full record.
    uint8 curIdx, maxIdx;

    // Warn temperatures for each sensor
    uint16 MBWarnTemp, CPUWarnTemp, Core1WarnTemp, Core2WarnTemp;
    TEXT szWarnCmd[2048];

    uint32 refreshRate;

    BOOL bSetEnv;
    BOOL bUseFahrenheit;

    tmp423_device_t dev;

    // Application.library part
    uint32 nAppID;
    struct MsgPort *pAppLibPort;
    BOOL bAlreadyNotified;
    TEXT szImageFile[2048];
};

#define DDF_BLINK 1
#define DDF_12

struct DockyBase {
    struct Library libNode;

    BPTR segList;

	struct ExecIFace *IExec;
	struct IntuitionIFace *IIntuition;
	struct GraphicsIFace *IGraphics;
	struct DiskfontIFace *IDiskfont;
    struct PCIIFace *IPCI;
    struct TimerIFace *ITimer;
    struct ApplicationIFace *IApplication;

	struct Library *IntuitionLib;
	struct GfxBase *GfxLib;
	struct Library *DiskfontLib;
    struct Library *ExpansionLib;
    struct Library *ApplicationLib;

    struct MsgPort *TimerPort;
    struct TimeRequest *TimerRequest;

    // ///////////////////////
    // SMBus interface
    smbus_channel_t channels[SMBUS_MAXCHANNELS];
    uint32 smbus_numchannels;
    // /
};

#define IExec db->IExec
#define IIntuition db->IIntuition
#define IGraphics db->IGraphics
#define IDiskfont db->IDiskfont
#define ITimer db->ITimer
#define IPCI db->IPCI
#define IApplication db->IApplication

#define ExecLib db->ExecLib
#define IntuitionLib db->IntuitionLib
#define GfxLib db->GfxLib
#define DiskfontLib db->DiskfontLib
#define ExpansionLib db->ExpansionLib
#define ApplicationLib db->ApplicationLib

uint32 DockyObtain (struct DockyIFace *Self);
uint32 DockyRelease (struct DockyIFace *Self);
void DockyExpunge (struct DockyIFace *Self);
struct DockyIFace *DockyClone (struct DockyIFace *Self);
BOOL DockyGet (struct DockyIFace *Self, uint32 msgType, uint32 *msgData);
BOOL DockySet (struct DockyIFace *Self, uint32 msgType, uint32 msgData);
BOOL DockyProcess (struct DockyIFace *Self, uint32 turnCount, uint32 *msgType, uint32 *msgData,
	BOOL *anotherTurn);

#endif
