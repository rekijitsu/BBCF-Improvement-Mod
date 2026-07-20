#include "hooks_palette.h"

#include "HookManager.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/gamestates.h"
#include "Audio/MusicManager.h"

DWORD GetCharObjPointersJmpBackAddr = 0;
void __declspec(naked)GetCharObjPointers()
{
	static char* addr = nullptr;

	LOG_ASM(2, "GetCharObjPointers\n");

	__asm
	{
		pushad
		add eax, 25E8h
		mov addr, eax
	}

	g_interfaces.player1.SetCharDataPtr(addr);
	addr += 0x4;
	g_interfaces.player2.SetCharDataPtr(addr);

	__asm
	{
		popad
		mov[eax + edi * 4 + 25E8h], esi
		jmp[GetCharObjPointersJmpBackAddr]
	}
}

DWORD ForceBloomOnJmpBackAddr = 0;
int restoredForceBloomOffAddr = 0;
void __declspec(naked)ForceBloomOn()
{
	static CharData* pCharObj = nullptr;
	static CharPaletteHandle* pCharHandle = nullptr;

	LOG_ASM(7, "ForceBloomOn\n");

	__asm
	{
		mov [pCharObj], edi
		pushad
	}

	if (pCharObj == g_interfaces.player1.GetData())
	{
		pCharHandle = &g_interfaces.player1.GetPalHandle();
	}
	else
	{
		pCharHandle = &g_interfaces.player2.GetPalHandle();
	}

	if (pCharHandle->IsCurrentPalWithBloom())
	{
		__asm jmp TURN_BLOOM_ON
	}

	__asm
	{
		popad
		jmp[restoredForceBloomOffAddr]
TURN_BLOOM_ON:
		popad
		jmp[ForceBloomOnJmpBackAddr]
	}
}

DWORD GetIsP1CPUJmpBackAddr = 0;
void __declspec(naked)GetIsP1CPU()
{
	LOG_ASM(2, "GetIsP1CPU\n");

	__asm
	{
		mov[eax + 1688h], edi
		mov g_gameVals.isP1CPU, edi;
		jmp[GetIsP1CPUJmpBackAddr]
	}
}

DWORD GetGameStateCharacterSelectJmpBackAddr = 0;
void __declspec(naked)GetGameStateCharacterSelect()
{
	LOG_ASM(2, "GetGameStateCharacterSelect\n");

	// The game is about to enter Character Select. Clean up the mod's scratch BGM
	// (stop Bank[13], null scratch slot 0x3E) and present the selectable anchor
	// track. Slot 0x3F already holds the anchor (the mod never overwrote it), so
	// the game's exit validation sees a selectable track -> no red debug screen.
	__asm
	{
		pushad
	}
	GetMusicManager().UnloadCustomBgm();
	__asm
	{
		popad
		mov dword ptr[ebx + 10Ch], 6
		jmp[GetGameStateCharacterSelectJmpBackAddr]
	}
}

DWORD OnEnterCharSelectFuncJmpBackAddr = 0;
void __declspec(naked) OnEnterCharSelectFuncEntry()
{
	// Hook on the ENTRY of the "enter character select" function (the one that
	// later does `mov [ebx+10Ch],6`). Force-clear the mod's BGM footprint HERE —
	// before the function's XACT-init / BGM loader run — leaving Bank[13] EMPTY so
	// the game rebuilds it natively. Reloading the anchor through our direct-COM
	// pipeline (the old RestoreAnchorForSceneExit) re-created foreign bank state
	// that the game's Character Select validation still rejected (red debug
	// screen) even though the presented track id was a valid, selectable one.
	__asm
	{
		pushad
	}
	GetMusicManager().ClearBgmForSceneExit();
	__asm
	{
		popad
		// Re-execute the overwritten 9-byte prologue we replaced with the jump.
		push ebp
		mov ebp, esp
		sub esp, 244h
		jmp[OnEnterCharSelectFuncJmpBackAddr]
	}
}

DWORD GetPalBaseAddressesJmpBackAddr = 0;
void __declspec(naked) GetPalBaseAddresses()
{
	static int counter = 0;
	static char* palPointer = 0;

	LOG_ASM(2, "GetPalBaseAddresses\n");

	__asm
	{
		pushad

		mov palPointer, eax
	}

	if (counter == 0)
	{
		g_interfaces.player1.GetPalHandle().SetPointerBasePal(palPointer);
	}
	else if (counter == 1)
	{
		g_interfaces.player2.GetPalHandle().SetPointerBasePal(palPointer);
	}
	else
	{
		counter = -1;
	}

	counter++;

	__asm
	{
		popad

		mov[ecx + 830h], eax
		jmp[GetPalBaseAddressesJmpBackAddr]
	}
}

DWORD GetPaletteIndexPointersJmpBackAddr = 0;
void __declspec(naked) GetPaletteIndexPointers()
{
	static int* pPalIndex = nullptr;

	LOG_ASM(2, "GetPaletteIndexPointers\n");

	__asm
	{
		pushad
		add esi, 8h
		mov pPalIndex, esi
	}

	LOG_ASM(2, "\t- P1 palIndex: 0x%p\n", pPalIndex);
	g_interfaces.player1.GetPalHandle().SetPointerPalIndex(pPalIndex);

	__asm
	{
		add esi, 20h
		mov pPalIndex, esi
	}

	LOG_ASM(2, "\t- P2 palIndex: 0x%p\n", pPalIndex);
	g_interfaces.player2.GetPalHandle().SetPointerPalIndex(pPalIndex);

	__asm
	{
		popad
		lea edi, [edx + 24D8h]
		jmp[GetPaletteIndexPointersJmpBackAddr]
	}
}
// DWORD P1InputJmpBackAddr = 0;
//void __declspec(naked)P1Input()
//{
//	LOG_ASM(2, "P1Input\n");
//	
//	static char* addr = nullptr;
//	static int playerNr = -1;
//	__asm
//	{
//		movzx edi, ax
//		mov[esi], di
//		mov[addr], esi
		
//	}
//	g_gameVals.P1InputJumpBackAdress = P1InputJmpBackAddr;

//	__asm
//	{
//		jmp[P1InputJmpBackAddr]
//	}
	
//}]

bool placeHooks_palette()
{
	GetCharObjPointersJmpBackAddr = HookManager::SetHook("GetCharObjPointers", "\x89\xB4\x00\x00\x00\x00\x00\x8B\x45",
		"xx?????xx", 7, GetCharObjPointers);

	GetPalBaseAddressesJmpBackAddr = HookManager::SetHook("GetPalBaseAddresses", "\x89\x81\x30\x08\x00\x00\x8b\xc8\xe8\x00\x00\x00\x00\x5f",
		"xxxxxxxxx????x", 6, GetPalBaseAddresses);

	GetPaletteIndexPointersJmpBackAddr = HookManager::SetHook("GetPaletteIndexPointers", "\x8d\xba\xd8\x24\x00\x00\xb9\x00\x00\x00\x00",
		"xxxxxxx????", 6, GetPaletteIndexPointers);

	GetGameStateCharacterSelectJmpBackAddr = HookManager::SetHook("GetGameStateCharacterSelect", "\xc7\x83\x0c\x01\x00\x00\x06\x00\x00\x00\xe8",
		"xxxxxxxxxxx", 10, GetGameStateCharacterSelect);

	// Entry of the "enter character select" function (prologue + security cookie +
	// `mov [ebp-0x244],ecx`), so we can restore the anchor track before its early
	// XACT-init call. Overwrite the 9-byte prologue; the hook re-executes it.
	OnEnterCharSelectFuncJmpBackAddr = HookManager::SetHook("OnEnterCharSelectFuncEntry",
		"\x55\x8b\xec\x81\xec\x44\x02\x00\x00\xa1\x00\x00\x00\x00\x33\xc5\x89\x45\xfc\x53\x56\x57\x89\x8d\xbc\xfd\xff\xff\xe8",
		"xxxxxxxxxx????xxxxxxxxxxxxxxx", 9, OnEnterCharSelectFuncEntry);

	ForceBloomOnJmpBackAddr = HookManager::SetHook("ForceBloomOn", "\x83\xfe\x15\x75", "xxxx", 5, ForceBloomOn, false);
	restoredForceBloomOffAddr = ForceBloomOnJmpBackAddr + HookManager::GetBytesFromAddr("ForceBloomOn", 4, 1);
	HookManager::ActivateHook("ForceBloomOn");

	GetIsP1CPUJmpBackAddr = HookManager::SetHook("GetIsP1CPU", "\x89\xB8\x00\x00\x00\x00\x8B\x83",
		"xx????xx", 6, GetIsP1CPU);


//	P1InputJmpBackAddr = HookManager::SetHook("P1Input", "\x0F\xB7\x00\x66\x89\x00\xE9\x00\x00\x00\x00\x53",
//		"xx?xx?x????x", 6, P1Input);

	return true;
}