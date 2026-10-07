#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 frag_p;

uniform vec3 lightColor;
// Direction towards the light (the sun is far: every point gets it from the
// same direction, wherever it is). Only the direction matters, not the length.
uniform vec3 lightPosition;
uniform vec3 viewPosition;

// 0 = lit, 1 = sky dome (unlit, stars twinkle), 2 = emissive (flat colour),
// 3 = procedural sky (horizon/zenith gradient, sun and stars)
uniform int unlit;
uniform float time;
uniform vec3 moonDir;
// Distant geometry fades into this colour (matches the sky at the horizon)
uniform vec3 fogColor;
// Procedural sky (unlit == 3): colour at the zenith, direction to the sun and
// how visible the stars are
uniform vec3 skyZenith;
uniform vec3 sunDir;
uniform float starAlpha;
uniform float forestHorizon; // 1: the procedural sky has a line of trees on the horizon, not dunes

// Spot lights (e.g. the RV's headlights), on top of the directional light:
// position, unit direction it points to, colour, and params = (cos of the
// inner cone, cos of the outer cone, range). Only the first spotCount count.
const int MAX_SPOTS = 8;
uniform int spotCount;
uniform vec3 spotPosition[MAX_SPOTS];
uniform vec3 spotDirection[MAX_SPOTS];
uniform vec3 spotColor[MAX_SPOTS];
uniform vec3 spotParams[MAX_SPOTS];

// The procedural sky draws dunes along the horizon (1) or not (0)
uniform int skyDunes;
// A canopy of leaves above the map (see Environment::canopyMask): 0 = none. The mask's channels
// R, G and B are its planes at canopyHeights.x, .y and .z (1 = open sky, 0 = leaves); it covers the
// square from canopyMin, canopySize a side
uniform int canopyOn;
uniform sampler2D canopyMask;
uniform vec2 canopyMin;
uniform float canopySize;
uniform vec3 canopyHeights;
uniform float canopyStrength;

// 1 = use diffuseColor instead of texture_diffuse1 (untextured materials)
uniform int useColor;
// Opacity of the mesh being drawn (1 = solid); less than 1 is blended
uniform float alpha;
uniform vec3 diffuseColor;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Stars of the procedural sky: one possible star per cell of a grid over the
// sphere (azimuth squeezed by cos(elevation) so the cells stay square)
float stars(vec3 dir) {
	float el = asin(clamp(dir.y, -1.0, 1.0));
	float az = atan(dir.z, dir.x);
	vec2 g = vec2(az * cos(el), el) * 420.0;
	vec2 cell = floor(g);
	float h = hash(cell);
	if (h < 0.965)
		return 0.0;
	vec2 centre = vec2(hash(cell + 7.1), hash(cell + 3.7)) * 0.6 + 0.2;
	float d = length(fract(g) - centre);
	float size = 0.18 + 0.22 * hash(cell + 1.3);
	float bright = 0.45 + 0.55 * hash(cell + 9.9);
	float twinkle = 1.0 - 0.4 * (0.5 + 0.5 * sin(time * 2.5 + h * 400.0));
	return smoothstep(size, 0.0, d) * bright * twinkle;
}

// ---- Dunes on the horizon of the procedural sky ----
// A few ridge lines, one behind the other, painted by azimuth: the farther the
// layer, the lower and hazier (it fades into the fog colour). The noise is
// periodic over the full turn (an integer number of cells), so it has no seam.
float duneNoise(float u, float period, float seed) {
	float i = floor(u);
	float f = fract(u);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash(vec2(mod(i, period), seed));
	float b = hash(vec2(mod(i + 1.0, period), seed));
	return mix(a, b, f);
}

// Height of the ridge of one layer, as the sine of its elevation, towards
// azimuth az: broad, smooth rolling dunes (two scales of soft noise, no sharp
// crests)
float duneHeight(float az, float period, float seed, float base, float amp) {
	float u = az / 6.2831853 * period;
	float broad = duneNoise(u, period, seed);
	float soft = duneNoise(u * 2.0, period * 2.0, seed + 3.0);
	return base + amp * (0.75 * broad + 0.25 * soft);
}

// Paints the dunes over the sky colour c, for the view direction dir
vec3 dunes(vec3 c, vec3 dir) {
	const vec3 SAND = vec3(0.78, 0.62, 0.40);
	float az = atan(dir.z, dir.x);
	float e = dir.y;
	vec2 vd = normalize(dir.xz + vec2(1e-5));
	// Sun side: the dunes that face it are lit, those against it dark
	vec2 sh = sunDir.xz;
	float sunAz = atan(sh.y, sh.x);
	float sunSide = length(sh) > 1e-4 ? dot(vd, normalize(sh)) * length(sh) : 0.0;
	// the same light as the ground has, so that the dunes meet the terrain
	float top = 0.3 + 0.9 * clamp(sunDir.y, 0.0, 1.0);

	// far -> near: period (dunes around the circle), seed, base, amplitude, haze
	const int LAYERS = 3;
	float period[LAYERS] = float[](9.0, 15.0, 25.0);
	float seed[LAYERS]   = float[](11.0, 47.0, 83.0);
	float base[LAYERS]   = float[](0.036, 0.024, 0.014);
	float amp[LAYERS]    = float[](0.040, 0.030, 0.020);
	float haze[LAYERS]   = float[](0.60, 0.42, 0.25);
	for (int i = 0; i < LAYERS; i++) {
		float h = duneHeight(az, period[i], seed[i], base[i], amp[i]);
		float d = 0.03;
		float slope = (duneHeight(az + d, period[i], seed[i], base[i], amp[i]) -
		               duneHeight(az - d, period[i], seed[i], base[i], amp[i])) / (2.0 * d);
		// facing the sun: a slope that rises towards +az faces -az
		float facing = clamp(-slope * 4.0 * sign(sin(sunAz - az)), -1.0, 1.0);
		// (soft: far dunes have little contrast, and are not as bright as the ground)
		float shade = clamp(0.8 * top * (1.0 - 0.10 * sunSide + 0.08 * facing), 0.0, 1.2);
		vec3 col = SAND * shade * lightColor;
		// the haze of a desert is dusty: the fog colour with some sand in it
		col = mix(col, mix(fogColor, SAND * top * lightColor * 1.1, 0.4), haze[i]);
		// a thin soft edge so the ridge is not jagged
		float cover = 1.0 - smoothstep(h - 0.0010, h + 0.0010, e);
		c = mix(c, col, cover);
	}
	return c;
}

// How much of the far light reaches the point p through the canopy (1 = all): the ray from p
// towards the light crosses each of its planes, and each one lets through what its mask says
float canopyLight(vec3 p, vec3 toLight) {
	if (canopyOn == 0)
		return 1.0;
	vec3 l = toLight;
	l.y = max(l.y, 0.08); // (with the light low, the shadows stretch, but not for ever)
	float through = 1.0;
	for (int i = 0; i < 3; i++) {
		float t = (canopyHeights[i] - p.y) / l.y;
		if (t <= 0.0)
			continue; // the point is above this plane
		vec2 uv = ((p + l * t).xz - canopyMin) / canopySize;
		if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
			continue; // beyond the canopy: open sky
		through *= texture(canopyMask, uv)[i];
	}
	return mix(1.0, through, canopyStrength);
}

// ---- A forest on the horizon of the procedural sky ----
// Layers of conifer tops, one behind the other (far = lower and hazier): each
// layer has trees of random heights, every tree a pointed triangle, painted by
// azimuth. The cells are an integer number per turn, so there is no seam.
float treeHeight(float az, float period, float seed, float base, float amp) {
	float u = az / 6.2831853 * period;
	float cell = floor(u);
	float tri = 1.0 - abs(fract(u) * 2.0 - 1.0);
	float tall = hash(vec2(mod(cell, period), seed));
	float broad = duneNoise(az / 6.2831853 * 12.0, 12.0, seed + 5.0);
	return base + amp * (0.35 + 0.65 * tall) * pow(tri, 0.9) + 0.012 * broad;
}

vec3 forest(vec3 c, vec3 dir) {
	const vec3 GREEN = vec3(0.07, 0.17, 0.08);
	float az = atan(dir.z, dir.x);
	float e = dir.y;
	float top = 0.3 + 0.9 * clamp(sunDir.y, 0.0, 1.0);
	// far -> near: trees around the circle, seed, base, amplitude, haze
	const int LAYERS = 3;
	float period[LAYERS] = float[](420.0, 300.0, 210.0);
	float seed[LAYERS]   = float[](21.0, 57.0, 93.0);
	float base[LAYERS]   = float[](0.006, 0.003, 0.0);
	float amp[LAYERS]    = float[](0.030, 0.034, 0.040);
	float haze[LAYERS]   = float[](0.65, 0.42, 0.22);
	for (int i = 0; i < LAYERS; i++) {
		float h = treeHeight(az, period[i], seed[i], base[i], amp[i]);
		vec3 col = GREEN * (0.55 + 0.45 * float(i + 1) / float(LAYERS)) * top * lightColor * 1.5;
		col = mix(col, mix(fogColor, GREEN * top * lightColor, 0.35), haze[i]);
		float cover = 1.0 - smoothstep(h - 0.0008, h + 0.0008, e);
		c = mix(c, col, cover);
	}
	return c;
}

void main() {
	if (unlit == 3) {
		vec3 dir = normalize(frag_p - viewPosition);
		float up = clamp(dir.y, 0.0, 1.0);
		vec3 c = mix(fogColor, skyZenith, pow(up, 0.6));
		// nothing below the horizon: just the horizon colour
		float above = smoothstep(-0.01, 0.06, dir.y);
		float sunCos = dot(dir, sunDir);
		vec3 warm = mix(vec3(1.0, 0.45, 0.15), vec3(1.0, 0.95, 0.8),
		                smoothstep(0.0, 0.45, sunDir.y));
		float sunVisible = smoothstep(-0.12, 0.0, sunDir.y);
		c += warm * (pow(max(sunCos, 0.0), 24.0) * 0.35 +
		             pow(max(sunCos, 0.0), 400.0) * 0.6) * sunVisible;
		c = mix(c, vec3(1.0, 0.97, 0.88), smoothstep(0.99955, 0.99975, sunCos) * above);
		c += vec3(0.9, 0.95, 1.0) * stars(dir) * starAlpha * above;
		if (forestHorizon > 0.5)
			c = forest(c, dir);
		else if (skyDunes == 1)
			c = dunes(c, dir); // over the sun and the stars: they set behind them
		FragColor = vec4(c, 1.0);
		return;
	}
	if (unlit == 2) {
		FragColor = vec4(useColor == 1 ? diffuseColor : texture(texture_diffuse1, TexCoord).rgb, 1.0);
		return;
	}
	if (unlit == 1) {
		vec3 c = texture(texture_diffuse1, TexCoord).rgb;
		vec3 dir = normalize(frag_p - viewPosition);
		// Twinkle the stars (bright texels), but not the moon and its glow
		float star = smoothstep(0.35, 0.9, max(c.r, max(c.g, c.b)));
		float away = 1.0 - smoothstep(0.985, 0.995, dot(dir, moonDir));
		float phase = hash(floor(TexCoord * vec2(1024.0, 512.0))) * 6.2831;
		float tw = 1.0 - 0.45 * star * away * (0.5 + 0.5 * sin(time * 2.5 + phase));
		FragColor = vec4(c * tw, 1.0);
		return;
	}
	float ambientStrength = 0.3;
	float specularStrength = 0.15;

	vec3 lightdir = normalize(lightPosition);
	vec3 norm = normalize(normal);
	vec3 viewDir = normalize(viewPosition - frag_p);
	vec3 reflectDir = reflect(-lightdir, norm);  	
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128);
	vec3 specular = specularStrength * spec * lightColor;
	float diff = max(dot(norm, lightdir), 0.0);
	float leaves = canopyLight(frag_p, lightdir); // (the shadows of a canopy, if there is one)
	vec3 diffuse = diff * lightColor * leaves;
	vec3 ambient = ambientStrength*lightColor;
	// (a texture with transparency, e.g. cracked glass, makes the mesh as transparent as it
	// is, on top of the material's own opacity: it only shows for a translucent mesh)
	vec4 texel = texture(texture_diffuse1, TexCoord);
	// (a texture with holes, like a leaf card, cuts them out of an opaque mesh)
	if (useColor == 0 && alpha >= 1.0 && texel.a < 0.35)
		discard;
	vec3 base = useColor == 1 ? diffuseColor : texel.rgb;
	float outAlpha = alpha * (useColor == 1 ? 1.0 : texel.a);
	vec3 spots = vec3(0.0);
	for (int i = 0; i < spotCount; i++) {
		vec3 toLight = spotPosition[i] - frag_p;
		float dist = length(toLight);
		vec3 l = toLight / dist;
		// 1 inside the inner cone, falling to 0 at the outer one
		float cone = smoothstep(spotParams[i].y, spotParams[i].x, dot(-l, spotDirection[i]));
		float fall = clamp(1.0 - dist / spotParams[i].z, 0.0, 1.0);
		float k = cone * fall * fall;
		float sd = max(dot(norm, l), 0.0);
		float ss = pow(max(dot(viewDir, reflect(-l, norm)), 0.0), 64);
		spots += k * (sd + specularStrength * ss) * spotColor[i];
	}
	vec3 lit = base*(ambient+diffuse+specular*leaves+spots);
	// (no distance fog: the sky is only fogColor at the horizon)
	FragColor = vec4(lit, outAlpha);
}