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

float2 CalculateMotionVector(float4 newPosition, float4 oldPosition, float2 screenSize)
{
	oldPosition /= oldPosition.w;
	oldPosition.xy = (oldPosition.xy + float2(1.0, 1.0)) / float2(2.0f, 2.0f);
	oldPosition.y = 1.0 - oldPosition.y;

	newPosition /= newPosition.w;
	newPosition.xy = (newPosition.xy + float2(1.0, 1.0)) / float2(2.0f, 2.0f);
	newPosition.y = 1.0 - newPosition.y;

	return (newPosition - oldPosition).xy;
}