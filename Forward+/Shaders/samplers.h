#ifndef SAMPLERS_H
#define SAMPLERS_H

SamplerState s_LinearWrap : register(s0);
SamplerState s_AnisoWrap : register(s1);
SamplerState s_PointWrap : register(s2);
SamplerState s_PointBorder : register(s3);
SamplerState s_LinearBorder : register(s4);
SamplerState s_PointClamp : register(s5);

#endif // SAMPLERS_H