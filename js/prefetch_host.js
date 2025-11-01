const decoder = new TextDecoder('utf-8');

export function createPrefetchHost(wasiFs) {
    let memory = null;
    const requestedPaths = new Set();
    let prefetchPromise = null;
    let commitIssued = false;

    function readPath(ptr, len) {
        if (!memory) {
            throw new Error('WASM memory not set for prefetch host');
        }
        const bytes = new Uint8Array(memory.buffer, ptr, len);
        let path = decoder.decode(bytes);
        const nul = path.indexOf('\0');
        if (nul !== -1) {
            path = path.slice(0, nul);
        }
        return path;
    }

    return {
        imports: {
            register_prefetch_path(ptr, len) {
                const path = readPath(ptr, len);
                if (path.length > 0) {
                    requestedPaths.add(path);
                }
                return 0;
            },
            commit_prefetches() {
                if (commitIssued) {
                    return 0;
                }
                commitIssued = true;

                const paths = Array.from(requestedPaths);
                if (paths.length === 0) {
                    prefetchPromise = Promise.resolve();
                    return 0;
                }

                prefetchPromise = wasiFs.prefetch(paths);
                return 0;
            },
        },
        setMemory(mem) {
            memory = mem;
        },
        async waitForPrefetch() {
            if (!prefetchPromise) {
                return;
            }
            await prefetchPromise;
        },
        hasPendingPrefetch() {
            return commitIssued;
        },
        reset() {
            requestedPaths.clear();
            commitIssued = false;
            prefetchPromise = null;
        },
    };
}
