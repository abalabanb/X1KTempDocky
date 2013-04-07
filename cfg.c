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

#include "cfg.h"

#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/utility.h>
#include <stdlib.h>

#define stricmp IUtility->Stricmp

int CFGBoolean (struct DiskObject *icon, char *attr) {
	char *val;
	val = IIcon->FindToolType(icon->do_ToolTypes, attr);
	if (!val) return FALSE;
	if (!stricmp(val, "no") || !stricmp(val, "false") || !stricmp(val, "off")) return FALSE;
	return TRUE;
}

int32 CFGInteger (struct DiskObject *icon, char *attr, int32 def) {
	char *val;
	val = IIcon->FindToolType(icon->do_ToolTypes, attr);
	if (val) IDOS->StrToLong(val, &def);
	return def;
}

int32 CFGHex (struct DiskObject *icon, char *attr, int32 def) {
	char *val;
	val = IIcon->FindToolType(icon->do_ToolTypes, attr);
	if (val) IDOS->HexToLong(val, &def);
	return def;
}

char *CFGString (struct DiskObject *icon, char *attr, char *def) {
	char *val;
	val = IIcon->FindToolType(icon->do_ToolTypes, attr);
	return val ? val : def;
}
