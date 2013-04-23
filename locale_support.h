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

#ifndef LOCALE_SUPPORT_H
#define LOCALE_SUPPORT_H

#define CATCOMP_NUMBERS
#include "locale.h"

#ifndef STR_ID
#define STR_ID(x) ((char *)(x))
#endif

struct LocaleInfo {
    struct Catalog     *Catalog;
	uint32             CodeSet;
};


void OpenLocaleCatalog (struct DockyData* dd, const char *catalog);
void CloseLocaleCatalog (struct DockyData* dd);
const char *GetString (struct DockyData* dd, LONG stringNum);
#ifdef HELPERS
void TranslateMenus (struct DockyData* dd,  struct NewMenu *nm);
void TranslateLabelArray (struct DockyData* dd, const char **array);
#endif

#endif
