float4 GetClipPos(float3 _modelPos, float4x4 modelToWorld, Camera camera)
{
	const float4 modelPos = float4(_modelPos, 1.0f);
	const float4 worldPos = mul(modelPos, modelToWorld);
	const float4 viewPos = mul(worldPos, camera.WorldToView);
	const float4 clipPos = mul(viewPos, camera.ViewToClip);
	return clipPos;
}

float4 GetClipPosWithJitter(float3 modelPos, float4x4 modelToWorld, Camera camera)
{
	const float4 clipPos = GetClipPos(modelPos, modelToWorld, camera);
	const float4 clipPosWithJitter = clipPos + float4(camera.Jitter, 0.0f, 0.0f) * clipPos.w;
	return clipPosWithJitter;
}