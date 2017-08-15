#include "opcodes.h"
#include <math.h>

struct stSAMP *g_SAMP;

BOOL InitOpcodes()
{
	return
			CLEO_RegisterOpcode(0x0C36, &op0C36) &&
			CLEO_RegisterOpcode(0x0C37, &op0C37) &&
			setupTextdraws();
}

struct SPLHXTEXTDRAW pltextdraws[PLTDCOUNT];
SGAMEDATA gamedata;

int isTextdrawValid(SPLHXTEXTDRAW *hxtd)
{
	if (hxtd->iHandle == INVALID_TEXTDRAW || !g_SAMP->pPools->pTextdraw->iIsListed[hxtd->iHandle]) {
		return 0;
	}
	if (g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle]->fX == hxtd->fTargetX &&
			g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle]->fY == hxtd->fTargetY) {
		return 1;
	}
	if (g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle]->fX == hxtd->fX &&
			g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle]->fY == hxtd->fY) {
		hxtd->handler(hxtd, g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle], TDHANDLER_ATTACH);
		return 1;
	}
	return 0;
}

void setupTD(int tdidx, unsigned int x, unsigned int y, unsigned int targetX, unsigned int targetY, TDHANDLER handler)
{
	pltextdraws[tdidx].iHandle = INVALID_TEXTDRAW;
	pltextdraws[tdidx].handler = handler;
	memcpy(&(pltextdraws[tdidx].fX), &x, 4);
	memcpy(&(pltextdraws[tdidx].fY), &y, 4);
	memcpy(&(pltextdraws[tdidx].fTargetX), &targetX, 4);
	memcpy(&(pltextdraws[tdidx].fTargetY), &targetY, 4);
	if (targetX == 0) {
		pltextdraws[tdidx].fTargetX = 2000.0f;
	}
	if (targetY == 0) {
		pltextdraws[tdidx].fTargetY = 2000.0f + 1.0f * (float) tdidx;
	}
}

DWORD hookedcall = NULL;
DWORD *samp_21A0B4 = NULL;
DWORD samp_21A0B4_val;

void __cdecl update_textdraws()
{
	DWORD samp_21A0B4_val = *samp_21A0B4;

#if DOTRACE
	if (hDbgFile == NULL) {
		hDbgFile = CreateFileA("tddbg.txt", FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		SetFilePointer(hDbgFile, 0, NULL, FILE_END);
	}
#endif

	gamedata.carspeed = (int) (14.5f * sqrt(gamedata.carspeedx * gamedata.carspeedx + gamedata.carspeedy * gamedata.carspeedy + gamedata.carspeedz * gamedata.carspeedz) / 7.5f);
	if (gamedata.carhp == 999) gamedata.carhp = 1000; // adjust anticheat hp

	TRACE("updating toupdate\n");
	int tdstoupdate[PLTDCOUNT];
	int tdstoupdatecount = 0;
	for (int i = 0; i < PLTDCOUNT; i++) {
		if (pltextdraws[i].handler != NULL && !isTextdrawValid(&pltextdraws[i])) {
			tdstoupdate[tdstoupdatecount++] = i;
			pltextdraws[i].iHandle = INVALID_TEXTDRAW;
		}
	}

	TRACE("updating handles\n");
	for (int i = 0; i < tdstoupdatecount; i++) {
		SPLHXTEXTDRAW *hxtd = &pltextdraws[tdstoupdate[i]];
		for (int j = 0; j < SAMP_MAX_TEXTDRAWS; j++) {
			if (!g_SAMP->pPools->pTextdraw->iIsListed[j]) {
				continue;
			}
			if (g_SAMP->pPools->pTextdraw->textdraw[j]->fX == hxtd->fX &&
					g_SAMP->pPools->pTextdraw->textdraw[j]->fY == hxtd->fY) {
				TRACE1("assigned a handle to a textdraw %d\n", tdstoupdate[i]);
				hxtd->iHandle = j;
				hxtd->handler(hxtd, g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle], TDHANDLER_ATTACH);
				break;
			}
		}
	}

	TRACE("invoking handles\n");
	for (int i = 0; i < PLTDCOUNT; i++) {
		SPLHXTEXTDRAW *hxtd = &pltextdraws[i];
		if (hxtd->iHandle != INVALID_TEXTDRAW && hxtd->handler != NULL) {
			TRACE1("textdraw with a handle %d\n", i);
			hxtd->handler(hxtd, g_SAMP->pPools->pTextdraw->textdraw[hxtd->iHandle], TDHANDLER_UPDATE);
		}
	}
}

void __declspec(naked) hookstuff()
{
	_asm {
		push ebx
		push ecx
		push edx
		push esi
		push edi
		push ebp
	}

	update_textdraws();

	_asm {
		xor eax, eax
		pop ebp
		pop edi
		pop esi
		pop edx
		pop ecx
		pop ebx
		mov eax, samp_21A0B4_val
		ret
	}
}

OpcodeResult WINAPI op0C37(CScriptThread *thread)
{
	gamedata.carhp = CLEO_GetIntOpcodeParam(thread);
	gamedata.carheading = CLEO_GetIntOpcodeParam(thread);
	gamedata.carspeedx = CLEO_GetFloatOpcodeParam(thread);
	gamedata.carspeedy = CLEO_GetFloatOpcodeParam(thread);
	gamedata.carspeedz = CLEO_GetFloatOpcodeParam(thread);
	gamedata.altitude = CLEO_GetIntOpcodeParam(thread);

	if (g_SAMP == NULL) {
		g_SAMP = getSamp();
		if (g_SAMP == NULL) {
			goto exitzero;
		}
	}

	if (g_SAMP->pPools == NULL ||
			g_SAMP->pPools->pTextdraw == NULL) {
		goto exitzero;
	}

	if (g_SAMP->ulPort != 7777 || strcmp("142.44.161.46", g_SAMP->szIP) != 0) {
		goto exitzero;
	}

	if (hookedcall) {
		goto exitzero;
	}

	HMODULE samp_dll = GetModuleHandle("samp.dll");
	DWORD mem = (DWORD)samp_dll + 0x1AD40;
	DWORD *sub = (DWORD*)(mem);
	hookedcall = *sub;
	hookedcall += mem + 5;
	samp_21A0B4 = (DWORD*)((DWORD)samp_dll + 0x21A0B4);

	CLEO_SetIntOpcodeParam(thread, mem);
	CLEO_SetIntOpcodeParam(thread, ((DWORD) &hookstuff) - mem + 1 - 4 - 2);
	return OR_CONTINUE;
exitzero:
	CLEO_SetIntOpcodeParam(thread, 0);
	CLEO_SetIntOpcodeParam(thread, 0);
	return OR_CONTINUE;
}