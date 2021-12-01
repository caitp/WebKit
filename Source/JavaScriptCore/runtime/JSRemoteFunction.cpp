/*
 * Copyright (C) 2021 Igalia S.L.
 * Author: Caitlin Potter <caitp@igalia.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSRemoteFunction.h"

#include "ExecutableBaseInlines.h"
#include "JSCInlines.h"
#include "ShadowRealmObject.h"

#include <wtf/Assertions.h>

namespace JSC {

const ClassInfo JSRemoteFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRemoteFunction) };

JSRemoteFunction::JSRemoteFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSCallee* targetFunction)
    : Base(vm, executable, globalObject, structure)
    , m_targetFunction(vm, this, targetFunction)
{
}

JSValue wrapValue(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();

    if (value.isPrimitive())
        return value;

    if (value.isCallable(vm)) {
        JSCallee* targetCallee = jsCast<JSCallee*>(value.asCell());
        ASSERT(targetCallee->structure()->globalObject() != targetGlobalObject);

        return JSValue(JSRemoteFunction::create(vm, targetGlobalObject, targetCallee));
    }

    return JSValue();
}

static inline JSValue wrapArgument(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = wrapValue(globalObject, targetGlobalObject, value);
    if (result.isEmpty())
        throwTypeError(globalObject, scope, "value passing between realms must be callable or primitive");
    return result;
}

static inline JSValue wraprReturnValue(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = wrapValue(globalObject, targetGlobalObject, value);
    if (result.isEmpty())
        throwTypeError(globalObject, scope, "value passing between realms must be callable or primitive");
    return result;
}

JSC_DEFINE_HOST_FUNCTION(remoteJSFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSRemoteFunction* remoteFunction = jsCast<JSRemoteFunction*>(callFrame->jsCallee());
    JSFunction* targetFunction = jsCast<JSFunction*>(remoteFunction->targetFunction());
    JSGlobalObject* targetGlobalObject = targetFunction->structure()->globalObject();

    RELEASE_ASSERT(targetGlobalObject != globalObject);

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue wrappedValue = wrapArgument(globalObject, targetGlobalObject, callFrame->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        args.append(wrappedValue);
    }
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }
    ExecutableBase* executable = targetFunction->executable();
    if (executable->hasJITCodeForCall()) {
        // Force the executable to cache its arity entrypoint.
        executable->entrypointFor(CodeForCall, MustCheckArity);
    }

    auto callData = getCallData(vm, targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    auto result = call(targetGlobalObject, targetFunction, callData, jsUndefined(), args);

    // Hide exceptions from calling realm
    if (scope.exception()) {
        scope.clearException();
        throwTypeError(globalObject, scope, "an error occurred in remote realm");
        return encodedJSValue();
    }

    auto wrappedResult = wraprReturnValue(globalObject, globalObject, result);
    RELEASE_AND_RETURN(scope, JSValue::encode(wrappedResult));
}

JSC_DEFINE_HOST_FUNCTION(remoteFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSRemoteFunction* remoteFunction = jsCast<JSRemoteFunction*>(callFrame->jsCallee());
    JSObject* targetFunction = remoteFunction->targetFunction();
    JSGlobalObject* targetGlobalObject = targetFunction->structure()->globalObject();

    RELEASE_ASSERT(targetGlobalObject != globalObject);

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue wrappedValue = wrapValue(globalObject, targetGlobalObject, callFrame->uncheckedArgument(i));
        args.append(wrappedValue);
    }
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    auto callData = getCallData(vm, targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    auto result = call(targetGlobalObject, targetFunction, callData, jsUndefined(), args);

    // Hide exceptions from calling realm
    if (scope.exception()) {
        scope.clearException();
        throwTypeError(globalObject, scope, "an error occurred in remote realm");
        return encodedJSValue();
    }

    auto wrappedResult = wrapValue(targetGlobalObject, globalObject, result);
    RELEASE_AND_RETURN(scope, JSValue::encode(wrappedResult));
}

JSC_DEFINE_HOST_FUNCTION(isRemoteFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(JSValue(static_cast<bool>(jsDynamicCast<JSRemoteFunction*>(globalObject->vm(), callFrame->uncheckedArgument(0)))));
}

JSC_DEFINE_HOST_FUNCTION(createRemoteFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(callFrame->argumentCount() == 2);
    JSCallee* targetFunction = jsCast<JSCallee*>(callFrame->uncheckedArgument(0));
    JSGlobalObject* destinationGlobalObject = globalObject;
    if (!callFrame->uncheckedArgument(1).isUndefinedOrNull()) {
        if (auto shadowRealm = jsDynamicCast<ShadowRealmObject*>(vm, callFrame->uncheckedArgument(1)))
            destinationGlobalObject = shadowRealm->globalObject();
        else
            destinationGlobalObject = jsCast<JSGlobalObject*>(callFrame->uncheckedArgument(1));
    }

    ASSERT(destinationGlobalObject != targetFunction->globalObject());

    auto result = JSRemoteFunction::create(vm, destinationGlobalObject, targetFunction);
    RELEASE_AND_RETURN(scope, JSValue::encode(result));
}

inline Structure* getRemoteFunctionStructure(JSGlobalObject* globalObject)
{
    // FIXME: implement globalObject-aware structure caching
    return globalObject->remoteFunctionStructure();
}

JSRemoteFunction* JSRemoteFunction::create(VM& vm, JSGlobalObject* globalObject, JSCallee* targetFunction)
{
    ASSERT(globalObject != targetFunction->structure()->globalObject());
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isJSFunction = getJSFunction(targetFunction);
    NativeExecutable* executable = vm.getRemoteFunction(isJSFunction);
    Structure* structure = getRemoteFunctionStructure(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSRemoteFunction* function = new (NotNull, allocateCell<JSRemoteFunction>(vm)) JSRemoteFunction(vm, executable, globalObject, structure, targetFunction);

    function->finishCreation(vm);
    return function;
}

void JSRemoteFunction::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

template<typename Visitor>
void JSRemoteFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSRemoteFunction* thisObject = jsCast<JSRemoteFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_targetFunction);
}

DEFINE_VISIT_CHILDREN(JSRemoteFunction);

} // namespace JSC
