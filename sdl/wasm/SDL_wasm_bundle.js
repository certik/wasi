// Bundle Loader for JSFS Binary Format
// Parses bundled files and creates a mapping for instant filesystem access

/**
 * Parse a JSFS bundle and return a Map of path -> Uint8Array
 *
 * Bundle Format:
 * - Header (9 bytes):
 *   - Magic bytes: 'JSFS' (4 bytes)
 *   - Version: 1 (1 byte)
 *   - Number of files: uint32 big-endian (4 bytes)
 * - Metadata (per file):
 *   - Path length: uint16 big-endian (2 bytes)
 *   - Path: UTF-8 string
 *   - File size: uint64 big-endian (8 bytes)
 *   - Content offset: uint64 big-endian (8 bytes)
 * - Contents: Concatenated file contents
 *
 * @param {ArrayBuffer} arrayBuffer - The bundle binary data
 * @returns {Map<string, Uint8Array>} - Map from file path to file contents
 */
export function loadBundle(arrayBuffer) {
    const view = new DataView(arrayBuffer);
    const bytes = new Uint8Array(arrayBuffer);
    const decoder = new TextDecoder('utf-8');

    let offset = 0;

    // Parse header
    const magic = decoder.decode(bytes.subarray(offset, offset + 4));
    offset += 4;

    if (magic !== 'JSFS') {
        throw new Error(`Invalid bundle magic: expected 'JSFS', got '${magic}'`);
    }

    const version = view.getUint8(offset);
    offset += 1;

    if (version !== 1) {
        throw new Error(`Unsupported bundle version: ${version}`);
    }

    const fileCount = view.getUint32(offset, false); // big-endian
    offset += 4;

    console.log(`[BundleLoader] Loading bundle: version=${version}, files=${fileCount}`);

    // Parse metadata
    const files = new Map();

    for (let i = 0; i < fileCount; i++) {
        // Read path length
        const pathLen = view.getUint16(offset, false); // big-endian
        offset += 2;

        // Read path
        const pathBytes = bytes.subarray(offset, offset + pathLen);
        const path = decoder.decode(pathBytes);
        offset += pathLen;

        // Read file size (uint64 - we'll use Number, should be safe for reasonable file sizes)
        const sizeHigh = view.getUint32(offset, false);
        const sizeLow = view.getUint32(offset + 4, false);
        const size = (sizeHigh * 0x100000000) + sizeLow;
        offset += 8;

        // Read content offset (uint64)
        const offsetHigh = view.getUint32(offset, false);
        const offsetLow = view.getUint32(offset + 4, false);
        const contentOffset = (offsetHigh * 0x100000000) + offsetLow;
        offset += 8;

        // Extract file content
        const content = bytes.slice(contentOffset, contentOffset + size);
        files.set(path, content);

        console.log(`[BundleLoader]   ${path}: ${size} bytes at offset ${contentOffset}`);
    }

    console.log(`[BundleLoader] Successfully loaded ${files.size} files from bundle`);
    return files;
}

/**
 * Fetch and load a bundle from a URL
 *
 * @param {string} url - The URL to fetch the bundle from
 * @returns {Promise<Map<string, Uint8Array>>} - Map from file path to file contents
 */
export async function fetchBundle(url) {
    console.log(`[BundleLoader] Fetching bundle from ${url}`);
    const response = await fetch(url);

    if (!response.ok) {
        throw new Error(`Failed to fetch bundle: HTTP ${response.status}`);
    }

    const arrayBuffer = await response.arrayBuffer();
    console.log(`[BundleLoader] Downloaded ${arrayBuffer.byteLength} bytes`);

    return loadBundle(arrayBuffer);
}
