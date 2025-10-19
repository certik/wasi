// Cubic Bezier curve shader for WebGPU Shader Toy
// Adapted from bezier.wgsl

fn sdSegment(p: vec2f, a: vec2f, b: vec2f) -> f32 {
    let pa = p - a;
    let ba = b - a;
    let h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

const eps: f32 = 0.0001;

fn isfinite(x: f32) -> bool {
    return (bitcast<u32>(x) & 0x7f800000u) != 0x7f800000u;
}

fn poly5(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, t: f32) -> f32 {
    return ((((a * t + b) * t + c) * t + d) * t + e) * t + f;
}

fn bisect5(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, t_in: vec2f, v: vec2f) -> f32 {
    var t = t_in;
    var x = (t.x + t.y) * 0.5;
    let s = select(-1.0, 1.0, v.x < v.y);

    for (var i = 0; i < 32; i++) {
        var y = a * x + b;
        var q = a * x + y;
        y = y * x + c;
        q = q * x + y;
        y = y * x + d;
        q = q * x + y;
        y = y * x + e;
        q = q * x + y;
        y = y * x + f;

        t = select(vec2f(t.x, x), vec2f(x, t.y), s * y < 0.0);
        var next = x - y / q;
        next = select((t.x + t.y) * 0.5, next, next >= t.x && next <= t.y);
        if (abs(next - x) < eps) {
            return next;
        }
        x = next;
    }
    return x;
}

fn root_find2(a: f32, b: f32, c: f32) -> array<f32, 6> {
    var result: array<f32, 6>;
    result[5] = 0.0;

    let disc = b * b - 4.0 * a * c;
    if (disc < 0.0) {
        return result;
    }
    if (disc == 0.0) {
        let s = -0.5 * b / a;
        if (isfinite(s)) {
            result[0] = s;
            result[5] = 1.0;
        }
        return result;
    }

    let h = sqrt(disc);
    let q = -0.5 * (b + select(-h, h, b > 0.0));
    var v = vec2f(q / a, c / q);
    if (v.x > v.y) {
        v = v.yx;
    }

    var count = 0;
    if (isfinite(v.x) && v.x >= 0.0 && v.x <= 1.0) {
        result[count] = v.x;
        count++;
    }
    if (isfinite(v.y) && v.y >= 0.0 && v.y <= 1.0) {
        result[count] = v.y;
        count++;
    }
    result[5] = f32(count);
    return result;
}

fn cy_find5(r4: array<f32, 6>, a: f32, b: f32, c: f32, d: f32, e: f32, f: f32) -> array<f32, 6> {
    var result: array<f32, 6>;
    var count = 0;
    let n = i32(r4[5]);

    var px = 0.0;
    var py = poly5(a, b, c, d, e, f, 0.0);

    for (var i = 0; i <= n; i++) {
        let x = select(r4[i], 1.0, i == n);
        let y = poly5(a, b, c, d, e, f, x);

        if (py * y <= 0.0 && !(py * y == 0.0)) {
            let v = bisect5(a, b, c, d, e, f, vec2f(px, x), vec2f(py, y));
            result[count] = v;
            count++;
        }
        px = x;
        py = y;
    }

    result[5] = f32(count);
    return result;
}

fn root_find5(da: f32, db: f32, dc: f32, dd: f32, de: f32, df: f32) -> array<f32, 6> {
    let r2 = root_find2(10.0 * da, 4.0 * db, dc);
    let r3 = cy_find5(r2, 0.0, 0.0, 10.0 * da, 6.0 * db, 3.0 * dc, dd);
    let r4 = cy_find5(r3, 0.0, 5.0 * da, 4.0 * db, 3.0 * dc, dd + dd, de);
    let r = cy_find5(r4, da, db, dc, dd, de, df);
    return r;
}

fn dot2(v: vec2f) -> f32 {
    return dot(v, v);
}

fn bezier(p: vec2f, p0: vec2f, p1: vec2f, p2: vec2f, p3: vec2f) -> f32 {
    let dp0 = p0 - p;
    let dp3 = p3 - p;
    var dist = min(dot2(dp0), dot2(dp3));

    let a = -p0 + 3.0 * (p1 - p2) + p3;
    let b = 3.0 * (p0 - 2.0 * p1 + p2);
    let c = 3.0 * (p1 - p0);
    let d = p0;

    let dmp = d - p;
    let da = 3.0 * dot(a, a);
    let db = 5.0 * dot(a, b);
    let dc = 4.0 * dot(a, c) + 2.0 * dot(b, b);
    let dd = 3.0 * (dot(a, dmp) + dot(b, c));
    let de = 2.0 * dot(b, dmp) + dot(c, c);
    let df = dot(c, dmp);

    let roots = root_find5(da, db, dc, dd, de, df);
    let count = i32(roots[5]);

    for (var i = 0; i < count; i++) {
        let t = roots[i];
        let dp = ((a * t + b) * t + c) * t + dmp;
        dist = min(dist, dot2(dp));
    }

    return sqrt(dist);
}

fn df_debug(d: f32) -> vec3f {
    var col = vec3f(0.0, 0.2, 0.5);
    col *= 0.7 + 0.3 * cos(120.0 * abs(d));
    return mix(col, vec3f(1.0), 1.0 - smoothstep(0.0, 0.02, abs(d)));
}

fn sat(x: f32) -> f32 {
    return clamp(x, 0.0, 1.0);
}

fn points_segments(c_in: vec3f, p: vec2f, p0: vec2f, p1: vec2f, p2: vec2f, p3: vec2f) -> vec3f {
    var c = c_in;

    let d0 = dot2(p - p0);
    let d1 = dot2(p - p1);
    let d2 = dot2(p - p2);
    let d3 = dot2(p - p3);
    let d = 0.02 - sqrt(min(min(min(d0, d1), d2), d3));
    c = mix(c, vec3f(1.0, 0.5, 0.0), sat(0.5 + d / fwidth(d)));

    let s0 = sdSegment(p, p0, p1);
    let s1 = sdSegment(p, p2, p3);
    let s = 0.005 - min(s0, s1);
    c = mix(c, vec3f(1.0, 0.5, 0.0), sat(0.5 + s / fwidth(s)) * 0.5);

    return c;
}

@fragment
fn fragmentMain(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let iTime = inputs.time;
    let fragCoord = vec2f(pos.x, inputs.size.y - pos.y);
    let iResolution = inputs.size.xy;

    var uv = (fragCoord * 2.0 - iResolution) / iResolution.y;
    let aspect = iResolution.x / iResolution.y;
    uv.x *= aspect;
    let scale = max(aspect, 1.0);
    uv /= scale;

    // Animated Bezier control points
    let t = iTime * 0.5;
    let p0 = vec2f(-1.0, -0.3 + 0.2 * sin(t));
    let p1 = vec2f(-0.3, 0.7 + 0.2 * cos(t * 1.3));
    let p2 = vec2f(0.3, 0.7 + 0.2 * sin(t * 1.7));
    let p3 = vec2f(1.0, -0.3 + 0.2 * cos(t * 0.9));

    let d = bezier(uv, p0, p1, p2, p3);
    var o = df_debug(d);
    o = points_segments(o, uv, p0, p1, p2, p3);

    return vec4f(pow(o, vec3f(1.0 / 2.2)), 1.0);
}
