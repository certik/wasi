const fs = require('fs');
const wasmBuffer = fs.readFileSync('example.wasm');

let wasmInstance;

const importObject = {
  wasi_snapshot_preview1: {
    fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
      if (!wasmInstance) {
        console.log('WASM instance not yet initialized');
        return 0;
      }
      const memory = new DataView(wasmInstance.exports.memory.buffer);
      let bytesWritten = 0;
      for (let i = 0; i < iovs_len; i++) {
        const ptr = memory.getUint32(iovs_ptr + i * 8, true);
        const len = memory.getUint32(iovs_ptr + i * 8 + 4, true);
        const str = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);
        process.stdout.write(Buffer.from(str));
        bytesWritten += len;
      }
      memory.setUint32(nwritten_ptr, bytesWritten, true);
      return 0;
    }
  }
};

WebAssembly.instantiate(wasmBuffer, importObject)
  .then(obj => {
    wasmInstance = obj.instance;
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
