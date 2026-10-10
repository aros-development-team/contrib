/*
**         $Filename: ievents.h $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

/*
	This file is modified from AmiVNC
	
	AmiVNC - Amiga experimental VNC server - Protocol ORL version 3.3
	(c) Stephane Guillard - stephane.guillard@steria.com
	This module is the mouse / key event handler.
	It is largely based on another module written by my friend Denis Spach for his own AVNC server,
	and as it solved a NEWPOINTERPOS problem, it went into AmiVNC as well.
*/

void do_pointer(struct IOStdReq *inputReqBlk, int buttons, int x, int y, struct Screen *screen);
void do_key(struct IOStdReq *inputReqBlk, int dn, int key);
