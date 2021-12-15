let r = new ShadowRealm;

let exportValue = r.evaluate(`
	(function(exportName, exportValue) {
		globalThis[exportName] = exportValue;
	})
`);

print("typeof globalThis.print: ", typeof globalThis.print);
print("typeof shadowRealm globalThis.print: ", r.evaluate("typeof globalThis.print"));
exportValue("print", globalThis.print);
print("typeof shadowRealm globalThis.print: ", r.evaluate("typeof globalThis.print"));
