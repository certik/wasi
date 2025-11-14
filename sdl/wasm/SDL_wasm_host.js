const fsDecoder = new TextDecoder('utf-8');

export function createWasmSDLHost(device, canvas) {
            let memory = null;
            let wasmBuddyAlloc = null;
            let wasmBuddyFree = null;
            let nextHandle = 1;
            const decoder = new TextDecoder('utf-8');
            const encoder = new TextEncoder();
            const SDL_GPU_SHADERFORMAT_WGSL = 1 << 6;

            function parseWGSLBindings(source) {
                const regex = /@group\((\d+)\)\s*@binding\((\d+)\)\s*var(?:<([^>]+)>)?\s+[A-Za-z0-9_]+\s*:\s*([^;]+);/g;
                const bindings = [];
                let match;
                while ((match = regex.exec(source)) !== null) {
                    const group = Number(match[1]);
                    const binding = Number(match[2]);
                    const addressSpace = match[3] ? match[3].toLowerCase() : '';
                    const type = match[4].trim();
                    let kind = null;
                    if (addressSpace.includes('uniform')) {
                        kind = 'uniform-buffer';
                    } else if (addressSpace.includes('storage')) {
                        kind = 'storage-buffer';
                    } else if (type.startsWith('texture_storage')) {
                        kind = 'storage-texture';
                    } else if (type.startsWith('texture_')) {
                        kind = 'texture';
                    } else if (type.startsWith('sampler')) {
                        kind = 'sampler';
                    }

                    if (kind) {
                        bindings.push({ group, binding, kind });
                    }
                }
                return bindings;
            }

            const context = canvas.getContext('webgpu');
            canvas.tabIndex = 0;
            canvas.addEventListener('click', () => {
                canvas.focus();
            });

            // SDL object storage
            const devices = new Map();
            const windows = new Map();
            const shaders = new Map();
            const pipelines = new Map();
            const commandBuffers = new Map();
            const renderPasses = new Map();
            const textures = new Map();
            const samplers = new Map();
            const buffers = new Map();
            const transferBuffers = new Map();
            const textureViews = new Map();
            const cachedEmptyBindGroups = new Map();
            const imageAssets = globalThis.__SDL_IMAGE_ASSETS || (globalThis.__SDL_IMAGE_ASSETS = new Map());

            // Mouse state
            let mouseX = 0;
            let mouseY = 0;
            let mouseLocked = false;
            let pendingPointerLockRequest = false;
            let fatalError = false;
            let fatalErrorReason = null;
            let fatalQuitDelivered = false;

            function tryRequestPointerLock() {
                if (document.pointerLockElement === canvas) {
                    pendingPointerLockRequest = false;
                    return;
                }

                pendingPointerLockRequest = true;
                canvas.focus();

                try {
                    const result = canvas.requestPointerLock?.();
                    if (result && typeof result.then === 'function') {
                        result.catch(() => {
                            pendingPointerLockRequest = true;
                        });
                    }
                } catch (err) {
                    console.warn('[SDL] requestPointerLock threw:', err);
                    pendingPointerLockRequest = true;
                }
            }

            // Event queue
            const eventQueue = [];

            // Error handling
            let lastError = "";

            function triggerFatalError(msg, detail) {
                if (fatalError) {
                    return;
                }
                fatalError = true;
                fatalErrorReason = msg;
                lastError = msg;
                console.error('[SDL Fatal]', msg);
                if (detail) {
                    console.error('[SDL Fatal Detail]', detail);
                }
                // Drop queued events and force the app to quit so logs stop after the error.
                eventQueue.length = 0;
                eventQueue.push({ type: 'quit' });
                fatalQuitDelivered = false;
            }

            function setError(msg) {
                lastError = msg;
                console.error('[SDL Error]', msg);
            }

            function readString(ptr, length) {
                if (!ptr || length === 0) return '';
                const bytes = new Uint8Array(memory.buffer, ptr, length);
                return decoder.decode(bytes);
            }

            function readCString(ptr) {
                if (!ptr || !memory) {
                    return '';
                }
                const bytes = new Uint8Array(memory.buffer);
                let end = ptr;
                while (end < bytes.length && bytes[end] !== 0) {
                    end++;
                }
                return decoder.decode(bytes.subarray(ptr, end));
            }

            function writeString(ptr, maxLen, str) {
                const bytes = encoder.encode(str);
                const len = Math.min(bytes.length, maxLen - 1);
                const dst = new Uint8Array(memory.buffer, ptr, maxLen);
                dst.set(bytes.subarray(0, len));
                dst[len] = 0; // null terminator
            }

            function mimeTypeForPath(path) {
                const lower = path.toLowerCase();
                if (lower.endsWith('.png')) return 'image/png';
                if (lower.endsWith('.jpg') || lower.endsWith('.jpeg')) return 'image/jpeg';
                if (lower.endsWith('.webp')) return 'image/webp';
                return null;
            }

            function isImagePath(path) {
                return mimeTypeForPath(path) !== null;
            }

            function loadImageElementFromBlob(blob) {
                return new Promise((resolve, reject) => {
                    const url = URL.createObjectURL(blob);
                    const img = new Image();
                    img.onload = () => {
                        URL.revokeObjectURL(url);
                        resolve(img);
                    };
                    img.onerror = (err) => {
                        URL.revokeObjectURL(url);
                        reject(err);
                    };
                    img.src = url;
                });
            }

            async function decodeImageAsset(path, bytes) {
                const mimeType = mimeTypeForPath(path);
                if (!mimeType) {
                    throw new Error(`Unsupported image type for ${path}`);
                }

                const slice = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
                const blob = new Blob([slice], { type: mimeType });
                let source;
                if (typeof createImageBitmap === 'function') {
                    source = await createImageBitmap(blob);
                } else {
                    source = await loadImageElementFromBlob(blob);
                }

                const width = source.width || source.naturalWidth;
                const height = source.height || source.naturalHeight;
                let canvas;
                if (typeof OffscreenCanvas !== 'undefined') {
                    canvas = new OffscreenCanvas(width, height);
                } else {
                    canvas = document.createElement('canvas');
                    canvas.width = width;
                    canvas.height = height;
                }

                const ctx = canvas.getContext('2d');
                if (!ctx) {
                    if (source.close) {
                        source.close();
                    }
                    throw new Error(`Failed to obtain 2D context for ${path}`);
                }

                ctx.drawImage(source, 0, 0);
                const imageData = ctx.getImageData(0, 0, width, height);
                if (source.close) {
                    source.close();
                }

                return {
                    width,
                    height,
                    pixels: new Uint8Array(imageData.data.buffer.slice(0)),
                };
            }

            async function preloadImageAssetsFromBundle(bundleMap) {
                if (!bundleMap) {
                    return;
                }

                const tasks = [];
                for (const [path, rawBytes] of bundleMap.entries()) {
                    if (!isImagePath(path)) {
                        continue;
                    }
                    if (imageAssets.has(path)) {
                        continue;
                    }
                    const bytes = rawBytes instanceof Uint8Array ? rawBytes : new Uint8Array(rawBytes);
                    tasks.push((async () => {
                        try {
                            const decoded = await decodeImageAsset(path, bytes);
                            imageAssets.set(path, decoded);
                            console.log(`[SDL] Pre-decoded image ${path}: ${decoded.width}x${decoded.height}`);
                        } catch (err) {
                            console.error(`[SDL] Failed to decode image ${path}:`, err);
                            throw err;
                        }
                    })());
                }

                if (tasks.length > 0) {
                    await Promise.all(tasks);
                }
            }

            function getAssetImage(path) {
                const asset = imageAssets.get(path);
                if (!asset) {
                    setError(`Asset not preloaded: ${path}`);
                    return null;
                }
                return asset;
            }

            function mapFilter(value) {
                return value === 1 ? 'linear' : 'nearest';
            }

            function mapMipmapMode(value) {
                return value === 1 ? 'linear' : 'nearest';
            }

            function mapAddressMode(value) {
                switch (value) {
                    case 0: return 'repeat';
                    case 3: return 'mirror-repeat';
                    case 1:
                    case 2:
                    default:
                        return 'clamp-to-edge';
                }
            }

            function getCommandBufferFromPass(passHandle) {
                const passEntry = renderPasses.get(passHandle);
                if (!passEntry) {
                    console.error('[SDL] bind samplers: invalid pass handle', passHandle);
                    return null;
                }
                return commandBuffers.get(passEntry.cmdbufHandle) || null;
            }

            function ensureBindGroups(cmdbuf) {
                if (fatalError) {
                    return;
                }
                if (!cmdbuf || !cmdbuf.currentPipeline) {
                    return;
                }
                const pipelineInfo = pipelines.get(cmdbuf.currentPipeline);
                if (!pipelineInfo) {
                    return;
                }

                const uniformLayout = pipelineInfo.uniformLayout || null;
                const fragmentUniformLayout = pipelineInfo.fragmentUniformLayout || null;
                const samplerLayout = pipelineInfo.samplerLayout || null;
                const uniformSlotCount = pipelineInfo.uniformSlotCount || 0;
                const fragmentUniformSlotCount = pipelineInfo.fragmentUniformSlotCount || 0;
                const uniformBindingIndex = pipelineInfo.uniformBindingIndex ?? 0;
                const fragmentUniformBindingIndex = pipelineInfo.fragmentUniformBindingIndex ?? 0;

                if (!uniformLayout || uniformSlotCount === 0) {
                    cmdbuf.uniformBindGroup = null;
                    cmdbuf.uniformBindGroupPipeline = null;
                    cmdbuf.uniformBindGroupDirty = false;
                } else if (cmdbuf.uniformBindGroupDirty) {
                    if (cmdbuf.uniformBuffer) {
                        cmdbuf.uniformBindGroup = device.createBindGroup({
                            layout: uniformLayout,
                            entries: [{
                                binding: uniformBindingIndex,
                                resource: { buffer: cmdbuf.uniformBuffer }
                            }]
                        });
                        cmdbuf.uniformBindGroupPipeline = cmdbuf.currentPipeline;
                    } else {
                        cmdbuf.uniformBindGroup = null;
                        cmdbuf.uniformBindGroupPipeline = null;
                    }
                    cmdbuf.uniformBindGroupDirty = false;
                }

                if (!fragmentUniformLayout || fragmentUniformSlotCount === 0) {
                    cmdbuf.fragmentUniformBindGroup = null;
                    cmdbuf.fragmentUniformBindGroupPipeline = null;
                    cmdbuf.fragmentUniformBindGroupDirty = false;
                } else if (cmdbuf.fragmentUniformBindGroupDirty) {
                    if (cmdbuf.uniformBuffer) {
                        cmdbuf.fragmentUniformBindGroup = device.createBindGroup({
                            layout: fragmentUniformLayout,
                            entries: [{
                                binding: fragmentUniformBindingIndex,
                                resource: { buffer: cmdbuf.uniformBuffer }
                            }]
                        });
                        cmdbuf.fragmentUniformBindGroupPipeline = cmdbuf.currentPipeline;
                    } else {
                        cmdbuf.fragmentUniformBindGroup = null;
                        cmdbuf.fragmentUniformBindGroupPipeline = null;
                    }
                    cmdbuf.fragmentUniformBindGroupDirty = false;
                }

                if (!samplerLayout) {
                    cmdbuf.textureBindGroup = null;
                    cmdbuf.textureBindGroupPipeline = null;
                    cmdbuf.textureBindGroupDirty = false;
                } else if (cmdbuf.textureBindGroupDirty) {
                    const samplerBindingInfo = pipelineInfo.samplerBindingInfo || [];
                    const entries = [];
                    const samplerResourceMap = new Map();
                    let missing = false;

                    for (const bindingInfo of samplerBindingInfo) {
                        const bound = cmdbuf.boundSamplers?.get(bindingInfo.slot);
                        if (!bound) {
                            missing = true;
                            break;
                        }
                        entries.push({ binding: bindingInfo.textureBinding, resource: bound.view });
                        if (!samplerResourceMap.has(bindingInfo.samplerBinding)) {
                            samplerResourceMap.set(bindingInfo.samplerBinding, bound.sampler);
                        }
                    }

                    if (!missing) {
                        for (const [binding, sampler] of samplerResourceMap.entries()) {
                            entries.push({ binding, resource: sampler });
                        }
                    }

                    const uniqueSamplerCount = samplerResourceMap.size;
                    const expectedEntryCount = samplerBindingInfo.length + uniqueSamplerCount;

                    if (!missing && entries.length === expectedEntryCount && entries.length > 0) {
                        cmdbuf.textureBindGroup = device.createBindGroup({
                            layout: samplerLayout,
                            entries
                        });
                        cmdbuf.textureBindGroupPipeline = cmdbuf.currentPipeline;
                    } else {
                        cmdbuf.textureBindGroup = null;
                        cmdbuf.textureBindGroupPipeline = null;
                    }
                    cmdbuf.textureBindGroupDirty = false;
                }
            }

            function updateUniformBufferForCommandBuffer(cmdbufHandle, data_ptr, length) {
                if (!memory) return;
                const cmdbuf = commandBuffers.get(cmdbufHandle);
                if (!cmdbuf) {
                    console.error('[SDL] updateUniformBuffer: invalid cmdbuf', cmdbufHandle);
                    return;
                }

                const uniformData = new Float32Array(memory.buffer, data_ptr, length / 4);
                if (!cmdbuf.uniformBuffer || cmdbuf.uniformBufferSize < length) {
                    if (cmdbuf.uniformBuffer) {
                        cmdbuf.uniformBuffer.destroy?.();
                    }
                    const bufferSize = Math.max(256, length);
                    cmdbuf.uniformBuffer = device.createBuffer({
                        size: bufferSize,
                        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
                    });
                    cmdbuf.uniformBufferSize = bufferSize;
                }

                device.queue.writeBuffer(cmdbuf.uniformBuffer, 0, uniformData);
                cmdbuf.uniformBindGroupDirty = true;
                cmdbuf.fragmentUniformBindGroupDirty = true;
                ensureBindGroups(cmdbuf);
            }

            function bindSamplersForPass(passHandle, firstSlot, bindingsPtr, count) {
                if (!memory || count === 0) {
                    return;
                }

                const cmdbuf = getCommandBufferFromPass(passHandle);
                if (!cmdbuf) {
                    return;
                }

                const dv = new DataView(memory.buffer);
                if (!cmdbuf.boundSamplers) {
                    cmdbuf.boundSamplers = new Map();
                }

                for (let i = 0; i < count; i++) {
                    const offset = bindingsPtr + i * 8;
                    const textureHandle = dv.getUint32(offset, true);
                    const samplerHandle = dv.getUint32(offset + 4, true);
                    const slot = firstSlot + i;

                    if (textureHandle === 0 || samplerHandle === 0) {
                        cmdbuf.boundSamplers.delete(slot);
                        continue;
                    }

                    const texture = textures.get(textureHandle);
                    const sampler = samplers.get(samplerHandle);

                    if (!texture || !sampler) {
                        setError(`Invalid texture/sampler binding: texture=${textureHandle}, sampler=${samplerHandle}`);
                        return;
                    }

                    let view = textureViews.get(textureHandle);
                    if (!view) {
                        try {
                            view = texture.createView();
                        } catch (err) {
                            setError(`Failed to create texture view: ${err.message}`);
                            console.error(err);
                            return;
                        }
                        textureViews.set(textureHandle, view);
                    }

                    cmdbuf.boundSamplers.set(slot, { view, sampler });
                }

                cmdbuf.textureBindGroupPipeline = null;
                cmdbuf.textureBindGroupDirty = true;
                ensureBindGroups(cmdbuf);
            }

            function getEmptyBindGroup(layout) {
                if (!layout) {
                    return null;
                }
                let bindGroup = cachedEmptyBindGroups.get(layout);
                if (!bindGroup) {
                    bindGroup = device.createBindGroup({ layout, entries: [] });
                    cachedEmptyBindGroups.set(layout, bindGroup);
                }
                return bindGroup;
            }

            function bindPipelineBindGroups(passEntry, cmdbuf, pipelineInfo) {
                const boundIndices = new Set();

                if (cmdbuf.uniformBindGroup &&
                    cmdbuf.uniformBindGroupPipeline === cmdbuf.currentPipeline &&
                    pipelineInfo.uniformGroupIndex !== null) {
                    passEntry.pass.setBindGroup(pipelineInfo.uniformGroupIndex, cmdbuf.uniformBindGroup);
                    boundIndices.add(pipelineInfo.uniformGroupIndex);
                }

                if (cmdbuf.fragmentUniformBindGroup &&
                    cmdbuf.fragmentUniformBindGroupPipeline === cmdbuf.currentPipeline &&
                    pipelineInfo.fragmentUniformGroupIndex !== null) {
                    passEntry.pass.setBindGroup(pipelineInfo.fragmentUniformGroupIndex, cmdbuf.fragmentUniformBindGroup);
                    boundIndices.add(pipelineInfo.fragmentUniformGroupIndex);
                }

                if (cmdbuf.textureBindGroup &&
                    cmdbuf.textureBindGroupPipeline === cmdbuf.currentPipeline &&
                    pipelineInfo.samplerGroupIndex !== null) {
                    passEntry.pass.setBindGroup(pipelineInfo.samplerGroupIndex, cmdbuf.textureBindGroup);
                    boundIndices.add(pipelineInfo.samplerGroupIndex);
                }

                const emptyIndices = pipelineInfo.emptyBindGroupIndices || [];
                if (emptyIndices.length > 0) {
                    const layouts = pipelineInfo.bindGroupLayouts || [];
                    for (const groupIndex of emptyIndices) {
                        if (boundIndices.has(groupIndex)) {
                            continue;
                        }
                        const layout = layouts[groupIndex];
                        const emptyBindGroup = getEmptyBindGroup(layout);
                        if (emptyBindGroup) {
                            passEntry.pass.setBindGroup(groupIndex, emptyBindGroup);
                        }
                    }
                }
            }

            // Set up event listeners
            canvas.addEventListener('mousemove', (e) => {
                if (mouseLocked) {
                    // Relative mouse motion for FPS controls
                    eventQueue.push({
                        type: 'mousemotion',
                        xrel: e.movementX,
                        yrel: e.movementY
                    });
                } else {
                    // Absolute mouse position
                    const rect = canvas.getBoundingClientRect();
                    const cssX = e.clientX - rect.left;
                    const cssY = e.clientY - rect.top;
                    mouseX = (cssX / rect.width) * canvas.width;
                    mouseY = (cssY / rect.height) * canvas.height;
                }
            });

            canvas.addEventListener('mousedown', () => {
                if (pendingPointerLockRequest && document.pointerLockElement !== canvas) {
                    tryRequestPointerLock();
                }
            });

            // Map JS key codes to SDL key codes
            function jsKeyToSDLKey(code) {
                const specialMap = {
                    'Space': 0x20,
                    'Enter': 0x0D,
                    'Tab': 0x09,
                    'Backspace': 0x08,
                    'Escape': 0x1B,
                    'ShiftLeft': 16,
                    'ShiftRight': 16,
                    'ControlLeft': 17,
                    'ControlRight': 17,
                    'AltLeft': 18,
                    'AltRight': 18,
                    'MetaLeft': 0x400000E3,   // SDLK_LGUI
                    'MetaRight': 0x400000E7,  // SDLK_RGUI
                    'ArrowLeft': 0x40000050,  // SDLK_LEFT
                    'ArrowUp': 0x40000052,    // SDLK_UP
                    'ArrowRight': 0x4000004f, // SDLK_RIGHT
                    'ArrowDown': 0x40000051,  // SDLK_DOWN
                    'CapsLock': 0x40000039,
                    'Delete': 0x7F,
                    'Insert': 0x40000049,
                    'Home': 0x4000004A,
                    'End': 0x4000004D,
                    'PageUp': 0x4000004B,
                    'PageDown': 0x4000004E,
                    'NumpadEnter': 0x40000058,
                };

                if (specialMap[code]) {
                    return specialMap[code];
                }

                if (code.startsWith('Key') && code.length >= 4) {
                    const letter = code.slice(3);
                    if (letter.length === 1) {
                        return letter.toLowerCase().charCodeAt(0);
                    }
                }

                if (code.startsWith('Digit') && code.length === 6) {
                    const digit = code.charAt(5);
                    if (digit >= '0' && digit <= '9') {
                        return digit.charCodeAt(0);
                    }
                }

                const punctuationMap = {
                    'Minus': '-'.charCodeAt(0),
                    'Equal': '='.charCodeAt(0),
                    'BracketLeft': '['.charCodeAt(0),
                    'BracketRight': ']'.charCodeAt(0),
                    'Backslash': '\\'.charCodeAt(0),
                    'Semicolon': ';'.charCodeAt(0),
                    'Quote': '\''.charCodeAt(0),
                    'Comma': ','.charCodeAt(0),
                    'Period': '.'.charCodeAt(0),
                    'Slash': '/'.charCodeAt(0),
                    'Backquote': '`'.charCodeAt(0),
                };

                if (punctuationMap[code]) {
                    return punctuationMap[code];
                }

                return 0;
            }

            window.addEventListener('keydown', (e) => {
                const sdlKey = jsKeyToSDLKey(e.code);
                if (sdlKey) {
                    e.preventDefault();
                    eventQueue.push({
                        type: 'keydown',
                        key: sdlKey
                    });
                }
            });

            window.addEventListener('keyup', (e) => {
                const sdlKey = jsKeyToSDLKey(e.code);
                if (sdlKey) {
                    e.preventDefault();
                    eventQueue.push({
                        type: 'keyup',
                        key: sdlKey
                    });
                }
            });

            // Handle pointer lock
            document.addEventListener('pointerlockchange', () => {
                mouseLocked = document.pointerLockElement === canvas;
                if (mouseLocked) {
                    pendingPointerLockRequest = false;
                }
            });

            document.addEventListener('pointerlockerror', () => {
                console.warn('[SDL] Pointer lock error');
                pendingPointerLockRequest = true;
            });

            window.addEventListener('beforeunload', () => {
                eventQueue.push({ type: 'quit' });
            });

            if (typeof device?.addEventListener === 'function') {
                device.addEventListener('uncapturederror', (event) => {
                    event?.preventDefault?.();
                    const message = event?.error?.message || event?.message || 'Unknown WebGPU error';
                    triggerFatalError(`WebGPU uncaptured error: ${message}`, event?.error || event);
                });
            } else if (device) {
                device.onuncapturederror = (event) => {
                    event?.preventDefault?.();
                    const message = event?.error?.message || event?.message || 'Unknown WebGPU error';
                    triggerFatalError(`WebGPU uncaptured error: ${message}`, event?.error || event);
                };
            }

            const imports = {
                init(flags) {
                    console.log('[SDL] Init with flags:', flags);
                    return 1; // success
                },

                quit() {
                    console.log('[SDL] Quit');
                },

                create_gpu_device(shader_format, debug) {
                    console.log('[SDL] CreateGPUDevice, format:', shader_format, 'debug:', debug);
                    const handle = nextHandle++;
                    devices.set(handle, device);
                    return handle;
                },

                destroy_gpu_device(deviceHandle) {
                    console.log('[SDL] DestroyGPUDevice:', deviceHandle);
                    devices.delete(deviceHandle);
                },

                create_window(title_ptr, title_len, w, h, flags) {
                    const title = readString(title_ptr, title_len);
                    console.log('[SDL] CreateWindow:', title, w, 'x', h, 'flags:', flags);

                    canvas.width = w;
                    canvas.height = h;
                    canvas.style.width = `${w}px`;
                    canvas.style.height = `${h}px`;

                    const handle = nextHandle++;
                    windows.set(handle, { canvas, width: w, height: h });
                    return handle;
                },

                destroy_window(windowHandle) {
                    console.log('[SDL] DestroyWindow:', windowHandle);
                    windows.delete(windowHandle);
                },

                claim_window_for_gpu_device(deviceHandle, windowHandle) {
                    console.log('[SDL] ClaimWindowForGPUDevice:', deviceHandle, windowHandle);

                    const format = navigator.gpu.getPreferredCanvasFormat();
                    context.configure({
                        device,
                        format,
                        usage: GPUTextureUsage.RENDER_ATTACHMENT,
                    });

                    return 1; // success
                },

                release_window_from_gpu_device(deviceHandle, windowHandle) {
                    console.log('[SDL] ReleaseWindowFromGPUDevice:', deviceHandle, windowHandle);
                },

                get_window_size_in_pixels(windowHandle, w_ptr, h_ptr) {
                    const win = windows.get(windowHandle);
                    if (win && memory) {
                        const view = new DataView(memory.buffer);
                        if (w_ptr) view.setInt32(w_ptr, win.width, true);
                        if (h_ptr) view.setInt32(h_ptr, win.height, true);
                    }
                },

                create_gpu_shader(deviceHandle, info_ptr) {
                    if (!memory) return 0;

                    const dv = new DataView(memory.buffer);
                    const code_ptr = dv.getUint32(info_ptr, true);
                    const code_size = dv.getUint32(info_ptr + 4, true);
                    const entrypoint_ptr = dv.getUint32(info_ptr + 8, true);
                    const format = dv.getUint32(info_ptr + 12, true);
                    const stage = dv.getUint32(info_ptr + 16, true);
                    const numSamplers = dv.getUint32(info_ptr + 20, true);
                    const numUniformBuffers = dv.getUint32(info_ptr + 24, true);
                    const numStorageBuffers = dv.getUint32(info_ptr + 28, true);
                    const numStorageTextures = dv.getUint32(info_ptr + 32, true);
                    const entryPoint = readCString(entrypoint_ptr) || 'main';

                    // Read shader code from WASM memory
                    const shaderBytes = new Uint8Array(memory.buffer, code_ptr, code_size);
                    const shaderCode = decoder.decode(shaderBytes);

                    console.log('[SDL] CreateGPUShader, stage:', stage, 'format:', format, 'code length:', code_size);

                    try {
                        const shaderModule = device.createShaderModule({ code: shaderCode });

                        let wgslBindings = null;
                        if (format === SDL_GPU_SHADERFORMAT_WGSL) {
                            wgslBindings = parseWGSLBindings(shaderCode);
                        }

                        // Check for compilation errors asynchronously
                        shaderModule.getCompilationInfo().then(info => {
                            if (info.messages.length > 0) {
                                console.error('[SDL] Shader compilation messages:', info.messages);
                            } else {
                                console.log('[SDL] Shader compiled successfully');
                            }
                        });

                        const handle = nextHandle++;
                        shaders.set(handle, {
                            module: shaderModule,
                            entryPoint,
                            stage,
                            format,
                            numSamplers,
                            numUniformBuffers,
                            numStorageBuffers,
                            numStorageTextures,
                            wgslBindings,
                        });
                        console.log('[SDL] CreateGPUShader, handle:', handle);
                        return handle;
                    } catch (e) {
                        console.error('[SDL] Failed to create shader:', e);
                        setError('Failed to create shader: ' + e.message);
                        return 0;
                    }
                },

                release_gpu_shader(deviceHandle, shaderHandle) {
                    console.log('[SDL] ReleaseGPUShader:', shaderHandle);
                    shaders.delete(shaderHandle);
                },

                get_gpu_swapchain_texture_format(deviceHandle, windowHandle) {
                    const format = navigator.gpu.getPreferredCanvasFormat();
                    console.log('[SDL] GetGPUSwapchainTextureFormat:', format);

                    // Map WebGPU format to SDL format enum
                    const formatMap = {
                        'bgra8unorm': 0x13, // SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
                        'rgba8unorm': 0x0F, // SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
                    };

                    return formatMap[format] || 0x13;
                },

                create_gpu_graphics_pipeline(deviceHandle, info_ptr) {
                    if (!memory) return 0;

                    const dv = new DataView(memory.buffer);

                    // Read pipeline create info
                    const vertexShaderHandle = dv.getUint32(info_ptr + 0, true);
                    const fragmentShaderHandle = dv.getUint32(info_ptr + 4, true);

                    const vertexBufferDescPtr = dv.getUint32(info_ptr + 8, true);
                    const numVertexBuffers = dv.getUint32(info_ptr + 12, true);
                    const vertexAttributePtr = dv.getUint32(info_ptr + 16, true);
                    const numVertexAttributes = dv.getUint32(info_ptr + 20, true);

                    const depthTestEnabled = dv.getUint8(info_ptr + 24) !== 0;
                    const depthWriteEnabled = dv.getUint8(info_ptr + 25) !== 0;
                    const depthCompareOp = dv.getUint32(info_ptr + 28, true);

                    const numColorTargets = dv.getUint32(info_ptr + 32, true);
                    const colorTargetDescPtr = dv.getUint32(info_ptr + 36, true);
                    const depthStencilFormat = dv.getUint32(info_ptr + 40, true);
                    const hasDepthStencilTarget = dv.getUint8(info_ptr + 44) !== 0;

                    const primitiveType = dv.getUint32(info_ptr + 48, true);

                    console.log('[SDL] CreateGPUGraphicsPipeline');
                    console.log('[SDL]   vertexShaderHandle = ', vertexShaderHandle);
                    console.log('[SDL]   fragmentShaderHandle = ', fragmentShaderHandle);

                    const vertexShaderInfo = shaders.get(vertexShaderHandle);
                    const fragmentShaderInfo = shaders.get(fragmentShaderHandle);

                    if (!vertexShaderInfo || !fragmentShaderInfo) {
                        setError('Invalid shader handles');
                        return 0;
                    }
                    const vertexShader = vertexShaderInfo.module;
                    const fragmentShader = fragmentShaderInfo.module;
                    const vertexEntryPoint = vertexShaderInfo.entryPoint || 'main';
                    const fragmentEntryPoint = fragmentShaderInfo.entryPoint || 'main';

                    const bindGroupLayouts = [];

                    const vertexWGSLBindings = vertexShaderInfo.wgslBindings || [];
                    const fragmentWGSLBindings = fragmentShaderInfo.wgslBindings || [];
                    const vertexUniformBinding = vertexWGSLBindings.find(binding => binding.kind === 'uniform-buffer');
                    const fragmentUniformBinding = fragmentWGSLBindings.find(binding => binding.kind === 'uniform-buffer');
                    const fragmentSamplerBindings = fragmentWGSLBindings
                        .filter(binding => binding.kind === 'texture' || binding.kind === 'sampler')
                        .sort((a, b) => (a.group - b.group) || (a.binding - b.binding));

                    let uniformLayout = null;
                    let uniformGroupIndex = null;
                    let uniformSlotCount = 0;
                    let uniformBindingIndex = 0;

                    let fragmentUniformLayout = null;
                    let fragmentUniformGroupIndex = null;
                    let fragmentUniformSlotCount = 0;
                    let fragmentUniformBindingIndex = 0;

                    const samplerEntries = [];
                    const samplerBindingInfo = [];
                    let samplerLayout = null;
                    let samplerGroupIndex = null;
                    const emptyBindGroupIndices = [];

                    const usingWGSLLayout = Boolean(vertexUniformBinding || fragmentUniformBinding || fragmentSamplerBindings.length > 0);

                    if (usingWGSLLayout) {
                        if (vertexUniformBinding) {
                            uniformGroupIndex = vertexUniformBinding.group;
                            uniformBindingIndex = vertexUniformBinding.binding;
                            uniformSlotCount = 1;
                            uniformLayout = device.createBindGroupLayout({
                                entries: [{
                                    binding: uniformBindingIndex,
                                    visibility: GPUShaderStage.VERTEX,
                                    buffer: { type: 'uniform' }
                                }]
                            });
                        }

                        if (fragmentUniformBinding) {
                            fragmentUniformGroupIndex = fragmentUniformBinding.group;
                            fragmentUniformBindingIndex = fragmentUniformBinding.binding;
                            fragmentUniformSlotCount = 1;
                            fragmentUniformLayout = device.createBindGroupLayout({
                                entries: [{
                                    binding: fragmentUniformBindingIndex,
                                    visibility: GPUShaderStage.FRAGMENT,
                                    buffer: { type: 'uniform' }
                                }]
                            });
                        }

                        if (fragmentSamplerBindings.length > 0) {
                            samplerGroupIndex = fragmentSamplerBindings[0].group;
                            const visibility = GPUShaderStage.FRAGMENT;
                            const samplerGroupBindings = fragmentSamplerBindings.filter(binding => binding.group === samplerGroupIndex);
                            for (const binding of samplerGroupBindings) {
                                if (binding.kind === 'texture') {
                                    samplerEntries.push({
                                        binding: binding.binding,
                                        visibility,
                                        texture: { sampleType: 'float' }
                                    });
                                } else if (binding.kind === 'sampler') {
                                    samplerEntries.push({
                                        binding: binding.binding,
                                        visibility,
                                        sampler: { type: 'filtering' }
                                    });
                                }
                            }

                            const textureBindings = samplerGroupBindings.filter(binding => binding.kind === 'texture');
                            const samplerBindings = samplerGroupBindings.filter(binding => binding.kind === 'sampler');
                            const resolvedSlotCount = textureBindings.length;
                            for (let slot = 0; slot < resolvedSlotCount; slot++) {
                                const textureBinding = textureBindings[slot]?.binding;
                                const samplerBinding = samplerBindings.length > 0
                                    ? samplerBindings[Math.min(slot, samplerBindings.length - 1)]?.binding
                                    : undefined;
                                if (textureBinding === undefined || samplerBinding === undefined) {
                                    continue;
                                }
                                samplerBindingInfo.push({ slot, textureBinding, samplerBinding });
                            }
                        }

                        if (samplerEntries.length > 0) {
                            samplerLayout = device.createBindGroupLayout({ entries: samplerEntries });
                        }

                        const groupIndices = [];
                        if (uniformLayout && uniformGroupIndex !== null) {
                            groupIndices.push(uniformGroupIndex);
                        }
                        if (samplerLayout && samplerGroupIndex !== null) {
                            groupIndices.push(samplerGroupIndex);
                        }
                        if (fragmentUniformLayout && fragmentUniformGroupIndex !== null) {
                            groupIndices.push(fragmentUniformGroupIndex);
                        }

                        const maxGroupIndex = groupIndices.length > 0 ? Math.max(...groupIndices) : -1;
                        if (maxGroupIndex >= 0) {
                            const emptyLayout = device.createBindGroupLayout({ entries: [] });
                            for (let group = 0; group <= maxGroupIndex; group++) {
                                if (group === uniformGroupIndex && uniformLayout) {
                                    bindGroupLayouts[group] = uniformLayout;
                                } else if (group === samplerGroupIndex && samplerLayout) {
                                    bindGroupLayouts[group] = samplerLayout;
                                } else if (group === fragmentUniformGroupIndex && fragmentUniformLayout) {
                                    bindGroupLayouts[group] = fragmentUniformLayout;
                                } else {
                                    bindGroupLayouts[group] = emptyLayout;
                                    emptyBindGroupIndices.push(group);
                                }
                            }
                        }
                    }

                    if (!usingWGSLLayout) {
                        const uniformEntries = [];
                        uniformSlotCount = Math.max(
                            vertexShaderInfo.numUniformBuffers || 0,
                            fragmentShaderInfo.numUniformBuffers || 0
                        );
                        for (let slot = 0; slot < uniformSlotCount; slot++) {
                            let visibility = 0;
                            if (slot < (vertexShaderInfo.numUniformBuffers || 0)) {
                                visibility |= GPUShaderStage.VERTEX;
                            }
                            if (slot < (fragmentShaderInfo.numUniformBuffers || 0)) {
                                visibility |= GPUShaderStage.FRAGMENT;
                            }
                            if (visibility !== 0) {
                                uniformEntries.push({
                                    binding: slot,
                                    visibility,
                                    buffer: { type: 'uniform' }
                                });
                            }
                        }
                        if (uniformEntries.length > 0) {
                            uniformGroupIndex = bindGroupLayouts.length;
                            uniformBindingIndex = 0;
                            uniformLayout = device.createBindGroupLayout({ entries: uniformEntries });
                            bindGroupLayouts.push(uniformLayout);
                        }

                        const maxSamplers = Math.max(
                            vertexShaderInfo.numSamplers || 0,
                            fragmentShaderInfo.numSamplers || 0
                        );
                        for (let slot = 0; slot < maxSamplers; slot++) {
                            let visibility = 0;
                            if (slot < (vertexShaderInfo.numSamplers || 0)) {
                                visibility |= GPUShaderStage.VERTEX;
                            }
                            if (slot < (fragmentShaderInfo.numSamplers || 0)) {
                                visibility |= GPUShaderStage.FRAGMENT;
                            }
                            if (visibility !== 0) {
                                const textureBinding = slot * 2;
                                const samplerBinding = slot * 2 + 1;
                                samplerEntries.push({
                                    binding: textureBinding,
                                    visibility,
                                    texture: { sampleType: 'float' }
                                });
                                samplerEntries.push({
                                    binding: samplerBinding,
                                    visibility,
                                    sampler: { type: 'filtering' }
                                });
                                samplerBindingInfo.push({ slot, textureBinding, samplerBinding });
                            }
                        }

                        if (samplerEntries.length > 0) {
                            samplerGroupIndex = bindGroupLayouts.length;
                            samplerLayout = device.createBindGroupLayout({ entries: samplerEntries });
                            bindGroupLayouts.push(samplerLayout);
                        }
                    }

                    const pipelineLayout = device.createPipelineLayout({
                        bindGroupLayouts
                    });

                    // Read vertex input state
                    const formatMap = {
                        0: 'float32',    // SDL_GPU_VERTEXELEMENTFORMAT_FLOAT
                        1: 'float32x2',  // SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2
                        2: 'float32x3',  // SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3
                        3: 'float32x4',  // SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4
                    };

                    const vertexBuffers = [];
                    if (vertexBufferDescPtr !== 0 && vertexAttributePtr !== 0 &&
                        numVertexBuffers > 0 && numVertexAttributes > 0) {
                        for (let bufIndex = 0; bufIndex < numVertexBuffers; bufIndex++) {
                            const descOffset = vertexBufferDescPtr + bufIndex * 12;
                            const slot = dv.getUint32(descOffset + 0, true);
                            const pitch = dv.getUint32(descOffset + 4, true);
                            const inputRate = dv.getUint32(descOffset + 8, true);

                            const attributes = [];
                            for (let attrIndex = 0; attrIndex < numVertexAttributes; attrIndex++) {
                                const attrOffset = vertexAttributePtr + attrIndex * 16;
                                const location = dv.getUint32(attrOffset + 0, true);
                                const bufferSlot = dv.getUint32(attrOffset + 4, true);
                                const format = dv.getUint32(attrOffset + 8, true);
                                const offset = dv.getUint32(attrOffset + 12, true);

                                if (bufferSlot !== slot) {
                                    continue;
                                }

                                const webgpuFormat = formatMap[format];
                                if (!webgpuFormat) {
                                    console.warn('[SDL] Unsupported vertex format', format);
                                    continue;
                                }

                                attributes.push({
                                    shaderLocation: location,
                                    offset,
                                    format: webgpuFormat
                                });
                            }

                            if (attributes.length > 0) {
                                vertexBuffers.push({
                                    arrayStride: pitch,
                                    stepMode: inputRate === 1 ? 'instance' : 'vertex',
                                    attributes
                                });
                            }
                        }
                    }

                    // Read depth stencil state
                    let depthStencil = undefined;
                    if (hasDepthStencilTarget) {
                        const compareOpMap = {
                            0: 'less',
                            1: 'less-equal',
                        };

                        const depthFormatMap = {
                            1: 'depth16unorm',
                        };
                        const depthFormat = depthFormatMap[depthStencilFormat] || 'depth16unorm';

                        depthStencil = {
                            format: depthFormat,
                            depthWriteEnabled: depthWriteEnabled && depthTestEnabled,
                            depthCompare: depthTestEnabled ? (compareOpMap[depthCompareOp] || 'less') : 'always',
                        };
                    }

                    try {
                        const pipelineDesc = {
                            layout: pipelineLayout,
                            vertex: {
                                module: vertexShader,
                                entryPoint: vertexEntryPoint,
                                buffers: vertexBuffers
                            },
                            fragment: {
                                module: fragmentShader,
                                entryPoint: fragmentEntryPoint,
                                targets: [{
                                    format: navigator.gpu.getPreferredCanvasFormat(),
                                }],
                            },
                            primitive: {
                                topology: 'triangle-list',
                            },
                        };

                        if (depthStencil) {
                            pipelineDesc.depthStencil = depthStencil;
                        }

                        const pipeline = device.createRenderPipeline(pipelineDesc);

                        const handle = nextHandle++;
                        pipelines.set(handle, {
                            pipeline,
                            bindGroupLayouts,
                            uniformLayout,
                            fragmentUniformLayout,
                            samplerLayout,
                            uniformSlotCount,
                            fragmentUniformSlotCount,
                            uniformGroupIndex,
                            fragmentUniformGroupIndex,
                            samplerGroupIndex,
                            uniformBindingIndex,
                            fragmentUniformBindingIndex,
                            samplerBindingInfo,
                            emptyBindGroupIndices
                        });
                        return handle;
                    } catch (e) {
                        setError('Failed to create pipeline: ' + e.message);
                        console.error(e);
                        return 0;
                    }
                },

                release_gpu_graphics_pipeline(deviceHandle, pipelineHandle) {
                    console.log('[SDL] ReleaseGPUGraphicsPipeline:', pipelineHandle);
                    pipelines.delete(pipelineHandle);
                },

                acquire_gpu_command_buffer(deviceHandle) {
                    if (fatalError) {
                        console.error('[SDL] AcquireGPUCommandBuffer after fatal error:', fatalErrorReason);
                        return 0;
                    }
                    const encoder = device.createCommandEncoder();
                    const handle = nextHandle++;
                    commandBuffers.set(handle, {
                        encoder,
                        uniformBuffer: null,
                        uniformBufferSize: 0,
                        uniformBindGroup: null,
                        uniformBindGroupPipeline: null,
                        fragmentUniformBindGroup: null,
                        fragmentUniformBindGroupPipeline: null,
                        textureBindGroup: null,
                        textureBindGroupPipeline: null,
                        boundSamplers: new Map(),
                        uniformBindGroupDirty: false,
                        fragmentUniformBindGroupDirty: false,
                        textureBindGroupDirty: false,
                        currentPipeline: null,
                        swapchainTextures: []
                    });
                    console.log('[SDL] AcquireGPUCommandBuffer, handle:', handle);
                    return handle;
                },

                wait_and_acquire_gpu_swapchain_texture(cmdbufHandle, windowHandle, texture_out_ptr, w_ptr, h_ptr) {
                    const texture = context.getCurrentTexture();
                    const textureHandle = nextHandle++;
                    textures.set(textureHandle, texture);

                    const cmdbuf = commandBuffers.get(cmdbufHandle);
                    if (cmdbuf) {
                        cmdbuf.swapchainTextures.push(textureHandle);
                    }

                    if (memory && texture_out_ptr) {
                        const dv = new DataView(memory.buffer);
                        dv.setUint32(texture_out_ptr, textureHandle, true);
                    }

                    const win = windows.get(windowHandle);
                    if (win && memory) {
                        const dv = new DataView(memory.buffer);
                        if (w_ptr) dv.setUint32(w_ptr, win.width, true);
                        if (h_ptr) dv.setUint32(h_ptr, win.height, true);
                    }

                    return 1; // success
                },

                begin_gpu_render_pass(cmdbufHandle, color_targets_ptr, num_color_targets, depth_stencil_ptr) {
                    if (!memory) return 0;

                    const cmdbuf = commandBuffers.get(cmdbufHandle);
                    if (!cmdbuf) {
                        console.error('[SDL] begin_gpu_render_pass: invalid cmdbuf handle', cmdbufHandle);
                        return 0;
                    }

                    const dv = new DataView(memory.buffer);

                    // Read color target info (assuming single target for now)
                    const textureHandle = dv.getUint32(color_targets_ptr, true);
                    const clear_r = dv.getFloat32(color_targets_ptr + 4, true);
                    const clear_g = dv.getFloat32(color_targets_ptr + 8, true);
                    const clear_b = dv.getFloat32(color_targets_ptr + 12, true);
                    const clear_a = dv.getFloat32(color_targets_ptr + 16, true);
                    const load_op = dv.getUint32(color_targets_ptr + 20, true);
                    const store_op = dv.getUint32(color_targets_ptr + 24, true);

                    const texture = textures.get(textureHandle);
                    if (!texture) return 0;

                    const renderPassDesc = {
                        colorAttachments: [{
                            view: texture.createView(),
                            loadOp: load_op === 0 ? 'clear' : 'load',
                            storeOp: store_op === 0 ? 'store' : 'discard',
                            clearValue: { r: clear_r, g: clear_g, b: clear_b, a: clear_a },
                        }],
                    };

                    // Read depth stencil target if provided
                    if (depth_stencil_ptr !== 0) {
                        const depthTextureHandle = dv.getUint32(depth_stencil_ptr, true);
                        const depth_clear = dv.getFloat32(depth_stencil_ptr + 4, true);
                        const depth_load_op = dv.getUint32(depth_stencil_ptr + 8, true);
                        const depth_store_op = dv.getUint32(depth_stencil_ptr + 12, true);

                        const depthTexture = textures.get(depthTextureHandle);
                        if (depthTexture) {
                            renderPassDesc.depthStencilAttachment = {
                                view: depthTexture.createView(),
                                depthLoadOp: depth_load_op === 0 ? 'clear' : 'load',
                                depthStoreOp: depth_store_op === 0 ? 'store' : 'discard',
                                depthClearValue: depth_clear,
                            };
                        }
                    }

                    const renderPass = cmdbuf.encoder.beginRenderPass(renderPassDesc);

                    const handle = nextHandle++;
                    renderPasses.set(handle, { pass: renderPass, cmdbufHandle: cmdbufHandle });
                    return handle;
                },

                end_gpu_render_pass(passHandle) {
                    console.log('[SDL] EndGPURenderPass, handle:', passHandle);
                    const passEntry = renderPasses.get(passHandle);
                    if (passEntry) {
                        passEntry.pass.end();
                        renderPasses.delete(passHandle);
                        console.log('[SDL] Render pass ended');
                    } else {
                        console.error('[SDL] end_gpu_render_pass: invalid handle', passHandle);
                    }
                },

                bind_gpu_graphics_pipeline(passHandle, pipelineHandle) {
                    console.log('[SDL] BindGPUGraphicsPipeline, pass:', passHandle, 'pipeline:', pipelineHandle);
                    const passEntry = renderPasses.get(passHandle);
                    const pipelineInfo = pipelines.get(pipelineHandle);

                    if (passEntry && pipelineInfo) {
                        passEntry.pass.setPipeline(pipelineInfo.pipeline);
                        const cmdbuf = commandBuffers.get(passEntry.cmdbufHandle);
                        if (cmdbuf) {
                            cmdbuf.currentPipeline = pipelineHandle;
                            cmdbuf.uniformBindGroupPipeline = null;
                            cmdbuf.fragmentUniformBindGroupPipeline = null;
                            cmdbuf.textureBindGroupPipeline = null;

                            // Check which bind groups this pipeline actually has
                            const hasBindGroup0 = pipelineInfo.uniformGroupIndex !== null && (pipelineInfo.uniformSlotCount || 0) > 0;
                            const hasFragmentUniformGroup = pipelineInfo.fragmentUniformGroupIndex !== null && (pipelineInfo.fragmentUniformSlotCount || 0) > 0;
                            const hasBindGroup1 = pipelineInfo.samplerGroupIndex !== null && (pipelineInfo.samplerBindingInfo?.length || 0) > 0;

                            console.log('[SDL] Pipeline bind groups', pipelineHandle, { hasBindGroup0, hasFragmentUniformGroup, hasBindGroup1 });

                            // Clear bind groups and mark as dirty only if pipeline has them
                            if (hasBindGroup0) {
                                cmdbuf.uniformBindGroup = null;
                                cmdbuf.uniformBindGroupDirty = true;
                            } else {
                                cmdbuf.uniformBindGroup = null;
                                cmdbuf.uniformBindGroupDirty = false;
                            }

                            if (hasFragmentUniformGroup) {
                                cmdbuf.fragmentUniformBindGroup = null;
                                cmdbuf.fragmentUniformBindGroupDirty = true;
                            } else {
                                cmdbuf.fragmentUniformBindGroup = null;
                                cmdbuf.fragmentUniformBindGroupPipeline = null;
                                cmdbuf.fragmentUniformBindGroupDirty = false;
                            }

                            if (hasBindGroup1) {
                                cmdbuf.textureBindGroup = null;
                                cmdbuf.textureBindGroupDirty = true;
                            } else {
                                cmdbuf.textureBindGroup = null;
                                cmdbuf.textureBindGroupPipeline = null;
                                cmdbuf.textureBindGroupDirty = false;
                            }

                            ensureBindGroups(cmdbuf);
                        }
                        console.log('[SDL] Pipeline set');
                    } else {
                        console.error('[SDL] bind_gpu_graphics_pipeline: invalid handles', { pass: !!passEntry, pipeline: !!pipelineInfo });
                    }
                },

                push_gpu_vertex_uniform_data(cmdbufHandle, slot, data_ptr, length) {
                    updateUniformBufferForCommandBuffer(cmdbufHandle, data_ptr, length);
                },

                push_gpu_fragment_uniform_data(cmdbufHandle, slot, data_ptr, length) {
                    updateUniformBufferForCommandBuffer(cmdbufHandle, data_ptr, length);
                },

                create_gpu_buffer(deviceHandle, info_ptr) {
                    if (!memory) return 0;
                    const dv = new DataView(memory.buffer);
                    const usage = dv.getUint32(info_ptr, true);
                    const size = dv.getUint32(info_ptr + 4, true);

                    const usageMap = {
                        1: GPUBufferUsage.VERTEX,  // SDL_GPU_BUFFERUSAGE_VERTEX
                        2: GPUBufferUsage.INDEX,   // SDL_GPU_BUFFERUSAGE_INDEX
                    };

                    const buffer = device.createBuffer({
                        size: size,
                        usage: usageMap[usage] | GPUBufferUsage.COPY_DST
                    });

                    const handle = nextHandle++;
                    buffers.set(handle, buffer);
                    console.log('[SDL] CreateGPUBuffer, handle:', handle, 'size:', size);
                    return handle;
                },

                release_gpu_buffer(deviceHandle, bufferHandle) {
                    console.log('[SDL] ReleaseGPUBuffer:', bufferHandle);
                    const buffer = buffers.get(bufferHandle);
                    if (buffer) {
                        buffer.destroy();
                        buffers.delete(bufferHandle);
                    }
                },

                create_gpu_transfer_buffer(deviceHandle, info_ptr) {
                    if (!memory) return 0;
                    const dv = new DataView(memory.buffer);
                    const usage = dv.getUint32(info_ptr, true);
                    const size = dv.getUint32(info_ptr + 4, true);

                    // Transfer buffers are mapped on CPU
                    const buffer = device.createBuffer({
                        size: size,
                        usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.COPY_SRC
                    });

                    const handle = nextHandle++;
                    transferBuffers.set(handle, { buffer, mappedData: null, size });
                    console.log('[SDL] CreateGPUTransferBuffer, handle:', handle, 'size:', size);
                    return handle;
                },

                release_gpu_transfer_buffer(deviceHandle, bufferHandle) {
                    console.log('[SDL] ReleaseGPUTransferBuffer:', bufferHandle);
                    const transferBuf = transferBuffers.get(bufferHandle);
                    if (transferBuf) {
                        transferBuf.buffer.destroy();
                        transferBuffers.delete(bufferHandle);
                    }
                },

                map_gpu_transfer_buffer(deviceHandle, bufferHandle, cycle) {
                    if (!memory || !wasmBuddyAlloc) return 0;
                    const transferBuf = transferBuffers.get(bufferHandle);
                    if (!transferBuf) return 0;

                    // If already mapped, return existing pointer
                    if (transferBuf.mappedPtr) {
                        return transferBuf.mappedPtr;
                    }

                    // Use buddy allocator to allocate memory for this buffer
                    const ptr = wasmBuddyAlloc(transferBuf.size);
                    if (!ptr) {
                        console.error('[SDL] MapGPUTransferBuffer: buddy_alloc failed for size', transferBuf.size);
                        return 0;
                    }

                    // Store the pointer so we can read from it later and free it on unmap
                    transferBuf.mappedPtr = ptr;

                    console.log('[SDL] MapGPUTransferBuffer, handle:', bufferHandle, 'size:', transferBuf.size, 'ptr:', ptr);
                    return ptr;
                },

                unmap_gpu_transfer_buffer(deviceHandle, bufferHandle) {
                    const transferBuf = transferBuffers.get(bufferHandle);
                    if (!transferBuf || !transferBuf.mappedPtr) return;

                    // Read the data from WASM memory into our transfer buffer
                    if (memory && transferBuf.mappedPtr) {
                        const data = new Uint8Array(memory.buffer, transferBuf.mappedPtr, transferBuf.size);
                        transferBuf.mappedData = data.slice(); // Copy the data

                        // For overlay buffer (17), validate vertices
                        if (bufferHandle === 17) {
                            const floats = new Float32Array(transferBuf.mappedData.buffer);

                            // Look for triangles that might span large areas
                            let largeTriangles = [];
                            for (let i = 0; i < Math.min(20000, floats.length / 6) - 2; i += 3) {
                                // Get triangle vertices
                                const x1 = floats[i * 6 + 0], y1 = floats[i * 6 + 1];
                                const x2 = floats[(i+1) * 6 + 0], y2 = floats[(i+1) * 6 + 1];
                                const x3 = floats[(i+2) * 6 + 0], y3 = floats[(i+2) * 6 + 1];

                                // Calculate triangle area (cross product / 2)
                                const area = Math.abs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)) / 2;

                                // If triangle is very large, record it
                                if (area > 0.5) { // > 0.5 in clip space is huge
                                    const r = floats[i * 6 + 2], g = floats[i * 6 + 3], b = floats[i * 6 + 4];
                                    largeTriangles.push({
                                        tri: i/3,
                                        area: area.toFixed(3),
                                        color: `rgb(${r.toFixed(2)},${g.toFixed(2)},${b.toFixed(2)})`,
                                        v1: `(${x1.toFixed(2)},${y1.toFixed(2)})`,
                                        v2: `(${x2.toFixed(2)},${y2.toFixed(2)})`,
                                        v3: `(${x3.toFixed(2)},${y3.toFixed(2)})`
                                    });
                                }
                            }

                            if (largeTriangles.length > 0) {
                                console.warn('[SDL] Found', largeTriangles.length, 'large overlay triangle(s):');
                                largeTriangles.slice(0, 3).forEach(t => {
                                    console.warn(`  Triangle ${t.tri}: area=${t.area}, color=${t.color}`);
                                    console.warn(`    v1=${t.v1}, v2=${t.v2}, v3=${t.v3}`);
                                });
                            } else {
                                console.log('[SDL] Overlay: no suspicious large triangles found');
                            }
                        } else {
                            console.log('[SDL] UnmapGPUTransferBuffer, size:', transferBuf.size);
                        }
                    }

                    // Free the allocated memory using buddy_free
                    if (wasmBuddyFree && transferBuf.mappedPtr) {
                        wasmBuddyFree(transferBuf.mappedPtr);
                        transferBuf.mappedPtr = null;
                    }
                },

                begin_gpu_copy_pass(cmdbufHandle) {
                    const cmdbuf = commandBuffers.get(cmdbufHandle);
                    if (!cmdbuf) {
                        console.error('[SDL] BeginGPUCopyPass: invalid cmdbuf', cmdbufHandle);
                        return 0;
                    }

                    // WebGPU doesn't have explicit copy passes like SDL GPU
                    // We'll handle copies directly in upload_to_gpu_buffer
                    const handle = nextHandle++;
                    cmdbuf.copyPassHandle = handle;
                    console.log('[SDL] BeginGPUCopyPass, handle:', handle, 'for cmdbuf:', cmdbufHandle);
                    return handle;
                },

                upload_to_gpu_buffer(copyPassHandle, source_ptr, dest_ptr, cycle) {
                    if (!memory) return;
                    const dv = new DataView(memory.buffer);

                    // Read source (transfer buffer location)
                    const transferBufferHandle = dv.getUint32(source_ptr, true);
                    const sourceOffset = dv.getUint32(source_ptr + 4, true);

                    // Read destination (buffer region)
                    const destBufferHandle = dv.getUint32(dest_ptr, true);
                    const destOffset = dv.getUint32(dest_ptr + 4, true);
                    const size = dv.getUint32(dest_ptr + 8, true);

                    const destBuffer = buffers.get(destBufferHandle);
                    const bufferSize = destBuffer ? destBuffer.size : 0;
                    const vertexSize = (destBufferHandle === 16) ? 24 : 36; // overlay=24, scene=36
                    const vertexCount = Math.floor(size / vertexSize);
                    console.log('[SDL] UploadToGPUBuffer, destBuf:', destBufferHandle, 'uploadSize:', size, 'bytes (', vertexCount, 'vertices)');

                    const transferBuf = transferBuffers.get(transferBufferHandle);

                    if (transferBuf && destBuffer && transferBuf.mappedData) {
                        let srcArray = transferBuf.mappedData;

                        // WORKAROUND: Filter out malformed overlay triangles (buffer 16)
                        if (destBufferHandle === 16) {
                            const floats = new Float32Array(srcArray.buffer, srcArray.byteOffset, size / 4);
                            let fixed = false;

                            // Check each triangle for excessive size
                            for (let i = 0; i < vertexCount - 2; i += 3) {
                                const x1 = floats[i * 6 + 0], y1 = floats[i * 6 + 1];
                                const x2 = floats[(i+1) * 6 + 0], y2 = floats[(i+1) * 6 + 1];
                                const x3 = floats[(i+2) * 6 + 0], y3 = floats[(i+2) * 6 + 1];

                                const area = Math.abs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1)) / 2;

                                // If triangle area > 0.5, collapse it to a degenerate triangle at origin
                                if (area > 0.5) {
                                    console.warn(`[SDL] Fixing malformed triangle ${i/3} with area ${area.toFixed(3)}`);
                                    for (let v = 0; v < 3; v++) {
                                        floats[(i + v) * 6 + 0] = 0; // x = 0
                                        floats[(i + v) * 6 + 1] = 0; // y = 0
                                    }
                                    fixed = true;
                                }
                            }

                            if (fixed) {
                                srcArray = new Uint8Array(floats.buffer, floats.byteOffset, size);
                            }
                        }

                        // Write mapped data directly to GPU buffer
                        device.queue.writeBuffer(destBuffer, destOffset, srcArray, sourceOffset, size);

                        // Track which buffers have been uploaded
                        if (!destBuffer.uploadedSize) {
                            destBuffer.uploadedSize = 0;
                        }
                        destBuffer.uploadedSize = Math.max(destBuffer.uploadedSize, destOffset + size);
                        destBuffer.uploadedVertexCount = vertexCount;
                    } else {
                        console.error('[SDL] UploadToGPUBuffer FAILED:', {
                            transferBuf: !!transferBuf,
                            destBuffer: !!destBuffer,
                            mappedData: transferBuf ? !!transferBuf.mappedData : false
                        });
                    }
                },

                upload_to_gpu_texture(copyPassHandle, source_ptr, dest_ptr, cycle) {
                    if (!memory) return;
                    const dv = new DataView(memory.buffer);

                    const transferBufferHandle = dv.getUint32(source_ptr, true);
                    const sourceOffset = dv.getUint32(source_ptr + 4, true);
                    const pixelsPerRow = dv.getUint32(source_ptr + 8, true);
                    const rowsPerLayer = dv.getUint32(source_ptr + 12, true);

                    const textureHandle = dv.getUint32(dest_ptr, true);
                    const mipLevel = dv.getUint32(dest_ptr + 4, true);
                    const layer = dv.getUint32(dest_ptr + 8, true);
                    const originX = dv.getUint32(dest_ptr + 12, true);
                    const originY = dv.getUint32(dest_ptr + 16, true);
                    const originZ = dv.getUint32(dest_ptr + 20, true);
                    const width = dv.getUint32(dest_ptr + 24, true);
                    const height = dv.getUint32(dest_ptr + 28, true);
                    const depth = dv.getUint32(dest_ptr + 32, true);

                    const transferBuf = transferBuffers.get(transferBufferHandle);
                    const texture = textures.get(textureHandle);

                    if (!transferBuf || !texture) {
                        console.error('[SDL] UploadToGPUTexture: invalid handles', transferBufferHandle, textureHandle);
                        return;
                    }
                    if (!transferBuf.mappedData) {
                        console.error('[SDL] UploadToGPUTexture: transfer buffer not populated');
                        return;
                    }

                    const bytesPerPixel = 4;
                    const bytesPerRow = pixelsPerRow * bytesPerPixel;

                    device.queue.writeTexture(
                        {
                            texture,
                            mipLevel,
                            origin: { x: originX, y: originY, z: originZ }
                        },
                        transferBuf.mappedData,
                        {
                            offset: sourceOffset,
                            bytesPerRow,
                            rowsPerImage: rowsPerLayer
                        },
                        {
                            width,
                            height,
                            depthOrArrayLayers: Math.max(depth, 1)
                        }
                    );
                },

                end_gpu_copy_pass(copyPassHandle) {
                    console.log('[SDL] EndGPUCopyPass, handle:', copyPassHandle);
                    // Nothing to do for WebGPU
                },

                bind_gpu_vertex_buffers(passHandle, first_slot, bindings_ptr, num_bindings) {
                    if (!memory) return;
                    const dv = new DataView(memory.buffer);
                    const passEntry = renderPasses.get(passHandle);
                    if (!passEntry) return;

                    for (let i = 0; i < num_bindings; i++) {
                        const bindingOffset = bindings_ptr + i * 8;
                        const bufferHandle = dv.getUint32(bindingOffset, true);
                        const offset = dv.getUint32(bindingOffset + 4, true);

                        const buffer = buffers.get(bufferHandle);
                        if (buffer) {
                            passEntry.pass.setVertexBuffer(first_slot + i, buffer, offset);
                            const bufferSize = buffer.size;
                            console.log('[SDL] BindGPUVertexBuffer, slot:', first_slot + i, 'handle:', bufferHandle, 'size:', bufferSize);
                        } else {
                            console.error('[SDL] BindGPUVertexBuffer: buffer not found, handle:', bufferHandle);
                        }
                    }
                },

                bind_gpu_index_buffer(passHandle, binding_ptr, index_element_size) {
                    if (!memory) return;
                    const dv = new DataView(memory.buffer);
                    const passEntry = renderPasses.get(passHandle);
                    if (!passEntry) return;

                    const bufferHandle = dv.getUint32(binding_ptr, true);
                    const offset = dv.getUint32(binding_ptr + 4, true);

                    const buffer = buffers.get(bufferHandle);
                    if (buffer) {
                        const format = index_element_size === 1 ? 'uint32' : 'uint16';
                        passEntry.pass.setIndexBuffer(buffer, format, offset);
                        console.log('[SDL] BindGPUIndexBuffer, format:', format, 'handle:', bufferHandle, 'buffer:', buffer);
                    } else {
                        console.error('[SDL] BindGPUIndexBuffer: buffer not found, handle:', bufferHandle);
                    }
                },

                draw_gpu_indexed_primitives(passHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance) {
                    console.log('[SDL] DrawGPUIndexedPrimitives, indices:', indexCount);
                    const passEntry = renderPasses.get(passHandle);
                    if (!passEntry) return;

                    const cmdbuf = commandBuffers.get(passEntry.cmdbufHandle);
                    if (cmdbuf) {
                        ensureBindGroups(cmdbuf);

                        const pipelineInfo = pipelines.get(cmdbuf.currentPipeline);
                        if (pipelineInfo) {
                            bindPipelineBindGroups(passEntry, cmdbuf, pipelineInfo);
                        }
                    }

                    passEntry.pass.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
                },

                create_gpu_texture(deviceHandle, info_ptr) {
                    if (!memory) return 0;
                    const dv = new DataView(memory.buffer);
                    const usage = dv.getUint32(info_ptr, true);
                    const format = dv.getUint32(info_ptr + 4, true);
                    const width = dv.getUint32(info_ptr + 8, true);
                    const height = dv.getUint32(info_ptr + 12, true);
                    const layers = dv.getUint32(info_ptr + 16, true) || 1;

                    let usageFlags = 0;
                    if (usage & 1) { // SDL_GPU_TEXTUREUSAGE_SAMPLER
                        usageFlags |= GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST;
                    }
                    if (usage & 2) { // SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                        usageFlags |= GPUTextureUsage.RENDER_ATTACHMENT;
                    }
                    if (usage & 4) { // SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
                        usageFlags |= GPUTextureUsage.RENDER_ATTACHMENT;
                    }
                    if (usageFlags === 0) {
                        usageFlags = GPUTextureUsage.TEXTURE_BINDING;
                    }

                    const formatMap = {
                        0: 'rgba8unorm',
                        1: 'depth16unorm',
                    };

                    const texture = device.createTexture({
                        size: { width, height, depthOrArrayLayers: layers },
                        format: formatMap[format] || 'rgba8unorm',
                        usage: usageFlags
                    });

                    const handle = nextHandle++;
                    textures.set(handle, texture);
                    if (usage & 1) {
                        textureViews.set(handle, texture.createView());
                    }
                    console.log('[SDL] CreateGPUTexture, handle:', handle, 'size:', width, 'x', height, 'usage:', usage);
                    return handle;
                },

                release_gpu_texture(deviceHandle, textureHandle) {
                    console.log('[SDL] ReleaseGPUTexture:', textureHandle);
                    const texture = textures.get(textureHandle);
                    if (texture) {
                        texture.destroy();
                        textures.delete(textureHandle);
                        textureViews.delete(textureHandle);
                    }
                },

                create_gpu_sampler(deviceHandle, info_ptr) {
                    if (!memory) return 0;
                    const dv = new DataView(memory.buffer);
                    const minFilter = dv.getUint32(info_ptr, true);
                    const magFilter = dv.getUint32(info_ptr + 4, true);
                    const mipmapMode = dv.getUint32(info_ptr + 8, true);
                    const addressU = dv.getUint32(info_ptr + 12, true);
                    const addressV = dv.getUint32(info_ptr + 16, true);
                    const addressW = dv.getUint32(info_ptr + 20, true);

                    const sampler = device.createSampler({
                        minFilter: mapFilter(minFilter),
                        magFilter: mapFilter(magFilter),
                        mipmapFilter: mapMipmapMode(mipmapMode),
                        addressModeU: mapAddressMode(addressU),
                        addressModeV: mapAddressMode(addressV),
                        addressModeW: mapAddressMode(addressW),
                    });

                    const handle = nextHandle++;
                    samplers.set(handle, sampler);
                    console.log('[SDL] CreateGPUSampler, handle:', handle);
                    return handle;
                },

                release_gpu_sampler(deviceHandle, samplerHandle) {
                    console.log('[SDL] ReleaseGPUSampler:', samplerHandle);
                    samplers.delete(samplerHandle);
                },

                bind_gpu_vertex_samplers(passHandle, firstSlot, bindings_ptr, numBindings) {
                    bindSamplersForPass(passHandle, firstSlot, bindings_ptr, numBindings);
                },

                bind_gpu_fragment_samplers(passHandle, firstSlot, bindings_ptr, numBindings) {
                    bindSamplersForPass(passHandle, firstSlot, bindings_ptr, numBindings);
                },

                set_window_relative_mouse_mode(windowHandle, enabled) {
                    console.log('[SDL] SetWindowRelativeMouseMode:', enabled);
                    if (enabled) {
                        tryRequestPointerLock();
                    } else {
                        pendingPointerLockRequest = false;
                        if (document.pointerLockElement === canvas) {
                            document.exitPointerLock();
                        }
                    }
                    return 1;
                },

                draw_gpu_primitives(passHandle, vertexCount, instanceCount, firstVertex, firstInstance) {
                    const passEntry = renderPasses.get(passHandle);
                    if (!passEntry) {
                        console.error('[SDL] DrawGPUPrimitives: invalid pass handle', passHandle);
                        return;
                    }

                    // Skip drawing if vertex count is 0
                    if (vertexCount === 0) {
                        console.log('[SDL] DrawGPUPrimitives: skipping draw with 0 vertices');
                        return;
                    }

                    // Check if we're drawing overlay (buffer 16)
                    const cmdbuf = commandBuffers.get(passEntry.cmdbufHandle);
                    const overlayBuffer = buffers.get(16);
                    const uploadedCount = overlayBuffer ? overlayBuffer.uploadedVertexCount : 0;
                    console.log('[SDL] DrawGPUPrimitives, verts:', vertexCount, '(uploaded:', uploadedCount, ')');

                    if (cmdbuf) {
                        ensureBindGroups(cmdbuf);

                        const pipelineInfo = pipelines.get(cmdbuf.currentPipeline);
                        if (pipelineInfo) {
                            bindPipelineBindGroups(passEntry, cmdbuf, pipelineInfo);
                        }
                    }

                    passEntry.pass.draw(vertexCount, instanceCount, firstVertex, firstInstance);
                },

                submit_gpu_command_buffer(cmdbufHandle) {
                    console.log('[SDL] SubmitGPUCommandBuffer, handle:', cmdbufHandle);
                    if (fatalError) {
                        console.warn('[SDL] Skipping command buffer submit after fatal error');
                        return;
                    }
                    const cmdbuf = commandBuffers.get(cmdbufHandle);
                    if (cmdbuf) {
                        const commandBuffer = cmdbuf.encoder.finish();
                        device.queue.submit([commandBuffer]);
                        if (cmdbuf.swapchainTextures) {
                            for (const handle of cmdbuf.swapchainTextures) {
                                textures.delete(handle);
                            }
                        }
                        commandBuffers.delete(cmdbufHandle);
                        console.log('[SDL] Command buffer submitted');
                    } else {
                        console.error('[SDL] submit_gpu_command_buffer: invalid handle', cmdbufHandle);
                    }
                },

                poll_event(event_ptr) {
                    if (fatalError && !fatalQuitDelivered && eventQueue.length === 0) {
                        eventQueue.push({ type: 'quit' });
                    }
                    if (eventQueue.length === 0) return 0;

                    const event = eventQueue.shift();
                    if (fatalError && event?.type === 'quit') {
                        fatalQuitDelivered = true;
                    }

                    if (memory && event_ptr) {
                        const dv = new DataView(memory.buffer);

                        if (event.type === 'quit') {
                            dv.setUint32(event_ptr, 0x100, true); // SDL_EVENT_QUIT
                        } else if (event.type === 'keydown') {
                            dv.setUint32(event_ptr, 0x300, true); // SDL_EVENT_KEY_DOWN
                            dv.setUint32(event_ptr + 4, event.key, true); // keycode
                        } else if (event.type === 'keyup') {
                            dv.setUint32(event_ptr, 0x301, true); // SDL_EVENT_KEY_UP
                            dv.setUint32(event_ptr + 4, event.key, true); // keycode
                        } else if (event.type === 'mousemotion') {
                            dv.setUint32(event_ptr, 0x400, true); // SDL_EVENT_MOUSE_MOTION
                            dv.setFloat32(event_ptr + 4, event.xrel, true); // xrel
                            dv.setFloat32(event_ptr + 8, event.yrel, true); // yrel
                        }
                    }

                    return 1;
                },

                get_mouse_state(x_ptr, y_ptr) {
                    if (memory) {
                        const dv = new DataView(memory.buffer);
                        if (x_ptr) dv.setFloat32(x_ptr, mouseX, true);
                        if (y_ptr) dv.setFloat32(y_ptr, mouseY, true);
                    }
                    return 0; // button state
                },

                get_error(buffer_ptr, buffer_size) {
                    if (memory && buffer_ptr && buffer_size > 0) {
                        writeString(buffer_ptr, buffer_size, lastError);
                    }
                    return buffer_ptr;
                },

                log(msg_ptr, msg_len) {
                    if (memory) {
                        const msg = readString(msg_ptr, msg_len);
                        console.log('[SDL Log]', msg);
                    }
                },

                get_asset_image_info(path_ptr, path_len, width_ptr, height_ptr) {
                    if (!memory) {
                        setError('Memory not set before image info request');
                        return 0;
                    }
                    const path = readString(path_ptr, path_len);
                    const asset = getAssetImage(path);
                    if (!asset) {
                        return 0;
                    }
                    const dv = new DataView(memory.buffer);
                    if (width_ptr) dv.setUint32(width_ptr, asset.width, true);
                    if (height_ptr) dv.setUint32(height_ptr, asset.height, true);
                    return 1;
                },

                copy_asset_image_rgba(path_ptr, path_len, dest_ptr, dest_len) {
                    if (!memory) {
                        setError('Memory not set before image copy request');
                        return 0;
                    }
                    const path = readString(path_ptr, path_len);
                    const asset = getAssetImage(path);
                    if (!asset) {
                        return 0;
                    }
                    const required = asset.width * asset.height * 4;
                    if (dest_len < required) {
                        setError(`Destination buffer too small for ${path}: need ${required}, have ${dest_len}`);
                        return 0;
                    }
                    const dest = new Uint8Array(memory.buffer, dest_ptr, required);
                    dest.set(asset.pixels);
                    return 1;
                },

                get_ticks() {
                    return performance.now();
                },
            };

            return {
                imports,
                setMemory(mem) {
                    memory = mem;
                },
                setBuddyAllocFunctions(allocFn, freeFn) {
                    wasmBuddyAlloc = allocFn;
                    wasmBuddyFree = freeFn;
                    console.log('[SDL] Buddy allocator functions registered');
                },
                async preloadAssetsFromBundle(bundleMap) {
                    await preloadImageAssetsFromBundle(bundleMap);
                },
            };
        }

export function createVirtualFileSystem(preloadedFiles) {
    const fileMap = new Map(preloadedFiles);
    let memory = null;
    const openFiles = new Map();
    let nextFd = 4;

    function getDataView() {
        if (!memory) {
            throw new Error('WASI memory not set');
        }
        return new DataView(memory.buffer);
    }

    function readPath(pathPtr, pathLen) {
        const bytes = new Uint8Array(memory.buffer, pathPtr, pathLen);
        let path = fsDecoder.decode(bytes);
        const nul = path.indexOf('\0');
        if (nul !== -1) {
            path = path.slice(0, nul);
        }
        return path;
    }

    function path_open(dirfd, dirflags, pathPtr, pathLen, oflags, rightsBase, rightsInheriting, fdflags, fdOutPtr) {
        try {
            const path = readPath(pathPtr, pathLen);
            const data = fileMap.get(path);
            if (!data) {
                return 44; // __WASI_ERRNO_NOENT
            }
            const fd = nextFd++;
            openFiles.set(fd, { data, offset: 0 });
            const dv = getDataView();
            dv.setUint32(fdOutPtr, fd, true);
            return 0;
        } catch (err) {
            console.error('[WASI] path_open failed', err);
            return 8; // __WASI_ERRNO_BADF
        }
    }

    function fd_read(fd, iovsPtr, iovsLen, nreadPtr) {
        const file = openFiles.get(fd);
        if (!file) {
            return 8;
        }
        const dv = getDataView();
        let total = 0;
        for (let i = 0; i < iovsLen; i++) {
            const ptr = dv.getUint32(iovsPtr + i * 8, true);
            const len = dv.getUint32(iovsPtr + i * 8 + 4, true);
            if (len === 0) {
                continue;
            }
            const remaining = file.data.length - file.offset;
            if (remaining <= 0) {
                break;
            }
            const toCopy = Math.min(len, remaining);
            const dest = new Uint8Array(memory.buffer, ptr, toCopy);
            dest.set(file.data.subarray(file.offset, file.offset + toCopy));
            file.offset += toCopy;
            total += toCopy;
            if (toCopy < len) {
                break;
            }
        }
        dv.setUint32(nreadPtr, total, true);
        return 0;
    }

    function fd_seek(fd, offset, whence, newOffsetPtr) {
        const file = openFiles.get(fd);
        if (!file) {
            return 8;
        }
        const dv = getDataView();
        const amount = Number(offset);
        if (whence === 0) { // SET
            file.offset = amount;
        } else if (whence === 1) { // CUR
            file.offset += amount;
        } else if (whence === 2) { // END
            file.offset = file.data.length + amount;
        } else {
            return 28; // __WASI_ERRNO_INVAL
        }
        if (file.offset < 0) {
            file.offset = 0;
        } else if (file.offset > file.data.length) {
            file.offset = file.data.length;
        }
        if (typeof dv.setBigUint64 === 'function') {
            dv.setBigUint64(newOffsetPtr, BigInt(file.offset), true);
        } else {
            dv.setUint32(newOffsetPtr, file.offset, true);
            dv.setUint32(newOffsetPtr + 4, 0, true);
        }
        return 0;
    }

    function fd_close(fd) {
        if (!openFiles.has(fd)) {
            return 8;
        }
        openFiles.delete(fd);
        return 0;
    }

    function fd_tell(fd, offsetPtr) {
        const file = openFiles.get(fd);
        if (!file) {
            return 8;
        }
        const dv = getDataView();
        if (typeof dv.setBigUint64 === 'function') {
            dv.setBigUint64(offsetPtr, BigInt(file.offset), true);
        } else {
            dv.setUint32(offsetPtr, file.offset, true);
            dv.setUint32(offsetPtr + 4, 0, true);
        }
        return 0;
    }

    function args_sizes_get(argcPtr, argvBufSizePtr) {
        const dv = getDataView();
        dv.setUint32(argcPtr, 0, true);
        dv.setUint32(argvBufSizePtr, 0, true);
        return 0;
    }

    function args_get(argvPtr, argvBufPtr) {
        return 0;
    }

    return {
        setMemory(mem) { memory = mem; },
        path_open,
        fd_read,
        fd_seek,
        fd_close,
        fd_tell,
        args_sizes_get,
        args_get,
    };
}
