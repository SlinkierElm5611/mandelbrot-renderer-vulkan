#version 450

layout(location = 0) in vec2 inPosition;

layout(binding = 0) uniform RenderAreaInformation {
    vec2 renderAreaSize;
    vec2 renderAreaOffset;
    uint numberOfIterations;
};

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 c = vec2(renderAreaOffset.x + inPosition.x * renderAreaSize.x,
            renderAreaOffset.y + inPosition.y * renderAreaSize.y);
    vec2 z = vec2(0.0, 0.0);
    gl_FragDepth = 0;
    for (uint i = 0; i < numberOfIterations; i++)
    {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (dot(z, z) > 4.0)
        {
            i = i % 12;
            if(i==0){
                outColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
            else if(i==1){
                outColor = vec4(0.5, 0.0, 0.0, 1.0);
            }
            else if(i==2){
                outColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
            else if(i==3){
                outColor = vec4(1.0, 0.5, 0.0, 1.0);
            }
            else if(i == 4){
                outColor = vec4(1.0, 1.0, 0.0, 1.0);
            }
            else if(i==5){
                outColor = vec4(1.0, 1.0, 0.5, 1.0);
            }
            else if(i==6){
                outColor = vec4(1.0, 1.0, 1.0, 1.0);
            }else if(i==7){
                outColor = vec4(0.5, 1.0, 1.0, 1.0);
            }else if(i==8){
                outColor = vec4(0.0, 1.0, 1.0, 1.0);
            }else if(i==9){
                outColor = vec4(0.0, 0.5, 1.0, 1.0);
            }else if(i==10){
                outColor = vec4(0.0, 0.0, 1.0, 1.0);
            }else if(i==11){
                outColor = vec4(0.5, 0.0, 1.0, 1.0);
            }
            return;
        }
    }
}
