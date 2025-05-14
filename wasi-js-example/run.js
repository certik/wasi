const { WASI } = require('@wasmtime/wasi');
const fs = require('fs').promises;

async function runWasm() {
    // Initialize WASI
    const wasi = new WASI({
        args: [],
        env: {},
    });

    // Load the WASM module
    const wasmBuffer = await fs.readFile('add.wasm');
    const wasmModule = await WebAssembly.compile(wasmBuffer);

    // Instantiate the module with WASI imports
    const instance = await WebAssembly.instantiate(wasmModule, {
        wasi_snapshot_preview1: wasi.wasiImport,
    });

    // Start the WASI instance (runs main, optional)
    wasi.start(instance);

    // Call the add function
    const add = instance.exports.add;
    const result = add(5, 7);
    console.log(`add(5, 7) = ${result}`);
}

runWasm().catch(console.error);
