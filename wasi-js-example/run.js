const { WASI } = require('node:wasi');
const fs = require('fs').promises;

async function runWasm() {
    // Initialize WASI
    const wasi = new WASI({
        version: 'preview1',
        args: [],
        env: {},
    });

    // Load the WASM module
    const wasmBuffer = await fs.readFile('../example.wasm');
    const wasmModule = await WebAssembly.compile(wasmBuffer);

    // Instantiate with WASI imports
    const instance = await WebAssembly.instantiate(wasmModule, wasi.getImportObject());

    // Optionally run main (for testing)
    wasi.start(instance);

    // Call the add function
    const add = instance.exports.add;
    const result = add(5, 7);
    console.log(`add(5, 7) = ${result}`);
}

runWasm().catch(console.error);
