import { init, Wasmer } from "@wasmer/sdk";
import { readFile } from "fs/promises";

async function runWasm() {
    // Initialize the Wasmer SDK
    await init();

    // Load the WASM module
    const wasmBuffer = await readFile("../example.wasm");
    const wasmModule = await WebAssembly.compile(wasmBuffer);

    // Create a Wasmer instance with WASI
    const instance = await Wasmer.fromModule(wasmModule, {
        args: [],
        env: {},
    });

    // Run main (optional, for testing)
    const { code, stdout } = await instance.entrypoint.run();
    console.log(`Main output: ${stdout}`);

    // Call the add function
    const add = instance.exports.add;
    const result = add(5, 7);
    console.log(`add(5, 7) = ${result}`);
}

runWasm().catch(console.error);
