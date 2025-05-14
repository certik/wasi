// example.js

/**
 * Asynchronously loads and instantiates the WebAssembly module.
 * Works in both Node.js and browser environments.
 * @param {string} wasmPath - Path or URL to the .wasm file.
 * @returns {Promise<WebAssembly.Exports>} - A promise that resolves to the Wasm module's exports.
 */
async function setupWasm(wasmPath = 'example.wasm') {
    let wasmInstance; // Will hold the Wasm instance to make its memory accessible to stubs if needed

    // Define the import object for WASI.
    // Your 'add' function doesn't use these, but the module declares them due to 'main' and stdio.
    // We provide minimal stubs to satisfy the Wasm module's import requirements during instantiation.
    const importObject = {
        wasi_snapshot_preview1: {
            // fd_write is used by printf. 'add' doesn't call it, but the module links it.
            fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
                // console.log('[WASI STUB] fd_write called');
                // This is a very basic stub. A real implementation would need to read from
                // wasmInstance.exports.memory, convert text, and print to console.
                // For now, we assume success and write 0 to nwritten_ptr if memory is ready.
                if (wasmInstance && wasmInstance.exports.memory) {
                    try {
                        const memoryView = new Uint32Array(wasmInstance.exports.memory.buffer);
                        memoryView[nwritten_ptr / 4] = 0; // Report 0 bytes written
                    } catch (e) {
                        // console.error("Error accessing memory in fd_write stub:", e);
                    }
                }
                return 0; // Return 0 for success
            },
            // proc_exit is called when main returns or exit() is called.
            proc_exit: (code) => {
                // console.log(`[WASI STUB] proc_exit called with code: ${code}`);
                // In a real application, you might throw an error or handle the exit.
                // For just calling 'add', this might not be reached unless C runtime calls it.
                if (typeof process !== 'undefined' && process.exit && code !== 0) {
                    // console.warn(`Wasm module attempted to exit with code ${code}`);
                }
                // It's important not to actually exit the Node process or break browser execution here
                // unless that's the desired behavior when the Wasm module intends to terminate.
            },
            // Stubs for environment variables (often needed by C runtime)
            environ_sizes_get: (environ_count_ptr, environ_buf_size_ptr) => {
                // console.log('[WASI STUB] environ_sizes_get called');
                if (wasmInstance && wasmInstance.exports.memory) {
                    const memoryView = new Uint32Array(wasmInstance.exports.memory.buffer);
                    memoryView[environ_count_ptr / 4] = 0; // No environment variables
                    memoryView[environ_buf_size_ptr / 4] = 0; // Buffer size 0
                }
                return 0;
            },
            environ_get: (environ_ptr_ptr, environ_buf_ptr) => {
                // console.log('[WASI STUB] environ_get called');
                return 0;
            },
            // Stubs for command-line arguments (often needed by C runtime)
            args_sizes_get: (argc_ptr, argv_buf_size_ptr) => {
                // console.log('[WASI STUB] args_sizes_get called');
                if (wasmInstance && wasmInstance.exports.memory) {
                    const memoryView = new Uint32Array(wasmInstance.exports.memory.buffer);
                    memoryView[argc_ptr / 4] = 0; // No arguments
                    memoryView[argv_buf_size_ptr / 4] = 0; // Buffer size 0
                }
                return 0;
            },
            args_get: (argv_ptr, argv_buf_ptr) => {
                // console.log('[WASI STUB] args_get called');
                return 0;
            },
            // Minimal stubs for other potentially required WASI functions to allow instantiation.
            // If instantiation fails due to missing imports, add them here with simple return values.
            fd_prestat_get: (fd, bufPtr) => 8, // WASI_EBADF (indicates no preopened directories)
            fd_prestat_dir_name: (fd, pathPtr, pathLen) => 28, // WASI_EINVAL
            fd_close: (fd) => 0, // Success
            fd_seek: (fd, offset_low, offset_high, whence, newOffsetPtr) => 70, // WASI_ESPIPE (not seekable, a common stub response)
            // Add more stubs like clock_time_get if specific errors arise:
            // clock_time_get: (clockid, precision, time) => { /* ... */ return 0; }
        }
    };

    let wasmBytes;
    if (typeof window !== 'undefined') { // Browser environment
        const response = await fetch(wasmPath);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status} when fetching ${wasmPath}`);
        }
        wasmBytes = await response.arrayBuffer();
    } else { // Node.js environment
        const fs = require('fs').promises; // Use promise-based fs
        const path = require('path');
        try {
            wasmBytes = await fs.readFile(path.join(__dirname, wasmPath));
        } catch (err) {
            // Fallback for potential __dirname issues in certain module contexts
            try {
                 wasmBytes = await fs.readFile(wasmPath);
            } catch (e) {
                 throw new Error(`Failed to read Wasm file in Node.js: ${err.message} & ${e.message}`);
            }
        }
    }

    // Compile the Wasm bytes then instantiate.
    // This allows the wasmInstance to be available in the scope of the importObject functions,
    // which is useful if they need to access Wasm memory (though access is still tricky during the exact moment of instantiation).
    const wasmModule = await WebAssembly.compile(wasmBytes);
    const instance = await WebAssembly.instantiate(wasmModule, importObject);
    wasmInstance = instance; // Make the instance available for stubs that might need its memory

    return wasmInstance.exports;
}

// --- Main execution logic (demonstrates usage) ---
async function main() {
    try {
        const wasmExports = await setupWasm('example.wasm');

        if (wasmExports && typeof wasmExports.add === 'function') {
            const a = 3;
            const b = 4;
            const result = wasmExports.add(a, b);
            console.log(`JavaScript calling Wasm: add(${a}, ${b}) = ${result}`); // Expected: 7

            const x = 15;
            const y = 27;
            const result2 = wasmExports.add(x, y);
            console.log(`JavaScript calling Wasm: add(${x}, ${y}) = ${result2}`); // Expected: 42

            const z = 1.5;
            const result3 = wasmExports.mysin(z);
            console.log(`JavaScript calling Wasm: mysin(${z}) = ${result3}`);
        } else {
            console.error("The 'add' function was not found in Wasm exports.");
            console.log("Available exports:", wasmExports);
        }
    } catch (error) {
        console.error("Error running Wasm module:", error);
    }
}

// --- Environment-specific invocation ---

// Check if running in Node.js
if (typeof process !== 'undefined' && process.versions != null && process.versions.node != null) {
    // Node.js execution
    main().catch(err => console.error("Unhandled error in Node.js main:", err));

    // To make it usable as a module in Node.js, e.g., `const myWasm = require('./example.js'); myWasm.runWasmAdd(1,2);`
    module.exports = {
        setupWasm, // Export setup if direct access to exports is needed
        runWasmAdd: async (a, b) => { // Convenience function
            const exports = await setupWasm('example.wasm');
            return exports.add(a, b);
        }
    };
} else if (typeof window !== 'undefined') {
    // Browser execution
    main().catch(err => console.error("Unhandled error in browser main:", err));

    // Expose a function to the global window object for easy testing from the browser console
    window.runWasmAddition = async (a, b) => {
        try {
            const exports = await setupWasm('example.wasm');
            return exports.add(a, b);
        } catch (error) {
            console.error("Error in window.runWasmAddition:", error);
            throw error; // Re-throw so it can be seen in console
        }
    };
    console.log("Wasm script 'example.js' loaded. Call 'await window.runWasmAddition(num1, num2);' in the console to test.");
}
