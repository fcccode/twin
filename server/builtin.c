/*
 *  builtin.c  --  create default menu, Clock and About windows.
 *
 *  Copyright (C) 1993-2000 by Massimiliano Ghilardi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <signal.h>
#include <unistd.h>

#include "twin.h"
#include "data.h"
#include "methods.h"

#include "main.h"
#include "hw.h"
#include "hw_multi.h"
#include "common.h"
#include "resize.h"
#include "draw.h"
#include "util.h"
#include "version.h"

#ifdef CONF__MODULES
# include "dl.h"
#endif

#if defined(CONF_TERM)
# include "term.h"
#elif defined (CONF__MODULES)
menu *Builtin_Term_Menu;
#define Term_Menu Builtin_Term_Menu
#endif

#define COD_QUIT	(udat)1 /* as in term.c */
#define COD_SPAWN	(udat)3 /* as COD_SPAWN in term.c */

#define COD_SUSPEND	(udat)10
#define COD_DETACH	(udat)11

#define COD_ABOUT_WIN	(udat)20
#define COD_CLOCK_WIN   (udat)21
#define COD_OPTION_WIN	(udat)22
#define COD_DISPLAY_WIN	(udat)23
#define COD_REFRESH	(udat)24

#define COD_TERM_ON	(udat)30
#define COD_TERM_OFF	(udat)31
#define COD_SOCKET_ON	(udat)32
#define COD_SOCKET_OFF	(udat)33

#define COD_O_NEWFONT	(udat)40
#define COD_O_SHADOWS	(udat)41
#define COD_O_Xp_SHADE	(udat)42
#define COD_O_Xn_SHADE	(udat)43
#define COD_O_Yp_SHADE	(udat)44
#define COD_O_Yn_SHADE	(udat)45
#define COD_O_ALWAYSCURSOR (udat)46
#define COD_O_NOBLINK	(udat)47
#define COD_O_HIDEMENU	(udat)48
#define COD_O_NOSCROLL	(udat)49

#define COD_D_REMOVE	(udat)60
#define COD_D_THIS	(udat)61

static msgport *Builtin_MsgPort;
static menu *Builtin_Menu;
static menuitem *Builtin_File;
#ifdef CONF__MODULES
static menuitem *Builtin_Modules;
#endif

static window *AboutWin, *ClockWin, *OptionWin, *DisplayWin;
window *ListWin;

static gadget *ButtonOK_About, *ButtonRemove, *ButtonThis;

static void Clock_Update(void) {
    struct timevalue *Time = &All->Now;
    struct tm *Date;
    byte Buffer[30];
    
    ClockWin->CurX=ClockWin->CurY=(uldat)0;
    Date = localtime(&Time->Seconds);
    
    sprintf((char *)Buffer, "%02u/%02u/%04u\n %02u:%02u:%02u",
	    (udat)Date->tm_mday, (udat)Date->tm_mon+1, (udat)Date->tm_year + 1900,
	    (udat)Date->tm_hour, (udat)Date->tm_min,   (udat)Date->tm_sec);
    if (!Act(WriteRow,ClockWin)(ClockWin, strlen(Buffer), Buffer))
	Error(NOMEMORY);
    
    Builtin_MsgPort->PauseDuration.Fraction = 1 FullSECs - Time->Fraction;
    Builtin_MsgPort->PauseDuration.Seconds = 0;
}

#if defined(CONF__MODULES) && !(defined(CONF_TERM) && defined(CONF_SOCKET))
static void TweakMenuRows(menuitem *Item, udat code, byte flag) {
    window *Win;
    row *Row;
    
    if ((Win = Item->Window) &&
	(Row = Act(SearchRowCode,Win)(Win, code, (uldat *)0)))
	Row->Flags = flag;
}

static void UpdateMenuRows(window *dummy) {
    module *Module = All->FirstModule;
    byte _TermSo = FALSE, _SocketSo = FALSE;

    while (Module) {
	if (Module->NameLen == 7 && !CmpMem("term.so", Module->Name, 7))
	    _TermSo = TRUE;
	else if (Module->NameLen == 9 && !CmpMem("socket.so", Module->Name, 9))
	    _SocketSo = TRUE;

	Module = Module->Next;
    }

#ifndef CONF_TERM    
    if (_TermSo) {
	TweakMenuRows(Builtin_Modules, COD_TERM_ON,    ROW_INACTIVE);
	TweakMenuRows(Builtin_Modules, COD_TERM_OFF,   ROW_ACTIVE);
	TweakMenuRows(Builtin_File,    COD_SPAWN,      ROW_ACTIVE);
    } else {
	TweakMenuRows(Builtin_Modules, COD_TERM_ON,    ROW_ACTIVE);
	TweakMenuRows(Builtin_Modules, COD_TERM_OFF,   ROW_INACTIVE);
	TweakMenuRows(Builtin_File,    COD_SPAWN,      ROW_INACTIVE);
    }
#endif
#ifndef CONF_SOCKET
    if (_SocketSo) {
	TweakMenuRows(Builtin_Modules, COD_SOCKET_ON,  ROW_INACTIVE);
	TweakMenuRows(Builtin_Modules, COD_SOCKET_OFF, ROW_ACTIVE);
    } else {
	TweakMenuRows(Builtin_Modules, COD_SOCKET_ON,  ROW_ACTIVE);
	TweakMenuRows(Builtin_Modules, COD_SOCKET_OFF, ROW_INACTIVE);
    }
#endif
}

#endif

#if defined(CONF__MODULES) || defined(CONF_TERM)
static void SpawnTerm(void) {
    msg *Msg;
    /* send a Msg to Term_MsgPort */
    if (Term_Menu && (Msg = Do(Create,Msg)(FnMsg, MSG_MENU_ROW, sizeof(event_menu)))) {
	Msg->Event.EventMenu.Menu = Term_Menu;
	Msg->Event.EventMenu.Code = COD_SPAWN;
	SendMsg(Term_Menu->MsgPort, Msg);
    }
}
#endif

static void SelectListWin(void) {
    screen *Screen = All->FirstScreen;
    window *Window = Screen->FirstWindow, *tempWin;
    uldat n = ListWin->CurY;
    
    for (tempWin = Window; ; Window = Window->Next) {
	if (!Window)
	    break;
	if (Window == ListWin || (Window->Attrib & WINDOW_MENU))
	    continue;
	if (!n)
	    break;
	n--;
    }
    if (!n && Window) {
	MakeFirstWindow(Window, TRUE);
	CenterWindow(Window);
    }
}



void UpdateOptionWin(void) {
    gadget *G;
    byte i, Flags = All->SetUp->Flags;
    udat list[] = {COD_O_Xp_SHADE, COD_O_Xn_SHADE, COD_O_Yp_SHADE, COD_O_Yn_SHADE, 0 };

    for (i = 0; list[i]; i++) {
	if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, list[i]))) {
	    if (Flags & SETUP_DO_SHADE)
		G->Flags &= ~GADGET_DISABLED;
	    else
		G->Flags |= GADGET_DISABLED;
	}
    }
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_SHADOWS)))
	G->Contents[0][1] = Flags & SETUP_DO_SHADE ? '�' : ' ';
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_ALWAYSCURSOR)))
	G->Contents[0][1] = Flags & SETUP_ALWAYSCURSOR ? '�' : ' ';
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_NOBLINK)))
	G->Contents[0][1] = Flags & SETUP_NOBLINK ? '�' : ' ';
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_NEWFONT)))
	G->Contents[0][1] = Flags & SETUP_NEW_FONT ? '�' : ' ';
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_HIDEMENU)))
	G->Contents[0][1] = Flags & SETUP_HIDEMENU ? '�' : ' ';
    if ((G = Act(SearchGadgetCode,OptionWin)(OptionWin, COD_O_NOSCROLL)))
	G->Contents[0][1] = Flags & SETUP_NOSCROLL ? '�' : ' ';
    
    OptionWin->CurX = 25; OptionWin->CurY = 1;
    i = (Flags & SETUP_DO_SHADE ? All->SetUp->DeltaXShade : 0) + '0';
    Act(WriteRow,OptionWin)(OptionWin, 1, &i);
    OptionWin->CurX = 25; OptionWin->CurY = 2;
    i = (Flags & SETUP_DO_SHADE ? All->SetUp->DeltaYShade : 0) + '0';
    Act(WriteRow,OptionWin)(OptionWin, 1, &i);
}

static void OptionH(msg *Msg) {
    byte Flags = All->SetUp->Flags, XShade = All->SetUp->DeltaXShade, YShade = All->SetUp->DeltaYShade;
    byte redraw = TRUE;
    
    switch (Msg->Event.EventGadget.Code) {
      case COD_O_NEWFONT:
	Flags ^= SETUP_NEW_FONT;
	break;
      case COD_O_SHADOWS:
	Flags ^= SETUP_DO_SHADE;
	break;
      case COD_O_Xp_SHADE:
	if (XShade < MAX_XSHADE)
	    XShade++;
	break;
      case COD_O_Xn_SHADE:
	if (XShade > 1)
	    XShade--;
	break;
      case COD_O_Yp_SHADE:
	if (YShade < MAX_YSHADE)
	    YShade++;
	break;
      case COD_O_Yn_SHADE:
	if (YShade > 1)
	    YShade--;
	break;
      case COD_O_NOBLINK:
	Flags ^= SETUP_NOBLINK;
	break;
      case COD_O_ALWAYSCURSOR:
	Flags ^= SETUP_ALWAYSCURSOR;
	redraw = FALSE;
	break;
      case COD_O_HIDEMENU:
	Flags ^= SETUP_HIDEMENU;
	HideMenu(!!(Flags & SETUP_HIDEMENU));
	redraw = FALSE;
	break;
      case COD_O_NOSCROLL:
	Flags ^= SETUP_NOSCROLL;
	redraw = FALSE;
	break;
      default:
	redraw = FALSE;
	break;
    }
    if (Flags != All->SetUp->Flags || XShade != All->SetUp->DeltaXShade
	|| YShade != All->SetUp->DeltaYShade) {
	
	All->SetUp->Flags = Flags;
	All->SetUp->DeltaXShade = XShade;
	All->SetUp->DeltaYShade = YShade;
	
	UpdateOptionWin();
	if (redraw == TRUE)
	    DrawArea(FULLSCREEN);
	else {
	    DrawAreaWindow(OptionWin, FALSE);
	    UpdateCursor();
	}
    }
}

void UpdateDisplayWin(window *displayWin) {
    display_hw *hw;
    uldat x = 12, y = 0;
    
    if (displayWin == DisplayWin) {
	DeleteList(DisplayWin->FirstRow);
    
	for (hw = All->FirstDisplayHW; hw; hw = hw->Next) {
	    Act(GotoXY,DisplayWin)(DisplayWin, x, y++);
	    if (!hw->NameLen)
		Act(WriteRow,DisplayWin)(DisplayWin, 9, "(no name)");
	    else
		Act(WriteRow,DisplayWin)(DisplayWin, hw->NameLen, hw->Name);
	}
	if (DisplayWin->Screen)
	    DrawAreaWindow(DisplayWin, FALSE);
    }
}

static void SelectRowWindow(window *CurrWin, uldat newCurY) {
    uldat oldCurY=CurrWin->CurY;

    CurrWin->CurY=newCurY;

    if (oldCurY!=newCurY) {
	DrawTextWindow(CurrWin, 0, oldCurY, CurrWin->XWidth-2, oldCurY);
	DrawTextWindow(CurrWin, 0, newCurY, CurrWin->XWidth-2, newCurY);
    }
}

static void DisplayGadgetH(msg *Msg) {
    display_hw *hw;
    uldat i;

    switch (Msg->Event.EventGadget.Code) {
      case COD_D_REMOVE:
	if ((i = DisplayWin->CurY) < DisplayWin->MaxNumRow) {
	    for (hw = All->FirstDisplayHW; hw && i; hw = hw->Next, i--)
		;
	    if (hw && !i)
		Delete(hw);
	}
	break;
      case COD_D_THIS:
	if (All->MouseHW) {
	    for (i = 0, hw = All->FirstDisplayHW; hw; hw = hw->Next, i++) {
		if (hw == All->MouseHW)
		    break;
	    }
	    if (hw)
		SelectRowWindow(DisplayWin, i);
	}
	break;
      default:
	break;
    }
}
	    
    

static void BuiltinH(msgport *MsgPort) {
    msg *Msg;
    event_any *Event;
    screen *Screen;
    window *NewWindow, *tempWin;
    udat Code;
    byte /*FontHeight,*/ Flags;
    
    Screen=All->FirstScreen;
    
    while ((Msg=Builtin_MsgPort->FirstMsg)) {
	Remove(Msg);

	Event=&Msg->Event;
	if (Msg->Type==MSG_WINDOW_GADGET) {
	    tempWin=Event->EventGadget.Window;
	    Code=Event->EventGadget.Code;
	    /*0 == Code Chiusura */
	    if (!Code || Code==COD_CANCEL || Code==COD_OK) {
		
		Act(UnMap,tempWin)(tempWin);
		/* no window needs Delete() here */
		
		if (tempWin == ClockWin)
		    Builtin_MsgPort->WakeUp = FALSE;
		
	    } else if (tempWin == OptionWin)
		OptionH(Msg);
	    else if (tempWin == DisplayWin)
		DisplayGadgetH(Msg);
	}
	else if (Msg->Type==MSG_MENU_ROW) {
	    if (Event->EventMenu.Menu==Builtin_Menu) {
		Code=Event->EventMenu.Code;
		Flags=All->SetUp->Flags;
		switch (Code) {
		  case COD_OPTION_WIN:
		  case COD_ABOUT_WIN:
		  case COD_DISPLAY_WIN:
		    if (Code == COD_OPTION_WIN)
			UpdateOptionWin(), NewWindow = OptionWin;
		    else if (Code == COD_ABOUT_WIN)
			NewWindow = AboutWin;
		    else if (Code == COD_DISPLAY_WIN)
			NewWindow = DisplayWin, UpdateDisplayWin(DisplayWin);
		    else
			break;
		    
		    if (NewWindow->Screen)
			Act(UnMap,NewWindow)(NewWindow);
		    NewWindow->Left=Screen->Left+(udat)20;
		    NewWindow->Up=Screen->Up+(udat)6;
		    Act(Map,NewWindow)(NewWindow, Screen);
		    break;
		  case COD_CLOCK_WIN:
		    NewWindow = ClockWin;
		    if (NewWindow->Screen)
			Act(UnMap,NewWindow)(NewWindow);
		    NewWindow->Left=Screen->Left;
		    NewWindow->Up=Screen->Up;
		    Act(Map,NewWindow)(NewWindow, Screen);
		    Builtin_MsgPort->WakeUp = TIMER_ALWAYS;
		    break;

		  case COD_QUIT:
		    Quit(0);
		    break;
		    
		  case COD_SUSPEND:
		    SuspendHW(TRUE);
		    
		    kill(getpid(), SIGSTOP);
		    
		    (void)RestartHW(TRUE);
		    break;

		  case COD_DETACH:
		    QuitHW();
		    break;
		    
#if defined(CONF__MODULES) && !defined(CONF_TERM)
		  case COD_TERM_ON:
		    if (!DlLoad(TermSo))
			break;
		    /* FALLTHROUGH */
#endif
#if defined(CONF__MODULES) || defined(CONF_TERM)
		 case COD_SPAWN:
		    SpawnTerm();
		    break;
#endif
#if defined(CONF__MODULES) && !defined(CONF_TERM)
		  case COD_TERM_OFF:
		    DlUnLoad(TermSo);
		    break;
#endif
#if defined(CONF__MODULES) && !defined(CONF_SOCKET)
		  case COD_SOCKET_OFF:
		    DlUnLoad(SocketSo);
		    if (All->FirstDisplayHW)
			break;
		    /* hmm... better to fire it up again */
		    /* FALLTHROUGH */
		  case COD_SOCKET_ON:
		    if (!DlLoad(SocketSo))
			break;
		    break;
#endif
		  default:
		    break;
		}
	    }
	} else if (Msg->Event.EventCommon.Window == ListWin ||
		   Msg->Event.EventCommon.Window == DisplayWin) {
	    
	    /* this code is partially duplicated from wm.c... :( */
	    byte State = STATE_MENU;
	    udat Funct, temp, Repeat, CallKey;
	    
	    if (Msg->Event.EventCommon.Window == ListWin &&
		Msg->Type==MSG_WINDOW_KEY) {
		
		CallKey = Msg->Event.EventKeyboard.Code;
		/* steal actions from All->GlobalKeyCodes[STATE_MENU] */
		if (All->GlobalKeyCodes[State][0])			/* Compressed Format */
		    switch (All->GlobalKeyCodes[State][0]) {
		      case 1:					/* parallel list */
			for (temp = 1; (Repeat = All->GlobalKeyCodes[State][temp]); temp += 2)
			    if (Repeat == CallKey) {
				Funct = All->GlobalKeyCodes[State][temp+1];
				break;
			    }
			break;
		      default:
			break;
		    }
		else
		    Funct = FnK(All->GlobalKeyCodes[State],CallKey);
		switch (Funct) {
		  case STMENU_BACKTO_DEF:
		    Act(UnMap,ListWin)(ListWin);
		    break;
		  case STMENU_ACT_BACKTO_DEF:
		    SelectListWin();
		    break;
		  case STMENU_PREV_ROW:
		    if (ListWin->CurY) {
			ListWin->CurY--;
			DrawTextWindow(ListWin, 0, ListWin->CurY, MAXULDAT-1, ListWin->CurY+1);
		    }
		    break;
		  case STMENU_NEXT_ROW:
		    if (ListWin->CurY + 1 < ListWin->MaxNumRow) {
			ListWin->CurY++;
			DrawTextWindow(ListWin, 0, ListWin->CurY-1, MAXULDAT-1, ListWin->CurY);
		    }
		    break;
		  case STMENU_PREV_ITEM:
		  case STMENU_NEXT_ITEM:
		  default:
		    break;
		}
	    } else if (Msg->Type==MSG_WINDOW_MOUSE) {
		dat EventMouseX = Msg->Event.EventMouse.X, EventMouseY = Msg->Event.EventMouse.Y;
		window *CurrWin = Msg->Event.EventCommon.Window;

		temp = EventMouseX >= 0 && EventMouseX <= CurrWin->XWidth-2
		    && EventMouseY >= 0 && EventMouseY <= CurrWin->YWidth-2
		    && (uldat)EventMouseY+CurrWin->YLogic < (uldat)CurrWin->MaxNumRow;

		SelectRowWindow(CurrWin, temp ? (uldat)EventMouseY+CurrWin->YLogic : MAXULDAT);
		
		if (CurrWin == ListWin &&
		    isRELEASE(Msg->Event.EventMouse.Code)) {
		    if (temp)
			SelectListWin();
		}
	    }
	}
	Delete(Msg);
    }
    if (Builtin_MsgPort->WakeUp)
	Clock_Update();
}

void FullUpdateListWin(window *listWin);

void InstallRemoveListWinHook(window *listWin) {    
    if (listWin == ListWin) {
	if (ListWin->Screen)
	    Act(InstallHook,ListWin)(ListWin, FullUpdateListWin, &ListWin->Screen->FnHookWindow);
	else
	    Act(RemoveHook,ListWin)(ListWin, FullUpdateListWin, ListWin->WhereHook);
    }
}

void UpdateListWin(void) {
    screen *Screen = All->FirstScreen;
    window *Window = Screen->FirstWindow;
    
    DeleteList(ListWin->FirstRow);
    ListWin->CurX = ListWin->CurY = 0;
    
    ListWin->XWidth = ListWin->MinXWidth;
    ListWin->YWidth = ListWin->MinYWidth;
    
    for (; Window; Window = Window->Next) {
	if (Window == ListWin || (Window->Attrib & WINDOW_MENU))
	    continue;
	Row4Menu(ListWin, (udat)0, ROW_ACTIVE, Window->LenTitle, Window->Title);
    }
}

void FullUpdateListWin(window *listWin) {
    if (listWin == ListWin && ListWin->Screen) {
	ResizeRelWindow(ListWin, ListWin->MinXWidth - ListWin->XWidth, ListWin->MinYWidth - ListWin->YWidth);
    
	UpdateListWin();
	
	DrawAreaWindow(ListWin, FALSE);
    }
}

byte InitBuiltin(void) {
    window *Window;
    byte *s, *greeting = "\n"
	"                TWIN             \n"
	"        Text WINdows manager     \n\n"
	"          Version " TWIN_VERSION_STR " by       \n\n"
	"        Massimiliano Ghilardi    \n\n"
	"         <max@Linuz.sns.it>      ";
    uldat grlen = strlen(greeting);

    
    All->SetUp->Flags |= SETUP_DO_SHADE; /* | GRAPH_MOUSE | NEW_FONT | NEW_PALETTE; */

    
    
    if ((Builtin_MsgPort=Do(Create,MsgPort)
	 (FnMsgPort, 5, "Builtin", (time_t)0, (frac_t)0, 0, BuiltinH)) &&
	(Builtin_Menu=Do(Create,Menu)
	 (FnMenu, Builtin_MsgPort, (byte)0x70, (byte)0x20, (byte)0x78, (byte)0x08, (byte)0x74, (byte)0x24, (byte)0)) &&
	Info4Menu(Builtin_Menu, ROW_ACTIVE, (uldat)42, " Hit PAUSE or Mouse Right Button for Menu ", "tttttttttttttttttttttttttttttttttttttttttt") &&
	
	(Window=Win4Menu(Builtin_Menu)) &&
	Row4Menu(Window, COD_CLOCK_WIN,  ROW_ACTIVE, 9, " Clock   ") &&
	Row4Menu(Window, COD_OPTION_WIN, ROW_ACTIVE, 9, " Options ") &&
	Row4Menu(Window, COD_DISPLAY_WIN,ROW_ACTIVE, 9, " Display ") &&
	Row4Menu(Window, COD_ABOUT_WIN,  ROW_ACTIVE, 9, " About   ") &&
	Item4Menu(Builtin_Menu, Window, TRUE, 3, " � ") &&
	
	(Window=Win4Menu(Builtin_Menu)) &&
#if defined(CONF_TERM)
	Row4Menu(Window, COD_SPAWN,  ROW_ACTIVE,10, " New Term  ") &&
#elif defined(CONF__MODULES)
	Row4Menu(Window, COD_SPAWN,ROW_INACTIVE,10, " New Term  ") &&
#endif
	Row4Menu(Window, (udat)0,    ROW_IGNORE,10, "����������") &&
	Row4Menu(Window, COD_DETACH, ROW_ACTIVE,10, " Detach   ") &&
	Row4Menu(Window, COD_SUSPEND,ROW_ACTIVE,10, " Suspend  ") &&
	Row4Menu(Window, COD_QUIT,   ROW_ACTIVE,10, " Quit     ") &&
	(Builtin_File=Item4Menu(Builtin_Menu, Window, TRUE, 6, " File ")) &&
	
	(Window=Win4Menu(Builtin_Menu)) &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Undo           ") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Redo           ") &&
	Row4Menu(Window, (udat)0, ROW_IGNORE,  16,"����������������") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Cut            ") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Copy           ") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Paste          ") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Clear          ") &&
	Row4Menu(Window, (udat)0, ROW_IGNORE,  16,"����������������") &&
	Row4Menu(Window, (udat)0, ROW_INACTIVE,16," Show Clipboard ") &&
	Item4Menu(Builtin_Menu, Window, TRUE, 6," Edit ") &&
	
#if defined(CONF__MODULES) && !(defined(CONF_TERM) && defined(CONF_SOCKET))
	(Window=Win4Menu(Builtin_Menu)) &&
	(Act(InstallHook,Window)(Window, UpdateMenuRows, &All->FnHookModule), TRUE) &&
	
#if !defined(CONF_TERM)
	Row4Menu(Window, COD_TERM_ON,	ROW_ACTIVE,  20, " Run Twin Term      ") &&
	Row4Menu(Window, COD_TERM_OFF,	ROW_INACTIVE,20, " Stop Twin Term     ") &&
#endif
#if !defined(CONF_SOCKET) && !defined(CONF_TERM)
	Row4Menu(Window, (udat)0,       ROW_IGNORE,  20, "��������������������") &&
#endif	
#if !defined(CONF_SOCKET)
	Row4Menu(Window, COD_SOCKET_ON,	ROW_ACTIVE,  20, " Run Socket Server  ") &&
	Row4Menu(Window, COD_SOCKET_OFF,ROW_INACTIVE,20, " Stop Socket Server ") &&
#endif
	(Builtin_Modules=Item4Menu(Builtin_Menu, Window, TRUE, 9," Modules ")) &&
#endif
	
	Item4MenuCommon(Builtin_Menu) &&
		
	(AboutWin = Do(Create,Window)
	 (FnWindow, (udat)7, " About ", "\x7F\x7F\x7F\x7F\x7F\x7F\x7F", Builtin_Menu, COL(BLACK,WHITE),
	  NOCURSOR, WINDOW_WANT_MOUSE|WINDOW_DRAG|WINDOW_CLOSE, WINFL_USE_DEFCOL,
	  (udat)40, (udat)15, (udat)0)) &&

	(ClockWin = Do(Create,Window)
	 (FnWindow, (udat)7, " Clock ", NULL, Builtin_Menu, COL(YELLOW,BLUE),
	  NOCURSOR, WINDOW_DRAG|WINDOW_CLOSE, WINFL_USE_DEFCOL,
	  (udat)12, (udat)4, (udat)0)) &&

	(OptionWin = Do(Create,Window)
	 (FnWindow, (udat)9, " Options ", NULL, Builtin_Menu, COL(HIGH|BLACK,BLACK),
	  NOCURSOR, WINDOW_WANT_MOUSE|WINDOW_DRAG|WINDOW_CLOSE,WINFL_USE_DEFCOL,
	  (udat)42, (udat)16,(udat)0)) &&

	(DisplayWin = Do(Create,Window)
	 (FnWindow, (udat)9, " Display ", NULL, Builtin_Menu, COL(HIGH|BLACK,WHITE),
	  NOCURSOR,
	  WINDOW_WANT_MOUSE|WINDOW_DRAG|WINDOW_RESIZE|WINDOW_CLOSE
	  |WINDOW_X_BAR|WINDOW_Y_BAR,
	  WINFL_USE_DEFCOL|WINFL_SEL_ROWCURR,
	  (udat)33, (udat)10,(udat)0)) &&

	(ListWin = Do(Create,Window)
	 (FnWindow, (udat)13, " Window List ", NULL, Builtin_Menu, COL(WHITE,BLUE),
	  NOCURSOR,
	  WINDOW_WANT_KEYS|WINDOW_WANT_MOUSE|WINDOW_DRAG|WINDOW_RESIZE|WINDOW_CLOSE
	  |WINDOW_X_BAR|WINDOW_Y_BAR,
	  WINFL_SEL_ROWCURR|WINFL_USE_DEFCOL,
	  (udat)17, (udat)9, (udat)0)) &&

	Act(WriteRow,AboutWin)(AboutWin, grlen, greeting) &&
	
	(ButtonOK_About=Do(CreateEmptyButton,Gadget)(FnGadget, AboutWin, (udat)8, (udat)1, (byte)0x70)) &&

	(ButtonRemove=Do(CreateEmptyButton,Gadget)(FnGadget, DisplayWin, (udat)8, (udat)1, (byte)0x70)) &&
	(ButtonThis  =Do(CreateEmptyButton,Gadget)(FnGadget, DisplayWin, (udat)8, (udat)1, (byte)0x70)) &&

	Do(Create,Gadget)(FnGadget, DisplayWin, COL(BLACK,WHITE), COL(BLACK,WHITE),
			  COL(BLACK,WHITE), COL(BLACK,WHITE),
			  0, GADGET_USE_DEFCOL|GADGET_DISABLED, 0, 0, 11, 8,
			  ((s = AllocMem(11*8)), WriteMem(s, ' ', 11*8), s),
			  NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&
	
	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_SHADOWS, GADGET_USE_DEFCOL, 2, 1, 11, 1,
			  "[ ] Shadows", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_Xp_SHADE, GADGET_USE_DEFCOL, 18, 1, 3, 1,
			  "[+]", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_Xn_SHADE, GADGET_USE_DEFCOL, 21, 1, 3, 1,
			  "[-]", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_Yp_SHADE, GADGET_USE_DEFCOL, 18, 2, 3, 1,
			  "[+]", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_Yn_SHADE, GADGET_USE_DEFCOL, 21, 2, 3, 1,
			  "[-]", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&
	
	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_ALWAYSCURSOR, GADGET_USE_DEFCOL, 2, 4, 22, 1,
			  "[ ] Always Show Cursor", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_NOBLINK, GADGET_USE_DEFCOL, 2, 6, 33, 1,
			  "[ ] Disable Blink/High Background", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_HIDEMENU, GADGET_USE_DEFCOL, 2, 8, 15, 1,
			  "[ ] Hidden Menu", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_NOSCROLL, GADGET_USE_DEFCOL, 2, 10, 28, 1,
			  "[ ] Disable Screen Scrolling", NULL, NULL, NULL, NULL, NULL, NULL, NULL) &&

	Do(Create,Gadget)(FnGadget, OptionWin, COL(BLACK,WHITE), COL(HIGH|WHITE,GREEN),
			  COL(HIGH|BLACK,WHITE), COL(HIGH|BLACK,BLACK),
			  COD_O_NEWFONT, GADGET_USE_DEFCOL, 2, 12, 15, 1,
			  "[ ] Custom Font", NULL, NULL, NULL, NULL, NULL, NULL, NULL)

	)
    {
	Act(SetColors,AboutWin)(AboutWin, 0x1FF, (hwcol)0x7A, (hwcol)0, (hwcol)0, (hwcol)0, (hwcol)0x7F,
				(hwcol)0x70, (hwcol)0x20, (hwcol)0x78, (hwcol)0x08);
	
	
	Act(SetColors,ClockWin)(ClockWin, 0x1FF, (hwcol)0x3E, (hwcol)0, (hwcol)0, (hwcol)0, (hwcol)0x9F,
				(hwcol)0x1E, (hwcol)0x3E, (hwcol)0x18, (hwcol)0x08);

	Act(SetColors,OptionWin)(OptionWin, 0x1FF, (hwcol)0x7A, (hwcol)0, (hwcol)0, (hwcol)0, (hwcol)0x7F,
				 (hwcol)0x78, (hwcol)0x20, (hwcol)0x78, (hwcol)0x08);

	Act(SetColors,ListWin)(ListWin, 0x1FF,
			       COL(HIGH|YELLOW,CYAN), COL(HIGH|GREEN,HIGH|BLUE), COL(WHITE,HIGH|BLUE),
			       COL(HIGH|WHITE,HIGH|BLUE), COL(HIGH|WHITE,HIGH|BLUE),
			       COL(WHITE,BLUE), COL(HIGH|BLUE,WHITE), COL(HIGH|BLACK,BLUE), COL(HIGH|BLACK,BLACK));
	Act(Configure,ListWin)(ListWin, 1<<2 | 1<<3, 0, 0, (udat)17, (udat)4, 0, 0);

	Act(SetColors,DisplayWin)(DisplayWin, 0x1FF, (hwcol)0x7A, (hwcol)0x7F, (hwcol)0x79, (hwcol)0xF9, (hwcol)0x7F,
				  (hwcol)0x70, (hwcol)0x20, (hwcol)0x78, (hwcol)0x08);

	Act(InstallHook,DisplayWin)(DisplayWin, UpdateDisplayWin, &All->FnHookDisplayHW);
	ListWin->MapUnMapHook = InstallRemoveListWinHook;
	
	Act(FillButton,ButtonOK_About)(ButtonOK_About, COD_OK, (udat)15, (udat)11, (udat)0, "   OK   ", (byte)0x2F, (byte)0x28);

	Act(FillButton,ButtonRemove)(ButtonRemove, COD_D_REMOVE, (udat)1, (udat)2, (udat)0, " Remove ", (byte)0x2F, (byte)0x28);
	Act(FillButton,ButtonThis)  (ButtonThis,   COD_D_THIS,   (udat)1, (udat)5, (udat)0, "  This  ", (byte)0x2F, (byte)0x28);

	OptionWin->CurX = 25; OptionWin->CurY = 1;
	Act(WriteRow,OptionWin)(OptionWin, 10, "  X Shadow");
	OptionWin->CurX = 25; OptionWin->CurY = 2;
	Act(WriteRow,OptionWin)(OptionWin, 10, "  Y Shadow");
	
	All->SetUp->Flags |= SETUP_DO_SHADE
#ifdef CONF_OPT_ALWAYSCURSOR
	    | SETUP_ALWAYSCURSOR
#endif
#ifdef CONF_OPT_NOBLINK
	    | SETUP_NOBLINK
#endif
#ifdef CONF_OPT_HIDEMENU
	    | SETUP_HIDEMENU
#endif
#ifdef CONF_OPT_NOSCROLL
	    | SETUP_NOSCROLL
#endif
	    ;

	All->BuiltinMenu=Builtin_Menu;
#ifdef CONF_OPT_HIDEMENU
	HideMenu(TRUE);
#else
	DrawMenuBar(All->FirstScreen, MINDAT, MAXDAT);
#endif

	UpdateOptionWin();
	
	return TRUE;
    }
    return FALSE;
}

