uniform sampler1D tex;
uniform vec2 c;
uniform int iter;

void main() {
	vec2 z;
	z.x = 3.0 * (gl_TexCoord[0].x - 0.5);
	z.y = 2.0 * (gl_TexCoord[0].y - 0.5);

	int i;
	for(i=0; i<iter; i++) {
		float x = (z.x * z.x - z.y * z.y) + c.x;
		float y = (z.y * z.x + z.x * z.y) + c.y;

		if((x * x + y * y) > 4.0) break;
		z.x = x;
		z.y = y;
	}

	gl_FragColor = texture1D(tex, (i == iter ? 0.0 : float(i)) / 100.0);
}
