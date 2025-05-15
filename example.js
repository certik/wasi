const fs = require('fs');

fs.readFile('example.wasm', (err, wasmBuffer) => {
  if (err) {
    console.error(err);
    return;
  }

  WebAssembly.instantiate(wasmBuffer)
    .then(obj => {
      const wasmExports = obj.instance.exports;

      const a = 3;
      const b = 4;
      const result = wasmExports.add(a, b);
      console.log(`JavaScript calling Wasm: add(${a}, ${b}) = ${result}`);

      const x = 15;
      const y = 27;
      const result2 = wasmExports.add(x, y);
      console.log(`JavaScript calling Wasm: add(${x}, ${y}) = ${result2}`);

      const z = 1.5;
      const result3 = wasmExports.mysin(z);
      console.log(`JavaScript calling Wasm: mysin(${z}) = ${result3}`);
    })
    .catch(err => console.error(err));
});
