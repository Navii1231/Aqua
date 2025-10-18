#ifndef UTILS_GLSL
#define UTILS_GLSL

vec2 GetTexCoords(in Face face, in CollisionInfo collisionInfo)
{
	vec2 t1 = sTexCoords[face.Indices.r];
	vec2 t2 = sTexCoords[face.Indices.g];
	vec2 t3 = sTexCoords[face.Indices.b];

	return t1 * collisionInfo.bCoords.r
		+ t2 * collisionInfo.bCoords.g
		+ t3 * collisionInfo.bCoords.b;
}

#endif