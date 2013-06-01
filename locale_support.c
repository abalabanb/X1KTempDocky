/*
** X1kTemp.docky
** (c) 2013 Alexandre Balaban <alexandre -(@)- balaban -(.)- fr>
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
#define CATCOMP_BLOCK
#include "locale_support.h"

void OpenLocaleCatalog (struct DockyData* dd, const char *catalog) {
    struct DockyBase* db = dd->Base;

	LocaleLib = (APTR)IExec->OpenLibrary("locale.library", 0);
	ILocale = (APTR)IExec->GetInterface(LocaleLib, "main", 1, NULL);
	if (ILocale) {
		dd->Catalog = ILocale->OpenCatalog(NULL, (char *)catalog,
			OC_BuiltInLanguage,	 "english",
			TAG_END);
	}
	dd->CodeSet = dd->Catalog ? dd->Catalog->cat_CodeSet : 4;

#ifndef NDEBUG
    IExec->DebugPrintF("[OpenLocaleCatalog] ILocale='%08x', ddCatalog='%08x'\n", ILocale, dd->Catalog);
#endif
}

void CloseLocaleCatalog (struct DockyData* dd) {
	struct DockyBase* db = dd->Base;

    if (ILocale) {
		ILocale->CloseCatalog(dd->Catalog);
		IExec->DropInterface((APTR)ILocale);
	}
	IExec->CloseLibrary((APTR)LocaleLib);
}

const char *GetString (struct DockyData* dd, LONG stringNum) {
	LONG         *l = NULL;
	UWORD        *w = NULL;
	CONST_STRPTR  builtIn = NULL;
	struct DockyBase* db = dd->Base;


	l = (LONG *)CatCompBlock;

	while (*l != stringNum)
	{
		w = (UWORD *)((ULONG)l + 4);
		l = (LONG *)((ULONG)l + (ULONG)*w + 6);
	}
	builtIn = (CONST_STRPTR)((ULONG)l + 6);

	if (ILocale) {
		return ILocale->GetCatalogStr(dd->Catalog, stringNum, (char *)builtIn);
	}
    return builtIn;
}

#ifdef HELPERS
void TranslateMenus (struct DockyData* dd, struct NewMenu *nm) {
	while (nm->nm_Type != NM_END) {
		if (nm->nm_Label && nm->nm_Label != NM_BARLABEL)
			nm->nm_Label = (char *)GetString(dd, (LONG)nm->nm_Label);
		nm++;
	}
}

void TranslateLabelArray (struct DockyData* dd, const char **array) {
	while (*array != STR_ID(-1)) {
		*array = GetString(dd, (LONG)*array);
		array++;
	}
	*array = NULL;
}
#endif

