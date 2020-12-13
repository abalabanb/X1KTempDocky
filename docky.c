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
#include "locale_support.h"

#define RGB8to32(RGB)   ((uint32)(RGB) * 0x01010101UL)
#define RED(a)          (((a)>>16) & 0xFF)
#define GREEN(a)        (((a)>>8)  & 0xFF)
#define BLUE(a)         ((a)       & 0xFF)
#define MAX_STRING_SIZE 12
#define DEFAULT_TIMESPAN 300

#define MB_CRITICAL     (dd->bUseFahrenheit?113:45)
#define CPU_CRITICAL    (dd->bUseFahrenheit?185:85)
#define CRITICAL_TIMESPAN 180
#define ABORT_SHUTDOWN  "ABORT_SHUTDOWN"

#define min(a,b) ((a)<=(b)?(a):(b))

extern const struct TagItem dockyTags[];

static void DockyRender (struct DockyBase *db, struct DockyData *dd);
static void CheckWarnTemperatures(struct DockyData *dd, uint16 nTempThreshold, uint16 nTempValue, uint32 *pLastNotified, CONST_STRPTR szSensor);
static void CheckCriticalTemperatures(struct DockyData *dd, uint16 nMB, uint16 nCore1, uint16 nCore2);
static void Sync(struct DockyData *dd);
static void TogglePowerOff(BOOL bActivate);

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

    d(bug("[SetAmiUpdateENVVariable] varpath='%s', progpath='%s' allocated sign 0x%08x\n", varpath, progpath, IExec->FindTask(NULL)->tc_SigAlloc));

    /* turn requesters back on */
    IDOS->SetProcWindow( oldwin );
}

/// DockyObtain
uint32 DockyObtain (struct DockyIFace *Self) {
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

    d(bug("[DockyObtain] allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
    OpenLocaleCatalog(dd, "X1kTemp.catalog");
    d(bug("[DockyObtain] after open locale allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
    dd->nAppID = IApplication->RegisterApplication("X1kTemp",
            REGAPP_URLIdentifier,   "balaban.fr",
            REGAPP_Description,     GetString(dd, MSG_APPLIB_DESCRIPTION),
            REGAPP_NoIcon,          TRUE,
            TAG_DONE);

    d(bug("[RegisterApplication] AppId=%ld allocated sign 0x%08x\n", dd->nAppID, IExec->FindTask(NULL)->tc_SigAlloc));

    return ++Self->Data.RefCount;
}
///
/// DockyRelease
uint32 DockyRelease (struct DockyIFace *Self) {
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

    CloseLocaleCatalog(dd);
    d(bug("[UnregisterApplication] before unreg %ld %08x allocated sign 0x%08x\n", dd->nAppID, IApplication, IExec->FindTask(NULL)->tc_SigAlloc));
    IApplication->UnregisterApplicationA(dd->nAppID, NULL);
    d(bug("[UnregisterApplication] after\n"));

    Self->Data.RefCount--;
    if (!Self->Data.RefCount && (Self->Data.Flags & IFLF_CLONED)) {
        struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
        IExec->DeleteInterface((struct Interface *)Self);
    }
    return Self->Data.RefCount;
}
///
/// DockyClone
struct DockyIFace *DockyClone (struct DockyIFace *Self) {
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    d(bug("DockyClone allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
    struct DockyIFace *docky = NULL;
    docky = (struct DockyIFace *)IExec->MakeInterface(Self->Data.LibBase, dockyTags);
    if (docky) {
        struct DockyData *dd = (struct DockyData *)((uint8 *)docky - docky->Data.NegativeSize);
        docky->Data.Flags |= IFLF_CLONED;

        IUtility->SetMem(dd, 0, sizeof(struct DockyData));

        dd->Base = db;
        dd->shadowpen = dd->textpen = dd->backpen = ~0;
        dd->shadowcolor = dd->textcolor = ~0;
        dd->graphcolor = 0xFF0000;
        dd->graphcolor2 = 0x006400;
        dd->nMBLastWarned = dd->nCPULastWarned = dd->nCore1LastWarned = dd->nCore2LastWarned = 0;

        dd->bSetEnv = FALSE;

        dd->font = GfxLib->DefaultFont;
        dd->size.width = dd->font->tf_XSize * (MAX_STRING_SIZE + 2);
        dd->size.height = dd->font->tf_YSize * 4 + 8;
        
        dd->curIdx = 0;
        dd->maxIdx = min(MAX_RECORD_LENGTH, dd->size.width);
        dd->refreshRate = 50;

        dd->MBWarnTemp = dd->CPUWarnTemp = dd-> Core1WarnTemp = dd->Core2WarnTemp = ~0;
    }
    d(bug("exiting DockyClone allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
    return docky;
}
///

/// ReadDockyPrefs
static void ReadDockyPrefs (struct DockyBase *db, struct DockyData *dd, char *filename) {
    struct DiskObject *icon;
    d(bug("[ReadDockyPrefs\n"));

    dd->Base = db;
    dd->shadowpen = dd->textpen = dd->backpen = ~0;
    dd->shadowcolor = dd->textcolor = dd->backcolor = ~0;
    dd->graphcolor = 0xFF0000;
    dd->graphcolor2 = 0x006400;

    dd->MBWarnTemp = dd->CPUWarnTemp = dd-> Core1WarnTemp = dd->Core2WarnTemp = ~0;
    dd->nMBLastWarned = dd->nCPULastWarned = dd->nCore1LastWarned = dd->nCore2LastWarned = dd->nCriticalLastNotified = 0;
    dd->nWarnTimespan = DEFAULT_TIMESPAN;

    dd->bfDisplay = displayAll;

    dd->font = GfxLib->DefaultFont;

    dd->bSetEnv = dd->bUseFahrenheit = dd->bSyncBeforePowerOff = dd->bNoCriticalCheck = FALSE;
    dd->szWarnCmd[0] = '\0' ;
    dd->szCriticalCmd[0] = '\0';
    IApplication->GetApplicationAttrs(dd->nAppID, APPATTR_Port, &dd->pAppLibPort, TAG_DONE);

    IUtility->Strlcpy(dd->szImageFile, filename, 2048);
    STRPTR pFilePart = (STRPTR)IDOS->FilePart(dd->szImageFile);
    if(pFilePart) *pFilePart = '\0';

    IUtility->Strlcat(dd->szImageFile, "X1kTempWarn", 2048);
    BPTR lock = IDOS->Open(dd->szImageFile, MODE_OLDFILE);
    if(lock)
    {
        if(!IDOS->DevNameFromFH(lock, dd->szImageFile, 2048, DN_FULLPATH))
            dd->szImageFile[0] = '\0';
    }
    else
    {
        dd->szImageFile[0] = '\0';
    }
    if('\0' == dd->szImageFile[0])
    {
        IUtility->Strlcpy(dd->szImageFile, "tbimages:warning", 2048);
    }

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
            font = IDiskfont->OpenDiskFont((struct TextAttr*)&ta);
            if (font) {
                dd->font = font;
                dd->freefont = TRUE;
            }
            else
                IExec->DebugPrintF("[DockyProcess] could not open font\n");
        }
        
        dd->shadowcolor = CFGHex(icon, "SHADOWCOLOR", 0);
        dd->textcolor = CFGHex(icon, "TEXTCOLOR", ~0);
        dd->backcolor = CFGHex(icon, "BACKCOLOR", ~0);
        dd->graphcolor = CFGHex(icon, "GRAPHCOLOR_UP", 0xFF0000);
        dd->graphcolor2 = CFGHex(icon, "GRAPHCOLOR_DOWN", 0x006400);
        dd->refreshRate = CFGInteger(icon, "REFRESH", 50);
        dd->bSetEnv = CFGBoolean(icon, "SETENV");
        dd->bUseFahrenheit = CFGBoolean(icon, "FAHRENHEIT");
        dd->bNoCriticalCheck = !CFGBoolean(icon, "CRITICAL_CHECK");
        dd->bSyncBeforePowerOff = CFGBoolean(icon, "SYNC_POWEROFF");
        dd->MBWarnTemp = CFGInteger(icon, "LOCAL_WARN", ~0);
        dd->CPUWarnTemp = CFGInteger(icon, "CPU_WARN", ~0);
        dd->Core1WarnTemp = CFGInteger(icon, "CORE1_WARN", ~0);
        dd->Core2WarnTemp = CFGInteger(icon, "CORE2_WARN", ~0);
        dd->nWarnTimespan = CFGInteger(icon, "WARN_TIMESPAN", DEFAULT_TIMESPAN);;
        STRPTR szTmp = CFGString(icon, "WARN_CMD", NULL);
        if(szTmp)
            IUtility->Strlcpy(dd->szWarnCmd, szTmp, 2048);
        szTmp = CFGString(icon, "CRITICAL_CMD", NULL);
        if(szTmp)
            IUtility->Strlcpy(dd->szCriticalCmd, szTmp, 2048);
        dd->bfDisplay = CFGBoolean(icon, "HIDE_LOCAL")?0:displayMB;
        dd->bfDisplay |= CFGBoolean(icon, "HIDE_CPU")?0:displayCPU;
        dd->bfDisplay |= CFGBoolean(icon, "HIDE_CORE1")?0:displayCore1;
        dd->bfDisplay |= CFGBoolean(icon, "HIDE_CORE2")?0:displayCore2;

        IIcon->FreeDiskObject(icon);
    }

    dd->size.width = dd->font->tf_XSize * (MAX_STRING_SIZE + 2);
    int nLineNo = (dd->bfDisplay&displayMB) + ((dd->bfDisplay&displayCPU)?1:0) + ((dd->bfDisplay&displayCore1)?1:0) + ((dd->bfDisplay&displayCore2)?1:0);
    dd->size.height = dd->font->tf_YSize * nLineNo + nLineNo * 2;

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
///
/// ReleaseDockyPens
static void ReleaseDockyPens (struct DockyBase *db, struct DockyData *dd) {
    d(bug("ReleaseDockyPens\n"));
    if (dd->scr) {
        struct ColorMap *cm = dd->scr->ViewPort.ColorMap;
        if (dd->shadowpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->shadowpen);
        if (dd->textpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->textpen);
        if (dd->backpen != (uint16)~0) IGraphics->ReleasePen(cm, dd->backpen);
        if (dd->dri) IIntuition->FreeScreenDrawInfo(dd->scr, dd->dri);
        dd->scr = NULL;
        dd->dri = NULL;
        dd->shadowpen = ~0;
        dd->textpen = ~0;
        dd->backpen = ~0;
    }
}
///
/// Obtain DockyPens
static void ObtainDockyPens (struct DockyBase *db, struct DockyData *dd, struct Screen *scr) {
    d(bug("ObtainDockyPens"));
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
        if(~0 != dd->backcolor)
            dd->backpen = IGraphics->ObtainBestPenA(cm,
                RGB8to32(RED(dd->backcolor)),
                RGB8to32(GREEN(dd->backcolor)),
                RGB8to32(BLUE(dd->backcolor)),
                NULL);
        dd->dri = IIntuition->GetScreenDrawInfo(scr);
    }
}
///
/// DockyExpunge
void DockyExpunge (struct DockyIFace *Self) {
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    d(bug("DockyExpunge allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
    if (!Self->Data.RefCount) {
        struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
        struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

        ReleaseDockyPens(db, dd);

        if (dd->freefont) IGraphics->CloseFont(dd->font);
        dd->font = NULL;

        IExec->FreeVec((uint8 *)Self - Self->Data.NegativeSize);
    }
    d(bug("exiting DockyExpunge allocated sign 0x%08x\n", IExec->FindTask(NULL)->tc_SigAlloc));
}
///
/// DockyGet
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

        case DOCKYGET_DockyPrefsChanged:
            // we do not use docky prefs feature
            res = FALSE;
            break;

        default:
            d(bug("DockyGet unhandled msgType 0x%08x\n", msgType));
            res = FALSE;
            break;

    }

    return res;
}
///
/// DockySet
BOOL DockySet (struct DockyIFace *Self, uint32 msgType, uint32 msgData) {
    struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);
    BOOL res = TRUE;

    switch (msgType) {

        case DOCKYSET_FileName:
            {
                char buffer[1024];
                IUtility->Strlcpy(buffer, (STRPTR)msgData, 1024);
                STRPTR pFilePart = (STRPTR)(IDOS->FilePart(buffer));
                *pFilePart = '\0';
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
            d(bug("DockySet unhandled msgType 0x%08x\n", msgType));
            res = FALSE;
            break;

    }

    return res;
}
///

/// DockProcess
BOOL DockyProcess (struct DockyIFace *Self, uint32 turnCount, uint32 *msgType, uint32 *msgData,
    BOOL *anotherTurn)
{
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    struct DockyData *dd = (struct DockyData *)((uint8 *)Self - Self->Data.NegativeSize);

    if(tmp423_read_all_temps(dd))
    {
        d(bug("[DockyProcess] could not read temps\n"));
        return FALSE;
    }

    uint8 nCorIdx = dd->curIdx?dd->curIdx-1:dd->maxIdx-1;

    // check critical temperatures
    if(!dd->bNoCriticalCheck)
        CheckCriticalTemperatures(dd, dd->MBTemp[nCorIdx], dd->Core1Temp[nCorIdx], dd->Core2Temp[nCorIdx]);

    // check warning temperatures
    CheckWarnTemperatures(dd, dd->MBWarnTemp, dd->MBTemp[nCorIdx],  &dd->nMBLastWarned, GetString(dd, MSG_RINGHIO_CASE_LABEL) );
    CheckWarnTemperatures(dd, dd->CPUWarnTemp, dd->CPUTemp[nCorIdx],  &dd->nCPULastWarned, GetString(dd, MSG_RINGHIO_CPU_LABEL) );
    CheckWarnTemperatures(dd, dd->Core1WarnTemp, dd->Core1Temp[nCorIdx],  &dd->nCore1LastWarned, GetString(dd, MSG_RINGHIO_CORE1_LABEL) );
    CheckWarnTemperatures(dd, dd->Core2WarnTemp, dd->Core2Temp[nCorIdx],  &dd->nCore2LastWarned, GetString(dd, MSG_RINGHIO_CORE2_LABEL) );

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

    if(dd->pAppLibPort)
    {
        struct ApplicationCustomMsg *pAppMsg = (struct ApplicationCustomMsg*)IExec->GetMsg(dd->pAppLibPort);
        if(pAppMsg)
        {
            if(APPLIBMT_CustomMsg == pAppMsg->almsg.type)
            {
                if(0 == IUtility->Stricmp(pAppMsg->customMsg, GetString(dd, MSG_RINGHIO_CASE_LABEL)))
                    dd->nMBLastWarned = ~0;
                else if(0 == IUtility->Stricmp(pAppMsg->customMsg, GetString(dd, MSG_RINGHIO_CPU_LABEL)))
                    dd->nCPULastWarned = ~0;
                else if(0 == IUtility->Stricmp(pAppMsg->customMsg, GetString(dd, MSG_RINGHIO_CORE1_LABEL)))
                    dd->nCore1LastWarned = ~0;
                else if(0 == IUtility->Stricmp(pAppMsg->customMsg, GetString(dd, MSG_RINGHIO_CORE2_LABEL)))
                    dd->nCore2LastWarned = ~0;
                else if(0 == IUtility->Stricmp(pAppMsg->customMsg, ABORT_SHUTDOWN))
                    TogglePowerOff(FALSE);
            }
            IExec->ReplyMsg((struct Message*)pAppMsg);
        }
    }

    return TRUE;
}
///

/// DockyRender
static void DockyRender (struct DockyBase *db, struct DockyData *dd) {
    if (dd->rp) {
        TEXT tmp[MAX_STRING_SIZE+2];

        uint16 shadowpen = (dd->shadowpen != (uint16)~0) ? dd->shadowpen : BLOCKPEN;
        uint16 textpen = (dd->textpen != (uint16)~0) ? dd->textpen : TEXTPEN;

        uint8 nIdx = 0, nCorIdx = 0;
        uint16 nTemp = 0;
        struct Pos rgPos[4] = { {dd->font->tf_XSize*1, 2 + dd->font->tf_Baseline},
                                {dd->font->tf_XSize*1, 4 + dd->font->tf_YSize+dd->font->tf_Baseline},
                                {dd->font->tf_XSize*1, 6 + dd->font->tf_YSize * 2 + dd->font->tf_Baseline},
                                {dd->font->tf_XSize*1, 8 + dd->font->tf_YSize * 3 + dd->font->tf_Baseline} };

        struct IBox rgBox[4] = {{0, rgPos[0].y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+3},
                                {0, rgPos[1].y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+2},
                                {0, rgPos[2].y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+3},
                                {0, rgPos[3].y-dd->font->tf_YSize, dd->size.width, dd->font->tf_YSize+2}};
        uint16 nDivider = dd->bUseFahrenheit?212:100;

        if(dd->backpen != (uint16)~0)
        {
            IGraphics->SetAPen(dd->rp, dd->backpen);
            IGraphics->RectFill(dd->rp, 0, 0, dd->size.width, dd->size.height);
        }

        uint8 nLineNo = 0;
        for(nIdx = 0; nIdx < dd->maxIdx; nIdx++)
        {
            nCorIdx = (dd->curIdx+nIdx)%(dd->maxIdx);
            nLineNo = 0;

            if(dd->bfDisplay & displayMB)
            {
                nTemp = dd->MBTemp[nCorIdx] * dd->font->tf_YSize / nDivider;
                if(0 != nTemp)
                    IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, rgPos[nLineNo].y-nTemp,
                                            1, nTemp+3, &rgBox[nLineNo], 0L, &dd->GradSpecGraph, dd->dri);
                nLineNo++;
            }

            if(dd->bfDisplay & displayCPU)
            {
                nTemp = dd->CPUTemp[nCorIdx] * dd->font->tf_YSize / nDivider;
                if(0 != nTemp)
                    IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, rgPos[nLineNo].y-nTemp,
                                            1, nTemp+3, &rgBox[nLineNo], 0L, &dd->GradSpecGraph, dd->dri);
                nLineNo++;
            }

            if(dd->bfDisplay & displayCore1)
            {
                nTemp = dd->Core1Temp[nCorIdx] * dd->font->tf_YSize / nDivider;
                if(0 != nTemp)
                    IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, rgPos[nLineNo].y-nTemp,
                                            1, nTemp+3, &rgBox[nLineNo], 0L, &dd->GradSpecGraph, dd->dri);
                nLineNo++;
            }

            if(dd->bfDisplay & displayCore2)
            {
                nTemp = dd->Core2Temp[nCorIdx] * dd->font->tf_YSize / nDivider;
                if(0 != nTemp)
                    IIntuition->DrawGradient(dd->rp, dd->font->tf_XSize + nIdx, rgPos[nLineNo].y-nTemp,
                                            1, nTemp+3, &rgBox[nLineNo], 0L, &dd->GradSpecGraph, dd->dri);
                nLineNo++;
            }
        }

        TEXT cUnit = dd->bUseFahrenheit?'F':'C';

        nLineNo = 0;
        IGraphics->SetFont(dd->rp, dd->font);
        if(dd->bfDisplay & displayMB)
        {
            IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1,  GetString(dd, MSG_CASE_FORMAT) , dd->MBTemp[nCorIdx], cUnit);
            IGraphics->Move(dd->rp, rgPos[nLineNo].x+1, rgPos[nLineNo].y+1);
            IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            IGraphics->Move(dd->rp, rgPos[nLineNo].x, rgPos[nLineNo].y);
            IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            nLineNo++;
        }

        if(dd->bfDisplay & displayCPU)
        {
            IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1,  GetString(dd, MSG_CPU_FORMAT) , dd->CPUTemp[nCorIdx], cUnit);
            IGraphics->Move(dd->rp, rgPos[nLineNo].x+1, rgPos[nLineNo].y+1);
            IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            IGraphics->Move(dd->rp, rgPos[nLineNo].x, rgPos[nLineNo].y);
            IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            nLineNo++;
        }

        if(dd->bfDisplay & displayCore1)
        {
            IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1,  GetString(dd, MSG_CORE1_FORMAT) , dd->Core1Temp[nCorIdx], cUnit);
            IGraphics->Move(dd->rp, rgPos[nLineNo].x+1, rgPos[nLineNo].y+1);
            IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            IGraphics->Move(dd->rp, rgPos[nLineNo].x, rgPos[nLineNo].y);
            IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            nLineNo++;
        }

        if(dd->bfDisplay & displayCore2)
        {
            IUtility->SNPrintf(tmp, MAX_STRING_SIZE+1,  GetString(dd, MSG_CORE2_FORMAT) , dd->Core2Temp[nCorIdx], cUnit);
            IGraphics->Move(dd->rp, rgPos[nLineNo].x+1, rgPos[nLineNo].y+1);
            IGraphics->SetABPenDrMd(dd->rp, shadowpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            IGraphics->Move(dd->rp, rgPos[nLineNo].x, rgPos[nLineNo].y);
            IGraphics->SetABPenDrMd(dd->rp, textpen, 0, JAM1);
            IGraphics->Text(dd->rp, tmp, strlen(tmp));
            nLineNo++;
        }
    }
}
///
/// CheckWarnTemperatures
static void CheckWarnTemperatures(struct DockyData *dd, uint16 nTempThreshold, uint16 nTempValue, uint32* pLastNotified, CONST_STRPTR szSensor)
{
    if(((uint16)~0 != nTempThreshold) && (nTempValue >= nTempThreshold))
    {
        struct DockyBase * db = dd->Base;
#ifndef NDEBUG
        IExec->DebugPrintF("[CheckWarnTemperatures] %s temperature exceeds %ld\n", szSensor, nTempThreshold);
#endif
        struct DateStamp dsNow = {0};
        IDOS->DateStamp(&dsNow);        
        uint32 nNow = IDOS->DateStampToSeconds(&dsNow);
#ifndef NDEBUG
        IExec->DebugPrintF("[CheckWarnTemperatures] now=%ld, old=%ld, timespan=%d\n", nNow, (uint32)*pLastNotified, dd->nWarnTimespan);
#endif
        if((~0 != *pLastNotified) && ((nNow - *pLastNotified) >= (uint32)dd->nWarnTimespan) && ((nNow - dd->nCriticalLastNotified) >= CRITICAL_TIMESPAN))
        {
#ifndef NDEBUG
            IExec->DebugPrintF("[CheckWarnTemperatures] need notification\n");
#endif
            if(0 != *dd->szWarnCmd)
            {
                IDOS->System(dd->szWarnCmd, NULL);
            }
            else
            {
                TEXT tmp[129];
                IUtility->SNPrintf(tmp, sizeof(tmp)/sizeof(TEXT),
                                    GetString(dd, MSG_RINGHIO_BODY_FORMAT) ,
                                    szSensor, nTempThreshold, dd->bUseFahrenheit?'F':'C');
                uint32 result = IApplication->Notify(dd->nAppID,
                                    APPNOTIFY_Title,         GetString(dd, MSG_RINGHIO_TITLE),
                                    APPNOTIFY_Update,        FALSE,
                                    APPNOTIFY_Pri,           0,
                                    APPNOTIFY_PubScreenName, "FRONT",
                                    APPNOTIFY_ImageFile,     dd->szImageFile,
                                    APPNOTIFY_CloseOnDC,     TRUE,
                                    APPNOTIFY_BackMsg,       szSensor,
                                    APPNOTIFY_Text,          tmp,
                                    TAG_DONE);
#ifndef NDEBUG
                IExec->DebugPrintF("[CheckWarnTemperatures] notify returned %ld\n", result);
#endif
            }
            *pLastNotified = nNow;
        }
    }
}
///

/// CheckCriticalTemperatures
static void CheckCriticalTemperatures(struct DockyData *dd, uint16 nMB, uint16 nCore1, uint16 nCore2)
{
    if((MB_CRITICAL <= nMB) || (CPU_CRITICAL <= nCore1) || (CPU_CRITICAL <= nCore2))
    {
        struct DockyBase * db = dd->Base;

#ifndef NDEBUG
        IExec->DebugPrintF("[CheckCriticalTemperatures] %ld <= %ld || %ld <= %ld || %ld <= %ld\n", MB_CRITICAL, nMB, CPU_CRITICAL, nCore1, CPU_CRITICAL, nCore2);
#endif
        struct DateStamp dsNow = {0};
        IDOS->DateStamp(&dsNow);
        uint32 nNow = IDOS->DateStampToSeconds(&dsNow);

        if((~0 != dd->nCriticalLastNotified) && ((nNow - dd->nCriticalLastNotified) >= CRITICAL_TIMESPAN))
        {
            if(0 != *dd->szCriticalCmd)
            {
                IDOS->System(dd->szCriticalCmd, NULL);
            }
            else
            {
                uint32 result = IApplication->Notify(dd->nAppID,
                                    APPNOTIFY_Title,         GetString(dd, MSG_RINGHIO_TITLE),
                                    APPNOTIFY_Update,        FALSE,
                                    APPNOTIFY_Pri,           0,
                                    APPNOTIFY_PubScreenName, "FRONT",
                                    APPNOTIFY_ImageFile,     dd->szImageFile,
                                    APPNOTIFY_CloseOnDC,     TRUE,
                                    APPNOTIFY_BackMsg,       ABORT_SHUTDOWN,
                                    APPNOTIFY_Text,          GetString(dd, MSG_RINGHIO_BODY_CRITICAL),
                                    TAG_DONE);
    #ifndef NDEBUG
                IExec->DebugPrintF("[CheckCriticalTemperatures] notify returned %ld\n", result);
    #endif
                if(dd->bSyncBeforePowerOff) Sync(dd);
                TogglePowerOff(TRUE);
            }
            dd->nCriticalLastNotified = nNow;
        }
    }
}
///

/// Sync
static void Sync(struct DockyData *dd)
{
    const uint32 flags = LDF_READ|LDF_DEVICES;
    struct DeviceNode *dn;

    struct DockyBase * db = dd->Base;

    dn = (struct DeviceNode*)IDOS->LockDosList(flags);
    while ((dn = (struct DeviceNode *)IDOS->NextDosEntry((struct DosList *)dn, flags)))
    {
        if (dn->dn_Port != NULL)
        {
            /* Skip RAM Disk */
            if (!IUtility->Strnicmp((char *)BADDR(dn->dn_Name)+1, "RAM", 3))
                continue;

            /* Cause filesystem to flush all pending writes. */
            /* Inhibit should do this as well but might be useful for old
               filesystems that don't support inhibit? */
            IDOS->FlushVolumePort(dn->dn_Port); /* Requires dos.library V53.90 */

            /* Inhibit filesystem. */
            IDOS->InhibitPort(dn->dn_Port, TRUE); /* Requires dos.library V53.88 */
        }
    }
    IDOS->UnLockDosList(flags);
}
///

/// X1000 PowerOff
static void TogglePowerOff(BOOL bActivate)
{
    *((uint8 *)(0xF5000007)) = bActivate?1:0;
}
///
