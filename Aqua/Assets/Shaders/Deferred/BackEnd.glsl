
// todo: incomplete shader when it comes to lighting

// Insert the outputs here...
layout(set = 0, binding = 0, IMAGE_FORMAT) uniform image2D uFragImage;

vec4 poissonBlur(sampler2D image, vec2 uv, uvec2 resolution, float radius) {
    // Poisson disk offsets � precomputed sample pattern
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
    );

    vec4 color = vec4(0.0);
    float total = 0.0;

    for (int i = 0; i < 16; i++) {
        vec2 offset = poissonDisk[i] * radius / vec2(resolution);
        color += texture(image, uv + offset);
        total += 1.0;
    }

    return color / total;
}

float CalcShadow(in vec3 position, float LdotN, uint idx)
{
    const float bias = 0.01;

    vec4 orthoPosition = uDepthCameras[idx].Projection * uDepthCameras[idx].View * vec4(position, 1.0);

    vec2 texCoord = (orthoPosition.xy + vec2(1.0)) * 0.5;

    //float occlusionDepth = texture(uDepthMap[idx], texCoord).r;

    float occlusionDepth = poissonBlur(uDepthMap[idx], texCoord, uvec2(2048, 2048), 1.0).r;

    // No blurring yet
    if(orthoPosition.z > occlusionDepth + bias * (1.1 - LdotN))
        return 0.0;

    return 1.0;
}

vec3 GetCameraPosition()
{
    mat4 Transposed = transpose(uCamera.View);
    vec3 a = Transposed[0].xyz;
    vec3 b = Transposed[1].xyz;
    vec3 c = Transposed[2].xyz;

    return -(uCamera.View[3].x * a + uCamera.View[3].y * b + uCamera.View[3].z * c);
}

void main()
{
    //uvec2 pSize = { 2048, 2048 };
    uvec2 TexelPos = gl_GlobalInvocationID.xy;

    if(TexelPos.x >= pSize.x || TexelPos.y >= pSize.y)
        return;

    vec2 TexCoords = vec2(TexelPos) / pSize;

    vec4 PositionValue = texture(uPositions, TexCoords);
    vec4 NormalValue = texture(uNormals, TexCoords);
    vec4 TexCoordValue = texture(uTexCoords, TexCoords);
    vec4 TangentValue = texture(uTangents, TexCoords);
    vec4 BitangentValue = texture(uBitangents, TexCoords);

    uvec3 RenderableMetaData = uvec3(PositionValue.w + 0.5, NormalValue.w + 0.5, TexCoordValue.w + 0.5);
    vec3 Position = PositionValue.xyz;
    vec3 Normal = normalize(NormalValue.xyz);
    vec3 Tangent = normalize(TangentValue.xyz);
    vec3 Bitangent = normalize(BitangentValue.xyz);
    vec3 TexCoord = TexCoordValue.xyz;

    // return if the material index doesn't match
    if(RenderableMetaData.g != pMaterialRef)
        return;

    BSDFInput bsdfInput;

    bsdfInput.ViewDir = normalize(GetCameraPosition() - Position.xyz);
    bsdfInput.Normal = Normal.xyz;

    vec4 glPos = uCamera.Projection * uCamera.View * vec4(PositionValue.xyz, 1.0);

    // if the fragment is beyond or at the max depth, we quit
    if(length(NormalValue.xyz) < SHADING_TOLERANCE)
    {
        // setting the skyblue color
        //imageStore(uFragImage, ivec2(TexelPos), vec4(0.0, 1.0, 1.0, 1.0));
        return;    
    }


    // all fragments correspond to objects within the space
    vec3 Color = vec3(0.0);
    
    for(uint i = 0; i < pDirectionalLightCount; i++)
    {
        bsdfInput.LightDir = -normalize(sDirectionalLights[i].Direction);
        float LdotN = dot(bsdfInput.LightDir, bsdfInput.Normal);
        //Color += Evaluate(bsdfInput) * CalcShadow(Position, LdotN, i);
        Color += Evaluate(bsdfInput);

        //FragColor = vec4(1.0);
        //FragColor.xyz = normalize(GetCameraPosition() - Position.xyz);
    }

    for(uint i = 0; i < pPointLightCount; i++)
    {
        vec3 LightDisplace = sPointLights[i].Position - Position.xyz;

        bsdfInput.LightDir = -normalize(LightDisplace);

        float Distance = length(LightDisplace);
        float Attenuation = Distance * (Distance * sPointLights[i].DropRate.x + sPointLights[i].DropRate.y);

        Color += Evaluate(bsdfInput) / Attenuation;
        //FragColor = vec4(1.0);
    }

    imageStore(uFragImage, ivec2(TexelPos), vec4(Color, 1.0));
}
