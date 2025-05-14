const WASI = require('wasi-js').default;
const fs = require('fs').promises;
const nodeBindings = require('wasi-js/dist/bindings/node');

async function runWasm() {
    // Provide explicit hrtime binding
    const bindings = {
        ...nodeBindings,
        fs,
        hrtime: () => process.hrtime.bigint() // Explicitly define hrtime
    };

    // Initialize WASI
    const wasi = new WASI({
        args: [],
        env: {},
        bindings
    });

    // Load the WASM module
    const wasmBuffer = await fs.readFile('../example.wasm');
    const wasmModule = await WebAssembly.compile(wasmBuffer);

    // Instantiate the module
    const instance = await WebAssembly.instantiate(wasmModule, wasi.getImports());

    // Run main (optional)
    wasi.start(instance);

    // Call the add function
    const add = instance.exports.add;
    const result = add(5, 7);
    console.log(`add(5, 7) = ${result}`);
}

runWasm().catch(console.error);
