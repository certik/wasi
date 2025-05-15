// example.js

// Use globalThis which works in both Node.js and browsers
const globalScope = globalThis;

let wasmInstance = null;
let wasmExports = null;
let logAreaElement = null;

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
      // Only handle stdout (fd=1) and stderr (fd=2) for text output
      if (fd !== 1 && fd !== 2) {
          // Log to console, but don't send to HTML log area for other fds
          console.warn(`fd_write called with unsupported fd: ${fd}. Output ignored for HTML log.`);
          // Still write to console in Node/Browser dev tools
           if (!wasmInstance) return -1; // Avoid errors if memory isn't there
           const memory = new DataView(wasmInstance.exports.memory.buffer);
           // Dummy write to nwritten_ptr if possible
           try { memory.setUint32(nwritten_ptr, 0, true); } catch(e) { console.error("Failed to set nwritten_ptr for unsupported fd:", e); }
          return 0; // Success, but no bytes handled for this fd
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
          const ptr = memory.getUint32(iovs_ptr + i * 8, true);
          const len = memory.getUint32(iovs_ptr + i * 8 + 4, true);

           // Ensure pointer and length are within memory bounds
          if (ptr + len > wasmInstance.exports.memory.buffer.byteLength) {
              console.error(`fd_write: iovs_ptr ${iovs_ptr}, iov ${i}: offset ${ptr} + length ${len} exceeds memory size ${wasmInstance.exports.memory.buffer.byteLength}`);
              return -1; // Indicate error
          }

          const byteSlice = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);

          // Decode the byte slice to a string using TextDecoder
          outputString += decoder.decode(byteSlice, { stream: true }); // Use { stream: true } for partial decoding

          bytesWritten += len;
        }

        // Final decode call to flush any buffered data from { stream: true }
        outputString += decoder.decode();


        // --- Output to Console (same as before) ---
        if (outputString.length > 0) {
             if (fd === 1) { // stdout
                 console.log(outputString);
             } else { // stderr (fd === 2)
                 console.error(outputString);
             }
        }

        // --- Output to HTML Text Area (Browser only) ---
        // Check if we are in a browser environment AND the log area element was found
        // Node.js will skip this because 'document' is not defined
        if (typeof document === 'object' && logAreaElement) {
            // Append the string output to the text area's value
            logAreaElement.value += outputString;
            // Optional: Auto-scroll to the bottom
            logAreaElement.scrollTop = logAreaElement.scrollHeight;
        }


        // Update the nwritten_ptr in WASM memory with the total *bytes* written
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
