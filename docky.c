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

#include "dockybase.h"
#include "cfg.h"

#define RGB8to32(RGB)   ((uint32)(RGB) * 0x01010101UL)
#define RED(a)          (((a)>>16) & 0xFF)
#define GREEN(a)        (((a)>>8)  & 0xFF)
#define BLUE(a)         ((a)       & 0xFF)
#define MAX_STRING_SIZE 12

#define min(a,b) ((a)<=(b)?(a):(b))

extern const struct TagItem dockyTags[];

static void DockyRender (struct DockyBase *db, struct DockyData *dd);
static void CheckWarnTemperatures(struct DockyData *dd, uint16 nTempThreshold, uint16 nTempValue);

uint32 strlen(CONST_STRPTR str)
{
    uint32 len = 0;
    if(!str) return 0;
    while(str[len]) len++;
    return len;
}

/**********************************************************
**
** The following function saves the variable name passed in
** 'varname' to the ENV(ARC) system so that the application
** can become AmiUpdate aware.
**
**********************************************************/
static void SetAmiUpdateENVVariable(struct DockyBase *db, CONST_STRPTR varname, CONST_STRPTR progpath )
{
    /* AmiUpdate support code */
    APTR oldwin = NULL;
    TEXT varpath[1024];
    varpath[0] = '\0';

    /* stop any "Insert volume..." type requesters */
    oldwin = IDOS->SetProcWindow((APTR)-1);

    /*
    finally set the variable to the
    path the executable was run from
    don't forget to supply the variable
    name to suit your application
    */
    IDOS->AddPart( varpath, "AppPaths", 1024 );
    IDOS->AddPart( varpath, varname, 1024 );
    IDOS->SetVar( varpath, progpath, -1, GVF_GLOBAL_ONLY|GVF_SAVE_VAR );

#ifndef NDEBUG
    IExec->DebugPrintF("[SetAmiUpdateENVVariable] varpath='%s', progpath='%s'\n", varpath, progpath);
#endif

    /* turn requesters back on */
    IDOS->SetProcWindow( oldwin );
}


uint32 DockyObtain (struct DockyIFace *Self) {
	return ++Self->Data.RefCount;
}

uint32 DockyRelease (struct DockyIFace *Self) {
	Self->Data.RefCount--;
	if (!Self->Data.RefCount && (Self->Data.Flags & IFLF_CLONED)) {
		struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
		IExec->DeleteInterface((struct Interface *)Self);
	}
	return Self->Data.RefCount;
}

struct DockyIFace *DockyClone (struct DockyIFace *Self) {
	struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
	struct DockyIFace *docky = NULL;
	docky = (struct DockyIFace *)IExec->MakeInterface(Self->Data.LibBase, dockyTags);
	if (docky) {
		struct DockyData *dd = (struct DockyData *)((uint8 *)docky - docky->Data.NegativeSize);
		docky->Data.Flags |= IFLF_CLONED;

		IUtility->SetMem(dd, 0, sizeof(struct DockyData));

        dd->Base = db;
		dd->shadowpen = dd->textpen = dd->graphpen = ~0;
		dd->shadowcolor = dd->textcolor = ~0;
        dd->graphcolor = 0xFF0000;
        dd->graphcolor2 = 0x006400;

        dd->bSetEnv = FALSE;

		dd->font = GfxLib->DefaultFont;
		dd->size.width = dd->font->tf_XSize * (MAX_STRING_SIZE + 2);
        dd->size.height = dd->font->tf_YSize * 4 + 8;
		dd->MBPos.x = dd->font->tf_XSize*1;
		dd->MBPos.y = 2+dd->font->tf_Baseline;
		dd->CPUPos.x = dd->font->tf_XSize*1;
		dd->CPUPos.y = 4+dd->font->tf_YSize+dd->font->tf_Baseline;
        dd->Core1Pos.x = dd->font->tf_XSize*1;
        dd->Core1Pos.y = 6 + dd->font->tf_YSize * 2 + dd->font->tf_Baseline;
        dd->Core2Pos.x = dd->font->tf_XSize*1;
        dd->Core2Pos.y = 8 + dd->font->tf_YSize * 3 + dd->font->tf_Baseline;
		
        dd->curIdx = 0;
        dd->maxIdx = min(MAX_RECORD_LENGTH, dd->size.width);
        dd->refreshRate = 50;

        dd->MBWarnTemp = dd->CPUWarnTemp = dd-> Core1WarnTemp = dd->Core2WarnTemp = ~0;
	}
	return docky;
}

static void ReadDockyPrefs (struct DockyBase *db, struct DockyData *dd, char *filename) {
	struct DiskObject *icon;

    dd->Base = db;
	dd->shadowpen = dd->textpen = dd->graphpen = ~0;
	dd->shadowcolor = dd->textcolor = ~0;
    dd->graphcolor = 0xFF0000;
    dd->graphcolor2 = 0x006400;

    dd->MBWarnTemp = dd->CPUWarnTemp = dd-> Core1WarnTemp = dd->Core2WarnTemp = ~0;

	dd->font = GfxLib->DefaultFont;

    dd->bSetEnv = FALSE;

	icon = IIcon->GetDiskObjectNew(filename);
	if (icon) {
		char *str;
		int32 val;

		str = CFGString(icon, "FONTNAME", NULL);
		val = CFGInteger(icon, "FONTSIZE", 0);
		if (str && val) {
			struct TextAttr ta = {0};
			struct TextFont *font;
			ta.ta_Name = str;
			ta.ta_YSize = val;
			font = IDiskfont->OpenDiskFont(&ta);
			if (font) {
				dd->font = font;
				dd->freefont = TRUE;
			}
		}
		
		dd->shadowcolor = CFGHex(icon, "SHADOWCOLOR", 0);
		dd->textcolor = CFGHex(icon, "TEXTCOLOR", ~0);
		dd->graphcolor = CFGHex(icon, "GRAPHCOLOR_UP", 0xFF0000);
		dd->graphcolor2 = CFGHex(icon, "GRAPHCOLOR_DOWN", 0x006400);
        dd->refreshRate = CFGInteger(icon, "REFRESH", 50);
        dd->bSetEnv = CFGBoolean(icon, "SETENV");
        dd->bUseFahrenheit = CFGBoolean(icon, "FAHRENHEIT");
        dd->MBWarnTemp = CFGInteger(icon, "LOCAL_WARN", ~0);
        dd->CPUWarnTemp = CFGInteger(icon, "CPU_WARN", ~0);
        dd->Core1WarnTemp = CFGInteger(icon, "CORE1_WARN", ~0);
        dd->Core2WarnTemp = CFGInteger(icon, "CORE2_WARN", ~0);

		IIcon->FreeDiskObject(icon);
	}

	dd->size.width = dd->font->tf_XSize * (MAX_STRING_SIZE + 2);
    dd->size.height = dd->font->tf_YSize * 4 + 8;
	dd->MBPos.x = dd->font->tf_XSize*1;
	dd->MBPos.y = 2+dd->font->tf_Baseline;
	dd->CPUPos.x = dd->font->tf_XSize*1;
	dd->CPUPos.y = 4+dd->font->tf_YSize+dd->font->tf_Baseline;
    dd->Core1Pos.x = dd->font->tf_XSize*1;
    dd->Core1Pos.y = 6 + dd->font->tf_YSize * 2 + dd->font->tf_Baseline;
    dd->Core2Pos.x = dd->font->tf_XSize*1;
    dd->Core2Pos.y = 8 + dd->font->tf_YSize * 3 + dd->font->tf_Baseline;

    dd->curIdx = 0;
    dd->maxIdx = min(MAX_RECORD_LENGTH, dd->size.width);

	dd->GradSpecGraph.Direction = IIntuition->DirectionVector(180);
	dd->GradSpecGraph.Mode = GRADMODE_COLOR;
	dd->GradSpecGraph.Specs.Abs.RGBEnd[0] = RED(dd->graphcolor);
	dd->GradSpecGraph.Specs.Abs.RGBEnd[1] = GREEN(dd->graphcolor);
	dd->GradSpecGraph.Specs.Abs.RGBEnd[2] = BLUE(dd->graphcolor);
	dd->GradSpecGraph.Specs.Abs.RGBStart[0] = RED(dd->graphcolor2);
	dd->GradSpecGraph.Specs.Abs.RGBStart[1] = GREEN(dd->graphcolor2);
	dd->GradSpecGraph.Specs.Abs.RGBStart[2] = BLUE(dd->graphcolor2);
}

static void ReleaseDockyPens (struct DockyBase *db, struct DockyData *dd) {
	if (dd->scr) {
		struct ColorMap *cm = dd->scr->ViewPort.ColorMap;
		if (dd->shadowpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->shadowpen);
		if (dd->textpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->textpen);
		if (dd->graphpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->graphpen);
        if (dd->dri) IIntuition->FreeScreenDrawInfo(dd->scr, dd->dri);
		dd->scr = NULL;
        dd->dri = NULL;
		dd->shadowpen = ~0;
		dd->textpen = ~0;
		dd->graphpen = ~0;
	}
}

static void ObtainDockyPens (struct DockyBase *db, struct DockyData *dd, struct Screen *scr) {
	ReleaseDockyPens(db, dd);
	if (dd->scr = scr) {
		struct ColorMap *cm = scr->ViewPort.ColorMap;
		dd->shadowpen = IGraphics->ObtainBestPenA(cm,
			RGB8to32((dd->shadowcolor >> 16) & 255),
			RGB8to32((dd->shadowcolor >> 8) & 255),
			RGB8to32(dd->shadowcolor & 255),
			NULL);
		dd->textpen = IGraphics->ObtainBestPenA(cm,
			RGB8to32((dd->textcolor >> 16) & 255),
			RGB8to32((dd->textcolor >> 8) & 255),
			RGB8to32(dd->textcolor & 255),
			NULL);
		dd->graphpen = IGraphics->ObtainBestPenA(cm,
			RGB8to32((dd->graphcolor >> 16) & 255),
			RGB8to32((dd->graphcolor >> 8) & 255),
			RGB8to32(dd->graphcolor & 255),
			NULL);
        dd->dri = IIntuition->GetScreenDrawInfo(scr);
	}
}

void DockyExpunge (struct DockyIFace *Self) {
	if (!Self->Data.RefCount) {
		struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
		struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

		ReleaseDockyPens(db, dd);

		if (dd->freefont) IGraphics->CloseFont(dd->font);
		dd->font = NULL;

		IExec->FreeVec((uint8 *)Self - Self->Data.NegativeSize);
	}
}

BOOL DockyGet (struct DockyIFace *Self, uint32 msgType, uint32 *msgData) {
	struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);
	BOOL res = TRUE;

	switch (msgType) {

		case DOCKYGET_Version:
			*msgData = DOCKYVERSION;
			break;

		case DOCKYGET_GetSize:
			*(struct DockySize *)msgData = dd->size;
			break;

		case DOCKYGET_FrameDelay:
			*msgData = dd->refreshRate;
			break;

		case DOCKYGET_RenderMode:
			*msgData = DOCKYRENDERMODE_RPPA;
			break;

		case DOCKYGET_Notifications:
			*msgData = 0;
			break;

		default:
			res = FALSE;
			break;

	}

	return res;
}

BOOL DockySet (struct DockyIFace *Self, uint32 msgType, uint32 msgData) {
	struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);
	BOOL res = TRUE;

	switch (msgType) {

		case DOCKYSET_FileName:
            {
    			char buffer[1024];
    			IUtility->Strlcpy(buffer, (STRPTR)msgData, 1024);
    			*(IDOS->FilePart(buffer)) = '\0';
    			SetAmiUpdateENVVariable(dd->Base, "X1kTemp.docky", buffer);

    			ReadDockyPrefs((void *)Self->Data.LibBase, dd, (char *)msgData);
            }
			break;

		case DOCKYSET_Screen:
			ObtainDockyPens((void *)Self->Data.LibBase, dd, (struct Screen *)msgData);
			break;

		case DOCKYSET_RenderDestination:
			{
				struct DockyRenderDestination *drd = (struct DockyRenderDestination *)msgData;
				switch (drd->renderMode) {
					case DOCKYRENDERMODE_RPPA:
						dd->size = drd->renderSize;
						dd->rp = drd->render.RP;
						break;
					default:
						dd->rp = NULL;
						res = FALSE;
						break;
				}
			}
			break;

		case DOCKYSET_RedrawNow:
			DockyRender((void *)Self->Data.LibBase, dd);
			break;

		case DOCKYSET_DockTypeChange:
			{
				struct DockyDockType *ddt=(struct DockyDockType *)msgData;
				if (dd->self.dockNr == ddt->dockNr && ddt->dockType != AMIDOCK_DockType_Icons)
					res = FALSE;
			}
			break;

		case DOCKYSET_DockyChange:
			dd->self = *(struct DockyObjectNr *)msgData;
			break;

		default:
			res = FALSE;
			break;

	}

	return res;
}

BOOL DockyProcess (struct DockyIFace *Self, uint32 turnCount, uint32 *msgType, uint32 *msgData,
	BOOL *anotherTurn)
{
	struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
	struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

    if(tmp423_read_all_temps(dd))
    {
#ifndef NDEBUG
    	IExec->DebugPrintF("[DockyProcess] could not read temps\n");
#endif
        return FALSE;
    }

    uint8 nCorIdx = dd->curIdx?dd->curIdx-1:dd->maxIdx-1;

    // check warning temperatures
    CheckWarnTemperatures(dd, dd->MBWarnTemp, dd->MBTemp[nCorIdx]);
    CheckWarnTemperatures(dd, dd->CPUWarnTemp, dd->CPUTemp[nCorIdx]);
    CheckWarnTemperatures(dd, dd->Core1WarnTemp, dd->Core1Temp[nCorIdx]);
    CheckWarnTemperatures(dd, dd->Core2WarnTemp, dd->Core2Temp[nCorIdx]);

    // handling environnement variables
    if(dd->bSetEnv)
    {
        TEXT szValue[6];
        TEXT cUnit = dd->bUseFahrenheit?'F':'C';

        IUtility->SNPrintf( szValue, sizeof(szValue)/sizeof(TEXT), "%ld°%lc", dd->MBTemp[nCorIdx], cUnit);
        IDOS->SetVar( "LocalTemp", szValue, -1, GVF_GLOBAL_ONLY );
        IUtility->SNPrintf( szValue, sizeof(szValue)/sizeof(TEXT), "%ld°%lc", dd->CPUTemp[nCorIdx], cUnit);
        IDOS->SetVar( "CPUTemp", szValue, -1, GVF_GLOBAL_ONLY );
        IUtility->SNPrintf( szValue, sizeof(szValue)/sizeof(TEXT), "%ld°%lc", dd->Core1Temp[nCorIdx], cUnit);
        IDOS->SetVar( "Core1Temp", szValue, -1, GVF_GLOBAL_ONLY );
        IUtility->SNPrintf( szValue, sizeof(szValue)/sizeof(TEXT), "%ld°%lc", dd->Core2Temp[nCorIdx], cUnit);
        IDOS->SetVar( "Core2Temp", szValue, -1, GVF_GLOBAL_ONLY );
    }

	return TRUE;
}

static void DockyRender (struct DockyBase *db, struct DockyData *dd) {
	if (dd->rp) {
        TEXT tmp[MAX_STRING_SIZE+2];

		uint16 shadowpen = (dd->shadowpen != (uint16)~0) ? dd->shadowpen : BLOCKPEN;
		uint16 textpen = (dd->textpen != (uint16)~0) ? dd->textpen : TEXTPEN;
        uint32 graphpen = (dd->graphpen != (uint16)~0) ? dd->graphpen : 3;

        uint8 nIdx = 0, nCorIdx = 0;
        uint16 nTemp = 0;
        struct IBox iBoxCPU = {0, dd->CPUPos.y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+3},
                    iBoxMB = {0, dd->MBPos.y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+2},
                    iBoxCore1 = {0, dd->Core1Pos.y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+3},
                    iBoxCore2 = {0, dd->Core2Pos.y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+2};
		uint16 nDivider = dd->bUseFahrenheit?212:100;

        IGraphics->SetABPenDrMd(dd->rp, graphpen, 0, JAM1);
        for(nIdx = 0; nIdx < dd->maxIdx; nIdx++)
        {
            nCorIdx = (dd->curIdx+nIdx)%(dd->maxIdx);

            nTemp = dd->MBTemp[nCorIdx] * dd->font->tf_YSize / nDivider;
            if(0 != nTemp)
    		   IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, dd->MBPos.y-nTemp,
                                        1, nTemp+3, &iBoxMB, 0L, &dd->GradSpecGraph, dd->dri);
            /*IGraphics->WritePixel(dd->rp, nIdx, dd->MBPos.y-nTemp);//, graphcolor);
            IGraphics->WritePixel(dd->rp, nIdx, dd->MBPos.y);*/

            nTemp = dd->CPUTemp[nCorIdx] * dd->font->tf_YSize / nDivider;
            if(0 != nTemp)
    		   IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, dd->CPUPos.y-nTemp,
                                        1, nTemp+3, &iBoxCPU, 0L, &dd->GradSpecGraph, dd->dri);
            //IGraphics->WritePixel(dd->rp, nIdx, dd->CPUPos.y-nTemp);//, graphcolor);
            nTemp = dd->Core1Temp[nCorIdx] * dd->font->tf_YSize / nDivider;
            if(0 != nTemp)
    		   IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, dd->Core1Pos.y-nTemp,
                                        1, nTemp+3, &iBoxCore1, 0L, &dd->GradSpecGraph, dd->dri);
            //IGraphics->WritePixel(dd->rp, nIdx, dd->Core1Pos.y-nTemp);//, graphcolor);
            nTemp = dd->Core2Temp[nCorIdx] * dd->font->tf_YSize / nDivider;
            if(0 != nTemp)
    		   IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, dd->Core2Pos.y-nTemp,
                                        1, nTemp+3, &iBoxCore2, 0L, &dd->GradSpecGraph, dd->dri);
            //IGraphics->WritePixel(dd->rp, nIdx, dd->Core2Pos.y-nTemp);//, graphcolor);
        }

        TEXT cUnit = dd->bUseFahrenheit?'F':'C';

        IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1, "Local: %3ld°%lc", dd->MBTemp[nCorIdx], cUnit);
		IGraphics->SetFont(dd->rp, dd->font);
		IGraphics->Move(dd->rp, dd->MBPos.x+1, dd->MBPos.y+1);
		IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));
		IGraphics->Move(dd->rp, dd->MBPos.x, dd->MBPos.y);
		IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));

        IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1, "CPU:   %3ld°%lc", dd->CPUTemp[nCorIdx], cUnit);
		IGraphics->Move(dd->rp, dd->CPUPos.x+1, dd->CPUPos.y+1);
		IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));
		IGraphics->Move(dd->rp, dd->CPUPos.x, dd->CPUPos.y);
		IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));

        IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1, "Core1: %3ld°%lc", dd->Core1Temp[nCorIdx], cUnit);
		IGraphics->Move(dd->rp, dd->Core1Pos.x+1, dd->Core1Pos.y+1);
		IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));
		IGraphics->Move(dd->rp, dd->Core1Pos.x, dd->Core1Pos.y);
		IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));

        IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1, "Core2: %3ld°%lc", dd->Core2Temp[nCorIdx], cUnit);
		IGraphics->Move(dd->rp, dd->Core2Pos.x+1, dd->Core2Pos.y+1);
		IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));
		IGraphics->Move(dd->rp, dd->Core2Pos.x, dd->Core2Pos.y);
		IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
		IGraphics->Text(dd->rp, tmp, strlen(tmp));
	}
}

static void CheckWarnTemperatures(struct DockyData *dd, uint16 nTempThreshold, uint16 nTempValue)
{
    if(((uint16)~0 != nTempThreshold) && (nTempValue >= nTempThreshold))
    {
        IDOS->TimedDosRequesterTags(TDR_TitleString, "X1KTemp Docky - Temperature Warning",
                                    TDR_FormatString, "Local temperature passed above %ld!",
                                    TDR_GadgetString, "OK",
                                    TDR_Arg1, nTempThreshold,
                                    TDR_ImageType, TDRIMAGE_WARNING,
                                    TAG_END);
    }
}

