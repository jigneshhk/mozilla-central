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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLButtonElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINSHTMLButtonElementIID, NS_IDOMNSHTMLBUTTONELEMENT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLButtonElementIID, NS_IDOMHTMLBUTTONELEMENT_IID);

NS_DEF_PTR(nsIDOMNSHTMLButtonElement);
NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMHTMLButtonElement);

//
// HTMLButtonElement property ids
//
enum HTMLButtonElement_slots {
  HTMLBUTTONELEMENT_FORM = -1,
  HTMLBUTTONELEMENT_ACCESSKEY = -2,
  HTMLBUTTONELEMENT_DISABLED = -3,
  HTMLBUTTONELEMENT_NAME = -4,
  HTMLBUTTONELEMENT_TABINDEX = -5,
  HTMLBUTTONELEMENT_TYPE = -6,
  HTMLBUTTONELEMENT_VALUE = -7
};

/***********************************************************************/
//
// HTMLButtonElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLButtonElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLButtonElement *a = (nsIDOMHTMLButtonElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLBUTTONELEMENT_FORM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_FORM, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMHTMLFormElement* prop;
        nsresult result = NS_OK;
        result = a->GetForm(&prop);
        if (NS_SUCCEEDED(result)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_ACCESSKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_ACCESSKEY, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetAccessKey(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_DISABLED:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_DISABLED, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRBool prop;
        nsresult result = NS_OK;
        result = a->GetDisabled(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_NAME, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetName(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_TABINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_TABINDEX, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRInt32 prop;
        nsresult result = NS_OK;
        result = a->GetTabIndex(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_TYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_TYPE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetType(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      case HTMLBUTTONELEMENT_VALUE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_VALUE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetValue(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, result);
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLButtonElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLButtonElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLButtonElement *a = (nsIDOMHTMLButtonElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLBUTTONELEMENT_ACCESSKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_ACCESSKEY, PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAccessKey(prop);
        
        break;
      }
      case HTMLBUTTONELEMENT_DISABLED:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_DISABLED, PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
        }
      
        a->SetDisabled(prop);
        
        break;
      }
      case HTMLBUTTONELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_NAME, PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLBUTTONELEMENT_TABINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_TABINDEX, PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_NOT_NUMBER_ERR);
        }
      
        a->SetTabIndex(prop);
        
        break;
      }
      case HTMLBUTTONELEMENT_VALUE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_HTMLBUTTONELEMENT_VALUE, PR_TRUE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetValue(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// HTMLButtonElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLButtonElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLButtonElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLButtonElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLButtonElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLButtonElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLButtonElementBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLButtonElementPtr nativeThis = nsnull;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIHTMLButtonElementIID, (void **)&nativeThis)) {
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLButtonElementIID, (void **)&nativeThis)) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }


  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_NSHTMLBUTTONELEMENT_BLUR, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    result = nativeThis->Blur();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLButtonElementFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLButtonElementPtr nativeThis = nsnull;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIHTMLButtonElementIID, (void **)&nativeThis)) {
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLButtonElementIID, (void **)&nativeThis)) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }


  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECMAN_ERR);
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, NS_DOM_PROP_NSHTMLBUTTONELEMENT_FOCUS, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    result = nativeThis->Focus();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLButtonElement
//
JSClass HTMLButtonElementClass = {
  "HTMLButtonElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLButtonElementProperty,
  SetHTMLButtonElementProperty,
  EnumerateHTMLButtonElement,
  ResolveHTMLButtonElement,
  JS_ConvertStub,
  FinalizeHTMLButtonElement
};


//
// HTMLButtonElement class properties
//
static JSPropertySpec HTMLButtonElementProperties[] =
{
  {"form",    HTMLBUTTONELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"accessKey",    HTMLBUTTONELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"disabled",    HTMLBUTTONELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"name",    HTMLBUTTONELEMENT_NAME,    JSPROP_ENUMERATE},
  {"tabIndex",    HTMLBUTTONELEMENT_TABINDEX,    JSPROP_ENUMERATE},
  {"type",    HTMLBUTTONELEMENT_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"value",    HTMLBUTTONELEMENT_VALUE,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLButtonElement class methods
//
static JSFunctionSpec HTMLButtonElementMethods[] = 
{
  {"blur",          NSHTMLButtonElementBlur,     0},
  {"focus",          NSHTMLButtonElementFocus,     0},
  {0}
};


//
// HTMLButtonElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLButtonElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLButtonElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLButtonElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLButtonElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitHTMLElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLButtonElementClass,      // JSClass
                         HTMLButtonElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLButtonElementProperties,  // proto props
                         HTMLButtonElementMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new HTMLButtonElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLButtonElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLButtonElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLButtonElement *aHTMLButtonElement;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitHTMLButtonElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLButtonElementIID, (void **)&aHTMLButtonElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLButtonElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLButtonElement);
  }
  else {
    NS_RELEASE(aHTMLButtonElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
