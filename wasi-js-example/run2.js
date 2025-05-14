const { WASI } = require('wasi-js');
const fs = require('fs').promises;
const nodeBindings = require('wasi-js/dist/bindings/node');

async function runWasm() {
    // Initialize WASI with Node.js bindings
    const wasi = new WASI({
        args: [],
        env: {},
        bindings: { ...nodeBindings, fs },
    });

    // Load the WASM module
    const wasmBuffer = await fs.readFile('../example.wasm');
    const wasmModule = await WebAssembly.compile(wasmBuffer);

    // Instantiate the module
    const instance = await WebAssembly.instantiate(wasmModule, wasi.getImports());

    // Optionally run main
    wasi.start(instance);

    // Call the add function
    const add = instance.exports.add;
    const result = add(5, 7);
    console.log(`add(5, 7) = ${result}`);
}

runWasm().catch(console.error);
