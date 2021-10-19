function createShader(gl, type, source) {
    var shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    var success = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
    if (success) {
        return shader;
    }

    console.log(gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
}
function createProgramFromSources(gl, vertexShader, fragmentShader) {
    var program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    var success = gl.getProgramParameter(program, gl.LINK_STATUS);
    if (success) {
        return program;
    }
    console.log(gl.getProgramInfoLog(program));
    gl.deleteProgram(program);
}

function initVertexBuffers(gl, program) {
    var verticesTexCoords = new Float32Array([
        -1.0, 1.0, 0.0, 1.0,
        -1.0, -1.0, 0.0, 0.0,
        1.0, 1.0, 1.0, 1.0,
        1.0, -1.0, 1.0, 0.0,
    ]);
    var n = 4;

    gl.useProgram(program);

    var vertexTexCoordBuffer = gl.createBuffer();
    if (!vertexTexCoordBuffer) {
        console.log('Failed to create the buffer object');
        return -1;
    }

    gl.bindBuffer(gl.ARRAY_BUFFER, vertexTexCoordBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, verticesTexCoords, gl.STATIC_DRAW);

    var FSIZE = verticesTexCoords.BYTES_PER_ELEMENT;
    var a_Position = gl.getAttribLocation(program, 'a_Position');
    if (a_Position < 0) {
        console.log('Failed to get the storage location of a_Position');
        return -1;
    }
    gl.vertexAttribPointer(a_Position, 2, gl.FLOAT, false, FSIZE * 4, 0);
    gl.enableVertexAttribArray(a_Position);

    var a_TexCoord = gl.getAttribLocation(program, 'a_TexCoord');
    if (a_TexCoord < 0) {
        console.log('Failed to get the storage location of a_TexCoord');
        return -1;
    }
    gl.vertexAttribPointer(a_TexCoord, 2, gl.FLOAT, false, FSIZE * 4, FSIZE * 2);
    gl.enableVertexAttribArray(a_TexCoord);
    return n;
}

sync = true
rgba_f = false
console.log('@sync: ', sync);

function getNV12Array(buffer, size) {
    return new Uint8Array(Module.HEAPU8.buffer, buffer, size);
}
function getRGBAArray(buffer, size) {
    return new Uint8Array(Module.HEAPU8.buffer, buffer, size);
}
function getRGBAfArray(buffer, size) {
    return new Float32Array(Module.HEAPF32.buffer, buffer, size);
}

function createInputTexture(gl, buffer, width, height) {
    var rgbaArray = null;
    if (rgba_f)
        rgbaArray = getRGBAfArray(buffer, width * height * 4);
    else
        rgbaArray = getRGBAArray(buffer, width * height * 4);

    rgbTexture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, rgbTexture);
    {
        if (rgba_f)
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, width, height, 0, gl.RGBA, gl.FLOAT, rgbaArray);
        else
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, rgbaArray);

        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    }
}

function destroyInputTexture(gl) {
    gl.bindTexture(gl.TEXTURE_2D, null);
    gl.deleteTexture(rgbTexture);
}

function createOutTexture(gl, sampler, width, height) {
    rgbRendertexture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, rgbRendertexture);
    {
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    }

    framebuffer = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, rgbRendertexture, 0);
}

function clientWaitAsync(gl, sync, flags, interval_ms) {
    return new Promise((resolve, reject) => {
        function test() {
            const res = gl.clientWaitSync(sync, flags, 0);
            if (res == gl.WAIT_FAILED) {
                reject();
                return;
            }
            if (res == gl.TIMEOUT_EXPIRED) {
                setTimeout(test, interval_ms);
                return;
            }
            resolve();
        }
        test();
    });
}

async function getBufferSubDataAsync(
    gl, target, buffer, srcByteOffset, dstBuffer, dstOffset, length) {
    const sync = gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0);
    gl.flush();

    await clientWaitAsync(gl, sync, 0, 3);
    gl.deleteSync(sync);

    gl.bindBuffer(target, buffer);
    gl.getBufferSubData(target, srcByteOffset, dstBuffer, dstOffset, length);
    gl.bindBuffer(target, null);

    return;
}

async function readPixelsAsync(gl, outptr, width, height, cont) {
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, buf);
    gl.bufferData(gl.PIXEL_PACK_BUFFER, width * height * 4, gl.STREAM_READ);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, 0);
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
    var outRgbArray = getRGBAArray(outptr, width * height * 4);
    await getBufferSubDataAsync(gl, gl.PIXEL_PACK_BUFFER, buf, 0, outRgbArray);

    cont();

    gl.deleteBuffer(buf);
    return;
}

nv12Buffer = 0
rgbBuffer = 0
rgbRenderBuffer = 0
inputNV12 = 0
width = 1280;
height = 720;
rgbTexture = 0
rgbRenderTexture = 0
appinitialized = false;

$(document).on("click", "[id^=start]", function () {
    if (nv12Buffer == 0)
        nv12Buffer = Module._createNV12Buffer(width, height);
    console.log(nv12Buffer)
    if (rgbBuffer == 0)
        if (rgba_f)
            rgbBuffer = Module._createRGBAfBuffer(width, height);
        else
            rgbBuffer = Module._createRGBABuffer(width, height);
    if (rgbRenderBuffer == 0)
        rgbRenderBuffer = Module._createRGBABuffer(width, height);
    console.log(rgbBuffer)
    if (inputNV12 == 0)
        inputNV12 = new Uint8Array(height * width * 3 / 2);
    console.log(inputNV12.length)
    Module.HEAPU8.set(inputNV12, nv12Buffer);
    if (rgba_f)
        micros = Module._speedtestf(nv12Buffer, rgbBuffer, width, height, 1000);
    else
        micros = Module._speedtest(nv12Buffer, rgbBuffer, width, height, 1000);
    console.log(micros)
});

$(document).on("click", "[id^=revstart]", function () {
    if (nv12Buffer == 0)
        nv12Buffer = Module._createNV12Buffer(width, height);
    console.log(nv12Buffer)
    if (rgbBuffer == 0)
        if (rgba_f)
            rgbBuffer = Module._createRGBAfBuffer(width, height);
        else
            rgbBuffer = Module._createRGBABuffer(width, height);
    if (rgbRenderBuffer == 0)
        rgbRenderBuffer = Module._createRGBABuffer(width, height);
    console.log(rgbBuffer)
    if (inputNV12 == 0)
        inputNV12 = new Uint8Array(height * width * 3 / 2);
    console.log(inputNV12.length)
    micros = Module._speedtest_YUV32toNV12(rgbRenderBuffer, nv12Buffer, width, height, 1000);
    console.log(micros)
});

const initRender = function () {
    if (appinitialized)
        return;
    const glcanvas = document.querySelector("#glcanvas");
    const gl = glcanvas.getContext("webgl2");

    if (!gl) {
        return;
    }

    const VSHADER_SOURCE = `
        attribute vec4 a_Position;
        attribute vec2 a_TexCoord;
        varying vec2 v_TexCoord;
        void main() {
          gl_Position = a_Position;
          v_TexCoord = 1.0 - a_TexCoord;
        }`

    const FSHADER_SOURCE = `
        precision mediump float;
        uniform sampler2D u_Sampler;
        varying vec2 v_TexCoord;

        vec4 Rgba2YUV(vec4 rgb) {
            float y = 0.257 * rgb.r + 0.504 * rgb.g + 0.098 * rgb.b;
            float u = 0.4954 * rgb.b - 0.57587 * y + 0.5;
            float v = 0.6266 * rgb.r - 0.73014 * y + 0.5;
            return vec4(y + 0.0625, u, v, 1.0);
        }
        void main() {
            vec4 color = texture2D(u_Sampler, v_TexCoord);
            gl_FragColor = Rgba2YUV(color);
        }`
    var vertexShader = createShader(gl, gl.VERTEX_SHADER, VSHADER_SOURCE);
    var fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, FSHADER_SOURCE);
    program = createProgramFromSources(gl, vertexShader, fragmentShader)
    if (!program) {
        console.log('Failed to intialize shaders.');
        return;
    }
    gl.getExtension('OES_texture_float');
    gl.getExtension('WEBGL_draw_buffers');
    var n = initVertexBuffers(gl, program);
    if (n < 0) {
        console.log('Failed to set the vertex information');
        return;
    }

    u_Sampler = gl.getUniformLocation(program, 'u_Sampler');
    if (!u_Sampler) {
        console.log('Failed to get the storage location of u_Sampler');
        return false;
    }

    performance.mark('readpixels');

    if (nv12Buffer == 0)
        nv12Buffer = Module._createNV12Buffer(width, height);
    console.log(nv12Buffer)
    if (rgbBuffer == 0)
        if (rgba_f)
            rgbBuffer = Module._createRGBAfBuffer(width, height);
        else
            rgbBuffer = Module._createRGBABuffer(width, height);
    if (rgbRenderBuffer == 0)
        rgbRenderBuffer = Module._createRGBABuffer(width, height);
    console.log(rgbBuffer)
    if (inputNV12 == 0)
        inputNV12 = new Uint8Array(height * width * 3 / 2);

    console.log('prepare input/output')
    createOutTexture(gl, u_Sampler, width, height);

    console.log('warmup')
    Module.HEAPU8.set(inputNV12, nv12Buffer);
    if (rgba_f)
        micros = Module._speedtestf(nv12Buffer, rgbBuffer, width, height, 10);
    else
        micros = Module._speedtest(nv12Buffer, rgbBuffer, width, height, 10);
    performance.mark('readpixels');

    appinitialized = true;
}

const refresh = function (nv12Array, notifyProcessed, notifyError) {
    const glcanvas = document.querySelector("#glcanvas");
    const gl = glcanvas.getContext("webgl2");

    if (!gl) {
        return;
    }

    performance.measure('start', 'readpixels');
    Module.HEAPU8.set(nv12Array, nv12Buffer);
    performance.measure('set nv12', 'readpixels');
    if (rgba_f)
        micros = Module._speedtestf(nv12Buffer, rgbBuffer, width, height, 1);
    else
        micros = Module._speedtest(nv12Buffer, rgbBuffer, width, height, 1);

    performance.measure('convert nv12', 'readpixels');

    createInputTexture(gl, rgbBuffer, width, height);

    performance.measure('create input/output', 'readpixels');

    gl.bindTexture(gl.TEXTURE_2D, rgbTexture);
    gl.uniform1i(u_Sampler, 0);

    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

    performance.measure('draw', 'readpixels');

    if (sync) {
        getRender(gl, rgbRenderBuffer, width, height, function (outRgbArray) {
            performance.measure('copy pixels', 'readpixels');
            micros = Module._speedtest_reverse(rgbRenderBuffer, nv12Buffer, width, height, 1);
            performance.measure('convert back nv12', 'readpixels');
            destroyInputTexture(gl);
            var nv12NewArray = getNV12Array(nv12Buffer, width * height * 3 / 2);
            nv12Array.set(nv12NewArray);

            //send notification the effect processing is finshed.
            if (notifyProcessed)
                notifyProcessed();
        });
        performance.measure('copy pixels', 'readpixels');
        destroyInputTexture(gl);
    }
    else {
        readPixelsAsync(gl, rgbRenderBuffer, width, height, function () {
            Module._YUV32toNV12(rgbRenderBuffer, rgbBuffer, width, height);
            var nv12 = getNV12Array(rgbBuffer, width * height * 3 / 2);
            nv12Array.set(nv12);

            //send notification the effect processing is finshed.
            if (notifyProcessed)
                notifyProcessed();

            performance.measure('convert back nv12', 'readpixels');
            destroyInputTexture(gl);
        });
        performance.measure('async read', 'readpixels');
    }
};

$(document).on("click", "[id=render]", function () {
    const glcanvas = document.querySelector("#glcanvas");
    const gl = glcanvas.getContext("webgl2");

    if (!gl) {
        return;
    }

    const VSHADER_SOURCE = `
        attribute vec4 a_Position;
        attribute vec2 a_TexCoord;
        varying vec2 v_TexCoord;
        void main() {
          gl_Position = a_Position;
          v_TexCoord = 1.0 - a_TexCoord;
        }`

    const FSHADER_SOURCE = `
        precision mediump float;
        uniform sampler2D u_Sampler;
        varying vec2 v_TexCoord;

        vec4 Rgba2YUV(vec4 rgb) {
            float y = 0.257 * rgb.r + 0.504 * rgb.g + 0.098 * rgb.b;
            float u = 0.4954 * rgb.b - 0.57587 * y + 0.5;
            float v = 0.6266 * rgb.r - 0.73014 * y + 0.5;
            return clamp(vec4(y + 0.0625, u, v, rgb.a), 0.0, 1.0);
        }
        void main() {
            vec4 color = texture2D(u_Sampler, v_TexCoord);
            gl_FragColor = Rgba2YUV(color);
            // gl_FragColor = color;
        }`

    var vertexShader = createShader(gl, gl.VERTEX_SHADER, VSHADER_SOURCE);
    var fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, FSHADER_SOURCE);
    var program = createProgramFromSources(gl, vertexShader, fragmentShader)
    if (!program) {
        console.log('Failed to intialize shaders.');
        return;
    }
    gl.getExtension('OES_texture_float');
    gl.getExtension('WEBGL_draw_buffers');
    var n = initVertexBuffers(gl, program);
    if (n < 0) {
        console.log('Failed to set the vertex information');
        return;
    }

    var u_Sampler = gl.getUniformLocation(program, 'u_Sampler');
    if (!u_Sampler) {
        console.log('Failed to get the storage location of u_Sampler');
        return false;
    }

    performance.mark('readpixels');

    if (nv12Buffer == 0)
        nv12Buffer = Module._createNV12Buffer(width, height);
    console.log(nv12Buffer)
    if (rgbBuffer == 0)
        rgbBuffer = Module._createRGBABuffer(width, height);
    if (rgbRenderBuffer == 0)
        rgbRenderBuffer = Module._createRGBABuffer(width, height);
    console.log(rgbBuffer)
    if (inputNV12 == 0)
        inputNV12 = new Uint8Array(height * width * 3 / 2);

    console.log('prepare input/output')
    createOutTexture(gl, u_Sampler, width, height);

    console.log('warmup')
    Module.HEAPU8.set(inputNV12, nv12Buffer);
    if (rgba_f)
        micros = Module._speedtestf(nv12Buffer, rgbBuffer, width, height, 10);
    else
        micros = Module._speedtest(nv12Buffer, rgbBuffer, width, height, 10);
    lasttimestamp = 0
    fps = 30
    fpsInterval = 1000.0 / fps
    performance.mark('readpixels');

    const drawScene = function () {
        timestamp = Date.now();
        elapsed = timestamp - lasttimestamp;
        if (elapsed > fpsInterval) {
            lasttimestamp = timestamp;
            performance.measure('start', 'readpixels');
            Module.HEAPU8.set(inputNV12, nv12Buffer);
            performance.measure('set nv12', 'readpixels');
            if (rgba_f)
                micros = Module._speedtestf(nv12Buffer, rgbBuffer, width, height, 1);
            else
                micros = Module._speedtest(nv12Buffer, rgbBuffer, width, height, 1);

            performance.measure('convert nv12', 'readpixels');

            createInputTexture(gl, rgbBuffer, width, height);

            performance.measure('create input/output', 'readpixels');

            // gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            gl.bindTexture(gl.TEXTURE_2D, rgbTexture);
            gl.uniform1i(u_Sampler, 0);
            gl.clearColor(.5, .7, 1, 1);

            gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

            performance.measure('draw', 'readpixels');

            if (sync) {
                getRender(gl, rgbRenderBuffer, width, height, function (outRgbArray) {
                    performance.measure('copy pixels', 'readpixels');
                    // micros = Module._speedtest_reverse(rgbRenderBuffer, nv12Buffer, width, height, 1);
                    performance.measure('convert back nv12', 'readpixels');
                    nv12Array = getNV12Array(nv12Buffer, width * height * 3 / 2);
                    inputNV12.set(nv12Array);
                    destroyInputTexture(gl);
                    requestAnimationFrame(drawScene);
                });
                performance.measure('copy pixels', 'readpixels');
                // console.log(performance.getEntriesByType("measure"));
                destroyInputTexture(gl);
                requestAnimationFrame(drawScene);
            }
            else {
                /*
                getRenderAsync(gl, rgbRenderBuffer, width, height, function (outRgbArray) {
                    performance.measure('copy pixels', 'readpixels');
                    // micros = Module._speedtest_reverse(rgbRenderBuffer, nv12Buffer, width, height, 1);
                    nv12Array = getNV12Array(nv12Buffer, width * height * 3 / 2);
                    inputNV12.set(nv12Array);
                    performance.measure('convert back nv12', 'readpixels');
                    destroyInputTexture(gl);
                    requestAnimationFrame(drawScene);
                });
                */
                readPixelsAsync(gl, rgbRenderBuffer, width, height, function () {
                    Module._YUV32toNV12(rgbRenderBuffer, nv12Buffer, width, height);
                    var nv12 = getNV12Array(nv12Buffer, width * height * 3 / 2);
                    inputNV12.set(nv12);
                    performance.measure('convert back nv12', 'readpixels');
                    destroyInputTexture(gl);
                });
                performance.measure('async read', 'readpixels');
                requestAnimationFrame(drawScene);
            }
        }
        else {
            requestAnimationFrame(drawScene);
        }
    }

    requestAnimationFrame(drawScene);
});
