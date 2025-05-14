// example.js

/**
 * Asynchronously loads and instantiates the WebAssembly module.
 * Works in both Node.js and browser environments.
 * @param {string} wasmPath - Path or URL to the .wasm file.
 * @returns {Promise<WebAssembly.Exports>} - A promise that resolves to the Wasm module's exports.
 */
async function setupWasm(wasmPath = 'example.wasm') {
    let wasmInstance; // Will hold the Wasm instance, accessible by stubs via closure

    // --- WASI Constants for fd_fdstat_get ---
    const WASI_FILETYPE_CHARACTER_DEVICE = 2;
    const WASI_FDFLAGS_APPEND = 1; // Example: fs_flags bit 0 is APPEND
    // For fs_rights_base, e.g., rights for writing to a file descriptor
    const WASI_RIGHTS_FD_WRITE = BigInt(1 << 6); // Right to fd_write

    const importObject = {
        wasi_snapshot_preview1: {
            /**
             * Close a file descriptor.
             * @param {number} fd - File descriptor.
             * @returns {number} WASI_ESUCCESS (0) on success.
             */
            fd_close: (fd) => {
                // console.debug(`[WASI STUB] fd_close(fd=${fd})`);
                return 0; // Return 0 for success (WASI_ESUCCESS)
            },

            /**
             * Get the attributes of a file descriptor.
             * @param {number} fd - File descriptor.
             * @param {number} statPtr - Pointer to memory where Fdstat structure will be written.
             * @returns {number} WASI_ESUCCESS (0) on success.
             */
            fd_fdstat_get: (fd, statPtr) => {
                // console.debug(`[WASI STUB] fd_fdstat_get(fd=${fd}, statPtr=${statPtr})`);
                if (wasmInstance && wasmInstance.exports.memory) {
                    const memoryView = new DataView(wasmInstance.exports.memory.buffer);
                    // For stdio (0: stdin, 1: stdout, 2: stderr), describe as character device.
                    if (fd === 0 || fd === 1 || fd === 2) {
                        // Fdstat structure layout (total 24 bytes for snapshot_preview1):
                        // fs_filetype (u8) @ offset 0
                        memoryView.setUint8(statPtr + 0, WASI_FILETYPE_CHARACTER_DEVICE, true);
                        // fs_flags (u16) @ offset 2 (after 1 byte filetype + 1 byte padding)
                        const flags = (fd === 1 || fd === 2) ? WASI_FDFLAGS_APPEND : 0;
                        memoryView.setUint16(statPtr + 2, flags, true);
                        // fs_rights_base (u64) @ offset 8 (after 2b flags + 4b padding)
                        let rights_base = BigInt(0);
                        if (fd === 0) { /* Placeholder for stdin read rights */ }
                        if (fd === 1 || fd === 2) rights_base |= WASI_RIGHTS_FD_WRITE;
                        memoryView.setBigUint64(statPtr + 8, rights_base, true);
                        // fs_rights_inheriting (u64) @ offset 16
                        memoryView.setBigUint64(statPtr + 16, BigInt(0), true); // Typically no inherited rights for stdio
                        return 0; // Success
                    } else {
                        return 8; // WASI_EBADF (Bad file descriptor) for others
                    }
                }
                return 50; // WASI_ENOMEM if memory isn't available (should not happen if instance is set)
            },

            /**
             * Adjust the offset of an open file descriptor.
             * @param {number} fd - File descriptor.
             * @param {bigint} offset_low - Low 32 bits of offset.
             * @param {bigint} offset_high - High 32 bits of offset (forming an i64 with offset_low).
             * @param {number} whence - Base for offset (0:SET, 1:CUR, 2:END).
             * @param {number} newOffsetPtr - Pointer to memory where new offset (u64) will be written.
             * @returns {number} WASI_ESUCCESS (0) on success.
             */
            fd_seek: (fd, offset_low, offset_high, whence, newOffsetPtr) => {
                // console.debug(`[WASI STUB] fd_seek(fd=${fd}, whence=${whence})`);
                // Stdio streams are typically not seekable.
                if (fd === 0 || fd === 1 || fd === 2) {
                    return 70; // WASI_ESPIPE (Illegal seek)
                }
                // For other FDs, this stub doesn't implement seeking.
                // Write 0 to newOffsetPtr and return success as a minimal stub.
                if (wasmInstance && wasmInstance.exports.memory) {
                    const memoryView = new DataView(wasmInstance.exports.memory.buffer);
                    try {
                        memoryView.setBigUint64(newOffsetPtr, BigInt(0), true); // Store 0 as the new offset
                    } catch (e) { /* Silently ignore if newOffsetPtr is invalid, though it shouldn't be */ }
                }
                return 0; // Pretend success for non-stdio file descriptors
            },

            /**
             * Write to a file descriptor.
             * @param {number} fd - File descriptor (1 for stdout, 2 for stderr).
             * @param {number} iovs_ptr - Pointer to an array of Ciovec structures.
             * @param {number} iovs_len - Number of Ciovec structures.
             * @param {number} nwritten_ptr - Pointer to memory where number of bytes written is stored.
             * @returns {number} WASI_ESUCCESS (0) on success.
             */
            fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
                let totalWrittenBytes = 0;
                let outputString = "";

                if (fd === 1 || fd === 2) { // Handle stdout and stderr
                    if (wasmInstance && wasmInstance.exports.memory) {
                        const memory = new Uint8Array(wasmInstance.exports.memory.buffer);
                        // Ciovec is {buf_ptr, buf_len}. In memory, it's an array of [ptr1, len1, ptr2, len2, ...]
                        const iovs_array_view = new Uint32Array(memory.buffer, iovs_ptr, iovs_len * 2);

                        for (let i = 0; i < iovs_len; i++) {
                            const current_iov_buf_ptr = iovs_array_view[i * 2];
                            const current_iov_buf_len = iovs_array_view[i * 2 + 1];
                            const chunk = memory.subarray(current_iov_buf_ptr, current_iov_buf_ptr + current_iov_buf_len);
                            outputString += new TextDecoder("utf-8").decode(chunk);
                            totalWrittenBytes += current_iov_buf_len;
                        }

                        if (outputString) {
                            // The printf in C main ends with "\n".
                            // console.log mimics this behavior by adding a newline if not present,
                            // or just prints the string. To match wasmtime's behavior more closely for
                            // typical printf, we can log the string as is.
                            if (fd === 1) console.log(outputString.endsWith('\n') ? outputString.slice(0,-1) : outputString);
                            if (fd === 2) console.error(outputString.endsWith('\n') ? outputString.slice(0,-1) : outputString);
                        }
                    }
                }
                // For other file descriptors, or if memory is not available, 0 bytes are "written".

                if (wasmInstance && wasmInstance.exports.memory) {
                    const memoryView = new DataView(wasmInstance.exports.memory.buffer);
                     try {
                        memoryView.setUint32(nwritten_ptr, totalWrittenBytes, true); // Store number of bytes written
                    } catch (e) { /* Silently ignore if nwritten_ptr is invalid */ }
                }
                return 0; // Success
            },

            /**
             * Terminate the process.
             * @param {number} exitCode - The exit code.
             */
            proc_exit: (exitCode) => {
                console.warn(`[WASI STUB] proc_exit called with exit_code: ${exitCode}`);
                // For a library function like 'add', we typically don't want to halt the JS environment.
                // If this were running a full Wasm application's main(), you might throw an error
                // or signal termination, especially for a non-zero exitCode.
                // Example: if (exitCode !== 0) throw new Error(`Wasm module exited with code ${exitCode}`);
            }
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
        const fs = require('fs').promises;
        const path = require('path');
        let resolvedWasmPath = path.isAbsolute(wasmPath) ? wasmPath : path.join(typeof __dirname !== 'undefined' ? __dirname : '.', wasmPath);
        try {
            wasmBytes = await fs.readFile(resolvedWasmPath);
        } catch (err) {
            // Fallback if __dirname is not available or path is relative to CWD
            if (err.code === 'ENOENT' && !path.isAbsolute(wasmPath)) {
                try {
                    wasmBytes = await fs.readFile(wasmPath); // Try wasmPath as relative to CWD
                } catch (e2) {
                    throw new Error(`Failed to read Wasm file: ${err.message} (tried ${resolvedWasmPath}) and ${e2.message} (tried ${wasmPath} relative to CWD)`);
                }
            } else {
                 throw err;
            }
        }
    }

    const wasmModule = await WebAssembly.compile(wasmBytes);
    const instance = await WebAssembly.instantiate(wasmModule, importObject);
    wasmInstance = instance; // Crucial: Make the instance available to the stubs via closure for memory access.

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
            // Note: The C main's printf output ("Testing add(3, 4) = 7\n") will NOT appear here
            // because we are only calling the 'add' export, not the Wasm's main entry point (_start).
            // If wasmExports._start() was called, then the fd_write stub would handle that printf.
            console.log(`JavaScript calling Wasm: add(${a}, ${b}) = ${result}`); // Expected: 7

            const x = 15;
            const y = 27;
            const result2 = wasmExports.add(x, y);
            console.log(`JavaScript calling Wasm: add(${x}, ${y}) = ${result2}`); // Expected: 42
        } else {
            console.error("The 'add' function was not found in Wasm exports.");
            console.log("Available exports:", wasmExports); // Log available exports for debugging
        }
    } catch (error) {
        console.error("Error running Wasm module:", error);
        if (error instanceof WebAssembly.LinkError) {
            console.error("LinkError details (check if all WASI imports are correctly stubbed):", error.message, error.stack);
        }
    }
}

// --- Environment-specific invocation (Node.js or Browser) ---
if (typeof process !== 'undefined' && process.versions != null && process.versions.node != null) {
    main().catch(err => console.error("Unhandled error in Node.js main:", err.stack || err));
    // Export for potential programmatic use in Node.js
    module.exports = {
        setupWasm,
        runWasmAdd: async (a, b) => {
            const exports = await setupWasm('example.wasm');
            return exports.add(a, b);
        }
    };
} else if (typeof window !== 'undefined') {
    main().catch(err => console.error("Unhandled error in browser main:", err.stack || err));
    // Expose a function to the global window object for easy testing from the browser console
    window.runWasmAddition = async (a, b) => {
        try {
            const exports = await setupWasm('example.wasm'); // ensure it's re-entrant or loads once
            return exports.add(a, b);
        } catch (error) {
            console.error("Error in window.runWasmAddition:", error.stack || error);
            throw error; // Re-throw so it can be seen in console
        }
    };
    console.log("Wasm script 'example.js' loaded. Call 'await window.runWasmAddition(num1, num2);' in the console to test.");
}
