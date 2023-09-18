
//**************************************************************************
//**
//** template.h : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: st_start.h,v $
//** $Revision: 1.5 $
//** $Date: 95/10/11 23:35:33 $
//** $Author: paul $
//**
//**************************************************************************

#ifndef __ST_START__
#define __ST_START__

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------
extern void ST_Init(void);
extern void ST_Done(void);

extern void ST_Progress(void);
extern void ST_NetProgress(void);
extern void ST_NetDone(void);

/* Maximum size of a debug message */
#define	MAX_ST_MSG		256

/* These two doesn't add a '\n' to the message, the caller must add it by himself */
extern void ST_Message(const char *message, ...) __attribute__((__format__(__printf__,1,2)));
extern void ST_RealMessage(const char *message, ...) __attribute__((__format__(__printf__,1,2)));


// PUBLIC DATA DECLARATIONS ------------------------------------------------

#endif	/* __ST_START__ */

