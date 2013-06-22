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

#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <exec/types.h>
#include <stdarg.h>

#define LIBNAME "X1kTemp.docky"

/* Version Tag */

#include "X1kTemp.docky_rev.h"
#include "readtemp.h"

CONST UBYTE USED verstag[] = VERSTAG;

struct Library *DOSLib;
struct Library *UtilityLib;
struct Library *IconLib;

struct DOSIFace *IDOS;
struct UtilityIFace *IUtility;
struct IconIFace *IIcon;

/*
 * The system (and compiler) rely on a symbol named _start which marks
 * the beginning of execution of an ELF file. To prevent others from 
 * executing this library, and to keep the compiler/linker happy, we
 * define an empty _start symbol here.
 *
 * On the classic system (pre-AmigaOS4) this was usually done by
 * moveq #0,d0
 * rts
 *
 */
int32 _start(void);

int32 _start(void)
{
    /* If you feel like it, open DOS and print something to the user */
    return 100;
}


/* Open the library */
STATIC struct Library *libOpen(struct LibraryManagerInterface *Self, ULONG version)
{
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase; 

    if (version > VERSION)
    {
        return NULL;
    }

    /* Add any specific open code here 
       Return 0 before incrementing OpenCnt to fail opening */


    /* Add up the open count */
    db->libNode.lib_OpenCnt++;
    return &db->libNode;
}


/* Close the library */
STATIC APTR libClose(struct LibraryManagerInterface *Self)
{
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    /* Make sure to undo what open did */


    /* Make the close count */
    db->libNode.lib_OpenCnt--;

    return 0;
}

int openLibs (struct DockyBase *db);
void closeLibs (struct DockyBase *db);

/* Expunge the library */
STATIC BPTR libExpunge(struct LibraryManagerInterface *Self)
{
    /* If your library cannot be expunged, return 0 */
    BPTR result = 0;
    struct DockyBase *db = (struct DockyBase *)Self->Data.LibBase;
    if (db->libNode.lib_OpenCnt == 0)
    {
	     result = db->segList;
        /* Undo what the init code did */

        smbus_shutdown(db);

		closeLibs(db);

        IExec->Remove((struct Node *)db);
        IExec->DeleteLibrary((struct Library *)db);
    }
    else
    {
        result = 0;
        db->libNode.lib_Flags |= LIBF_DELEXP;
    }
    return result;
}

/* The ROMTAG Init Function */
STATIC struct Library *libInit(struct DockyBase *db, BPTR seglist, struct Interface *exec)
{
	db->libNode.lib_Node.ln_Type = NT_LIBRARY;
	db->libNode.lib_Node.ln_Pri  = 0;
	db->libNode.lib_Node.ln_Name = LIBNAME;
	db->libNode.lib_Flags        = LIBF_SUMUSED|LIBF_CHANGED;
	db->libNode.lib_Version      = VERSION;
	db->libNode.lib_Revision     = REVISION;
	db->libNode.lib_IdString     = VSTRING;

	db->segList = seglist;
	IExec = (struct ExecIFace *)exec;

	if (openLibs(db)) {
        ULONG lMachine = MACHINETYPE_UNKNOWN;
        IExpansion->GetMachineInfoTags(GMIT_Machine,&lMachine,TAG_END);
        if (MACHINETYPE_X1000 == lMachine)
        {
            if (smbus_startup(db))
        		return &db->libNode;
        }
        else
        {
            IDOS->TimedDosRequesterTags(TDR_TitleString,    "X1KTemp - Error",
                                        TDR_FormatString,   "X1KTemp can only operate on AmigaOne X1000.",
                                        TDR_GadgetString,   "Exit",
                                        TDR_ImageType,      TDRIMAGE_ERROR,
                                        TAG_DONE);
        }
	}
	closeLibs(db);
	return NULL;
}

int openLibs (struct DockyBase *db) {
	DOSLib = IExec->OpenLibrary("dos.library", 52);
	if (!DOSLib) return FALSE;
	IDOS = (struct DOSIFace *)IExec->GetInterface(DOSLib, "main", 1, NULL);
	if (!IDOS) return FALSE;
	UtilityLib = IExec->OpenLibrary("utility.library", 52);
	if (!UtilityLib) return FALSE;
	IUtility = (struct UtilityIFace *)IExec->GetInterface(UtilityLib, "main", 1, NULL);
	if (!IUtility) return FALSE;
	IconLib = IExec->OpenLibrary("icon.library", 52);
	if (!IconLib) return FALSE;
	IIcon = (struct IconIFace *)IExec->GetInterface(IconLib, "main", 1, NULL);
	if (!IIcon) return FALSE;
	IntuitionLib = IExec->OpenLibrary("intuition.library", 52);
	if (!IntuitionLib) return FALSE;
	IIntuition = (struct IntuitionIFace *)IExec->GetInterface(IntuitionLib, "main", 1, NULL);
	if (!IIntuition) return FALSE;
	GfxLib = (struct GfxBase *)IExec->OpenLibrary("graphics.library", 52);
	if (!GfxLib) return FALSE;
	IGraphics = (struct GraphicsIFace *)IExec->GetInterface((struct Library *)GfxLib, "main",
		1, NULL);
	if (!IGraphics) return FALSE;
	DiskfontLib = IExec->OpenLibrary("diskfont.library", 52);
	if (!DiskfontLib) return FALSE;
	IDiskfont = (struct DiskfontIFace *)IExec->GetInterface(DiskfontLib, "main", 1, NULL);
	if (!IDiskfont) return FALSE;
	ApplicationLib = IExec->OpenLibrary("application.library", 53);
	if (!ApplicationLib) return FALSE;
	IApplication = (struct ApplicationIFace *)IExec->GetInterface(ApplicationLib, "application", 1, NULL);
	if (!IApplication) return FALSE;
	ExpansionLib = IExec->OpenLibrary("expansion.library", 53);
    if (!ExpansionLib) return FALSE;
    IExpansion = (struct ExpansionIFace*)IExec->GetInterface(ExpansionLib, "main", 1, NULL);
    if (!IExpansion) return FALSE;
	return TRUE;
}

void closeLibs (struct DockyBase *db) {
	if(IDiskfont) IExec->DropInterface((struct Interface *)IDiskfont);
	if(DiskfontLib) IExec->CloseLibrary(DiskfontLib);
	if(IGraphics) IExec->DropInterface((struct Interface *)IGraphics);
	if(GfxLib) IExec->CloseLibrary((struct Library *)GfxLib);
	if(IIntuition) IExec->DropInterface((struct Interface *)IIntuition);
	if(IntuitionLib) IExec->CloseLibrary(IntuitionLib);
	if(IIcon) IExec->DropInterface((struct Interface *)IIcon);
	if(IconLib) IExec->CloseLibrary(IconLib);
	if(IUtility) IExec->DropInterface((struct Interface *)IUtility);
	if(UtilityLib) IExec->CloseLibrary(UtilityLib);
	if(IDOS) IExec->DropInterface((struct Interface *)IDOS);
	if(DOSLib) IExec->CloseLibrary(DOSLib);
	if(IApplication) IExec->DropInterface((struct Interface *)IApplication);
	if(ApplicationLib) IExec->CloseLibrary(ApplicationLib);
    if(IExpansion) IExec->DropInterface((struct Interface *)IExpansion);
    if(ExpansionLib) IExec->CloseLibrary(ExpansionLib);
}

/* ------------------- Manager Interface ------------------------ */
/* These are generic. Replace if you need more fancy stuff */
STATIC LONG _manager_Obtain(struct LibraryManagerInterface *Self)
{
    return Self->Data.RefCount++;
}

STATIC ULONG _manager_Release(struct LibraryManagerInterface *Self)
{
    return Self->Data.RefCount--;
}

/* Manager interface vectors */
STATIC CONST APTR lib_manager_vectors[] =
{
    (APTR)_manager_Obtain,
    (APTR)_manager_Release,
    NULL,
    NULL,
    (APTR)libOpen,
    (APTR)libClose,
    (APTR)libExpunge,
    NULL,
    (APTR)-1
};

/* "__library" interface tag list */
STATIC CONST struct TagItem lib_managerTags[] =
{
    { MIT_Name,        (Tag)"__library"         },
    { MIT_VectorTable, (Tag)lib_manager_vectors },
    { MIT_Version,     1                        },
    { TAG_END,         0                        }
};

/* ------------------- Library Interface(s) ------------------------ */

STATIC CONST APTR docky_vectors[] =
{
    (APTR)DockyObtain,
    (APTR)DockyRelease,
    (APTR)DockyExpunge,
    (APTR)DockyClone,
    (APTR)DockyGet,
	(APTR)DockySet,
	(APTR)DockyProcess,
    (APTR)-1
};

/* Uncomment this line (and see below) if your library has a 68k jump table */
//extern APTR VecTable68K[];

CONST struct TagItem dockyTags[] =
{
    { MIT_Name,        (Tag)"docky"             },
    { MIT_VectorTable, (Tag)docky_vectors       },
	{ MIT_DataSize,    sizeof(struct DockyData) },
	{ MIT_Flags,       IFLF_PRIVATE             },
    { MIT_Version,     1                        },
    { TAG_END,         0                        }
};

STATIC CONST CONST_APTR libInterfaces[] =
{
    lib_managerTags,
    dockyTags,
    NULL
};

STATIC CONST struct TagItem libCreateTags[] =
{
    { CLT_DataSize,		sizeof(struct DockyBase) },
    { CLT_InitFunc,		(Tag)libInit             },
    { CLT_Interfaces,	(Tag)libInterfaces       },
    /* Uncomment the following line if you have a 68k jump table */
    //{ CLT_Vector68K,	(Tag)VecTable68K         },
    { TAG_END,			0                        }
};


/* ------------------- ROM Tag ------------------------ */
CONST struct Resident USED lib_res =
{
    RTC_MATCHWORD,
    (struct Resident *)&lib_res,
    (APTR)(&lib_res + 1),
    RTF_NATIVE|RTF_AUTOINIT, /* Add RTF_COLDSTART if you want to be resident */
    VERSION,
    NT_LIBRARY, /* Make this NT_DEVICE if needed */
    0, /* PRI, usually not needed unless you're resident */
    LIBNAME,
    VSTRING,
    (APTR)libCreateTags
};
