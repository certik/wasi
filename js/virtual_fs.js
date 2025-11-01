// Virtual Filesystem for WASM with on-demand HTTP fetching
// Implements WASI filesystem operations by fetching files from server when opened

export function createFetchingVirtualFileSystem() {
    const fileCache = new Map();  // path -> Uint8Array
    const openFiles = new Map();  // fd -> {data, offset}
    let memory = null;
    let nextFd = 4;
    const decoder = new TextDecoder('utf-8');

    function getDataView() {
        if (!memory) {
            throw new Error('WASI memory not set');
        }
        return new DataView(memory.buffer);
    }

    function readPath(pathPtr, pathLen) {
        const bytes = new Uint8Array(memory.buffer, pathPtr, pathLen);
        let path = decoder.decode(bytes);
        const nul = path.indexOf('\0');
        if (nul !== -1) {
            path = path.slice(0, nul);
        }
        return path;
    }

    const pendingFetches = new Map(); // path -> Promise<Uint8Array>

    async function fetchAndCache(path) {
        if (fileCache.has(path)) {
            return fileCache.get(path);
        }
        if (pendingFetches.has(path)) {
            return pendingFetches.get(path);
        }

        const fetchPromise = (async () => {
            try {
                console.log(`[VFS] Fetching: ${path}`);
                const response = await fetch(path);
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}`);
                }
                const arrayBuffer = await response.arrayBuffer();
                const data = new Uint8Array(arrayBuffer);
                fileCache.set(path, data);
                console.log(`[VFS] Cached: ${path} (${data.length} bytes)`);
                return data;
            } catch (err) {
                console.error(`[VFS] Fetch error for ${path}:`, err);
                throw err;
            } finally {
                pendingFetches.delete(path);
            }
        })();

        pendingFetches.set(path, fetchPromise);
        return fetchPromise;
    }

    // WASI path_open - fetch file on demand
    function path_open(dirfd, dirflags, pathPtr, pathLen, oflags,
                            rightsBase, rightsInheriting, fdflags, fdOutPtr) {
        try {
            const path = readPath(pathPtr, pathLen);
            console.log(`[VFS] Opening: ${path}`);

            const data = fileCache.get(path);
            if (!data) {
                console.error(`[VFS] File not cached: ${path}`);
                return 44; // ENOENT - No such file or directory
            }

            const fd = nextFd++;
            openFiles.set(fd, { data, offset: 0 });
            const dv = getDataView();
            dv.setUint32(fdOutPtr, fd, true);
            console.log(`[VFS] Opened ${path} as fd ${fd}`);
            return 0; // Success
        } catch (err) {
            console.error('[VFS] path_open failed:', err);
            return 8; // EBADF - Bad file descriptor
        }
    }

    async function prefetch(paths) {
        if (!paths || paths.length === 0) {
            return;
        }

        for (const path of paths) {
            if (fileCache.has(path)) {
                continue;
            }
            await fetchAndCache(path);
        }
    }

    // WASI fd_read
    function fd_read(fd, iovsPtr, iovsLen, nreadPtr) {
        const file = openFiles.get(fd);
        if (!file) {
            console.error(`[VFS] fd_read: fd ${fd} not found`);
            return 8; // EBADF
        }

        const dv = getDataView();
        let total = 0;

        for (let i = 0; i < iovsLen; i++) {
            const ptr = dv.getUint32(iovsPtr + i * 8, true);
            const len = dv.getUint32(iovsPtr + i * 8 + 4, true);
            const toRead = Math.min(len, file.data.length - file.offset);

            if (toRead > 0) {
                const chunk = file.data.subarray(file.offset, file.offset + toRead);
                new Uint8Array(memory.buffer, ptr, toRead).set(chunk);
                file.offset += toRead;
                total += toRead;
            }
        }

        dv.setUint32(nreadPtr, total, true);
        return 0; // Success
    }

    // WASI fd_close
    function fd_close(fd) {
        if (openFiles.delete(fd)) {
            console.log(`[VFS] Closed fd ${fd}`);
            return 0; // Success
        }
        console.error(`[VFS] fd_close: fd ${fd} not found`);
        return 8; // EBADF
    }

    // WASI fd_seek
    function fd_seek(fd, offset, whence, newOffsetPtr) {
        const file = openFiles.get(fd);
        if (!file) {
            return 8; // EBADF
        }

        // whence: 0 = SET, 1 = CUR, 2 = END
        if (whence === 0) {
            file.offset = Number(offset);
        } else if (whence === 1) {
            file.offset += Number(offset);
        } else if (whence === 2) {
            file.offset = file.data.length + Number(offset);
        }

        if (newOffsetPtr) {
            const dv = getDataView();
            dv.setBigUint64(newOffsetPtr, BigInt(file.offset), true);
        }

        return 0; // Success
    }

    // Stub implementations for args
    function args_sizes_get(argcPtr, argvBufSizePtr) {
        if (memory) {
            const dv = getDataView();
            dv.setUint32(argcPtr, 0, true);
            dv.setUint32(argvBufSizePtr, 0, true);
        }
        return 0;
    }

    function args_get(argvPtr, argvBufPtr) {
        return 0; // No arguments
    }

    // Preload files from a bundle map
    function preloadFromBundle(bundleMap) {
        for (const [path, content] of bundleMap.entries()) {
            fileCache.set(path, content);
            console.log(`[VFS] Preloaded from bundle: ${path} (${content.length} bytes)`);
        }
        console.log(`[VFS] Preloaded ${bundleMap.size} files from bundle`);
    }

    return {
        path_open,
        fd_read,
        fd_close,
        fd_seek,
        fd_tell: fd_seek,  // fd_tell is same as fd_seek in practice
        args_sizes_get,
        args_get,
        prefetch,
        preloadFromBundle,
        setMemory(mem) {
            memory = mem;
            console.log('[VFS] Memory set');
        }
    };
}
