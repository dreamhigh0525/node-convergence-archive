// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This tests that we can correctly call Function.prototype.call in the DFG, but more precisely, that we give the correct this object in case it is undefined"
);

var myObject = { call: function() { return [myObject, "myObject.call"] } };
var myFunction = function (arg1) { return [this, "myFunction", arg1] };
var myFunctionWithCall = function (arg1) { return [this, "myFunctionWithCall", arg1] };
myFunctionWithCall.call = function (arg1) { return [this, "myFunctionWithCall.call", arg1] };
Function.prototype.aliasedCall = Function.prototype.call;

for (var i = 0; i < 100; ++i) {
    shouldBe("myObject.call()", '[myObject, "myObject.call"]');
    shouldBe("myFunction('arg1')", '[this, "myFunction", "arg1"]');
    shouldBe("myFunction.call(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
    shouldBe("myFunction.call()", '[this, "myFunction", undefined]');
    shouldBe("myFunction.call(null)", '[this, "myFunction", undefined]');
    shouldBe("myFunction.call(undefined)", '[this, "myFunction", undefined]');
    shouldBe("myFunction.aliasedCall(myObject, 'arg1')", '[myObject, "myFunction", "arg1"]');
    shouldBe("myFunction.aliasedCall()", '[this, "myFunction", undefined]');
    shouldBe("myFunction.aliasedCall(null)", '[this, "myFunction", undefined]');
    shouldBe("myFunction.aliasedCall(undefined)", '[this, "myFunction", undefined]');
    shouldBe("myFunctionWithCall.call(myObject, 'arg1')", '[myFunctionWithCall, "myFunctionWithCall.call", myObject]');
    shouldBe("myFunctionWithCall.aliasedCall(myObject, 'arg1')", '[myObject, "myFunctionWithCall", "arg1"]');
}
