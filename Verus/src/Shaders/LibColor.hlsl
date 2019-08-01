// See: https://en.wikipedia.org/wiki/Grayscale
float Greyscale(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 Desaturate(float3 color, float alpha, float lum = 1.0)
{
	const float grey = Greyscale(color)*lum;
	return lerp(color, grey, alpha);
}
