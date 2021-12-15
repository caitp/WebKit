function assertThrows(thing, kind) {
	let error;
	let threw = false;
	let kindString = () => {
		return kind.name;
	}
	try {
		thing();
	} catch (e) {
		error = e;
		threw = true;
	}

	if (!threw)
		throw new Error("Expected to throw " + kindString() + ", but did not throw");

	if (!(error instanceof kind))
		throw new Error("Expected to throw " + kindString() + ", but threw " + error);
}
function assert(expr, message) {
	if (!expr)
		throw new Error(message || "Assertion failed");
}
print("creating ShadowRealm");
let r = new ShadowRealm;

print("test remote-calling a function which throws from outside a ShadowRealm");
r.evaluate("globalThis.lastValue = 1;");
if (globalThis.lastValue === 1)
	throw new Error("ran in wrong realm");

print("shadowrealm evaluate function which invokes its argument with `lastValue`");
let f = r.evaluate("(function(f) { return f(lastValue); })");

assert(!r.evaluate("!!globalThis.print"));
let exportValue = r.evaluate("(function(name, value) { this[name] = value; })");
print("exporting function print");
exportValue("print", globalThis.print);
print("testing that the export worked");
assert(r.evaluate("!!this.print"));

print("shadowrealm evaluate function which throws a TypeError");
let thrower = r.evaluate("(function thrower() { globalThis.print('throwing \"butts\"'); /*throw new TypeError('butts');*/ })");

print("test remote-calling a function which throws from outside a ShadowRealm");
assertThrows(() => { f(thrower); }, TypeError);


print("done!");