/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
** C++ access to NSPR locks (PRLock)
*/

#if defined(_RCLOCK_H)
#else
#define _RCLOCK_H

#include "rcbase.h"

#include <prlock.h>

class PR_IMPLEMENT(RCLock): public RCBase
{
public:
    RCLock();
    virtual ~RCLock();

    void Acquire();                 /* non-reentrant */
    void Release();                 /* should be by owning thread */

    friend class RCCondition;

private:
    RCLock(const RCLock&);          /* can't do that */
    void operator=(const RCLock&);  /* nor that */

    PRLock *lock;
};  /* RCLock */

/*
** Class: RCEnter
**
** In scope locks. You can only allocate them on the stack. The language
** will insure that they get released (by calling the destructor) when
** the thread leaves scope, even if via an exception.
*/
class PR_IMPLEMENT(RCEnter)
{
public:
    ~RCEnter();                     /* releases the lock */
    RCEnter(RCLock*);               /* acquires the lock */

private:
    RCLock *lock;

    RCEnter();
    RCEnter(const RCEnter&);
    void operator=(const RCEnter&);

    void *operator new(PRSize) { return NULL; }
    void operator delete(void*) { }
};  /* RCEnter */


inline RCEnter::RCEnter(RCLock* ml) { lock = ml; lock->Acquire(); }
inline RCEnter::~RCEnter() { lock->Release(); lock = NULL; }

#endif /* defined(_RCLOCK_H) */

/* RCLock.h */
