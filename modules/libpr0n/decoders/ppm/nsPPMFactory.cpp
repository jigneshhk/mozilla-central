/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsPPMDecoder.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPPMDecoder)

static nsModuleComponentInfo components[] =
{
  { "ppm decoder",
    NS_PPMDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-portable-pixmap",
    nsPPMDecoderConstructor, },
};

NS_IMPL_NSGETMODULE(nsPPMDecoderModule, components)

