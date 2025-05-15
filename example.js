// example.js

// Use globalThis which works in both Node.js and browsers
const globalScope = globalThis;

let wasmInstance = null;
let wasmExports = null;

// Function to load WASM bytes, works in Node.js and browser
async function loadWasmBytes(url) {
  // Check if we are in a Node.js environment
  if (typeof process === 'object' && process.versions != null && process.versions.node != null) {
    // Node.js
    // Use dynamic import to avoid requiring 'fs' in the browser
    const fs = await import('fs');
    try {
      console.log(`Loading WASM from file: ${url} in Node.js`);
      return fs.readFileSync(url);
    } catch (e) {
      console.error(`Error reading WASM file in Node.js: ${e}`);
      throw e;
    }
  } else {
    // Browser environment
    try {
      console.log(`Workspaceing WASM from URL: ${url} in browser`);
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return await response.arrayBuffer();
    } catch (e) {
      console.error(`Error fetching WASM file in browser: ${e}`);
      throw e;
    }
  }
}

// Use the standard TextDecoder, available globally in modern environments
const decoder = new TextDecoder();

const importObject = {
  wasi_snapshot_preview1: {
    fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
      // We'll only handle stdout (fd=1) and stderr (fd=2) for console output
      // Other file descriptors would require a more complete WASI implementation.
      if (fd !== 1 && fd !== 2) {
          console.error(`fd_write called with unsupported fd: ${fd}`);
          // In a real WASI env, this might return an error code like ERRNO_BADF
          return -1; // Indicate an error happened
      }

      if (!wasmInstance) {
        console.error('WASM instance not yet initialized in fd_write');
        return -1; // Indicate error
      }

      try {
        const memory = new DataView(wasmInstance.exports.memory.buffer);
        let bytesWritten = 0;
        let outputString = ''; // Accumulate the output as a string

        for (let i = 0; i < iovs_len; i++) {
          // Read the iovec structure (pointer, length)
          const ptr = memory.getUint32(iovs_ptr + i * 8, true); // pointer to buffer
          const len = memory.getUint32(iovs_ptr + i * 8 + 4, true); // length of buffer

          // Ensure pointer and length are within memory bounds
          if (ptr + len > wasmInstance.exports.memory.buffer.byteLength) {
              console.error(`fd_write: iovs_ptr ${iovs_ptr}, iov ${i}: offset ${ptr} + length ${len} exceeds memory size ${wasmInstance.exports.memory.buffer.byteLength}`);
              return -1; // Indicate error
          }

          // Create a Uint8Array view of the buffer in WASM memory
          const byteSlice = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);

          // Decode the byte slice to a string using TextDecoder
          // Use { stream: true } because output might be fragmented across multiple iovs
          outputString += decoder.decode(byteSlice, { stream: true });

          bytesWritten += len;
        }

        // Final decode call to flush any buffered data from { stream: true }
        outputString += decoder.decode();

        // Log the accumulated string to the console
        // console.log and console.error handle string output correctly in both environments
        if (outputString.length > 0) {
             if (fd === 1) { // stdout
                 console.log(outputString);
             } else { // stderr (fd === 2)
                 console.error(outputString);
             }
        }


        // Update the nwritten_ptr in WASM memory with the total *bytes* written
        // (This is what the WASM module expects)
        memory.setUint32(nwritten_ptr, bytesWritten, true);

        return 0; // Return 0 to indicate success (WASI convention)

      } catch (e) {
          console.error(`Error inside fd_write implementation: ${e}`);
          return -1; // Indicate error
      }
    }
  }
};

// Main function to load and run the WASM
async function loadAndRunWasm() {
    console.log('Loading WASM...');
    const wasmBytes = await loadWasmBytes('example.wasm');
    console.log('WASM bytes loaded.');

    const obj = await WebAssembly.instantiate(wasmBytes, importObject);
    wasmInstance = obj.instance;
    wasmExports = wasmInstance.exports;
    console.log('WASM instantiated successfully.');

    // --- Call WASM functions ---

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
}

// Run the main function
loadAndRunWasm();
