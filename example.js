const fs = require('fs');
const wasmBuffer = fs.readFileSync('example.wasm');

const importObject = {
  env: {
    log_message: (ptr, instance) => {
      const memory = new Uint8Array(instance.exports.memory.buffer);
      const str = [];
      let i = ptr;
      while (memory[i] !== 0) {
        str.push(String.fromCharCode(memory[i]));
        i++;
      }
      console.log(str.join(''));
    }
  }
};

WebAssembly.instantiate(wasmBuffer, importObject)
  .then(obj => {
    const wasmInstance = obj.instance;
    importObject.env.log_message = (ptr) => {
      importObject.env.log_message(ptr, wasmInstance);
    };

    const wasmExports = wasmInstance.exports;

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
