/*
	C-Dogs SDL
	A port of the legendary (and fun) action/arcade cdogs.

	Copyright (c) 2013-2015, 2018, 2020-2022 Cong Xu
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.
	Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include "weapon_menu.h"

#include <assert.h>

#include <cdogs/ai_coop.h>
#include <cdogs/draw/draw_actor.h>
#include <cdogs/draw/drawtools.h>
#include <cdogs/draw/nine_slice.h>
#include <cdogs/font.h>

#include "material.h"

#define NO_GUN_LABEL "(None)"
#define END_MENU_LABEL "(End)"
#define WEAPON_MENU_WIDTH 80
#define EQUIP_MENU_SLOT_HEIGHT 40
#define WEAPON_MENU_MAX_ROWS 4
#define GUN_BG_W 40
#define GUN_BG_H 25
#define SCROLL_H 12

static GunType SlotType(const int slot)
{
	if (slot < MELEE_SLOT)
	{
		return GUNTYPE_NORMAL;
	}
	else if (slot == MELEE_SLOT)
	{
		return GUNTYPE_MELEE;
	}
	return GUNTYPE_GRENADE;
}

static const WeaponClass *GetGun(
	const CArray *weapons, const int idx, const int slot)
{
	if (slot >= MAX_WEAPONS)
	{
		return NULL;
	}
	int idx2 = 0;
	CA_FOREACH(const WeaponClass *, wc, *weapons)
	if ((*wc)->Type != SlotType(slot))
	{
		continue;
	}
	if (idx == idx2)
	{
		return *wc;
	}
	idx2++;
	CA_FOREACH_END()
	return NULL;
}

static const WeaponClass *GetSelectedGun(const WeaponMenuData *data)
{
	return GetGun(&data->weapons, data->gunIdx, data->EquipSlot);
}

static bool IsGunEquipped(const PlayerData *pd, const WeaponClass *wc)
{
	if (wc == NULL)
	{
		return false;
	}
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (pd->guns[i] == wc)
		{
			return true;
		}
	}
	return false;
}

static void WeaponSelect(menu_t *menu, int cmd, void *data)
{
	UNUSED(menu);
	WeaponMenuData *d = data;

	if (cmd & CMD_BUTTON1)
	{
		// Add the selected weapon to the slot
		const WeaponClass *selectedGun = GetSelectedGun(d);
		d->SelectResult = WEAPON_MENU_SELECT;
		if (selectedGun == NULL)
		{
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
		else
		{
			SoundPlay(&gSoundDevice, selectedGun->SwitchSound);
		}
	}
	else if (cmd & CMD_BUTTON2)
	{
		d->SelectResult = WEAPON_MENU_CANCEL;
		MenuPlaySound(MENU_SOUND_BACK);
	}
}

static int CountNumGuns(const WeaponMenuData *data, const int slot)
{
	if (slot >= MAX_WEAPONS)
	{
		return 0;
	}
	// Count total guns
	int numGuns = 0;
	CA_FOREACH(const WeaponClass *, wc, data->weapons)
	if ((*wc)->Type != SlotType(slot))
	{
		continue;
	}
	numGuns++;
	CA_FOREACH_END()
	return numGuns;
}

static bool IsSlotDisabled(const WeaponMenuData *data, const int slot)
{
	if (slot < 0 || slot > MAX_WEAPONS)
	{
		return true;
	}
	// Disable end option if nothing equipped
	const PlayerData *pData = PlayerDataGetByUID(data->PlayerUID);
	if (slot == MAX_WEAPONS)
	{
		return PlayerGetNumWeapons(pData) == 0;
	}
	return CountNumGuns(data, slot) == 0;
}

static void ClampScroll(WeaponMenuData *data)
{
	// Update menu scroll based on selected gun
	const int numGuns = CountNumGuns(data, data->EquipSlot);
	const int selectedRow = data->gunIdx / data->cols;
	const int lastRow = numGuns / data->cols;
	int minRow = MAX(0, selectedRow - WEAPON_MENU_MAX_ROWS + 1);
	int maxRow =
		MIN(selectedRow, MAX(0, DIV_ROUND_UP(numGuns + 1, data->cols) -
									WEAPON_MENU_MAX_ROWS));
	// If the selected row is the last row on screen, and we can still
	// scroll down (i.e. show the scroll down button), scroll down
	if (selectedRow - data->scroll == WEAPON_MENU_MAX_ROWS - 1 &&
		selectedRow < lastRow)
	{
		minRow++;
	}
	else if (selectedRow == data->scroll && selectedRow > 0)
	{
		maxRow--;
	}
	data->scroll = CLAMP(data->scroll, minRow, maxRow);
}

static void DrawEquipSlot(
	const WeaponMenuData *data, GraphicsDevice *g, const int slot,
	const char *label, const struct vec2i pos, const FontAlign align)
{
	const bool selected = data->EquipSlot == slot;
	color_t color = data->equipping ? colorDarkGray : colorWhite;
	color_t mask = color;
	if (selected)
	{
		if (data->equipping)
		{
			color = colorYellow;
			mask = colorWhite;
		}
		else
		{
			const color_t bg = {0, 255, 255, 64};
			// Add 1px padding
			const struct vec2i bgPos = svec2i_subtract(pos, svec2i_one());
			const struct vec2i bgSize =
				svec2i(WEAPON_MENU_WIDTH / 2 + 2, EQUIP_MENU_SLOT_HEIGHT + 2);
			DrawRectangle(g, bgPos, bgSize, bg, true);

			color = colorRed;
		}
	}
	if (IsSlotDisabled(data, slot))
	{
		color = mask = colorDarkGray;
	}
	const PlayerData *pData = PlayerDataGetByUID(data->PlayerUID);

	int y = pos.y;

	const int bgSpriteIndex = (slot & 1) + (int)selected * 2;
	const Pic *slotBG = CArrayGet(&data->slotBGSprites->pics, bgSpriteIndex);
	const struct vec2i bgPos = svec2i(pos.x, y);
	Draw9Slice(
		g, slotBG,
		Rect2iNew(
			svec2i(bgPos.x + 1, bgPos.y + 1),
			svec2i(WEAPON_MENU_WIDTH / 2, EQUIP_MENU_SLOT_HEIGHT - 2)),
		11, (slot & 1) ? 4 : 13, 12, (slot & 1) ? 13 : 4, true, mask,
		SDL_FLIP_NONE);

	const FontOpts fopts = {
		align, ALIGN_START, svec2i(WEAPON_MENU_WIDTH / 2, FontH()),
		svec2i(3, 1), color};
	FontStrOpt(label, pos, fopts);

	const Pic *gunIcon = pData->guns[slot]
							 ? pData->guns[slot]->Icon
							 : PicManagerGetPic(&gPicManager, "peashooter");
	// Draw icon at center of slot
	const struct vec2i gunPos = svec2i_subtract(
		svec2i_add(
			bgPos,
			svec2i_scale_divide(
				svec2i(WEAPON_MENU_WIDTH / 2, EQUIP_MENU_SLOT_HEIGHT), 2)),
		svec2i_scale_divide(gunIcon->size, 2));
	PicRender(
		gunIcon, g->gameWindow.renderer, gunPos,
		pData->guns[slot] ? mask : colorBlack, 0, svec2_one(), SDL_FLIP_NONE,
		Rect2iZero());

	if (data->SlotHasNew[slot])
	{
		DrawCross(
			g, svec2i(pos.x + WEAPON_MENU_WIDTH / 2 - 6, y + 13), colorGreen);
	}

	y += EQUIP_MENU_SLOT_HEIGHT - FontH() - 1;
	const char *gunName =
		pData->guns[slot] ? pData->guns[slot]->name : NO_GUN_LABEL;
	const FontOpts fopts2 = {
		align, ALIGN_START, svec2i(WEAPON_MENU_WIDTH / 2, FontH()),
		svec2i(3, 0), color};
	FontStrOpt(gunName, svec2i(pos.x, y), fopts2);
}
static void DrawEquipMenu(
	const menu_t *menu, GraphicsDevice *g, const struct vec2i pos,
	const struct vec2i size, const void *data)
{
	UNUSED(menu);
	const WeaponMenuData *d = data;
	const PlayerData *pData = PlayerDataGetByUID(d->PlayerUID);

	DrawEquipSlot(d, g, 0, "I", svec2i(pos.x, pos.y), ALIGN_START);
	DrawEquipSlot(
		d, g, 1, "II", svec2i(pos.x + WEAPON_MENU_WIDTH / 2, pos.y),
		ALIGN_END);
	DrawEquipSlot(
		d, g, MELEE_SLOT, "Melee",
		svec2i(pos.x, pos.y + EQUIP_MENU_SLOT_HEIGHT), ALIGN_START);
	DrawEquipSlot(
		d, g, 3, "Bombs",
		svec2i(pos.x + WEAPON_MENU_WIDTH / 2, pos.y + EQUIP_MENU_SLOT_HEIGHT),
		ALIGN_END);

	const WeaponClass *gun = NULL;
	if (d->display.GunIdx >= 0 && d->display.GunIdx < MAX_WEAPONS)
	{
		gun = pData->guns[d->display.GunIdx];
	}
	DrawCharacterSimple(
		&pData->Char,
		svec2i(
			pos.x + WEAPON_MENU_WIDTH / 2, pos.y + EQUIP_MENU_SLOT_HEIGHT + 8),
		DIRECTION_DOWN, false, false, gun);

	const struct vec2i endSize = FontStrSize(END_MENU_LABEL);
	const bool endDisabled = IsSlotDisabled(d, MAX_WEAPONS) || d->equipping;
	DisplayMenuItem(
		g,
		Rect2iNew(
			svec2i(
				CENTER_X(pos, size, endSize.x),
				pos.y + EQUIP_MENU_SLOT_HEIGHT * 2 + FontH()),
			FontStrSize(END_MENU_LABEL)),
		END_MENU_LABEL, d->EquipSlot == MAX_WEAPONS, endDisabled, colorWhite);
}
static int HandleInputEquipMenu(int cmd, void *data)
{
	WeaponMenuData *d = data;
	PlayerData *p = PlayerDataGetByUID(d->PlayerUID);

	int newSlot = d->EquipSlot;
	if (cmd & CMD_BUTTON1)
	{
		MenuPlaySound(MENU_SOUND_ENTER);
		return 1;
	}
	else if (cmd & CMD_BUTTON2)
	{
		if (d->EquipSlot < MAX_WEAPONS)
		{
			// Unequip
			p->guns[d->EquipSlot] = NULL;
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
	}
	else if (cmd & CMD_LEFT)
	{
		if (d->EquipSlot < MAX_WEAPONS && (d->EquipSlot & 1) == 1)
		{
			newSlot--;
			if (IsSlotDisabled(data, newSlot))
			{
				// Try switching up-left or down-left
				if (!IsSlotDisabled(data, newSlot - 2))
				{
					newSlot -= 2;
				}
				else if (!IsSlotDisabled(data, newSlot + 2))
				{
					newSlot += 2;
				}
			}
		}
	}
	else if (cmd & CMD_RIGHT)
	{
		if (d->EquipSlot < MAX_WEAPONS && (d->EquipSlot & 1) == 0)
		{
			newSlot++;
			if (IsSlotDisabled(data, newSlot))
			{
				// Try switching up-right or down-right
				if (!IsSlotDisabled(data, newSlot - 2))
				{
					newSlot -= 2;
				}
				else if (!IsSlotDisabled(data, newSlot + 2))
				{
					newSlot += 2;
				}
			}
		}
	}
	else if (cmd & CMD_UP)
	{
		if (d->EquipSlot >= MAX_WEAPONS)
		{
			// Keep going back until we find an enabled slot
			do
			{
				newSlot--;
			} while (newSlot > 0 && IsSlotDisabled(data, newSlot));
		}
		else if (d->EquipSlot >= 2)
		{
			newSlot -= 2;
			if (IsSlotDisabled(data, newSlot))
			{
				// Try switching to the other slot above (up-left or up-right),
				// or skip the row and keep going back until an enabled slot
				if (!IsSlotDisabled(data, newSlot ^ 1))
				{
					newSlot ^= 1;
				}
				else
				{
					do
					{
						newSlot--;
					} while (newSlot > 0 && IsSlotDisabled(data, newSlot));
				}
			}
		}
	}
	else if (cmd & CMD_DOWN)
	{
		if (d->EquipSlot < MAX_WEAPONS)
		{
			newSlot = MIN(MAX_WEAPONS, d->EquipSlot + 2);
			if (IsSlotDisabled(data, newSlot))
			{
				// Try switching to the other slot below (down-left or
				// down-right), or skip the row and keep going forward until an
				// enabled slot
				if (!IsSlotDisabled(data, newSlot ^ 1))
				{
					newSlot ^= 1;
				}
				else
				{
					do
					{
						newSlot++;
					} while (newSlot < MAX_WEAPONS &&
							 IsSlotDisabled(data, newSlot));
				}
			}
		}
	}

	if (newSlot != d->EquipSlot && !IsSlotDisabled(d, newSlot))
	{
		d->EquipSlot = newSlot;
		MenuPlaySound(MENU_SOUND_SWITCH);
	}

	// Display gun based on menu index
	if (d->EquipSlot < MAX_WEAPONS)
	{
		d->display.GunIdx = d->EquipSlot;
	}

	ClampScroll(d);

	return 0;
}
static menu_t *CreateEquipMenu(
	MenuSystem *ms, EventHandlers *handlers, GraphicsDevice *g,
	const struct vec2i pos, const struct vec2i size, WeaponMenuData *data)
{
	const struct vec2i weaponsSize =
		svec2i(WEAPON_MENU_WIDTH, EQUIP_MENU_SLOT_HEIGHT * 2 + FontH());
	const struct vec2i weaponsPos = svec2i(
		pos.x - WEAPON_MENU_WIDTH, CENTER_Y(pos, size, weaponsSize.y) - 12);

	MenuSystemInit(ms, handlers, g, weaponsPos, weaponsSize);
	ms->align = MENU_ALIGN_LEFT;
	MenuAddExitType(ms, MENU_TYPE_RETURN);

	// Count number of each gun type, and disable extra menu items
	int numGuns = 0;
	int numMelee = 0;
	int numGrenades = 0;
	CA_FOREACH(const WeaponClass *, wc, data->weapons)
	if ((*wc)->Type == GUNTYPE_GRENADE)
	{
		numGrenades++;
	}
	else if ((*wc)->Type == GUNTYPE_MELEE)
	{
		numMelee++;
	}
	else
	{
		numGuns++;
	}
	CA_FOREACH_END()
	for (int i = 0; i < MELEE_SLOT; i++)
	{
		data->EquipEnabled[i] = i < numGuns;
	}
	data->EquipEnabled[MELEE_SLOT] = numMelee > 0;
	for (int i = 0; i < MAX_GRENADES; i++)
	{
		data->EquipEnabled[i + MAX_GUNS] = i < numGrenades;
	}

	// Pre-select the End menu
	data->EquipSlot = MAX_WEAPONS;
	menu_t *menu =
		MenuCreateCustom("", DrawEquipMenu, HandleInputEquipMenu, data);

	return menu;
}

static bool HasWeapon(const CArray *weapons, const WeaponClass *wc);
static menu_t *CreateGunMenu(WeaponMenuData *data);
void WeaponMenuCreate(
	WeaponMenu *menu, const CArray *weapons, const CArray *prevWeapons,
	const int numPlayers, const int player, const int playerUID,
	EventHandlers *handlers, GraphicsDevice *graphics)
{
	MenuSystem *ms = &menu->ms;
	WeaponMenuData *data = &menu->data;
	struct vec2i pos, size;
	int w = graphics->cachedConfig.Res.x;
	int h = graphics->cachedConfig.Res.y;

	data->display.PlayerUID = playerUID;
	data->display.currentMenu = NULL;
	data->display.Dir = DIRECTION_DOWN;
	data->PlayerUID = playerUID;
	data->slotBGSprites =
		PicManagerGetSprites(&gPicManager, "hud/gun_slot_bg");
	data->gunBGSprites = PicManagerGetSprites(&gPicManager, "hud/gun_bg");
	data->gunIdx = -1;
	CArrayCopy(&data->weapons, weapons);
	CArrayInit(&data->weaponIsNew, sizeof(bool));
	CA_FOREACH(const WeaponClass *, wc, data->weapons)
	const bool isNew = !HasWeapon(prevWeapons, *wc);
	CArrayPushBack(&data->weaponIsNew, &isNew);
	if (isNew)
	{
		if ((*wc)->Type == GUNTYPE_GRENADE)
		{
			data->SlotHasNew[MAX_GUNS] = true;
		}
		else if ((*wc)->Type == GUNTYPE_MELEE)
		{
			data->SlotHasNew[MELEE_SLOT] = true;
		}
		else
		{
			for (int i = 0; i < MELEE_SLOT; i++)
			{
				data->SlotHasNew[i] = true;
			}
		}
	}
	CA_FOREACH_END()

	switch (numPlayers)
	{
	case 1:
		// Single menu, entire screen
		pos = svec2i(w / 2, 0);
		size = svec2i(w / 2, h);
		break;
	case 2:
		// Two menus, side by side
		pos = svec2i(player * w / 2 + w / 4, 0);
		size = svec2i(w / 4, h);
		break;
	case 3:
	case 4:
		// Four corners
		pos = svec2i((player & 1) * w / 2 + w / 4, (player / 2) * h / 2);
		size = svec2i(w / 4, h / 2);
		break;
	default:
		CASSERT(false, "not implemented");
		pos = svec2i(w / 2, 0);
		size = svec2i(w / 2, h);
		break;
	}
	const Pic *gunBG = CArrayGet(&data->gunBGSprites->pics, 0);
	data->cols = CLAMP(size.x * 3 / (gunBG->size.x + 2) / 4, 2, 4);

	MenuSystemInit(ms, handlers, graphics, pos, size);
	ms->align = MENU_ALIGN_LEFT;
	PlayerData *pData = PlayerDataGetByUID(playerUID);
	menu->ms.root = menu->ms.current = CreateGunMenu(data);
	MenuSystemAddCustomDisplay(
		ms, MenuDisplayPlayerControls, &data->PlayerUID);

	// Create equipped weapons menu
	menu->msEquip.root = menu->msEquip.current =
		CreateEquipMenu(&menu->msEquip, handlers, graphics, pos, size, data);

	// For AI players, pre-pick their weapons and go straight to menu end
	if (pData->inputDevice == INPUT_DEVICE_AI)
	{
		menu->data.EquipSlot = MAX_WEAPONS;
		menu->data.equipping = false;
		menu->msEquip.current = NULL;
		AICoopSelectWeapons(pData, player, weapons);
	}
}
static bool HasWeapon(const CArray *weapons, const WeaponClass *wc)
{
	if (weapons == NULL)
	{
		return true;
	}
	CA_FOREACH(const WeaponClass *, wc2, *weapons)
	if (*wc2 == wc)
	{
		return true;
	}
	CA_FOREACH_END()
	return false;
}
static void DrawGunMenu(
	const menu_t *menu, GraphicsDevice *g, const struct vec2i pos,
	const struct vec2i size, const void *data);
static int HandleInputGunMenu(int cmd, void *data);
static menu_t *CreateGunMenu(WeaponMenuData *data)
{
	menu_t *menu = MenuCreateCustom("", DrawGunMenu, HandleInputGunMenu, data);

	MenuSetPostInputFunc(menu, WeaponSelect, data);

	return menu;
}
static void DrawGun(
	const WeaponMenuData *data, GraphicsDevice *g, const int idx,
	const WeaponClass *wc, const bool isNew, const struct vec2i pos);
static void DrawGunMenu(
	const menu_t *menu, GraphicsDevice *g, const struct vec2i pos,
	const struct vec2i size, const void *data)
{
	UNUSED(menu);
	const WeaponMenuData *d = data;
	if (d->EquipSlot >= MAX_WEAPONS)
	{
		return;
	}
	const int weaponsHeight = EQUIP_MENU_SLOT_HEIGHT * 2 + FontH();
	const int weaponsY = CENTER_Y(pos, size, weaponsHeight) - 12;
	const color_t color = d->equipping ? colorWhite : colorGray;
	const struct vec2i scrollSize = svec2i(d->cols * GUN_BG_W - 2, SCROLL_H);
	bool scrollDown = false;

	// Draw guns: red if selected, yellow if equipped
	int idx = 0;
	CA_FOREACH(const WeaponClass *, wc, d->weapons)
	if ((*wc)->Type != SlotType(d->EquipSlot))
	{
		continue;
	}
	const int row = idx / d->cols;
	if (row >= d->scroll && row < d->scroll + WEAPON_MENU_MAX_ROWS)
	{
		const bool *isNew = CArrayGet(&d->weaponIsNew, _ca_index);
		DrawGun(d, g, idx, *wc, *isNew, svec2i(pos.x, pos.y + weaponsY));
	}
	idx++;
	if (idx / d->cols == d->scroll + WEAPON_MENU_MAX_ROWS)
	{
		scrollDown = true;
		break;
	}
	CA_FOREACH_END()

	// Draw scroll buttons
	const Pic *gradient = PicManagerGetPic(&gPicManager, "hud/gradient");
	if (d->scroll > 0)
	{
		const Pic *scrollPic = CArrayGet(&d->gunBGSprites->pics, 0);
		const Rect2i scrollRect =
			Rect2iNew(svec2i(pos.x + 3, pos.y + 1 + weaponsY), scrollSize);
		PicRender(
			gradient, g->gameWindow.renderer,
			svec2i(
				scrollRect.Pos.x + scrollSize.x / 2,
				pos.y + gradient->size.y / 2 + weaponsY + scrollSize.y - 1),
			colorBlack, 0, svec2((float)scrollSize.x, 1), SDL_FLIP_NONE,
			Rect2iZero());
		Draw9Slice(
			g, scrollPic, scrollRect, 3, 3, 3, 3, true, color, SDL_FLIP_NONE);
		FontOpts fopts = FontOptsNew();
		fopts.Area = scrollRect.Size;
		fopts.HAlign = ALIGN_CENTER;
		fopts.VAlign = ALIGN_CENTER;
		fopts.Mask = color;
		FontStrOpt(ARROW_UP, scrollRect.Pos, fopts);
	}
	if (scrollDown)
	{
		const Pic *scrollPic = CArrayGet(&d->gunBGSprites->pics, 0);
		const Rect2i scrollRect = Rect2iNew(
			svec2i(
				pos.x + 3, pos.y - 1 + weaponsY +
							   GUN_BG_H * WEAPON_MENU_MAX_ROWS - SCROLL_H),
			scrollSize);
		PicRender(
			gradient, g->gameWindow.renderer,
			svec2i(
				scrollRect.Pos.x + scrollSize.x / 2,
				pos.y - gradient->size.y / 2 + weaponsY +
					GUN_BG_H * WEAPON_MENU_MAX_ROWS - SCROLL_H -
					gradient->size.y / 2 + 1),
			colorBlack, 0, svec2((float)scrollSize.x, 1), SDL_FLIP_VERTICAL,
			Rect2iZero());
		Draw9Slice(
			g, scrollPic, scrollRect, 3, 3, 3, 3, true, color, SDL_FLIP_NONE);
		FontOpts fopts = FontOptsNew();
		fopts.Area = scrollRect.Size;
		fopts.HAlign = ALIGN_CENTER;
		fopts.VAlign = ALIGN_CENTER;
		fopts.Mask = color;
		FontStrOpt(ARROW_DOWN, scrollRect.Pos, fopts);
	}
	else
	{
		DrawGun(d, g, idx, NULL, false, svec2i(pos.x, pos.y + weaponsY));
	}
}
static void DrawGun(
	const WeaponMenuData *data, GraphicsDevice *g, const int idx,
	const WeaponClass *wc, const bool isNew, const struct vec2i pos)
{
	const bool selected = data->gunIdx == idx;
	const PlayerData *pData = PlayerDataGetByUID(data->PlayerUID);
	const bool equipped = IsGunEquipped(pData, wc);
	const int bgSpriteIndex = (int)selected;
	const Pic *gunBG = CArrayGet(&data->gunBGSprites->pics, bgSpriteIndex);
	const struct vec2i bgSize = svec2i(GUN_BG_W, GUN_BG_H);
	const struct vec2i bgPos = svec2i(
		pos.x + 2 + (idx % data->cols) * GUN_BG_W,
		pos.y + (idx / data->cols - data->scroll) * GUN_BG_H);
	color_t color = data->equipping ? colorWhite : colorGray;
	const color_t mask = color;
	if (selected && data->equipping)
	{
		const color_t bg = {0, 255, 255, 64};
		DrawRectangle(g, bgPos, bgSize, bg, true);
		color = colorRed;
	}
	else if (equipped)
	{
		color = colorYellow;
	}

	Draw9Slice(
		g, gunBG,
		Rect2iNew(
			svec2i(bgPos.x + 1, bgPos.y + 1),
			svec2i(GUN_BG_W - 2, GUN_BG_H - 2)),
		3, 3, 3, 3, true, mask, SDL_FLIP_NONE);

	// Draw icon at center of slot
	const Pic *gunIcon =
		wc ? wc->Icon : PicManagerGetPic(&gPicManager, "peashooter");
	const struct vec2i gunPos = svec2i_subtract(
		svec2i_add(bgPos, svec2i_scale_divide(bgSize, 2)),
		svec2i_scale_divide(gunIcon->size, 2));
	PicRender(
		gunIcon, g->gameWindow.renderer, gunPos, wc ? mask : colorBlack, 0,
		svec2_one(), SDL_FLIP_NONE, Rect2iZero());

	if (isNew)
	{
		DrawCross(g, svec2i(bgPos.x + GUN_BG_W - 6, bgPos.y + 5), colorGreen);
	}

	const FontOpts fopts = {
		ALIGN_CENTER, ALIGN_END, bgSize, svec2i(2, 2), color};
	const char *gunName = wc ? wc->name : NO_GUN_LABEL;
	FontStrOpt(gunName, bgPos, fopts);
}
static int HandleInputGunMenu(int cmd, void *data)
{
	WeaponMenuData *d = data;
	PlayerData *p = PlayerDataGetByUID(d->PlayerUID);

	// Pre-select the equipped gun for the slot
	bool hasSelected = d->gunIdx >= 0;
	if (!hasSelected)
	{
		d->gunIdx = 0;
	}

	// Count total guns
	int numGuns = 0;
	CA_FOREACH(const WeaponClass *, wc, d->weapons)
	if ((*wc)->Type != SlotType(d->EquipSlot))
	{
		continue;
	}
	if (*wc == p->guns[d->EquipSlot])
	{
		hasSelected = true;
	}
	if (!hasSelected)
	{
		d->gunIdx++;
	}
	numGuns++;
	CA_FOREACH_END()

	if (cmd & CMD_BUTTON1)
	{
		return 1;
	}
	else if (cmd & CMD_BUTTON2)
	{
		return 1;
	}
	else if (cmd & CMD_LEFT)
	{
		if ((d->gunIdx % d->cols) > 0)
		{
			d->gunIdx--;
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
	}
	else if (cmd & CMD_RIGHT)
	{
		if ((d->gunIdx % d->cols) < d->cols - 1 && numGuns > 0)
		{
			d->gunIdx++;
			if (d->gunIdx > numGuns)
			{
				d->gunIdx = MAX(0, d->gunIdx - d->cols);
			}
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
	}
	else if (cmd & CMD_UP)
	{
		if (d->gunIdx >= d->cols)
		{
			d->gunIdx -= d->cols;
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
	}
	else if (cmd & CMD_DOWN)
	{
		if ((d->gunIdx + d->cols) <
			DIV_ROUND_UP(numGuns + 1, d->cols) * d->cols)
		{
			d->gunIdx = MIN(numGuns, d->gunIdx + d->cols);
			MenuPlaySound(MENU_SOUND_SWITCH);
		}
	}

	ClampScroll(d);

	return 0;
}

void WeaponMenuTerminate(WeaponMenu *menu)
{
	MenuSystemTerminate(&menu->ms);
	MenuSystemTerminate(&menu->msEquip);
	CArrayTerminate(&menu->data.weapons);
	CArrayTerminate(&menu->data.weaponIsNew);
}

void WeaponMenuUpdate(WeaponMenu *menu, const int cmd)
{
	PlayerData *p = PlayerDataGetByUID(menu->data.PlayerUID);
	if (menu->data.equipping)
	{
		MenuProcessCmd(&menu->ms, cmd);
		switch (menu->data.SelectResult)
		{
		case WEAPON_MENU_NONE:
			break;
		case WEAPON_MENU_SELECT: {
			const WeaponClass *selectedGun = GetSelectedGun(&menu->data);
			// See if the selected gun is already equipped; if so swap it with
			// the current slot
			for (int i = 0; i < MAX_WEAPONS; i++)
			{
				if (p->guns[i] == selectedGun)
				{
					p->guns[i] = p->guns[menu->data.EquipSlot];
					break;
				}
			}
			p->guns[menu->data.EquipSlot] = selectedGun;
		}
			// fallthrough
		case WEAPON_MENU_CANCEL:
			// Switch back to equip menu
			menu->data.equipping = false;
			menu->ms.current = menu->ms.root;
			break;
		default:
			CASSERT(false, "unhandled case");
			break;
		}
	}
	else
	{
		MenuProcessCmd(&menu->msEquip, cmd);
		if (menu->data.EquipSlot < MAX_WEAPONS)
		{
			if (MenuIsExit(&menu->msEquip))
			{
				// Open weapon selection menu
				menu->data.equipping = true;
				menu->data.gunIdx = -1;
				menu->msEquip.current = menu->msEquip.root;
				menu->data.SelectResult = WEAPON_MENU_NONE;
			}
			else if (
				menu->data.gunIdx >
				CountNumGuns(&menu->data, menu->data.EquipSlot))
			{
				menu->data.gunIdx = -1;
			}
		}
	}

	// Disable the equip/weapon menus based on equipping state
	MenuSetDisabled(menu->msEquip.root, menu->data.equipping);
	if (menu->ms.current)
	{
		MenuSetDisabled(menu->ms.current, !menu->data.equipping);
	}
}

bool WeaponMenuIsDone(const WeaponMenu *menu)
{
	return menu->msEquip.current == NULL && !menu->data.equipping &&
		   menu->data.EquipSlot == MAX_WEAPONS;
}

void WeaponMenuDraw(const WeaponMenu *menu)
{
	MenuDisplay(&menu->msEquip);
	MenuDisplay(&menu->ms);
}
