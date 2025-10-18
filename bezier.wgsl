struct Uniforms {
    resolution: vec2<f32>,
    p0: vec2<f32>,
    p1: vec2<f32>,
    p2: vec2<f32>,
    p3: vec2<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

fn sdSegment(p: vec2<f32>, a: vec2<f32>, b: vec2<f32>) -> f32 {
    let pa = p - a;
    let ba = b - a;
    let h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

fn sdPoint(p: vec2<f32>, pt: vec2<f32>) -> f32 {
    return length(p - pt);
}

const eps: f32 = 0.0001;

// Return true if x is not a NaN nor an infinite
fn isfinite(x: f32) -> bool {
    return (bitcast<u32>(x) & 0x7f800000u) != 0x7f800000u;
}

fn poly5(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, t: f32) -> f32 {
    return ((((a * t + b) * t + c) * t + d) * t + e) * t + f;
}

// Newton bisection for 5th degree polynomial
fn bisect5(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32, t_in: vec2<f32>, v: vec2<f32>) -> f32 {
    var t = t_in;
    var x = (t.x + t.y) * 0.5;
    let s = select(-1.0, 1.0, v.x < v.y);

    for (var i = 0; i < 32; i++) {
        // Evaluate polynomial and derivative using Horner's method
        var y = a * x + b;
        var q = a * x + y;
        y = y * x + c;
        q = q * x + y;
        y = y * x + d;
        q = q * x + y;
        y = y * x + e;
        q = q * x + y;
        y = y * x + f;

        t = select(vec2(t.x, x), vec2(x, t.y), s * y < 0.0);
        var next = x - y / q;
        next = select((t.x + t.y) * 0.5, next, next >= t.x && next <= t.y);
        if (abs(next - x) < eps) {
            return next;
        }
        x = next;
    }
    return x;
}

// Quadratic root finder: solve ax²+bx+c=0 (clamped to [0,1])
fn root_find2(a: f32, b: f32, c: f32) -> array<f32, 6> {
    var result: array<f32, 6>;
    result[5] = 0.0; // count

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
    var v = vec2(q / a, c / q);
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

// Find roots using bisection on quintic
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
            let v = bisect5(a, b, c, d, e, f, vec2(px, x), vec2(py, y));
            result[count] = v;
            count++;
        }
        px = x;
        py = y;
    }

    result[5] = f32(count);
    return result;
}

// Hierarchical root finder for 5th degree polynomial
fn root_find5(da: f32, db: f32, dc: f32, dd: f32, de: f32, df: f32) -> array<f32, 6> {
    // Degree 2
    let r2 = root_find2(10.0 * da, 4.0 * db, dc);

    // Degree 3
    let r3 = cy_find5(r2, 0.0, 0.0, 10.0 * da, 6.0 * db, 3.0 * dc, dd);

    // Degree 4
    let r4 = cy_find5(r3, 0.0, 5.0 * da, 4.0 * db, 3.0 * dc, dd + dd, de);

    // Degree 5
    let r = cy_find5(r4, da, db, dc, dd, de, df);

    return r;
}

fn dot2(v: vec2<f32>) -> f32 {
    return dot(v, v);
}

// Cubic Bezier distance function
fn bezier(p: vec2<f32>, p0: vec2<f32>, p1: vec2<f32>, p2: vec2<f32>, p3: vec2<f32>) -> f32 {
    // Start by testing distance to boundary points
    let dp0 = p0 - p;
    let dp3 = p3 - p;
    var dist = min(dot2(dp0), dot2(dp3));

    // Bezier cubic points to polynomial coefficients
    let a = -p0 + 3.0 * (p1 - p2) + p3;
    let b = 3.0 * (p0 - 2.0 * p1 + p2);
    let c = 3.0 * (p1 - p0);
    let d = p0;

    // Solve D'(t)=0 where D(t) is distance squared
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

// Distance field debug visualization (Inigo Quilez colorscheme)
fn df_debug(d: f32) -> vec3<f32> {
    var col = vec3<f32>(0.0, 0.2, 0.5);
    col *= 0.7 + 0.3 * cos(120.0 * abs(d));
    return mix(col, vec3<f32>(1.0), 1.0 - smoothstep(0.0, 0.02, abs(d)));
}

fn sat(x: f32) -> f32 {
    return clamp(x, 0.0, 1.0);
}

// Render control points and tangent segments
fn points_segments(c_in: vec3<f32>, p: vec2<f32>, p0: vec2<f32>, p1: vec2<f32>, p2: vec2<f32>, p3: vec2<f32>) -> vec3<f32> {
    var c = c_in;

    // Points
    let d0 = dot2(p - p0);
    let d1 = dot2(p - p1);
    let d2 = dot2(p - p2);
    let d3 = dot2(p - p3);
    let d = 0.02 - sqrt(min(min(min(d0, d1), d2), d3));
    c = mix(c, vec3<f32>(1.0, 0.5, 0.0), sat(0.5 + d / fwidth(d)));

    // Segments
    let s0 = sdSegment(p, p0, p1);
    let s1 = sdSegment(p, p2, p3);
    let s = 0.005 - min(s0, s1);
    c = mix(c, vec3<f32>(1.0, 0.5, 0.0), sat(0.5 + s / fwidth(s)) * 0.5);

    return c;
}

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
    let positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 3.0, -1.0),
        vec2<f32>(-1.0,  3.0)
    );
    return vec4<f32>(positions[in_vertex_index], 0.0, 1.0);
}

@fragment
fn fs_main(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    let fragCoord = vec2<f32>(pos.x, uniforms.resolution.y - pos.y);
    var uv = (fragCoord * 2.0 - uniforms.resolution) / uniforms.resolution.y;
    let aspect = uniforms.resolution.x / uniforms.resolution.y;
    uv.x *= aspect;
    let scale = max(aspect, 1.0);
    uv /= scale;

    // Calculate distance to cubic Bezier curve
    let d = bezier(uv, uniforms.p0, uniforms.p1, uniforms.p2, uniforms.p3);

    // Apply distance field visualization
    var o = df_debug(d);

    // Overlay control points and tangent segments
    o = points_segments(o, uv, uniforms.p0, uniforms.p1, uniforms.p2, uniforms.p3);

    // Apply gamma correction
    return vec4<f32>(pow(o, vec3<f32>(1.0 / 2.2)), 1.0);
}
