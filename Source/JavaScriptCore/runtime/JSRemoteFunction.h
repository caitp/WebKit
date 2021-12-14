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

#pragma once

#include "AuxiliaryBarrier.h"
#include "JSObject.h"
#include <wtf/TaggedArrayStoragePtr.h>

namespace JSC {

JSC_DECLARE_HOST_FUNCTION(remoteJSFunctionCall);
JSC_DECLARE_HOST_FUNCTION(remoteFunctionCall);
JSC_DECLARE_HOST_FUNCTION(isRemoteFunction);
JSC_DECLARE_HOST_FUNCTION(createRemoteFunction);

// JSRemoteFunction creates a bridge between its native Realm and a remote one.
// When invoked, arguments are wrapped to prevent leaking information across the realm boundary.
// The return value and any abrupt completions are also filtered.
class JSRemoteFunction final : public JSFunction {
public:
    typedef JSFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.remoteFunctionSpace<mode>();
    }

    JS_EXPORT_PRIVATE static JSRemoteFunction* create(VM&, JSGlobalObject*, JSCell* targetCallable);

    JSCell* target() { return m_target.get(); }
    JSFunction* targetFunction() {
        return static_cast<JSFunction*>(getJSFunction(target()));
    }
    JSGlobalObject* targetGlobalObject() { return targetFunction()->globalObject(); }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info()); 
    }
    
    static ptrdiff_t offsetOfTarget() { return OBJECT_OFFSETOF(JSRemoteFunction, m_target); }

    DECLARE_INFO;

private:
    JSRemoteFunction(VM&, NativeExecutable*, JSGlobalObject*, Structure*, JSCell* targetCallable);

    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    WriteBarrier<JSCell> m_target;
};

}
