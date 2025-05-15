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
    const fs = await import('fs'); // Dynamic import for Node.js specific module
    try {
      return fs.readFileSync(url);
    } catch (e) {
      console.error(`Error reading WASM file in Node.js: ${e}`);
      throw e;
    }
  } else {
    // Browser environment
    try {
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

const importObject = {
  wasi_snapshot_preview1: {
    fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
      if (!wasmInstance) {
        console.log('WASM instance not yet initialized');
        return 0; // Return success but did nothing
      }

      try {
        const memory = new DataView(wasmInstance.exports.memory.buffer);
        let bytesWritten = 0;
        let output = ''; // Accumulate output for browsers

        for (let i = 0; i < iovs_len; i++) {
          const ptr = memory.getUint32(iovs_ptr + i * 8, true);
          const len = memory.getUint32(iovs_ptr + i * 8 + 4, true);
          const str = new Uint8Array(wasmInstance.exports.memory.buffer, ptr, len);

          // Environment-specific output
          if (typeof process === 'object' && process.versions != null && process.versions.node != null) {
            // Node.js
            process.stdout.write(Buffer.from(str)); // Use Buffer for Node.js compatibility
          } else {
            // Browser
            output += new TextDecoder().decode(str);
          }

          bytesWritten += len;
        }

        // In browser, log the accumulated output after the loop
        if (!(typeof process === 'object' && process.versions != null && process.versions.node != null)) {
             if (output.length > 0) {
                 console.log(output);
             }
        }


        // Update the nwritten_ptr with the total bytes written
        memory.setUint32(nwritten_ptr, bytesWritten, true);

        return 0; // Return 0 for success
      } catch (e) {
          console.error(`Error in fd_write: ${e}`);
          // Returning a non-zero value might indicate an error to the WASM module
          // WASI typically uses 0 for success, specific non-zeros for errors.
          // A generic non-zero is reasonable here for an unhandled JS error.
          return -1; // Or some other non-zero error code
      }
    }
  }
};

// Main function to load and run the WASM
async function loadAndRunWasm() {
  try {
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

    // Assuming mysin exists and takes a float
    // Note: Floating point handling between JS and WASM might require care
    // depending on how the WASM function is defined and exposed.
    // If mysin expects a double, ensure the WASM function signature matches.
    if (wasmExports.mysin) {
        const z = 1.5;
        const result3 = wasmExports.mysin(z);
        console.log(`JavaScript calling Wasm: mysin(${z}) = ${result3}`);
    } else {
        console.log('WASM export "mysin" not found.');
    }


    // --- Expose a function globally for browser console interaction ---
    // Attach to globalThis so it's available as `window.runWasmAddition` in browser
    // and `global.runWasmAddition` in Node (though less common usage in Node interactive shell)
    globalScope.runWasmAddition = function(num1, num2) {
        if (!wasmExports || !wasmExports.add) {
             console.error("WASM not loaded or add function not exported.");
             return null;
        }
        console.log(`runWasmAddition called with ${num1}, ${num2}`);
        const sum = wasmExports.add(num1, num2);
        console.log(`WASM add(${num1}, ${num2}) = ${sum}`);
        return sum;
    };
    console.log('runWasmAddition function exposed globally.');


  } catch (err) {
    console.error('Error loading or running WASM:', err);
  }
}

// Run the main function
loadAndRunWasm();
