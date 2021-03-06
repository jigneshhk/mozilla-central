/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadPool_h__
#define nsThreadPool_h__

#include "nsIThreadPool.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsEventQueue.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"

class nsThreadPool : public nsIThreadPool, public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSITHREADPOOL
  NS_DECL_NSIRUNNABLE

  nsThreadPool();

private:
  ~nsThreadPool();

  void ShutdownThread(nsIThread *thread);
  nsresult PutEvent(nsIRunnable *event);

  nsCOMArray<nsIThread> mThreads;
  nsEventQueue          mEvents;
  PRUint32              mThreadLimit;
  PRUint32              mIdleThreadLimit;
  PRUint32              mIdleThreadTimeout;
  PRUint32              mIdleCount;
  nsCOMPtr<nsIThreadPoolListener> mListener;
  bool                  mShutdown;
};

#define NS_THREADPOOL_CLASSNAME "nsThreadPool"
#define NS_THREADPOOL_CID                          \
{ /* 547ec2a8-315e-4ec4-888e-6e4264fe90eb */       \
  0x547ec2a8,                                      \
  0x315e,                                          \
  0x4ec4,                                          \
  {0x88, 0x8e, 0x6e, 0x42, 0x64, 0xfe, 0x90, 0xeb} \
}

#endif  // nsThreadPool_h__
