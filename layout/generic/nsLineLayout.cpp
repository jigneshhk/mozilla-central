/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsLayoutAtoms.h"
#include "nsPlaceholderFrame.h"
#include "nsIReflowCommand.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIContent.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h" 
#include "nsINameSpaceManager.h"

#ifdef DEBUG
#undef  NOISY_HORIZONTAL_ALIGN
#undef  NOISY_VERTICAL_ALIGN
#undef   REALLY_NOISY_VERTICAL_ALIGN
#undef  NOISY_REFLOW
#undef   REALLY_NOISY_REFLOW
#undef  NOISY_PUSHING
#undef   REALLY_NOISY_PUSHING
#undef DEBUG_ADD_TEXT
#undef  NOISY_MAX_ELEMENT_SIZE
#undef   REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_CAN_PLACE_FRAME
#undef NOISY_TRIM
#undef REALLY_NOISY_TRIM
#endif

MOZ_DECL_CTOR_COUNTER(nsTextRun);

nsTextRun::nsTextRun()
{
  MOZ_COUNT_CTOR(nsTextRun);
  mNext = nsnull;
}

nsTextRun::~nsTextRun()
{
  MOZ_COUNT_DTOR(nsTextRun);
}

#ifdef DEBUG
void
nsTextRun::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  PRInt32 n = mArray.Count();
  fprintf(out, "%p: count=%d <", this, n);
  for (i = 0; i < n; i++) {
    nsIFrame* text = (nsIFrame*) mArray.ElementAt(i);
    nsAutoString tmp;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(text->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(tmp);
      fputs(tmp, out);
    }
    printf("@%p ", text);
  }
  fputs(">\n", out);
}
#endif

//----------------------------------------------------------------------

#define PLACED_LEFT  0x1
#define PLACED_RIGHT 0x2

MOZ_DECL_CTOR_COUNTER(nsLineLayout);

nsLineLayout::nsLineLayout(nsIPresContext* aPresContext,
                           nsISpaceManager* aSpaceManager,
                           const nsHTMLReflowState* aOuterReflowState,
                           PRBool aComputeMaxElementSize)
  : mPresContext(aPresContext),
    mSpaceManager(aSpaceManager),
    mBlockReflowState(aOuterReflowState),
    mBlockRS(nsnull),/* XXX temporary */
    mMinLineHeight(0),
    mComputeMaxElementSize(aComputeMaxElementSize)
{
  MOZ_COUNT_CTOR(nsLineLayout);

  // Stash away some style data that we need
  aOuterReflowState->frame->GetStyleData(eStyleStruct_Text,
                                         (const nsStyleStruct*&) mStyleText);
  mTextAlign = mStyleText->mTextAlign;
  mLineNumber = 0;
  mColumn = 0;
  mEndsInWhiteSpace = PR_TRUE;
  mUnderstandsWhiteSpace = PR_FALSE;
  mTextStartsWithNBSP = PR_FALSE;
  mFirstLetterStyleOK = PR_FALSE;
  mIsTopOfPage = PR_FALSE;
  mUpdatedBand = PR_FALSE;
  mPlacedFloaters = 0;
  mImpactedByFloaters = PR_FALSE;
  mLastFloaterWasLetterFrame = PR_FALSE;
  mTotalPlacedFrames = 0;
  mTopEdge = mBottomEdge = 0;
  mReflowTextRuns = nsnull;
  mTextRun = nsnull;

  // Instead of always pre-initializing the free-lists for frames and
  // spans, we do it on demand so that situations that only use a few
  // frames and spans won't waste alot of time in unneeded
  // initialization.
  mInitialFramesFreed = mInitialSpansFreed = 0;
  mFrameFreeList = nsnull;
  mSpanFreeList = nsnull;

  mCurrentSpan = mRootSpan = nsnull;
  mSpanDepth = 0;

  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
  mKnowStrictMode = PR_FALSE;
}

nsLineLayout::nsLineLayout(nsIPresContext* aPresContext)
  : mPresContext(aPresContext)
{
  MOZ_COUNT_CTOR(nsLineLayout);

  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
  mRootSpan = nsnull;
  mSpanFreeList = nsnull;
  mFrameFreeList = nsnull;
}

nsLineLayout::~nsLineLayout()
{
  MOZ_COUNT_DTOR(nsLineLayout);

  NS_ASSERTION(nsnull == mRootSpan, "bad line-layout user");
  nsTextRun::DeleteTextRuns(mTextRuns);

  // Free up all of the per-span-data items that were allocated on the heap
  PerSpanData* psd = mSpanFreeList;
  while (nsnull != psd) {
    PerSpanData* nextSpan = psd->mNextFreeSpan;
    if ((psd < &mSpanDataBuf[0]) ||
        (psd >= &mSpanDataBuf[NS_LINELAYOUT_NUM_SPANS])) {
      delete psd;
    }
    psd = nextSpan;
  }

  // Free up all of the per-frame-data items that were allocated on the heap
  PerFrameData* pfd = mFrameFreeList;
  while (nsnull != pfd) {
    PerFrameData* nextFrame = pfd->mNext;
    if ((pfd < &mFrameDataBuf[0]) ||
        (pfd >= &mFrameDataBuf[NS_LINELAYOUT_NUM_FRAMES])) {
      delete pfd;
    }
    pfd = nextFrame;
  }
}

PRBool
nsLineLayout::InStrictMode()
{
// XXX For now this is always part of the build until harishd lands
// the dtd auto-detect code...
#if 1
  static PRBool forceStrictMode = PR_FALSE;
#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_BEOS)
  {
    static int firstTime = 1;
    if (firstTime) {
      if (getenv("GECKO_FORCE_STRICT_MODE")) {
        forceStrictMode = PR_TRUE;
      }
      firstTime = 0;
    }
  }
#endif
  if (forceStrictMode) {
    mKnowStrictMode = PR_TRUE;
    mInStrictMode = PR_TRUE;
    return mInStrictMode;
  }
#endif
  if (!mKnowStrictMode) {
    mKnowStrictMode = PR_TRUE;
    mInStrictMode = PR_TRUE;

    // Dig up the compatabilty mode out of the underlying document, if
    // we can find it.
    if (mBlockReflowState->frame) {
      nsCOMPtr<nsIContent> content;
      mBlockReflowState->frame->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIDocument> doc;
        content->GetDocument(*getter_AddRefs(doc));
        if (doc) {
          nsCOMPtr<nsIHTMLDocument> hdoc(do_QueryInterface(doc));
          if (hdoc) {
            nsDTDMode mode;
            hdoc->GetDTDMode(mode);
            if (eDTDMode_NoQuirks != mode) {
              mInStrictMode = PR_FALSE;
            }
          }
        }
      }
    }
  }
  return mInStrictMode;
}

void
nsLineLayout::BeginLineReflow(nscoord aX, nscoord aY,
                              nscoord aWidth, nscoord aHeight,
                              PRBool aImpactedByFloaters,
                              PRBool aIsTopOfPage)
{
  NS_ASSERTION(nsnull == mRootSpan, "bad linelayout user");
#ifdef DEBUG
  if ((aWidth != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aWidth)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aHeight)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": BeginLineReflow: %d,%d,%d,%d %s\n",
         aX, aY, aWidth, aHeight,
         aIsTopOfPage ? "top-of-page" : "");
#endif
#ifdef DEBUG
  mSpansAllocated = mSpansFreed = mFramesAllocated = mFramesFreed = 0;
#endif

  mColumn = 0;
  mEndsInWhiteSpace = PR_TRUE;
  mUnderstandsWhiteSpace = PR_FALSE;
  mTextStartsWithNBSP = PR_FALSE;
  mFirstLetterStyleOK = PR_FALSE;
  mIsTopOfPage = aIsTopOfPage;
  mUpdatedBand = PR_FALSE;
  mPlacedFloaters = 0;
  mImpactedByFloaters = aImpactedByFloaters;
  mTotalPlacedFrames = 0;
  mCanPlaceFloater = PR_TRUE;
  mLineEndsInBR = PR_FALSE;
  mSpanDepth = 0;
  mMaxTopBoxHeight = mMaxBottomBoxHeight = 0;

  ForgetWordFrames();

  PerSpanData* psd;
  NewPerSpanData(&psd);
  mCurrentSpan = mRootSpan = psd;
  psd->mReflowState = mBlockReflowState;
  psd->mLeftEdge = aX;
  psd->mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    psd->mRightEdge = aX + aWidth;
  }

  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }

  switch (mStyleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    psd->mNoWrap = PR_TRUE;
    break;
  default:
    psd->mNoWrap = PR_FALSE;
    break;
  }
  psd->mDirection = mBlockReflowState->mStyleDisplay->mDirection;
  psd->mChangedFrameDirection = PR_FALSE;
}

void
nsLineLayout::EndLineReflow()
{
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": EndLineReflow: width=%d\n", mRootSpan->mX - mRootSpan->mLeftEdge);
#endif

  FreeSpan(mRootSpan);
  mCurrentSpan = mRootSpan = nsnull;

  NS_ASSERTION(mSpansAllocated == mSpansFreed, "leak");
  NS_ASSERTION(mFramesAllocated == mFramesFreed, "leak");

#if 0
  static PRInt32 maxSpansAllocated = NS_LINELAYOUT_NUM_SPANS;
  static PRInt32 maxFramesAllocated = NS_LINELAYOUT_NUM_FRAMES;
  if (mSpansAllocated > maxSpansAllocated) {
    printf("XXX: saw a line with %d spans\n", mSpansAllocated);
    maxSpansAllocated = mSpansAllocated;
  }
  if (mFramesAllocated > maxFramesAllocated) {
    printf("XXX: saw a line with %d frames\n", mFramesAllocated);
    maxFramesAllocated = mFramesAllocated;
  }
#endif
}

// XXX swtich to a single mAvailLineWidth that we adjust as each frame
// on the line is placed. Each span can still have a per-span mX that
// tracks where a child frame is going in its span; they don't need a
// per-span mLeftEdge?

void
nsLineLayout::UpdateBand(nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight,
                         PRBool aPlacedLeftFloater,
                         nsIFrame* aFloaterFrame)
{
  PerSpanData* psd = mRootSpan;
  NS_PRECONDITION(psd->mX == psd->mLeftEdge, "update-band called late");
#ifdef DEBUG
  if ((aWidth != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aWidth)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aHeight)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif

  // Compute the difference between last times width and the new width
  nscoord deltaWidth = 0;
  if (NS_UNCONSTRAINEDSIZE != psd->mRightEdge) {
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aWidth, "switched constraints");
    nscoord oldWidth = psd->mRightEdge - psd->mLeftEdge;
    deltaWidth = aWidth - oldWidth;
  }
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": UpdateBand: %d,%d,%d,%d deltaWidth=%d %s floater\n",
         aX, aY, aWidth, aHeight, deltaWidth,
         aPlacedLeftFloater ? "left" : "right");
#endif

  psd->mLeftEdge = aX;
  psd->mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    psd->mRightEdge = aX + aWidth;
  }
  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
  mUpdatedBand = PR_TRUE;
  mPlacedFloaters |= (aPlacedLeftFloater ? PLACED_LEFT : PLACED_RIGHT);
  mImpactedByFloaters = PR_TRUE;

  nsCOMPtr<nsIAtom> frameType;
  aFloaterFrame->GetFrameType(getter_AddRefs(frameType));
  mLastFloaterWasLetterFrame = nsLayoutAtoms::letterFrame == frameType.get();

  // Now update all of the open spans...
  mRootSpan->mContainsFloater = PR_TRUE;              // make sure mRootSpan gets updated too
  psd = mCurrentSpan;
  while (psd != mRootSpan) {
    NS_ASSERTION(nsnull != psd, "null ptr");
    if (nsnull == psd) {
      break;
    }
    NS_ASSERTION(psd->mX == psd->mLeftEdge, "bad floater placement");
    if (NS_UNCONSTRAINEDSIZE == aWidth) {
      psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
    }
    else {
      psd->mRightEdge += deltaWidth;
    }
    psd->mContainsFloater = PR_TRUE;
#ifdef NOISY_REFLOW
    printf("  span %p: oldRightEdge=%d newRightEdge=%d\n",
           psd, psd->mRightEdge - deltaWidth, psd->mRightEdge);
#endif
    psd = psd->mParent;
  }
}

// Note: Only adjust the outermost frames (the ones that are direct
// children of the block), not the ones in the child spans. The reason
// is simple: the frames in the spans have coordinates local to their
// parent therefore they are moved when their parent span is moved.
void
nsLineLayout::UpdateFrames()
{
  NS_ASSERTION(nsnull != mRootSpan, "UpdateFrames with no active spans");

  PerSpanData* psd = mRootSpan;
  if (NS_STYLE_DIRECTION_LTR == psd->mDirection) {
    if (PLACED_LEFT & mPlacedFloaters) {
      PerFrameData* pfd = psd->mFirstFrame;
      while (nsnull != pfd) {
        pfd->mBounds.x = psd->mX;
        pfd = pfd->mNext;
      }
    }
  }
  else if (PLACED_RIGHT & mPlacedFloaters) {
    // XXX handle DIR=right-to-left
  }
}

nsresult
nsLineLayout::NewPerSpanData(PerSpanData** aResult)
{
  PerSpanData* psd = mSpanFreeList;
  if (nsnull == psd) {
    if (mInitialSpansFreed < NS_LINELAYOUT_NUM_SPANS) {
      // use one of the ones defined in our struct...
      psd = &mSpanDataBuf[mInitialSpansFreed++];
    }
    else {
      psd = new PerSpanData;
      if (nsnull == psd) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  else {
    mSpanFreeList = psd->mNextFreeSpan;
  }
  psd->mParent = nsnull;
  psd->mFrame = nsnull;
  psd->mFirstFrame = nsnull;
  psd->mLastFrame = nsnull;
  psd->mContainsFloater = PR_FALSE;
  psd->mZeroEffectiveSpanBox = PR_FALSE;
  psd->mIgnoreMinLineHeight = PR_FALSE;

#ifdef DEBUG
  mSpansAllocated++;
#endif
  *aResult = psd;
  return NS_OK;
}

nsresult
nsLineLayout::BeginSpan(nsIFrame* aFrame,
                        const nsHTMLReflowState* aSpanReflowState,
                        nscoord aLeftEdge,
                        nscoord aRightEdge)
{
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth+1);
  nsFrame::ListTag(stdout, aFrame);
  printf(": BeginSpan leftEdge=%d rightEdge=%d\n", aLeftEdge, aRightEdge);
#endif

  PerSpanData* psd;
  nsresult rv = NewPerSpanData(&psd);
  if (NS_SUCCEEDED(rv)) {
    // Link up span frame's pfd to point to its child span data
    PerFrameData* pfd = mCurrentSpan->mLastFrame;
    NS_ASSERTION(pfd->mFrame == aFrame, "huh?");
    pfd->mSpan = psd;

    // Init new span
    psd->mFrame = pfd;
    psd->mParent = mCurrentSpan;
    psd->mReflowState = aSpanReflowState;
    psd->mLeftEdge = aLeftEdge;
    psd->mX = aLeftEdge;
    psd->mRightEdge = aRightEdge;

    const nsStyleText* styleText;
    aSpanReflowState->frame->GetStyleData(eStyleStruct_Text,
                                          (const nsStyleStruct*&) styleText);
    switch (styleText->mWhiteSpace) {
    case NS_STYLE_WHITESPACE_PRE:
    case NS_STYLE_WHITESPACE_NOWRAP:
        psd->mNoWrap = PR_TRUE;
        break;
    default:
        psd->mNoWrap = PR_FALSE;
        break;
    }
    psd->mDirection = aSpanReflowState->mStyleDisplay->mDirection;
    psd->mChangedFrameDirection = PR_FALSE;

    // Switch to new span
    mCurrentSpan = psd;
    mSpanDepth++;
  }
  return rv;
}

void
nsLineLayout::EndSpan(nsIFrame* aFrame,
                      nsSize& aSizeResult,
                      nsSize* aMaxElementSize)
{
  NS_ASSERTION(mSpanDepth > 0, "end-span without begin-span");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  nsFrame::ListTag(stdout, aFrame);
  printf(": EndSpan width=%d\n", mCurrentSpan->mX - mCurrentSpan->mLeftEdge);
#endif
  PerSpanData* psd = mCurrentSpan;
  nscoord width = 0;
  nscoord maxHeight = 0;
  nscoord maxElementWidth = 0;
  nscoord maxElementHeight = 0;
  if (nsnull != psd->mLastFrame) {
    width = psd->mX - psd->mLeftEdge;
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      if (pfd->mBounds.height > maxHeight) maxHeight = pfd->mBounds.height;

      // Compute max-element-size if necessary
      if (aMaxElementSize) {
        nscoord mw = pfd->mMaxElementSize.width +
          pfd->mMargin.left + pfd->mMargin.right;
        if (maxElementWidth < mw) {
          maxElementWidth = mw;
        }
        nscoord mh = pfd->mMaxElementSize.height +
          pfd->mMargin.top + pfd->mMargin.bottom;
        if (maxElementHeight < mh) {
          maxElementHeight = mh;
        }
      }
      pfd = pfd->mNext;
    }
  }
  aSizeResult.width = width;
  aSizeResult.height = maxHeight;
  if (aMaxElementSize) {
    if (psd->mNoWrap) {
      // When we have a non-breakable span, it's max-element-size
      // width is its entire width.
      aMaxElementSize->width = width;
      aMaxElementSize->height = maxHeight;
    }
    else {
      aMaxElementSize->width = maxElementWidth;
      aMaxElementSize->height = maxElementHeight;
    }
  }

  mSpanDepth--;
  mCurrentSpan->mReflowState = nsnull;  // no longer valid so null it out!
  mCurrentSpan = mCurrentSpan->mParent;
}

PRInt32
nsLineLayout::GetCurrentSpanCount() const
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");
  PRInt32 count = 0;
  PerFrameData* pfd = mRootSpan->mFirstFrame;
  while (nsnull != pfd) {
    count++;
    pfd = pfd->mNext;
  }
  return count;
}

void
nsLineLayout::SplitLineTo(PRInt32 aNewCount)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");

#ifdef REALLY_NOISY_PUSHING
  printf("SplitLineTo %d (current count=%d); before:\n", aNewCount,
         GetCurrentSpanCount());
  DumpPerSpanData(mRootSpan, 1);
#endif
  PerSpanData* psd = mRootSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (--aNewCount == 0) {
      // Truncate list at pfd (we keep pfd, but anything following is freed)
      PerFrameData* next = pfd->mNext;
      pfd->mNext = nsnull;
      psd->mLastFrame = pfd;

      // Now release all of the frames following pfd
      pfd = next;
      while (nsnull != pfd) {
        next = pfd->mNext;
        pfd->mNext = mFrameFreeList;
        mFrameFreeList = pfd;
#ifdef DEBUG
        mFramesFreed++;
#endif
        if (nsnull != pfd->mSpan) {
          FreeSpan(pfd->mSpan);
        }
        pfd = next;
      }
      break;
    }
    pfd = pfd->mNext;
  }
#ifdef NOISY_PUSHING
  printf("SplitLineTo %d (current count=%d); after:\n", aNewCount,
         GetCurrentSpanCount());
  DumpPerSpanData(mRootSpan, 1);
#endif
}

void
nsLineLayout::PushFrame(nsIFrame* aFrame)
{
  PerSpanData* psd = mCurrentSpan;
  NS_ASSERTION(psd->mLastFrame->mFrame == aFrame, "pushing non-last frame");

#ifdef REALLY_NOISY_PUSHING
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("PushFrame %p, before:\n", psd);
  DumpPerSpanData(psd, 1);
#endif

  // Take the last frame off of the span's frame list
  PerFrameData* pfd = psd->mLastFrame;
  if (pfd == psd->mFirstFrame) {
    // We are pushing away the only frame...empty the list
    psd->mFirstFrame = nsnull;
    psd->mLastFrame = nsnull;
  }
  else {
    PerFrameData* prevFrame = pfd->mPrev;
    prevFrame->mNext = nsnull;
    psd->mLastFrame = prevFrame;
  }

  // Now free it, and if it has a span, free that too
  pfd->mNext = mFrameFreeList;
  mFrameFreeList = pfd;
#ifdef DEBUG
  mFramesFreed++;
#endif
  if (nsnull != pfd->mSpan) {
    FreeSpan(pfd->mSpan);
  }
#ifdef NOISY_PUSHING
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("PushFrame: %p after:\n", psd);
  DumpPerSpanData(psd, 1);
#endif
}

void
nsLineLayout::FreeSpan(PerSpanData* psd)
{
  // Free its frames
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (nsnull != pfd->mSpan) {
      FreeSpan(pfd->mSpan);
    }
    PerFrameData* next = pfd->mNext;
    pfd->mNext = mFrameFreeList;
    mFrameFreeList = pfd;
#ifdef DEBUG
    mFramesFreed++;
#endif
    pfd = next;
  }

  // Now put the span on the free list since its free too
  psd->mNextFreeSpan = mSpanFreeList;
  mSpanFreeList = psd;
#ifdef DEBUG
  mSpansFreed++;
#endif
}

PRBool
nsLineLayout::IsZeroHeight()
{
  PerSpanData* psd = mCurrentSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (0 != pfd->mBounds.height) {
      return PR_FALSE;
    }
    pfd = pfd->mNext;
  }
  return PR_TRUE;
}

nsresult
nsLineLayout::NewPerFrameData(PerFrameData** aResult)
{
  PerFrameData* pfd = mFrameFreeList;
  if (nsnull == pfd) {
    if (mInitialFramesFreed < NS_LINELAYOUT_NUM_FRAMES) {
      // use one of the ones defined in our struct...
      pfd = &mFrameDataBuf[mInitialFramesFreed++];
    }
    else {
      pfd = new PerFrameData;
      if (nsnull == pfd) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  else {
    mFrameFreeList = pfd->mNext;
  }
  pfd->mSpan = nsnull;
  pfd->mNext = nsnull;
  pfd->mPrev = nsnull;
  pfd->mFrame = nsnull;
  pfd->mRelativePos = PR_FALSE;
  pfd->mIsTextFrame = PR_FALSE;
  pfd->mIsNonEmptyTextFrame = PR_FALSE;
  pfd->mIsLetterFrame = PR_FALSE;
  pfd->mIsSticky = PR_FALSE;


#ifdef DEBUG
  pfd->mVerticalAlign = 0xFF;
  pfd->mRelativePos = PRBool(0xFF);
  mFramesAllocated++;
#endif
  *aResult = pfd;
  return NS_OK;
}

PRBool
nsLineLayout::CanPlaceFloaterNow() const
{
  return mCanPlaceFloater;
}

PRBool
nsLineLayout::LineIsEmpty() const
{
  return 0 == mTotalPlacedFrames;
}

PRBool
nsLineLayout::LineIsBreakable() const
{
  if ((0 != mTotalPlacedFrames) || mImpactedByFloaters) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsLineLayout::ReflowFrame(nsIFrame* aFrame,
                          nsIFrame** aNextRCFrame,
                          nsReflowStatus& aReflowStatus,
                          nsHTMLReflowMetrics* aMetrics,
                          PRBool& aPushedFrame)
{
  // Initialize OUT parameter
  aPushedFrame = PR_FALSE;

  PerFrameData* pfd;
  nsresult rv = NewPerFrameData(&pfd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  PerSpanData* psd = mCurrentSpan;
  psd->AppendFrame(pfd);

#ifdef REALLY_NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("%p: Begin ReflowFrame pfd=%p ", psd, pfd);
  nsFrame::ListTag(stdout, aFrame);
  printf("\n");
#endif

  // Compute the available size for the frame. This available width
  // includes room for the side margins and for the text-indent.
  nsSize availSize;
  if (NS_UNCONSTRAINEDSIZE == psd->mRightEdge) {
    availSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.width = psd->mRightEdge - psd->mX;
    if (psd->mNoWrap) {
      // Make up a width to use for reflowing into.  XXX what value to
      // use? for tables, we want to limit it; for other elements
      // (e.g. text) it can be unlimited...
      availSize.width = psd->mReflowState->availableWidth;
    }
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    availSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.height = mBottomEdge - mTopEdge;
  }

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(&state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }  
  else if (*aNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    *aNextRCFrame = nsnull;
  }
  else if (psd->mReflowState->reason == eReflowReason_StyleChange) {
    reason = eReflowReason_StyleChange;
  }
  else if (psd->mReflowState->reason == eReflowReason_Dirty) {
    if (state & NS_FRAME_IS_DIRTY)
      reason = eReflowReason_Dirty;
  }
  else {
    const nsHTMLReflowState* rs = psd->mReflowState;
    if (rs->reason == eReflowReason_Incremental) {
      // If the incremental reflow command is a StyleChanged reflow and
      // it's target is the current span, then make sure we send
      // StyleChange reflow reasons down to the children so that they
      // don't over-optimize their reflow.
      nsIReflowCommand* rc = rs->reflowCommand;
      if (rc) {
        nsIReflowCommand::ReflowType type;
        rc->GetType(type);
        if (type == nsIReflowCommand::StyleChanged) {
          nsIFrame* parentFrame = psd->mFrame
            ? psd->mFrame->mFrame
            : mBlockReflowState->frame;
          nsIFrame* target;
          rc->GetTarget(target);
          if (target == parentFrame) {
            reason = eReflowReason_StyleChange;
          }
        }
        else if (type == nsIReflowCommand::ReflowDirty &&
                 (state & NS_FRAME_IS_DIRTY)) {          
          reason = eReflowReason_Dirty;
        }
      }
    }
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, *psd->mReflowState, aFrame,
                                availSize, reason);
  reflowState.mLineLayout = this;
  reflowState.isTopOfPage = mIsTopOfPage;
  mUnderstandsWhiteSpace = PR_FALSE;
  mTextStartsWithNBSP = PR_FALSE;

  // Stash copies of some of the computed state away for later
  // (vertical alignment, for example)
  pfd->mFrame = aFrame;
  pfd->mMargin = reflowState.mComputedMargin;
  pfd->mBorderPadding = reflowState.mComputedBorderPadding;
  pfd->mFrameType = reflowState.mFrameType;
  pfd->mRelativePos =
    reflowState.mStylePosition->mPosition == NS_STYLE_POSITION_RELATIVE;
  if (pfd->mRelativePos) {
    pfd->mOffsets = reflowState.mComputedOffsets;
  }

  // We want to guarantee that we always make progress when
  // formatting. Therefore, if the object being placed on the line is
  // too big for the line, but it is the only thing on the line
  // (including counting floaters) then we go ahead and place it
  // anyway. Its also true that if the object is a part of a larger
  // object (a multiple frame word) then we will place it on the line
  // too.
  //
  // Capture this state *before* we reflow the frame in case it clears
  // the state out. We need to know how to treat the current frame
  // when breaking.
  PRBool notSafeToBreak = CanPlaceFloaterNow() || InWord();

  // Apply left margins (as appropriate) to the frame computing the
  // new starting x,y coordinates for the frame.
  ApplyLeftMargin(pfd, reflowState);

  // Let frame know that are reflowing it. Note that we don't bother
  // positioning the frame yet, because we're probably going to end up
  // moving it when we do the vertical alignment
  nscoord x = pfd->mBounds.x;
  nscoord y = pfd->mBounds.y;

  aFrame->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the current spans
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nsSize innerMaxElementSize;
  nsHTMLReflowMetrics metrics(mComputeMaxElementSize
                              ? &innerMaxElementSize
                              : nsnull);
#ifdef DEBUG
  metrics.width = nscoord(0xdeadbeef);
  metrics.height = nscoord(0xdeadbeef);
  metrics.ascent = nscoord(0xdeadbeef);
  metrics.descent = nscoord(0xdeadbeef);
  if (mComputeMaxElementSize) {
    metrics.maxElementSize->width = nscoord(0xdeadbeef);
    metrics.maxElementSize->height = nscoord(0xdeadbeef);
  }
#endif
  nscoord tx = x - psd->mReflowState->mComputedBorderPadding.left;
  nscoord ty = y - psd->mReflowState->mComputedBorderPadding.top;
  mSpaceManager->Translate(tx, ty);

  pfd->mIsTextFrame = PR_FALSE;
  pfd->mIsLetterFrame = PR_FALSE;
  pfd->mIsNonEmptyTextFrame = PR_FALSE;
  pfd->mIsSticky = PR_FALSE;

  aFrame->Reflow(mPresContext, metrics, reflowState, aReflowStatus);

  // XXX See if the frame is a placeholderFrame and if it is process
  // the floater.
  nsIAtom* frameType;
  aFrame->GetFrameType(&frameType);
  if (frameType) {
    if (frameType == nsLayoutAtoms::placeholderFrame) {
      nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)aFrame)->GetOutOfFlowFrame();
      if (outOfFlowFrame) {
        const nsStylePosition*  position;

        // Make sure it's floated and not absolutely positioned
        outOfFlowFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
        if (!position->IsAbsolutelyPositioned()) {
          if (eReflowReason_Incremental == reason) {
            InitFloater((nsPlaceholderFrame*)aFrame);
          }
          else {
            AddFloater((nsPlaceholderFrame*)aFrame);
          }
          nsIAtom* oofft;
          outOfFlowFrame->GetFrameType(&oofft);
          if (oofft) {
            if (oofft == nsLayoutAtoms::letterFrame) {
              mFirstLetterStyleOK = PR_FALSE;
            }
            NS_RELEASE(oofft);
          }
        }
      }
    }
    else if (frameType == nsLayoutAtoms::textFrame) {
      // Note non-empty text-frames for inline frame compatability hackery
      pfd->mIsTextFrame = PR_TRUE;
      if (metrics.width) {
        pfd->mIsNonEmptyTextFrame = PR_TRUE;
      }
    }
    else if (frameType == nsLayoutAtoms::letterFrame) {
      pfd->mIsLetterFrame = PR_TRUE;
    }
    NS_RELEASE(frameType);
  }

  mSpaceManager->Translate(-tx, -ty);

#ifdef DEBUG
  // Note: break-before means ignore the reflow metrics since the
  // frame will be reflowed another time.
  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    if (CRAZY_WIDTH(metrics.width) || CRAZY_HEIGHT(metrics.height)) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" metrics=%d,%d!\n", metrics.width, metrics.height);
    }
    if (mComputeMaxElementSize &&
        ((nscoord(0xdeadbeef) == metrics.maxElementSize->width) ||
         (nscoord(0xdeadbeef) == metrics.maxElementSize->height))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set max-element-size!\n");
      metrics.maxElementSize->width = 0;
      metrics.maxElementSize->height = 0;
    }
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
    // Note: there are common reflow situations where this *correctly*
    // occurs; so only enable this debug noise when you really need to
    // analyze in detail.
    if (mComputeMaxElementSize &&
        ((metrics.maxElementSize->width > metrics.width) ||
         (metrics.maxElementSize->height > metrics.height))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(": WARNING: maxElementSize=%d,%d > metrics=%d,%d\n",
             metrics.maxElementSize->width,
             metrics.maxElementSize->height,
             metrics.width, metrics.height);
    }
#endif
    if ((metrics.width == nscoord(0xdeadbeef)) ||
        (metrics.height == nscoord(0xdeadbeef)) ||
        (metrics.ascent == nscoord(0xdeadbeef)) ||
        (metrics.descent == nscoord(0xdeadbeef))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set whad %d,%d,%d,%d!\n", metrics.width, metrics.height,
             metrics.ascent, metrics.descent);
    }
  }
#endif
#ifdef NOISY_MAX_ELEMENT_SIZE
  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    if (mComputeMaxElementSize) {
      printf("  ");
      nsFrame::ListTag(stdout, aFrame);
      printf(": maxElementSize=%d,%d wh=%d,%d,\n",
             metrics.maxElementSize->width,
             metrics.maxElementSize->height,
             metrics.width, metrics.height);
    }
  }
#endif

  aFrame->GetFrameState(&state);
  if (NS_FRAME_OUTSIDE_CHILDREN & state) {
    pfd->mCombinedArea = metrics.mOverflowArea;
  }
  else {
    pfd->mCombinedArea.x = 0;
    pfd->mCombinedArea.y = 0;
    pfd->mCombinedArea.width = metrics.width;
    pfd->mCombinedArea.height = metrics.height;
  }
  pfd->mBounds.width = metrics.width;
  pfd->mBounds.height = metrics.height;
  if (mComputeMaxElementSize) {
    pfd->mMaxElementSize = *metrics.maxElementSize;
  }

  // Size the frame and size its view (if it has one)
  aFrame->SizeTo(mPresContext, metrics.width, metrics.height);
  nsIView*  view;
  aFrame->GetView(mPresContext, &view);
  if (view) {
    nsIViewManager  *vm;

    view->GetViewManager(vm);
    vm->ResizeView(view, metrics.width, metrics.height);
    NS_RELEASE(vm);
  }

  // Tell the frame that we're done reflowing it
  aFrame->DidReflow(mPresContext, NS_FRAME_REFLOW_FINISHED);

  if (aMetrics) {
    *aMetrics = metrics;
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      nsIFrame* kidNextInFlow;
      aFrame->GetNextInFlow(&kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsHTMLContainerFrame* parent;
        aFrame->GetParent((nsIFrame**) &parent);
        parent->DeleteChildsNextInFlow(mPresContext, aFrame);
      }
    }

    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(pfd, reflowState, notSafeToBreak, metrics, aReflowStatus)) {
      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(pfd, metrics);
      PerSpanData* span = pfd->mSpan;
      if (span) {
        // The frame we just finished reflowing is an inline
        // container.  It needs its child frames vertically aligned,
        // so do most of it now.
        VerticalAlignFrames(span);
      }
    }
    else {
      PushFrame(aFrame);
      aPushedFrame = PR_TRUE;
    }
  }
  else {
    PushFrame(aFrame);
  }
  mTextStartsWithNBSP = PR_FALSE;           // reset for next time

#ifdef REALLY_NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("End ReflowFrame ");
  nsFrame::ListTag(stdout, aFrame);
  printf(" status=%x\n", aReflowStatus);
#endif
  return rv;
}

void
nsLineLayout::ApplyLeftMargin(PerFrameData* pfd,
                              nsHTMLReflowState& aReflowState)
{
  // If this is the first frame in the block, and its the first line
  // of a block then see if the text-indent property amounts to
  // anything.
  nscoord indent = 0;
  if (InBlockContext() && (0 == mLineNumber) && CanPlaceFloaterNow()) {
    nsStyleUnit unit = mStyleText->mTextIndent.GetUnit();
    if (eStyleUnit_Coord == unit) {
      indent = mStyleText->mTextIndent.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) {
      nscoord width =
        nsHTMLReflowState::GetContainingBlockContentWidth(mBlockReflowState->parentReflowState);
      if (0 != width) {
        indent = nscoord(mStyleText->mTextIndent.GetPercentValue() * width);
      }
    }
  }

  // Adjust available width to account for the indent and the margins
  aReflowState.availableWidth -= indent + pfd->mMargin.left +
    pfd->mMargin.right;

  // NOTE: While the x coordinate remains relative to the parent span,
  // the y coordinate is fixed at the top edge for the line. During
  // VerticalAlignFrames we will repair this so that the y coordinate
  // is properly set and relative to the appropriate span.
  PerSpanData* psd = mCurrentSpan;
  pfd->mBounds.x = psd->mX + indent;
  pfd->mBounds.y = mTopEdge;

  // Compute left margin
  nsIFrame* prevInFlow;
  switch (aReflowState.mStyleDisplay->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
  case NS_STYLE_FLOAT_RIGHT:
    // When something is floated, its margins are applied there
    // not here.
    break;

  case NS_STYLE_FLOAT_NONE:
    // Only apply left-margin on the first-in flow for inline frames
    pfd->mFrame->GetPrevInFlow(&prevInFlow);
    if (nsnull != prevInFlow) {
      // Zero this out so that when we compute the max-element-size
      // of the frame we will properly avoid adding in the left
      // margin.
      pfd->mMargin.left = 0;
    }
    pfd->mBounds.x += pfd->mMargin.left;
    break;
  }
}

/**
 * See if the frame can be placed now that we know it's desired size.
 * We can always place the frame if the line is empty. Note that we
 * know that the reflow-status is not a break-before because if it was
 * ReflowFrame above would have returned false, preventing this method
 * from being called. The logic in this method assumes that.
 *
 * Note that there is no check against the Y coordinate because we
 * assume that the caller will take care of that.
 */
PRBool
nsLineLayout::CanPlaceFrame(PerFrameData* pfd,
                            const nsHTMLReflowState& aReflowState,
                            PRBool aNotSafeToBreak,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  // Compute right margin to use
  nscoord rightMargin = 0;
  if (0 != pfd->mBounds.width) {
    switch (aReflowState.mStyleDisplay->mFloats) {
      default:
        NS_NOTYETIMPLEMENTED("Unsupported floater type");
        // FALL THROUGH

      case NS_STYLE_FLOAT_LEFT:
      case NS_STYLE_FLOAT_RIGHT:
        // When something is floated, its margins are applied there
        // not here.
        break;

      case NS_STYLE_FLOAT_NONE:
        // Only apply right margin for the last-in-flow
        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
          // Zero this out so that when we compute the
          // max-element-size of the frame we will properly avoid
          // adding in the right margin.
          pfd->mMargin.right = 0;
        }
        rightMargin = pfd->mMargin.right;
        break;
    }
  }
  pfd->mMargin.right = rightMargin;

  PerSpanData* psd = mCurrentSpan;
  if (psd->mNoWrap) {
    // When wrapping is off, everything fits.
    return PR_TRUE;
  }

#ifdef NOISY_CAN_PLACE_FRAME
  if (nsnull != psd->mFrame) {
    nsFrame::ListTag(stdout, psd->mFrame->mFrame);
  }
  else {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
  } 
  printf(": aNotSafeToBreak=%s frame=", aNotSafeToBreak ? "true" : "false");
  nsFrame::ListTag(stdout, pfd->mFrame);
  printf(" frameWidth=%d\n", pfd->mBounds.XMost() + rightMargin - psd->mX);
#endif

  // Set outside to PR_TRUE if the result of the reflow leads to the
  // frame sticking outside of our available area.
  PRBool outside = pfd->mBounds.XMost() + rightMargin > psd->mRightEdge;
  if (!outside) {
    // If it fits, it fits
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> inside\n");
#endif
    return PR_TRUE;
  }

  // When it doesn't fit, check for a few special conditions where we
  // allow it to fit anyway.
  if (0 == pfd->mMargin.left + pfd->mBounds.width + rightMargin) {
    // Empty frames always fit right where they are
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> empty frame fits\n");
#endif
    return PR_TRUE;
  }

  if (aNotSafeToBreak) {
    // There are no frames on the line or we are in the first word on
    // the line. If the line isn't impacted by a floater then the
    // current frame fits.
    if (!mImpactedByFloaters) {
#ifdef NOISY_CAN_PLACE_FRAME
      printf("   ==> not-safe and not-impacted fits: ");
      while (nsnull != psd) {
        printf("<psd=%p x=%d left=%d> ", psd, psd->mX, psd->mLeftEdge);
        psd = psd->mParent;
      }
      printf("\n");
#endif
      return PR_TRUE;
    }
    else if (mLastFloaterWasLetterFrame) {
      // Another special case: see if the floater is a letter
      // frame. If it is, then allow the frame next to it to fit.
      if (pfd->mIsNonEmptyTextFrame) {
        // This must be the first piece of non-empty text (because
        // aNotSafeToBreak is true) or its a piece of text that is
        // part of a larger word.
        pfd->mIsSticky = PR_TRUE;
      }
      else if (pfd->mSpan) {
        PerFrameData* pf = pfd->mSpan->mFirstFrame;
        while (pf) {
          if (pf->mIsSticky) {
            // If one of the spans children was sticky then the span
            // itself is sticky.
            pfd->mIsSticky = PR_TRUE;
          }
          pf = pf->mNext;
        }
      }

      if (pfd->mIsSticky) {
#ifdef NOISY_CAN_PLACE_FRAME
        printf("   ==> last floater was letter frame && frame is sticky\n");
#endif
        return PR_TRUE;
      }
    }
  }

  // If this is a piece of text inside a letter frame...
  if (pfd->mIsNonEmptyTextFrame) {
    if (psd->mFrame && psd->mFrame->mIsLetterFrame) {
      nsIFrame* prevInFlow;
      psd->mFrame->mFrame->GetPrevInFlow(&prevInFlow);
      if (prevInFlow) {
        nsIFrame* prevPrevInFlow;
        prevInFlow->GetPrevInFlow(&prevPrevInFlow);
        if (!prevPrevInFlow) {
          // And it's the first continuation of the letter frame...
          // Then make sure that the text fits
          return PR_TRUE;
        }
      }
    }
  }
  else if (pfd->mIsLetterFrame) {
    // If this is the first continuation of the letter frame...
    nsIFrame* prevInFlow;
    pfd->mFrame->GetPrevInFlow(&prevInFlow);
    if (prevInFlow) {
      nsIFrame* prevPrevInFlow;
      prevInFlow->GetPrevInFlow(&prevPrevInFlow);
      if (!prevPrevInFlow) {
        return PR_TRUE;
      }
    }
  }

  // Special check for span frames
  if (pfd->mSpan && pfd->mSpan->mContainsFloater) {
    // If the span either directly or indirectly contains a floater then
    // it fits. Why? It's kind of complicated, but here goes:
    //
    // 1. CanPlaceFrame is used for all frame placements on a line,
    // and in a span. This includes recursively placement of frames
    // inside of spans, and the span itself. Because the logic always
    // checks for room before proceeding (the code above here), the
    // only things on a line will be those things that "fit".
    //
    // 2. Before a floater is placed on a line, the line has to be empty
    // (otherwise its a "below current line" flaoter and will be placed
    // after the line).
    //
    // Therefore, if the span directly or indirectly has a floater
    // then it means that at the time of the placement of the floater
    // the line was empty. Because of #1, only the frames that fit can
    // be added after that point, therefore we can assume that the
    // current span being placed has fit.
    //
    // So how do we get here and have a span that should already fit
    // and yet doesn't: Simple: span's that have the no-wrap attribute
    // set on them and contain a floater and are placed where they
    // don't naturally fit.
    return PR_TRUE;
 }

  // Yet another special check. If the text happens to have started
  // with a non-breaking space, then we make it sticky on its left
  // edge...Which means that whatever piece of text we just formatted
  // will be the piece that fits (the text frame logic knows to stop
  // when it runs out of room).
  if (pfd->mIsNonEmptyTextFrame && mTextStartsWithNBSP) {
    return PR_TRUE;
  }

#ifdef NOISY_CAN_PLACE_FRAME
  printf("   ==> didn't fit\n");
#endif
  aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  return PR_FALSE;
}

/**
 * Place the frame. Update running counters.
 */
void
nsLineLayout::PlaceFrame(PerFrameData* pfd, nsHTMLReflowMetrics& aMetrics)
{
  // If frame is zero width then do not apply its left and right margins.
  PerSpanData* psd = mCurrentSpan;
  PRBool emptyFrame = PR_FALSE;
  if ((0 == pfd->mBounds.width) && (0 == pfd->mBounds.height)) {
    pfd->mBounds.x = psd->mX;
    pfd->mBounds.y = mTopEdge;
    emptyFrame = PR_TRUE;
  }

  // Record ascent and update max-ascent and max-descent values
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;
//XXX  mCarriedOutTopMargin = aMetrics.mCarriedOutTopMargin;
  mCarriedOutBottomMargin = aMetrics.mCarriedOutBottomMargin;

  // If the band was updated during the reflow of that frame then we
  // need to adjust any prior frames that were reflowed.
  if (mUpdatedBand && InBlockContext()) {
    UpdateFrames();
    mUpdatedBand = PR_FALSE;
  }

  // Advance to next X coordinate
  psd->mX = pfd->mBounds.XMost() + pfd->mMargin.right;

  // If the frame is a not aware of white-space and it takes up some
  // width, disable leading white-space compression for the next frame
  // to be reflowed.
  if (!mUnderstandsWhiteSpace && pfd->mBounds.width) {
    mEndsInWhiteSpace = PR_FALSE;
  }

  // Count the number of frames on the line...
  mTotalPlacedFrames++;
  if (psd->mX != psd->mLeftEdge) {
    // As soon as a frame placed on the line advances an X coordinate
    // of any span we can no longer place a floater on the line.
    mCanPlaceFloater = PR_FALSE;
  }
}

nsresult
nsLineLayout::AddBulletFrame(nsIFrame* aFrame,
                             const nsHTMLReflowMetrics& aMetrics)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");

  PerFrameData* pfd;
  nsresult rv = NewPerFrameData(&pfd);
  if (NS_SUCCEEDED(rv)) {
    mRootSpan->AppendFrame(pfd);
    pfd->mFrame = aFrame;
    pfd->mMargin.SizeTo(0, 0, 0, 0);
    pfd->mBorderPadding.SizeTo(0, 0, 0, 0);
    pfd->mFrameType = NS_CSS_FRAME_TYPE_INLINE|NS_FRAME_REPLACED_ELEMENT;
    pfd->mRelativePos = PR_FALSE;
    pfd->mAscent = aMetrics.ascent;
    pfd->mDescent = aMetrics.descent;
    pfd->mIsTextFrame = PR_FALSE;
    pfd->mIsNonEmptyTextFrame = PR_FALSE;
    pfd->mIsLetterFrame = PR_FALSE;
    pfd->mIsSticky = PR_FALSE;

    // Note: y value will be updated during vertical alignment
    aFrame->GetRect(pfd->mBounds);
    pfd->mCombinedArea = aMetrics.mOverflowArea;
    if (mComputeMaxElementSize) {
      pfd->mMaxElementSize.SizeTo(aMetrics.width, aMetrics.height);
    }
  }
  return rv;
}

#ifdef DEBUG
void
nsLineLayout::DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent)
{
  nsFrame::IndentBy(stdout, aIndent);
  printf("%p: left=%d x=%d right=%d\n", psd, psd->mLeftEdge,
         psd->mX, psd->mRightEdge);
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nsFrame::IndentBy(stdout, aIndent+1);
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf(" %d,%d,%d,%d\n", pfd->mBounds.x, pfd->mBounds.y,
           pfd->mBounds.width, pfd->mBounds.height);
    if (pfd->mSpan) {
      DumpPerSpanData(pfd->mSpan, aIndent + 1);
    }
    pfd = pfd->mNext;
  }
}
#endif

#define VALIGN_OTHER  0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

void
nsLineLayout::VerticalAlignFrames(nsRect& aLineBoxResult,
                                  nsSize& aMaxElementSizeResult)
{
  // Synthesize a PerFrameData for the block frame
  PerFrameData rootPFD;
  rootPFD.mFrame = mBlockReflowState->frame;
  rootPFD.mFrameType = mBlockReflowState->mFrameType;
  rootPFD.mAscent = 0;
  rootPFD.mDescent = 0;
  mRootSpan->mFrame = &rootPFD;

  // Partially place the children of the block frame. The baseline for
  // this operation is set to zero so that the y coordinates for all
  // of the placed children will be relative to there.
  PerSpanData* psd = mRootSpan;
  VerticalAlignFrames(psd);

  // Compute the line-height. The line-height will be the larger of:
  //
  // [1] maxY - minY (the distance between the highest childs top edge
  // and the lowest childs bottom edge)
  //
  // [2] the maximum logical box height (since not every frame may have
  // participated in #1; for example: top/bottom aligned frames)
  //
  // [3] the minimum line height (line-height property set on the
  // block frame)
  nscoord lineHeight = psd->mMaxY - psd->mMinY;

  // Now that the line-height is computed, we need to know where the
  // baseline is in the line. Position baseline so that mMinY is just
  // inside the top of the line box.
  nscoord baselineY;
  if (psd->mMinY < 0) {
    baselineY = mTopEdge - psd->mMinY;
  }
  else {
    baselineY = mTopEdge;
  }

  // It's possible that the line-height isn't tall enough because of
  // the blocks minimum line-height.
  if (!psd->mZeroEffectiveSpanBox) {
    if (0 != lineHeight) {
      // If line contains nothing but empty boxes that have no height
      // then don't apply the min-line-height.
      //
      // Note: This is how we hide lines that contain nothing but
      // compressed whitespace.
      if (PR_FALSE==psd->mIgnoreMinLineHeight) {  // factor in min line height (part of bug 24186)
        if (lineHeight < mMinLineHeight) {
          // Apply half of the extra space to the top of the line as top
          // leading
          nscoord extra = mMinLineHeight - lineHeight;
          baselineY += extra / 2;
          lineHeight = mMinLineHeight;
        }
      }
      // XXX else...I think this is where we need code to sniff out the underlying
      // line height as given by the child <font size=x>, in quirks mode.  rest of 24186
    }
  }

  // It's also possible that the line-height isn't tall enough because
  // of top/bottom aligned elements that were not accounted for in
  // min/max Y.
  //
  // The CSS2 spec doesn't really say what happens when to the
  // baseline in this situations. What we do is if the largest top
  // aligned box height is greater than the line-height then we leave
  // the baseline alone. If the largest bottom aligned box is greater
  // than the line-height then we slide the baseline down by the extra
  // amount.
  //
  // Navigator 4 gives precedence to the first top/bottom aligned
  // object.  We just let bottom aligned objects win.
  if (lineHeight < mMaxBottomBoxHeight) {
    // When the line is shorter than the maximum top aligned box
    nscoord extra = mMaxBottomBoxHeight - lineHeight;
    baselineY += extra;
    lineHeight = mMaxBottomBoxHeight;
  }
  if (lineHeight < mMaxTopBoxHeight) {
    lineHeight = mMaxTopBoxHeight;
  }
#ifdef NOISY_VERTICAL_ALIGN
  printf("  ==> lineHeight=%d baselineY=%d\n", lineHeight, baselineY);
#endif

  // Now position all of the frames in the root span. We will also
  // recurse over the child spans and place any top/bottom aligned
  // frames we find.
  // XXX PERFORMANCE: set a bit per-span to avoid the extra work
  // (propogate it upward too)
  PerFrameData* pfd = psd->mFirstFrame;
  nscoord maxElementWidth = 0;
  nscoord maxElementHeight = 0;
  while (nsnull != pfd) {
    // Compute max-element-size if necessary
    if (mComputeMaxElementSize) {
      nscoord mw = pfd->mMaxElementSize.width +
        pfd->mMargin.left + pfd->mMargin.right;
      if (maxElementWidth < mw) {
        maxElementWidth = mw;
      }
      nscoord mh = pfd->mMaxElementSize.height +
        pfd->mMargin.top + pfd->mMargin.bottom;
      if (maxElementHeight < mh) {
        maxElementHeight = mh;
      }
    }
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mVerticalAlign, "umr");
#endif
    switch (pfd->mVerticalAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.y = mTopEdge - pfd->mBorderPadding.top +
            span->mTopLeading;
        }
        else {
          pfd->mBounds.y = mTopEdge + pfd->mMargin.top;
        }
        break;
      case VALIGN_BOTTOM:
        if (span) {
          // Compute bottom leading
          pfd->mBounds.y = mTopEdge + lineHeight -
            pfd->mBounds.height + pfd->mBorderPadding.bottom -
            span->mBottomLeading;
        }
        else {
          pfd->mBounds.y = mTopEdge + lineHeight - pfd->mMargin.bottom -
            pfd->mBounds.height;
        }
        break;
      case VALIGN_OTHER:
        pfd->mBounds.y += baselineY;
        break;
    }
    pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
    printf("  ");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf(": y=%d\n", pfd->mBounds.y);
#endif
    if (span) {
      nscoord distanceFromTop = pfd->mBounds.y - mTopEdge;
      PlaceTopBottomFrames(span, distanceFromTop, lineHeight);
    }
    pfd = pfd->mNext;
  }

  // Fill in returned line-box and max-element-size data
  aLineBoxResult.x = psd->mLeftEdge;
  aLineBoxResult.y = mTopEdge;
  aLineBoxResult.width = psd->mX - psd->mLeftEdge;
  aLineBoxResult.height = lineHeight;
  mFinalLineHeight = lineHeight;
  aMaxElementSizeResult.width = maxElementWidth;
  aMaxElementSizeResult.height = maxElementHeight;

  // Undo root-span mFrame pointer to prevent brane damage later on...
  mRootSpan->mFrame = nsnull;
}

void
nsLineLayout::PlaceTopBottomFrames(PerSpanData* psd,
                                   nscoord aDistanceFromTop,
                                   nscoord aLineHeight)
{
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mVerticalAlign, "umr");
#endif
    switch (pfd->mVerticalAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.y = -aDistanceFromTop - pfd->mBorderPadding.top +
            span->mTopLeading;
        }
        else {
          pfd->mBounds.y = -aDistanceFromTop + pfd->mMargin.top;
        }
        pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d dTop=%d [bp.top=%d topLeading=%d]\n",
               pfd->mBounds.y, aDistanceFromTop,
               span ? pfd->mBorderPadding.top : 0,
               span ? span->mTopLeading : 0);
#endif
        break;
      case VALIGN_BOTTOM:
        if (span) {
          // Compute bottom leading
          pfd->mBounds.y = -aDistanceFromTop + aLineHeight -
            pfd->mBounds.height + pfd->mBorderPadding.bottom -
            span->mBottomLeading;
        }
        else {
          pfd->mBounds.y = -aDistanceFromTop + aLineHeight -
            pfd->mMargin.bottom - pfd->mBounds.height;
        }
        pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d\n", pfd->mBounds.y);
#endif
        break;
    }
    if (span) {
      nscoord distanceFromTop = aDistanceFromTop + pfd->mBounds.y;
      PlaceTopBottomFrames(span, distanceFromTop, aLineHeight);
    }
    pfd = pfd->mNext;
  }
}


/* assumptions:
   (1) we're in quirks mode
   (2) aFrame in an inline frame, and cannot contain block frames

   what we do:
   (1) look for <font size=x> covering every text frame that decends from aFrame

   results:
   if all conditions are met, 
     set aOutResult to PR_TRUE
     set aPSD->mQuirksModeFontMetrics to the largest font found 

  to do:
  (2) look for explicit line height setting
*/
void 
nsLineLayout::ComputeQuirksModeLineHeightData(nsIRenderingContext *aRC, 
                                              PerSpanData *aPSD, 
                                              nsIFrame    *aFrame, 
                                              PRBool      *aOutResult)
{
  if (!aOutResult) return;
  *aOutResult = PR_FALSE;
  if (!aFrame || !aRC) return;    //note, aPSD can be null

  nsCOMPtr<nsIFontMetrics> largestFont;
  nsIFrame *spanFrameWithLargestFont = aFrame;

  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  if (content)
  {
    nsCOMPtr<nsIAtom> tag;
    content->GetTag(*(getter_AddRefs(tag)));
    if (nsHTMLAtoms::font==tag.get())
    {
      nsAutoString resultString;
      nsresult result;
      result = content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::size, resultString);
      if (NS_CONTENT_ATTR_HAS_VALUE==result) 
      {
        *aOutResult = PR_TRUE;
      }
    }
    // if we didn't find it, let's check out it's children
    if (PR_FALSE==*aOutResult)
    {
      // ComputeQuirksModeLineHeightData of all in-flow children
      PRBool childResult = PR_FALSE;
      nsIFrame *child;
      aFrame->FirstChild(mPresContext, nsnull, &child);
      while (child)
      {
        ComputeQuirksModeLineHeightData(aRC, aPSD, child, &childResult);
        if (PR_FALSE==childResult) {
          break;  // a child failed, bail
        }
        spanFrameWithLargestFont = child; // that's dumb for now, need to really compute this
        child->GetNextSibling(&child);
      }
      *aOutResult = childResult;  // only true if all children returned true
    }
  }

  // if we found a font, set the font to be used in quirks-mode line height calculation
  // if the caller requested us to (by passing in a non-null aPSD
  if (PR_TRUE==*aOutResult && aPSD)
  {
    NS_ASSERTION(spanFrameWithLargestFont, "failed to compute spanFrameWithLargestFont");
    if (!spanFrameWithLargestFont)
      return;
    const nsStyleFont* font;
    spanFrameWithLargestFont->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font);
    aRC->SetFont(font->mFont);
    // this is wrong, we should be setting this to the largest (tallest) font
    aRC->GetFontMetrics(*getter_AddRefs(aPSD->mQuirksModeFontMetrics));
  }
}

// Vertically place frames within a given span. Note: this doesn't
// place top/bottom aligned frames as those have to wait until the
// entire line box height is known. This is called after the span
// frame has finished being reflowed so that we know its height.
void
nsLineLayout::VerticalAlignFrames(PerSpanData* psd)
{
  // Get parent frame info
  PerFrameData* spanFramePFD = psd->mFrame;
  nsIFrame* spanFrame = spanFramePFD->mFrame;

  // Get the parent frame's font for all of the frames in this span
  const nsStyleFont* parentFont;
  spanFrame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)parentFont);
  nsIRenderingContext* rc = mBlockReflowState->rendContext;
  rc->SetFont(parentFont->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  rc->GetFontMetrics(*getter_AddRefs(fm));

  PRBool zeroEffectiveSpanBox = PR_FALSE;
  PRBool preMode = (mStyleText->mWhiteSpace == NS_STYLE_WHITESPACE_PRE) ||
    (mStyleText->mWhiteSpace == NS_STYLE_WHITESPACE_MOZ_PRE_WRAP);

  // See if the span is an empty continuation. It's an empty continuation iff:
  // - it has a prev-in-flow
  // - it has no next in flow
  // - it's zero sized
  nsIFrame* spanNextInFlow;
  spanFrame->GetNextInFlow(&spanNextInFlow);
  nsIFrame* spanPrevInFlow;
  spanFrame->GetPrevInFlow(&spanPrevInFlow);
  PRBool emptyContinuation = spanPrevInFlow && !spanNextInFlow &&
    (0 == spanFramePFD->mBounds.width) && (0 == spanFramePFD->mBounds.height);

#ifdef NOISY_VERTICAL_ALIGN
  nsFrame::ListTag(stdout, spanFrame);
  printf(": preMode=%s strictMode=%s w/h=%d,%d emptyContinuation=%s",
         preMode ? "yes" : "no",
         InStrictMode() ? "yes" : "no",
         spanFramePFD->mBounds.width, spanFramePFD->mBounds.height,
         emptyContinuation ? "yes" : "no");
  if (psd != mRootSpan) {
    printf(" bp=%d,%d,%d,%d margin=%d,%d,%d,%d",
           spanFramePFD->mBorderPadding.top,
           spanFramePFD->mBorderPadding.right,
           spanFramePFD->mBorderPadding.bottom,
           spanFramePFD->mBorderPadding.left,
           spanFramePFD->mMargin.top,
           spanFramePFD->mMargin.right,
           spanFramePFD->mMargin.bottom,
           spanFramePFD->mMargin.left);
  }
  printf("\n");
#endif

  // Compute the span's mZeroEffectiveSpanBox flag. What we are trying
  // to determine is how we should treat the span: should it act
  // "normally" according to css2 or should it effectively
  // "disappear".
  //
  // In general, if the document being processed is in strict mode
  // then it should act normally (with one exception). The exception
  // case is when a span is continued and yet the span is empty
  // (e.g. compressed whitespace). For this kind of span we treat it
  // as if it were not there so that it doesn't impact the
  // line-height.
  //
  // In compatability mode, we should sometimes make it disappear. The
  // cases that matter are those where the span contains no real text
  // elements that would provide an ascent and descent and
  // height. However, if css style elements have been applied to the
  // span (border/padding/margin) so that it's clear the document
  // author is intending css2 behavior then we act as if strict mode
  // is set.
  //
  // Finally, for pre-formatted content we don't bother doing this
  // check because pre-formatted content is always formatted as it is
  // found.
  if (!preMode && (emptyContinuation || !InStrictMode()) &&
      ((psd == mRootSpan) ||
       ((0 == spanFramePFD->mBorderPadding.top) &&
        (0 == spanFramePFD->mBorderPadding.right) &&
        (0 == spanFramePFD->mBorderPadding.bottom) &&
        (0 == spanFramePFD->mBorderPadding.left) &&
        (0 == spanFramePFD->mMargin.top) &&
        (0 == spanFramePFD->mMargin.right) &&
        (0 == spanFramePFD->mMargin.bottom) &&
        (0 == spanFramePFD->mMargin.left)))) {
    // This code handles an issue with compatability with non-css
    // conformant browsers. In particular, there are some cases
    // where the font-size and line-height for a span must be
    // ignored and instead the span must *act* as if it were zero
    // sized. In general, if the span contains any non-compressed
    // text or contains another span which contains non-compressed
    // text, then we don't use this logic.
    zeroEffectiveSpanBox = PR_TRUE;
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      if (pfd->mIsNonEmptyTextFrame) {
        zeroEffectiveSpanBox = PR_FALSE;
        break;
      }
      PerSpanData* frameSpan = pfd->mSpan;
      if (frameSpan) {
        if (!frameSpan->mZeroEffectiveSpanBox) {
          // Propogate prescence of non-empty text outward
          zeroEffectiveSpanBox = PR_FALSE;
          break;
        }
      }
      pfd = pfd->mNext;
    }
  }
  psd->mZeroEffectiveSpanBox = zeroEffectiveSpanBox;

  // Setup baselineY, minY, and maxY
  nscoord baselineY, minY, maxY;
  if (psd == mRootSpan) {
    // Use a zero baselineY since we don't yet know where the baseline
    // will be (until we know how tall the line is; then we will
    // know). In addition, use extreme values for the minY and maxY
    // values so that only the child frames will impact their values
    // (since these are children of the block, there is no span box to
    // provide initial values).
    baselineY = minY = maxY = 0;
#ifdef NOISY_VERTICAL_ALIGN
    nsFrame::ListTag(stdout, spanFrame);
    printf(": pass1 valign frames: topEdge=%d minLineHeight=%d zeroEffectiveSpanBox=%s\n",
           mTopEdge, mMinLineHeight,
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }
  else {
    if (zeroEffectiveSpanBox) {
      // When the span-box is to be ignored, zero out all the initial
      // values so that the span doesn't impact the final line
      // height. The contents of the span can impact the final line
      // height.
      psd->mTopLeading = 0;
      psd->mBottomLeading = 0;
      psd->mLogicalHeight = 0;
      spanFramePFD->mAscent = 0;
      spanFramePFD->mBounds.height = 0;
      baselineY = minY = maxY = 0;
    }
    else {
      // Compute the logical height for this span. The logical height
      // is based on the line-height value, not the font-size. Also
      // compute the top leading.
      nscoord logicalHeight =
        nsHTMLReflowState::CalcLineHeight(mPresContext, rc, spanFrame);
      nscoord contentHeight = spanFramePFD->mBounds.height -
        spanFramePFD->mBorderPadding.top - spanFramePFD->mBorderPadding.bottom;
      nscoord leading = logicalHeight - contentHeight;
      psd->mTopLeading = leading / 2;
      psd->mBottomLeading = leading - psd->mTopLeading;
      psd->mLogicalHeight = logicalHeight;

      // The initial values for the min and max Y values are in the spans
      // coordinate space, and cover the logical height of the span. If
      // there are child frames in this span that stick out of this area
      // then the minY and maxY are updated by the amount of logical
      // height that is outside this range.
      minY = spanFramePFD->mBorderPadding.top - psd->mTopLeading;
      maxY = minY + psd->mLogicalHeight;

      // This is the distance from the top edge of the parents visual
      // box to the baseline. The span already computed this for us,
      // so just use it.
      baselineY = spanFramePFD->mAscent;
    }
#ifdef NOISY_VERTICAL_ALIGN
    nsFrame::ListTag(stdout, spanFrame);
    printf(": baseLine=%d logicalHeight=%d topLeading=%d h=%d bp=%d,%d zeroEffectiveSpanBox=%s\n",
           baselineY, psd->mLogicalHeight, psd->mTopLeading,
           spanFramePFD->mBounds.height,
           spanFramePFD->mBorderPadding.top, spanFramePFD->mBorderPadding.bottom,
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }

  nscoord maxTopBoxHeight = 0;
  nscoord maxBottomBoxHeight = 0;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nsIFrame* frame = pfd->mFrame;

    // Compute the logical height of the frame
    nscoord logicalHeight;
    nscoord topLeading;
    PerSpanData* frameSpan = pfd->mSpan;
    if (frameSpan) {
      // For span frames the logical-height and top-leading was
      // pre-computed when the span was reflowed.
      logicalHeight = frameSpan->mLogicalHeight;
      topLeading = frameSpan->mTopLeading;
    }
    else {
      // For other elements the logical height is the same as the
      // frames height plus its margins.
      logicalHeight = pfd->mBounds.height + pfd->mMargin.top +
        pfd->mMargin.bottom;
      topLeading = 0;
    }

    // Get vertical-align property
    const nsStyleText* textStyle;
    frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
#ifdef DEBUG
    if (eStyleUnit_Inherit == verticalAlignUnit) {
      printf("XXX: vertical-align: inherit not implemented for ");
      nsFrame::ListTag(stdout, frame);
      printf("\n");
    }
#endif
#ifdef NOISY_VERTICAL_ALIGN
    printf("  ");
    nsFrame::ListTag(stdout, frame);
    printf(": verticalAlignUnit=%d (enum == %d)\n",
           verticalAlignUnit,
           ((eStyleUnit_Enumerated == verticalAlignUnit)
            ? textStyle->mVerticalAlign.GetIntValue()
            : -1));
#endif

    PRUint8 verticalAlignEnum;
    nscoord parentAscent, parentDescent, parentXHeight;
    nscoord parentSuperscript, parentSubscript;
    nscoord coordOffset, percentOffset, elementLineHeight;
    nscoord revisedBaselineY;
    switch (verticalAlignUnit) {
      case eStyleUnit_Enumerated:
      default:
        if (eStyleUnit_Enumerated == verticalAlignUnit) {
          verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
        }
        else {
          verticalAlignEnum = NS_STYLE_VERTICAL_ALIGN_BASELINE;
        }
        switch (verticalAlignEnum) {
          default:
          case NS_STYLE_VERTICAL_ALIGN_BASELINE:
            // The elements baseline is aligned with the baseline of
            // the parent.
            if (frameSpan) {
              // XXX explain
              pfd->mBounds.y = baselineY - pfd->mAscent;
            }
            else {
              // For non-span elements the borders, padding and
              // margins are significant. Use the visual box height
              // and the bottom margin as the distance off of the
              // baseline.
              pfd->mBounds.y = baselineY - pfd->mAscent - pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUB:
            // Lower the baseline of the box to the subscript offset
            // of the parent's box. This is identical to the baseline
            // alignment except for the addition of the subscript
            // offset to the baseline Y.
            fm->GetSubscriptOffset(parentSubscript);
            revisedBaselineY = baselineY + parentSubscript;
            if (frameSpan) {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
            }
            else {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
                pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUPER:
            // Raise the baseline of the box to the superscript offset
            // of the parent's box. This is identical to the baseline
            // alignment except for the subtraction of the superscript
            // offset to the baseline Y.
            fm->GetSuperscriptOffset(parentSuperscript);
            revisedBaselineY = baselineY - parentSuperscript;
            if (frameSpan) {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
            }
            else {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
                pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TOP:
            pfd->mVerticalAlign = VALIGN_TOP;
            if (logicalHeight > maxTopBoxHeight) {
              maxTopBoxHeight = logicalHeight;
            }
            break;

          case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
            pfd->mVerticalAlign = VALIGN_BOTTOM;
            if (logicalHeight > maxBottomBoxHeight) {
              maxBottomBoxHeight = logicalHeight;
            }
            break;

          case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
            // Align the midpoint of the frame with 1/2 the parents
            // x-height above the baseline.
            fm->GetXHeight(parentXHeight);
            if (frameSpan) {
              pfd->mBounds.y = baselineY -
                (parentXHeight + pfd->mBounds.height)/2;
            }
            else {
              pfd->mBounds.y = baselineY - (parentXHeight + logicalHeight)/2 +
                pfd->mMargin.top;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
            // The top of the logical box is aligned with the top of
            // the parent elements text.
            fm->GetMaxAscent(parentAscent);
            if (frameSpan) {
              pfd->mBounds.y = baselineY - parentAscent -
                pfd->mBorderPadding.top + frameSpan->mTopLeading;
            }
            else {
              pfd->mBounds.y = baselineY - parentAscent + pfd->mMargin.top;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
            // The bottom of the logical box is aligned with the
            // bottom of the parent elements text.
            fm->GetMaxDescent(parentDescent);
            if (frameSpan) {
              pfd->mBounds.y = baselineY + parentDescent -
                pfd->mBounds.height + pfd->mBorderPadding.bottom -
                frameSpan->mBottomLeading;
            }
            else {
              pfd->mBounds.y = baselineY + parentDescent -
                pfd->mBounds.height - pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;
        }
        break;

      case eStyleUnit_Coord:
        // According to the CSS2 spec (10.8.1), a positive value
        // "raises" the box by the given distance while a negative value
        // "lowers" the box by the given distance (with zero being the
        // baseline). Since Y coordinates increase towards the bottom of
        // the screen we reverse the sign.
        coordOffset = textStyle->mVerticalAlign.GetCoordValue();
        revisedBaselineY = baselineY - coordOffset;
        if (frameSpan) {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
        }
        else {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
            pfd->mMargin.bottom;
        }
        pfd->mVerticalAlign = VALIGN_OTHER;
        break;

      case eStyleUnit_Percent:
        // Similar to a length value (eStyleUnit_Coord) except that the
        // percentage is a function of the elements line-height value.
        elementLineHeight =
          nsHTMLReflowState::CalcLineHeight(mPresContext, rc, frame);
        percentOffset = nscoord(
          textStyle->mVerticalAlign.GetPercentValue() * elementLineHeight
          );
        revisedBaselineY = baselineY - percentOffset;
        if (frameSpan) {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
        }
        else {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
            pfd->mMargin.bottom;
        }
        pfd->mVerticalAlign = VALIGN_OTHER;
        break;
    }

    // Update minY/maxY for frames that we just placed. Do not factor
    // text into the equation.
    if (pfd->mVerticalAlign == VALIGN_OTHER) {
      // Text frames do not contribute to the min/max Y values for the
      // line (instead their parent frame's font-size contributes).
      if (!pfd->mIsTextFrame) {
        nscoord yTop, yBottom;
        if (frameSpan) {
          // For spans that were are now placing, use their position
          // plus their already computed min-Y and max-Y values for
          // computing yTop and yBottom.
          yTop = pfd->mBounds.y + frameSpan->mMinY;
          yBottom = pfd->mBounds.y + frameSpan->mMaxY;
        }
        else {
          yTop = pfd->mBounds.y - pfd->mMargin.top;
          yBottom = yTop + logicalHeight;
        }
        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;
#ifdef NOISY_VERTICAL_ALIGN
        printf("     raw: a=%d d=%d h=%d bp=%d,%d logical: h=%d leading=%d y=%d minY=%d maxY=%d\n",
               pfd->mAscent, pfd->mDescent, pfd->mBounds.height,
               pfd->mBorderPadding.top, pfd->mBorderPadding.bottom,
               logicalHeight,
               pfd->mSpan ? topLeading : 0,
               pfd->mBounds.y, minY, maxY);
#endif
      }
      if (psd != mRootSpan) {
        frame->SetRect(mPresContext, pfd->mBounds);
      }
    }
    pfd = pfd->mNext;
  }

  /* This is where we fix bug 24186.
   * In quirks mode only, in some situations we want to ignore the
   * minimum line height on the parent block.
   * We set psd->mIgnoreMinLineHeight as a result of this logic:
         0) in standard mode? 
            psd->mIgnoreMinLineHeight=false, stop
         1) does the block have an explicit line height?  
            psd->mIgnoreMinLineHeight=false, stop
         2) examine each frame in the span.  
            if all text frames are entirely enclosed by a <font size=x> tag
            psd->mIgnoreMinLineHeight=true
         3) otherwise, iMLH=false

   * To Do:
   *   a) cache this computation
   */
  if (psd == mRootSpan) 
  {
    psd->mIgnoreMinLineHeight=PR_TRUE;  // we'll set this to false in all the right places...
    if (InStrictMode())
    { // we're in standard mode
      psd->mIgnoreMinLineHeight = PR_FALSE; // (0)
    }
    else 
    {
      // test to see if the block has an explicit line height set
      nsIFrame *blockFrame = nsnull;
      if (psd->mFrame)
      {
        blockFrame = psd->mFrame->mFrame; 
        nsCOMPtr<nsIStyleContext> sc;
        blockFrame->GetStyleContext(getter_AddRefs(sc));
        if (sc) 
        {
          const nsStyleText* text = (const nsStyleText*)
            sc->GetStyleData(eStyleStruct_Text);
          if (text && eStyleUnit_Normal != text->mLineHeight.GetUnit()) 
          {
            psd->mIgnoreMinLineHeight = PR_FALSE; // (1)
          }
        }
      }
      // finally, look at each span in the block, and see if a line height is set
      // or a <font size=x> is found.  For now, just look at <font size=x>
      if (PR_TRUE==psd->mIgnoreMinLineHeight)
      {
        // abc PRBool ignore=PR_FALSE;  
        PRBool ignore=PR_TRUE;  
        PerFrameData* pfd = psd->mFirstFrame;
        while (nsnull != pfd) 
        {
          ComputeQuirksModeLineHeightData(rc, psd, pfd->mFrame, &ignore);
          // abc if (PR_TRUE==ignore) { // we found one, short-circuit the loop
          if (PR_FALSE==ignore) { // we found one, short-circuit the loop
            break;
          }
          pfd = pfd->mNext;
        }
        psd->mIgnoreMinLineHeight = ignore; // (2) and (3)
      }
    }
  }

  // Factor in the minimum line-height when handling the root-span for
  // the block.
  if (psd == mRootSpan) {
    // If this span's width is non-zero then it can't possibly be a
    // line that contains nothing but zero width items. Therefore we
    // should factor in the block element's minimum line-height (as
    // defined in section 10.8.1 of the css2 spec).
    if (psd->mZeroEffectiveSpanBox) {
      // Leave minY/maxY values untouched
    }
    else {
      if ((psd->mX != psd->mLeftEdge) || preMode) {
#ifdef NOISY_VERTICAL_ALIGN
        printf("  ignoreMinLH = %d\n", psd->mIgnoreMinLineHeight);
        printf("  ==> adjusting min/maxY: currentValues: %d,%d", minY, maxY);
#endif
        nscoord minimumLineHeight = 0;
        if (PR_FALSE==psd->mIgnoreMinLineHeight) {
          minimumLineHeight = mMinLineHeight;
        }
        nscoord fontAscent, fontHeight;
        fm->GetMaxAscent(fontAscent);
        if (nsHTMLReflowState::UseComputedHeight()) {
          fontHeight = parentFont->mFont.size;
        }
        else 
        {
          if (PR_TRUE==psd->mIgnoreMinLineHeight) {
            psd->mQuirksModeFontMetrics->GetHeight(fontHeight);
            psd->mQuirksModeFontMetrics->GetMaxAscent(fontAscent);
          }
          else {
            fm->GetHeight(fontHeight);
          }
        }

        nscoord leading = minimumLineHeight - fontHeight;
        nscoord yTop = -fontAscent - leading/2;
        nscoord yBottom = yTop + minimumLineHeight;
        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;

#ifdef NOISY_VERTICAL_ALIGN
        printf(" new values: %d,%d\n", minY, maxY);
#endif
      }
      else {
        // XXX issues:
        // [1] BR's on empty lines stop working
        // [2] May not honor css2's notion of handling empty elements
        // [3] blank lines in a pre-section ("\n") (handled with preMode)
#ifdef NOISY_VERTICAL_ALIGN
        printf("  ==> zapping min/maxY: currentValues: %d,%d newValues: 0,0\n",
               minY, maxY);
#endif
        minY = maxY = 0;
      }
    }
  }

  psd->mMinY = minY;
  psd->mMaxY = maxY;
#ifdef NOISY_VERTICAL_ALIGN
  printf("  ==> minY=%d maxY=%d delta=%d maxTopBoxHeight=%d maxBottomBoxHeight=%d\n",
         minY, maxY, maxY - minY, maxTopBoxHeight, maxBottomBoxHeight);
#endif
  if (maxTopBoxHeight > mMaxTopBoxHeight) {
    mMaxTopBoxHeight = maxTopBoxHeight;
  }
  if (maxBottomBoxHeight > mMaxBottomBoxHeight) {
    mMaxBottomBoxHeight = maxBottomBoxHeight;
  }
}

PRBool
nsLineLayout::TrimTrailingWhiteSpaceIn(PerSpanData* psd,
                                       nscoord* aDeltaWidth)
{
// XXX what about NS_STYLE_DIRECTION_RTL?
  if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
    *aDeltaWidth = 0;
    return PR_TRUE;
  }

  PerFrameData* pfd = psd->mFirstFrame;
  if (!pfd) {
    *aDeltaWidth = 0;
    return PR_FALSE;
  }
  pfd = pfd->Last();
  while (nsnull != pfd) {
#ifdef REALLY_NOISY_TRIM
    nsFrame::ListTag(stdout, (psd == mRootSpan
                              ? mBlockReflowState->frame
                              : psd->mFrame->mFrame));
    printf(": attempting trim of ");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf("\n");
#endif
    PerSpanData* childSpan = pfd->mSpan;
    if (childSpan) {
      // Maybe the child span has the trailing white-space in it?
      if (TrimTrailingWhiteSpaceIn(childSpan, aDeltaWidth)) {
        nscoord deltaWidth = *aDeltaWidth;
        if (deltaWidth) {
          // Adjust the child spans frame size
          pfd->mBounds.width -= deltaWidth;
          if (psd != mRootSpan) {
            // When the child span is not a direct child of the block
            // we need to update the child spans frame rectangle
            // because it most likely will not be done again. Spans
            // that are direct children of the block will be updated
            // later, however, because the VerticalAlignFrames method
            // will be run after this method.
            nsRect r;
            nsIFrame* f = pfd->mFrame;
            f->GetRect(r);
            r.width -= deltaWidth;
            f->SetRect(mPresContext, r);
          }

          // Adjust the right edge of the span that contains the child span
          psd->mX -= deltaWidth;

          // Slide any frames that follow the child span over by the
          // right amount. The only thing that can follow the child
          // span is empty stuff, so we are just making things
          // sensible (keeping the combined area honest).
          while (pfd->mNext) {
            pfd = pfd->mNext;
            pfd->mBounds.x -= deltaWidth;
          }
        }
        return PR_TRUE;
      }
    }
    else if (!pfd->mIsTextFrame) {
      // If we hit a frame on the end that's not text, then there is
      // no trailing whitespace to trim. Stop the search.
      *aDeltaWidth = 0;
      return PR_TRUE;
    }
    else if (pfd->mIsNonEmptyTextFrame) {
      nscoord deltaWidth = 0;
      pfd->mFrame->TrimTrailingWhiteSpace(mPresContext,
                                          *mBlockReflowState->rendContext,
                                          deltaWidth);
#ifdef NOISY_TRIM
      nsFrame::ListTag(stdout, (psd == mRootSpan
                                ? mBlockReflowState->frame
                                : psd->mFrame->mFrame));
      printf(": trim of ");
      nsFrame::ListTag(stdout, pfd->mFrame);
      printf(" returned %d\n", deltaWidth);
#endif
      if (deltaWidth) {
        pfd->mBounds.width -= deltaWidth;
        pfd->mCombinedArea.width -= deltaWidth;
        if (0 == pfd->mBounds.width) {
          pfd->mMaxElementSize.width = 0;
          pfd->mMaxElementSize.height = 0;
        }

        // See if the text frame has already been placed in its parent
        if (psd != mRootSpan) {
          // The frame was already placed during psd's
          // reflow. Update the frames rectangle now.
          pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
        }

        // Adjust containing span's right edge
        psd->mX -= deltaWidth;

        // Slide any frames that follow the text frame over by the
        // right amount. The only thing that can follow the text
        // frame is empty stuff, so we are just making things
        // sensible (keeping the combined area honest).
        while (pfd->mNext) {
          pfd = pfd->mNext;
          pfd->mBounds.x -= deltaWidth;
        }
      }

      // Pass up to caller so they can shrink their span
      *aDeltaWidth = deltaWidth;
      return PR_TRUE;
    }
    pfd = pfd->mPrev;
  }

  *aDeltaWidth = 0;
  return PR_FALSE;
}

PRBool
nsLineLayout::TrimTrailingWhiteSpace()
{
  PerSpanData* psd = mRootSpan;
  nscoord deltaWidth;
  TrimTrailingWhiteSpaceIn(psd, &deltaWidth);
  return 0 != deltaWidth;
}

PRBool
nsLineLayout::HorizontalAlignFrames(nsRect& aLineBounds,
                                    PRBool aAllowJustify,
                                    PRBool aShrinkWrapWidth)
{
  PerSpanData* psd = mRootSpan;
  nscoord availWidth = psd->mRightEdge;
  if (NS_UNCONSTRAINEDSIZE == availWidth) {
    // Don't bother horizontal aligning on pass1 table reflow
#ifdef NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": skipping horizontal alignment in pass1 table reflow\n");
#endif
    return PR_TRUE;
  }
  availWidth -= psd->mLeftEdge;
  nscoord remainingWidth = availWidth - aLineBounds.width;
#ifdef NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": availWidth=%d lineWidth=%d delta=%d\n",
           availWidth, aLineBounds.width, remainingWidth);
#endif
  if (remainingWidth > 0) {
    nscoord dx = 0;
    switch (mTextAlign) {
      case NS_STYLE_TEXT_ALIGN_DEFAULT:
        if (NS_STYLE_DIRECTION_LTR == psd->mDirection) {
          // default alignment for left-to-right is left so do nothing
          break;
        }
        // Fall through to align right case for default alignment
        // used when the direction is right-to-left.

      case NS_STYLE_TEXT_ALIGN_RIGHT:
      case NS_STYLE_TEXT_ALIGN_MOZ_RIGHT:
        dx = remainingWidth;
        break;

      case NS_STYLE_TEXT_ALIGN_LEFT:
        break;

      case NS_STYLE_TEXT_ALIGN_JUSTIFY:
        // If this is not the last line then go ahead and justify the
        // frames in the line. If it is the last line then if the
        // direction is right-to-left then we right-align the frames.
        if (aAllowJustify) {
          break;
        }
        else if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
          // right align the frames
          dx = remainingWidth;
        }
        break;

      case NS_STYLE_TEXT_ALIGN_CENTER:
      case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
        dx = remainingWidth / 2;
        break;
    }
    if (0 != dx) {
      // If we need to move the frames but we're shrink wrapping, then
      // we need to wait until the final width is known
      if (aShrinkWrapWidth) {
        return PR_FALSE;
      }

      PerFrameData* pfd = psd->mFirstFrame;
      while (nsnull != pfd) {
        pfd->mBounds.x += dx;
        pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
        pfd = pfd->mNext;
      }
      aLineBounds.width += dx;
    }

    if ((NS_STYLE_DIRECTION_RTL == psd->mDirection) &&
        !psd->mChangedFrameDirection) {
      psd->mChangedFrameDirection = PR_TRUE;
  
      /* Assume that all frames have been right aligned.*/
      if (aShrinkWrapWidth) {
        return PR_FALSE;
      }
      PerFrameData* pfd = psd->mFirstFrame;
      PRUint32 maxX = psd->mRightEdge;
      while (nsnull != pfd) {
        pfd->mBounds.x = maxX - pfd->mBounds.width;
        pfd->mFrame->SetRect(mPresContext, pfd->mBounds);
        maxX = pfd->mBounds.x;
        pfd = pfd->mNext;
      }
    }
  }

  return PR_TRUE;
}

void
nsLineLayout::RelativePositionFrames(nsRect& aCombinedArea)
{
  RelativePositionFrames(mRootSpan, aCombinedArea);
}

void
nsLineLayout::RelativePositionFrames(PerSpanData* psd, nsRect& aCombinedArea)
{
  nsPoint origin;
  nsRect spanCombinedArea;
  PerFrameData* pfd;

  nscoord minX, minY, maxX, maxY;
  if (nsnull != psd->mFrame) {
    // The minimum combined area for the frames in a span covers the
    // entire span frame.
    pfd = psd->mFrame;
    minX = 0;
    minY = 0;
    maxX = pfd->mBounds.width;
    maxY = pfd->mBounds.height;
  }
  else {
    // The minimum combined area for the frames that are direct
    // children of the block starts at the upper left corner of the
    // line and is sized to match the size of the line's bounding box
    // (the same size as the values returned from VerticalAlignFrames)
    minX = psd->mLeftEdge;
    maxX = psd->mX;
    minY = mTopEdge;
    maxY = mTopEdge + mFinalLineHeight;
  }

  pfd = psd->mFirstFrame;
  PRBool updatedCombinedArea = PR_FALSE;
  while (nsnull != pfd) {
    nscoord x = pfd->mBounds.x;
    nscoord y = pfd->mBounds.y;

    // Adjust the origin of the frame
    if (pfd->mRelativePos) {
      nsIFrame* frame = pfd->mFrame;
      frame->GetOrigin(origin);
      // XXX what about right and bottom?
      nscoord dx = pfd->mOffsets.left;
      nscoord dy = pfd->mOffsets.top;
      frame->MoveTo(mPresContext, origin.x + dx, origin.y + dy);
      x += dx;
      y += dy;
    }

    // Note: the combined area of a child is in its coordinate
    // system. We adjust the childs combined area into our coordinate
    // system before computing the aggregated value by adding in
    // <b>x</b> and <b>y</b> which were computed above.
    nsRect* r = &pfd->mCombinedArea;
    if (pfd->mSpan) {
      // Compute a new combined area for the child span before
      // aggregating it into our combined area.
      r = &spanCombinedArea;
      RelativePositionFrames(pfd->mSpan, spanCombinedArea);
    }

    // Only if the frame has some area do we let it affect the
    // combined area. Otherwise empty frames placed next to a floating
    // element will cause the floaters margin to be relevant, which we
    // don't want to happen.
    if (r->width && r->height) {
      nscoord xl = x + r->x;
      nscoord xr = x + r->XMost();
      if (xl < minX) {
        minX = xl;
      }
      if (xr > maxX) {
        maxX = xr;
      }
      nscoord yt = y + r->y;
      nscoord yb = y + r->YMost();
      if (yt < minY) {
        minY = yt;
      }
      if (yb > maxY) {
        maxY = yb;
      }
      updatedCombinedArea = PR_TRUE;
    }

    pfd = pfd->mNext;
  }

  // Compute aggregated combined area
  if (updatedCombinedArea) {
    aCombinedArea.x = minX;
    aCombinedArea.y = minY;
    aCombinedArea.width = maxX - minX;
    aCombinedArea.height = maxY - minY;
  }
  else {
    aCombinedArea.x = 0;
    aCombinedArea.y = minY;
    aCombinedArea.width = 0;
    aCombinedArea.height = 0;
  }

  // If we just computed a spans combined area, we need to update its
  // NS_FRAME_OUTSIDE_CHILDREN bit..
  if (nsnull != psd->mFrame) {
    pfd = psd->mFrame;
    nsIFrame* frame = pfd->mFrame;
    nsFrameState oldState;
    frame->GetFrameState(&oldState);
    nsFrameState newState = oldState & ~NS_FRAME_OUTSIDE_CHILDREN;
    if ((minX < 0) || (minY < 0) ||
        (maxX > pfd->mBounds.width) || (maxY > pfd->mBounds.height)) {
      newState |= NS_FRAME_OUTSIDE_CHILDREN;
    }
    if (newState != oldState) {
      frame->SetFrameState(newState);
    }
  }
}

void
nsLineLayout::ForgetWordFrame(nsIFrame* aFrame)
{
  NS_ASSERTION((void*)aFrame == mWordFrames[0], "forget-word-frame");
  if (0 != mWordFrames.Count()) {
    mWordFrames.RemoveElementAt(0);
  }
}

nsIFrame*
nsLineLayout::FindNextText(nsIFrame* aFrame)
{
  // Only the first-in-flows are present in the text run list so
  // backup from the argument frame to its first-in-flow.
  for (;;) {
    nsIFrame* prevInFlow;
    aFrame->GetPrevInFlow(&prevInFlow);
    if (nsnull == prevInFlow) {
      break;
    }
    aFrame = prevInFlow;
  }

  // Now look for the frame that follows aFrame's first-in-flow
  nsTextRun* run = mReflowTextRuns;
  while (nsnull != run) {
    PRInt32 ix = run->mArray.IndexOf(aFrame);
    if (ix >= 0) {
      if (ix < run->mArray.Count() - 1) {
        return (nsIFrame*) run->mArray[ix + 1];
      }
    }
    run = run->mNext;
  }
  return nsnull;
}

nsresult
nsLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mNewTextRun) {
    mNewTextRun = new nsTextRun();
    if (nsnull == mNewTextRun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *mTextRunP = mNewTextRun;
    mTextRunP = &mNewTextRun->mNext;
  }
#ifdef DEBUG_ADD_TEXT
  {
    // Check that text-frame is not already there
    PRInt32 ix = mNewTextRun->mArray.IndexOf((void*)aTextFrame);
    NS_ASSERTION(ix < 0, "text frame already in text run");
  }
#endif
  mNewTextRun->mArray.AppendElement(aTextFrame);/* XXX out-of-memory */
  return NS_OK;
}

void
nsLineLayout::EndTextRun()
{
  if (mNewTextRun) {
    PRInt32 numTextInRun = mNewTextRun->mArray.Count();
    if (numTextInRun < 2) {
      // Don't bother remembering empty text-runs: reset the array
      // back to zero elements. This effectively prepares this
      // text-run for the next round. If it turns out there is no next
      // round then we will get rid of it later in TakeTextRuns.
      mNewTextRun->mArray.Clear();
    }
  }
  mNewTextRun = nsnull;
}

nsTextRun*
nsLineLayout::TakeTextRuns()
{
  nsTextRun* result = mTextRuns;
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;

  // Eliminate any text-runs that are empty
  nsTextRun** rp = &result;
  nsTextRun* run = *rp;
  while (run) {
    if (0 == run->mArray.Count()) {
      *rp = run->mNext;
      delete run;
    }
    else {
      rp = &run->mNext;
    }
    run = *rp;
  }

  return result;
}

PRBool
nsLineLayout::TreatFrameAsBlock(nsIFrame* aFrame)
{
  const nsStyleDisplay* display;
  const nsStylePosition* position;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  aFrame->GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&) position);
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    return PR_FALSE;
  }
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    return PR_FALSE;
  }
  switch (display->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_RUN_IN:
  case NS_STYLE_DISPLAY_COMPACT:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}
